#include <atomic>
#include <cstdint>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "table/terark_zip_table.h"

using namespace TERARKDB_NAMESPACE;

// Setup stuff

// Initialize the DB given a path, and return a handle for later use
std::uintptr_t cdbdirect_initialize(const std::string &path) {

  TerarkZipTableOptions tzt_options;
  // TerarkZipTable requires a temp directory other than data directory, a slow
  // device is acceptable
  tzt_options.localTempDir = "/tmp";

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

std::atomic<size_t> count(0);

void IterateRange(DB *db, const Range &range) {

  std::unique_ptr<Iterator> it(db->NewIterator(ReadOptions()));
  const Comparator *cmp = db->GetOptions().comparator;

  for (it->Seek(range.start);
       it->Valid() && (cmp->Compare(it->key(), range.limit) < 0); it->Next()) {

    count.fetch_add(1, std::memory_order_relaxed);
  }
}

//
// just split the full range in non-overlapping ranges based on SST files
//
std::vector<Range> BuildRangesFromSSTs(DB *db, size_t num_threads) {

  std::unique_ptr<Iterator> it(db->NewIterator(ReadOptions()));
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
  std::vector<Range> merged;
  for (size_t i = 0; i < files.size(); ++i) {
    merged.push_back(
        Range(files[i].smallestkey,
              (i + 1 == files.size())
                  ? last_key_str // TODO increase by 1, eg. add "\0"
                  : files[i + 1].smallestkey));
  }

  size_t num_ranges = std::min(num_threads, merged.size());
  std::vector<Range> out;
  size_t per_range = merged.size() / num_ranges;
  size_t remainder = merged.size() % num_ranges;
  size_t idx = 0;
  for (size_t i = 0; i < num_ranges; ++i) {
    size_t chunk = per_range + (i < remainder ? 1 : 0);
    Range r = Range(merged[idx].start, merged[idx + chunk - 1].limit);
    out.push_back(r);
    idx += chunk;
  }
  return out;
}

int main() {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t size = cdbdirect_size(handle);
  std::cout << "DB count: " << size << std::endl;

  DB *db = reinterpret_cast<DB *>(handle);

  const size_t num_threads = 16;
  auto ranges = BuildRangesFromSSTs(db, num_threads);

  std::vector<std::thread> workers;
  for (auto &r : ranges) {
    workers.emplace_back(IterateRange, db, r);
  }
  for (auto &t : workers)
    t.join();

  std::cout << "Exact count: " << count << std::endl;

  cdbdirect_finalize(handle);
  return 0;
}
