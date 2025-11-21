#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "table/terark_zip_table.h"

#include "cdbdirect.h"
#include "fen2cdb.h"

using namespace TERARKDB_NAMESPACE;

// Initialize the DB given a path, and return a handle for later use
std::uintptr_t cdbdirect_initialize(const std::string &path) {

  TerarkZipTableOptions tzt_options;
  // TerarkZipTable requires a temp directory other than data directory, a slow
  // device is acceptable
  tzt_options.localTempDir = "/tmp";
  tzt_options.warmUpIndexOnOpen = false;

  // minPreadLen=-1, read from mmap
  // minPreadLen=0, read with pread
  // cacheCapacityBytes>0, read with pread+O_DIRECT into own page cache
  // indexCacheRatio is louds cache, precomputed near top level offsets for tree
  // walk, cpu optimization at the cost of ram with bbt, you just raise
  // blockcache, which is somewhat equivilent to cacheCapacityBytes iterators,
  // bbt is entirely sequential, new format is roughly sequential on keys
  // sequential on values, i.e. just index walk costs

  tzt_options.minPreadLen = 0;
  tzt_options.indexCacheRatio = 0.000;
  tzt_options.cacheCapacityBytes = 1 * 1024 * 1024 * 1024LL;

  BlockBasedTableOptions table_options;
  table_options.block_cache = NewLRUCache(32 * 1024 * 1024 * 1024LL);
  // table_options.no_block_cache = true;
  Options options;
  options.IncreaseParallelism();
  options.table_factory.reset(
      NewTerarkZipTableFactory(tzt_options, options.table_factory));

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

// scores outside of [-15000, 15000] are assumed to be (cursed) TB wins or
// (possibly incorrect) mates, apart from TB draws and stalemates, which are
// both encoded with -30001
int backprop_score(int child_score) {
  if (child_score == -30001)
    return 0;
  if (child_score >= 15000)
    return -child_score + 1;
  if (child_score <= -15000)
    return -child_score - 1;
  return -child_score;
}

//
// given a db key, return the corresponding fen.
// given there are two possible fens (with equivalent scored moves), pick the
// one depending on the natural_order
//
std::string key_to_fen(const std::string &key, bool natural_order) {
  // key starts with 'h' followed by binary hexfen
  assert(key.size() > 1);
  assert(key[0] == 'h');
  auto hexfen = bin2hex(key.substr(1));
  auto fen = cbhexfen2fen(hexfen);
  auto BWfen = cbgetBWfen(fen);
  std::string BWhexfen = cbfen2hexfen(BWfen);
  return (hexfen < BWhexfen) == natural_order ? fen : BWfen;
}

//
// Turn the value string into a vector of scored moves, sorted by score
// natural_order must match the order used to to turn the key into a fen
//
std::vector<std::pair<std::string, int>>
value_to_scoredMoves(const std::string &value, bool natural_order) {

  std::vector<std::pair<std::string, int>> result;

  // If we have a hit decode the answer
  if (!value.empty()) {
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
  ReadOptions read_options;
  read_options.verify_checksums = false;
  Status s = db->Get(read_options, key, &value);

  // If we have a hit decode the answer
  if (s.ok()) {
    return value_to_scoredMoves(value, natural_order);
  }

  // signal failed probe.
  return value_to_scoredMoves("", natural_order);
}

//
// given a range, iterate over it, calling evaluate_entry for each entry
//
void IterateRange(
    DB *db, const RangeStorage &range,
    const std::function<bool(const std::string &,
                             const std::vector<std::pair<std::string, int>> &)>
        &evaluate_entry) {

  const Comparator *cmp = db->GetOptions().comparator;
  ReadOptions read_options;
  read_options.verify_checksums = false;
  std::unique_ptr<Iterator> it(db->NewIterator(read_options));

  for (it->Seek(range.start);
       it->Valid() && (cmp->Compare(it->key(), range.limit) < 0); it->Next()) {

    // here we pick natural_order = true, but in principle the same db entry
    // could be used to return two equivalent (but different) fens
    auto fen = key_to_fen(it->key().ToString(), true);
    auto scored = value_to_scoredMoves(it->value().ToString(), true);
    if (!evaluate_entry(fen, scored))
      break;
  }
}

//
// Create ranges from the SST files in the DB, and partition them evenly for all
// threads
//
std::vector<RangeStorage> BuildRangesFromSSTs(DB *db, size_t num_threads) {

  ReadOptions read_options;
  read_options.verify_checksums = false;
  std::unique_ptr<Iterator> it(db->NewIterator(read_options));
  it->SeekToLast();
  std::string last_key_str = it->key().ToString();

  std::vector<LiveFileMetaData> files;
  db->GetLiveFilesMetaData(&files);
  const Comparator *cmp = db->GetOptions().comparator;

  // Sort globally by smallest key (ignore level for iteration ordering)
  std::sort(files.begin(), files.end(),
            [&cmp](const LiveFileMetaData &a, const LiveFileMetaData &b) {
              return cmp->Compare(a.smallestkey, b.smallestkey) < 0;
            });

  // Turn into non-overlapping ranges
  std::vector<RangeStorage> merged;
  for (size_t i = 0; i < files.size(); ++i) {
    merged.push_back(RangeStorage(
        files[i].smallestkey,
        (i + 1 == files.size())
            ? last_key_str + "z" // Adding 'z' to ensure inclusion of last key
            : files[i + 1].smallestkey));
  }

  // Now partition merged ranges evenly by count or total file size
  // Partition merged ranges as evenly as possible among threads
  size_t num_ranges = std::min(num_threads, merged.size());
  std::vector<RangeStorage> out;
  size_t per_range = merged.size() / num_ranges;
  size_t remainder = merged.size() % num_ranges;
  size_t idx = 0;
  for (size_t i = 0; i < num_ranges; ++i) {
    size_t chunk = per_range + (i < remainder ? 1 : 0);
    out.push_back(
        RangeStorage(merged[idx].start, merged[idx + chunk - 1].limit));
    idx += chunk;
  }

  return out;
}

//
// apply the given function to all entries in the DB, using multiple threads
// the function receives the fen and the scored moves vector, and can return
// false to stop iteration early
//
void cdbdirect_apply(
    std::uintptr_t handle, size_t num_threads,
    const std::function<bool(const std::string &,
                             const std::vector<std::pair<std::string, int>> &)>
        &evaluate_entry) {

  DB *db = reinterpret_cast<DB *>(handle);

  auto ranges = BuildRangesFromSSTs(db, num_threads);

  std::vector<std::thread> workers;
  for (auto &r : ranges) {
    workers.emplace_back(IterateRange, db, r, evaluate_entry);
  }
  for (auto &t : workers)
    t.join();
}
