#include <algorithm>
#include <cassert>
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

enum class STM { WHITE, BLACK, NONE };

STM inverted_stm(STM stm) {
  assert(stm != STM::NONE);
  return stm == STM::WHITE ? STM::BLACK : STM::WHITE;
}

STM fen_to_stm(const std::string &fen) {
  return fen.find(" w ") != std::string::npos ? STM::WHITE : STM::BLACK;
}

enum class MinPlyType { INIT, SINGLE, DUAL, NONE };

struct CDB {
  DB *db;
  MinPlyType min_ply_type;
};

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
  // table_options.block_cache = NewLRUCache(32 * 1024 * 1024 * 1024LL);
  table_options.block_cache = NewClockCache(32 * 1024 * 1024 * 1024LL);
  // table_options.no_block_cache = true;
  Options options;
  options.IncreaseParallelism();
  options.table_factory.reset(
      NewTerarkZipTableFactory(tzt_options, options.table_factory));

  CDB *cdb = new CDB;

  // open DB
  Status s = DB::OpenForReadOnly(options, path, &cdb->db);
  if (!s.ok()) {
    std::cerr << s.ToString() << std::endl;
    std::exit(1);
  }
  const auto handle = reinterpret_cast<std::uintptr_t>(cdb);

  // detect the encoding scheme for min_ply with a one-off query of startpos
  cdb->min_ply_type = MinPlyType::INIT;
  const auto startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
  auto result = cdbdirect_get(handle, startpos);
  int ply = result.back().second;
  switch (ply) {
  case 0:
    // the legacy scheme: one min_ply for both fen and BWfen:
    // heuristic measure for distance to root, not exact
    cdb->min_ply_type = MinPlyType::SINGLE;
    break;
  case 256: // 0x0100: hi = 1, lo = 0(unset)
    // one min_ply each for fen and BWfen:
    // exact upper bound for distance to root and proof of legality
    cdb->min_ply_type = MinPlyType::DUAL;
    break;
  default:
    cdb->min_ply_type = MinPlyType::NONE;
    std::cerr << "Could not detect min_ply encoding scheme, ply = " << ply
              << std::endl;
    std::cerr << "cdbdirect will convert any min_ply to -1." << std::endl;
    break;
  }

  return handle;
}

// Return the size of the DB
std::uint64_t cdbdirect_size(std::uintptr_t handle) {

  CDB *cdb = reinterpret_cast<CDB *>(handle);

  std::uint64_t size[1];
  cdb->db->GetIntProperty("rocksdb.estimate-num-keys", size);

  return size[0];
}

