#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

#include "cdbdirect.h"
#include "fen2cdb.h"

using namespace TERARKDB_NAMESPACE;

// Initialize the DB given a path, and return a handle for later use
std::uintptr_t cdbdirect_initialize(const std::string &path) {

  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(32 * 1024 * 1024 * 1024LL);
  // table_options.no_block_cache = true;
  Options options;
  options.IncreaseParallelism();
  options.table_factory.reset(NewBlockBasedTableFactory(table_options));

  DB *db;

  // open DB
  Status s = DB::OpenForReadOnly(options, path, &db);
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());

  return reinterpret_cast<std::uintptr_t>(db);
}

// Return the size of the DB
std::uint64_t cdbdirect_size(std::uintptr_t handle) {

  DB *db = reinterpret_cast<DB *>(handle);

  std::uint64_t size[1];
  db->GetIntProperty("rocksdb.estimate-num-keys", size);

  return size[0];
}

// Finalize the DB
std::uintptr_t cdbdirect_finalize(std::uintptr_t handle) {

  DB *db = reinterpret_cast<DB *>(handle);

  // safely close the DB.
  delete db;

  return 0;
}

// scores outside of [-15000, 15000] are assumed to be (cursed) TB wins or mates,
// apart from stalemates, which are encoded with -30001
int backprop_score(int child_score) {
  if (child_score == -30001)
    return 0;
  if (child_score >= 15000)
    return -child_score + 1;
  if (child_score <= -15000)
    return -child_score - 1;
  return -child_score;
}

// Probe the DB, get back a vector of moves containing the known scored moves of
// cdb fen: a position fen *without move counters* (as they have no meaning in
// cdb) The result vector contains pairs of moves (algebraic notation) with
// their score, sorted. The result vector always contains as last element one
// special move a0a0 with score: -2  (pos not in db), -1  (no known distance to
// root)
// >=0 (shortest known distance to root)
std::vector<std::pair<std::string, int>> cdbdirect_get(std::uintptr_t handle,
                                                       const std::string &fen) {

  DB *db = reinterpret_cast<DB *>(handle);

  std::vector<std::pair<std::string, int>> result;

  // The fen or its black-white mirrored equivalent is to be probed,
  // depending on their hexfen order
  std::string inputfen = fen;
  std::string hexfen = cbfen2hexfen(inputfen);
  std::string BWfen = cbgetBWfen(inputfen);
  std::string BWhexfen = cbfen2hexfen(BWfen);
  bool natural_order = hexfen < BWhexfen;

  // generate the binary fen with prefix 'h' has key
  std::string key = 'h' + hex2bin(natural_order ? hexfen : BWhexfen);

  // get value (prefix binary fen by 'h')
  std::string value;
  Status s = db->Get(ReadOptions(), key, &value);

  // If we have a hit decode the answer
  if (s.ok()) {
    // decode the value to scoredMoves
    std::vector<StrPair> scoredMoves;
    get_hash_values(value, scoredMoves);

    result.reserve(scoredMoves.size());

    // convert, the special move a0a0 encodes distance to root
    int ply = -1;
    for (auto &pair : scoredMoves) {
      if (pair.first != "a0a0") {
        result.push_back(
            std::make_pair(natural_order ? pair.first : cbgetBWmove(pair.first),
                           backprop_score(std::stoi(pair.second))));
      } else {
        ply = std::stoi(pair.second);
      }
    }

    // sort moves
    std::sort(result.begin(), result.end(),
              [](const std::pair<std::string, int> &a,
                 const std::pair<std::string, int> &b) {
                return a.second > b.second;
              });

    // add ply distance
    result.push_back(std::make_pair(std::string("a0a0"), ply));

  } else {
    // signal failed probe.
    result.push_back(std::make_pair(std::string("a0a0"), -2));
  }

  return result;
}