// Finalize the DB
std::uintptr_t cdbdirect_finalize(std::uintptr_t handle) {

  CDB *cdb = reinterpret_cast<CDB *>(handle);

  // safely close the DB.
  delete cdb->db;
  delete cdb;

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
// given a db key, return the fen corresponding to the key, and its BW mirror
//
std::pair<std::string, std::string> key_to_fens(const std::string &key) {
  // key starts with 'h' followed by binary hexfen
  assert(key.size() > 1);
  assert(key[0] == 'h');
  auto hexfen = bin2hex(key.substr(1));
  auto fen = cbhexfen2fen(hexfen);
  auto BWfen = cbgetBWfen(fen);
  return {fen, BWfen};
}

//
// Turn the value string into a vector of scored moves, sorted by score.
// The In/Out variable fen_stm indicates which of fen and BWfen to choose.
// If fen_stm == STM::NONE, then the function itself picks a fen.
//
std::vector<std::pair<std::string, int>>
value_to_scoredMoves(const std::string &value, STM key_stm, STM &fen_stm,
                     MinPlyType min_ply_type) {

  if (value.empty()) {
    // signal failed probe
    return {{"a0a0", -2}};
  }

  // decode the value to scoredMoves
  std::vector<StrPair> scoredMoves;
  get_hash_values(value, scoredMoves);

  std::vector<std::pair<std::string, int>> result;
  result.reserve(scoredMoves.size());

  int white_ply = -1, black_ply = -1;
  for (auto &pair : scoredMoves) {
    if (pair.first == "a0a0") {
      // the special move a0a0 encodes min_ply
      int ply = std::stoi(pair.second);
      switch (min_ply_type) {
      case MinPlyType::INIT:
        //
        // only called by cdbdirect_initialize(), to detect the min_ply scheme
        //
        return {{"a0a0", ply}};

      case MinPlyType::SINGLE:
        //
        // the legacy scheme: one min_ply for both fen and BWfen
        //

        // legacy may have rare overflows into the negative numbers
        ply = std::max(ply, -1);

        if (ply >= 0) {
          white_ply = ply % 2 ? ply + 1 : ply;
          black_ply = ply % 2 ? ply : ply + 1;
        }
        break;

      case MinPlyType::DUAL: {
        //
        // one min_ply each for fen and BWfen: ply = hi|lo = n_white|n_black,
        // with wtm ply = 2 (n_white - 1) and btm ply = 2 n_black - 1
        // n_white = 0 and n_black = 0 indicate that the value is undefined
        //

        white_ply = std::max(2 * (ply >> 8) - 2, -1);
        black_ply = 2 * (ply & 0xFF) - 1;
        break;
      }

      case MinPlyType::NONE:
        //
        // unable to decode the min_ply value, falling back to -1
        //
        break;
      }
    } else
      result.push_back({fen_stm == STM::NONE || fen_stm == key_stm
                            ? pair.first
                            : cbgetBWmove(pair.first),
                        backprop_score(std::stoi(pair.second))});
  }

  // for the iterator, pick the fen that is reachable (in fewer plies)
  // if none of the two is reachable, by default pick the key's fen
  if (fen_stm == STM::NONE) {
    if (white_ply >= 0 && (black_ply < 0 || white_ply < black_ply))
      fen_stm = STM::WHITE;
    else if (black_ply >= 0 && (white_ply < 0 || white_ply > black_ply))
      fen_stm = STM::BLACK;
    else
      fen_stm = key_stm;

    // if fen stm and key stm differ, adjust the move notations
    if (fen_stm != key_stm)
      for (auto &pair : result)
        pair.first = cbgetBWmove(pair.first);
  }

  // sort moves and add ply distance
  std::sort(
      result.begin(), result.end(),
      [](const std::pair<std::string, int> &a,
         const std::pair<std::string, int> &b) { return a.second > b.second; });

  result.push_back({"a0a0", fen_stm == STM::WHITE ? white_ply : black_ply});

  return result;
}

// Probe the DB, get back a vector of moves containing the known scored moves of
// cdb fen: a position fen *without move counters* (as they have no meaning in
// cdb). The result vector contains pairs of moves (in uci notation) with their
// score, sorted. As last element the vector always contains the special move
// a0a0 with score: -2  (pos not in db), -1  (no known distance to root),
// >=0 (shortest known distance to root).
std::vector<std::pair<std::string, int>> cdbdirect_get(std::uintptr_t handle,
                                                       const std::string &fen) {

  CDB *cdb = reinterpret_cast<CDB *>(handle);

  // The fen or its black-white mirrored equivalent is to be probed,
  // depending on their hexfen order
  std::string hexfen = cbfen2hexfen(fen);
  std::string BWfen = cbgetBWfen(fen);
  std::string BWhexfen = cbfen2hexfen(BWfen);
  STM fen_stm = fen_to_stm(fen);
  STM key_stm = hexfen < BWhexfen ? fen_stm : inverted_stm(fen_stm);

  // generate the binary fen with prefix 'h' as key, and get the value
  std::string key = 'h' + hex2bin(std::min(hexfen, BWhexfen));

  std::string value;
  ReadOptions read_options;
  read_options.verify_checksums = false;
  Status s = cdb->db->Get(read_options, key, &value);

  // decode the answer if we have a hit, otherwise signal failed probe
  return value_to_scoredMoves(s.ok() ? value : "", key_stm, fen_stm,
                              cdb->min_ply_type);
}

//
// given a range, iterate over it, calling evaluate_entry for each entry
//
void IterateRange(
    CDB *cdb, const RangeStorage &range,
    const std::function<bool(const std::string &,
                             const std::vector<std::pair<std::string, int>> &)>
        &evaluate_entry) {

  const Comparator *cmp = cdb->db->GetOptions().comparator;
  ReadOptions read_options;
  read_options.verify_checksums = false;
  std::unique_ptr<Iterator> it(cdb->db->NewIterator(read_options));

  for (it->Seek(range.start);
       it->Valid() && (cmp->Compare(it->key(), range.limit) < 0); it->Next()) {

    // get the key's fen and BWfen
    auto fens = key_to_fens(it->key().ToString());
    STM key_stm = fen_to_stm(fens.first), fen_stm = STM::NONE;

    auto scored = value_to_scoredMoves(it->value().ToString(), key_stm, fen_stm,
                                       cdb->min_ply_type);

    if (!evaluate_entry(key_stm == fen_stm ? fens.first : fens.second, scored))
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
            ? last_key_str +
                  '\xff' // Adding '0xFF' to ensure inclusion of last key
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

  CDB *cdb = reinterpret_cast<CDB *>(handle);

  auto ranges = BuildRangesFromSSTs(cdb->db, num_threads);

  std::vector<std::thread> workers;
  for (auto &r : ranges) {
    workers.emplace_back(IterateRange, cdb, r, evaluate_entry);
  }
  for (auto &t : workers)
    t.join();
}
