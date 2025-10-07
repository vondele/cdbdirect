#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

#include "cdbdirect.h"

using namespace TERARKDB_NAMESPACE;

std::atomic<size_t> count(0);
std::mutex cout_mutex;

void IterateRange(DB *db, const Range &range) {

  // debug
  if (false) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "Search in range " << key_to_fen(range.start.ToString(), true)
              << " to " << key_to_fen(range.limit.ToString(), true)
              << std::endl;
  }

  std::unique_ptr<Iterator> it(db->NewIterator(ReadOptions()));
  const Comparator *cmp = db->GetOptions().comparator;

  for (it->Seek(range.start);
       it->Valid() && (cmp->Compare(it->key(), range.limit) < 0); it->Next()) {

    // count entries
    size_t peek = count.fetch_add(1, std::memory_order_relaxed);

    // status update
    if (peek % 100000000 == 0) {
      auto fen = key_to_fen(it->key().ToString(), true);
      auto res = value_to_scoredMoves(it->value().ToString(), true);
      std::cout << "Counted " << peek << " entries so far..." << std::endl;
      std::cout << fen << "\n";
      for (auto &e : res) {
        std::cout << e.first << " " << e.second << "\n";
      }
    }
  }
  assert(it->status().ok());
}

std::vector<Range> BuildRangesFromSSTs(DB *db, size_t num_threads) {

  std::unique_ptr<Iterator> it(db->NewIterator(ReadOptions()));

  it->SeekToFirst();
  std::string first_key_str = it->key().ToString();
  std::cout << "first_key_str: " << key_to_fen(first_key_str, true)
            << std::endl;

  it->SeekToLast();
  std::string last_key_str = it->key().ToString();
  std::cout << "last_key_str: " << key_to_fen(last_key_str, true) << std::endl;

  uint64_t size_bytes;
  Range fullRange(first_key_str, last_key_str);
  db->GetApproximateSizes(&fullRange, 1, &size_bytes);
  std::cout << "Total size bytes: " << size_bytes << std::endl;

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

  // Now partition merged ranges evenly by count or total file size
  // Partition merged ranges as evenly as possible among threads
  size_t num_ranges = std::min(num_threads, merged.size());
  std::cout << "Merging " << files.size() << " SSTs into " << merged.size()
            << " disjoint ranges, partitioning into " << num_ranges
            << " ranges for threads." << std::endl;
  std::vector<Range> out;
  size_t per_range = merged.size() / num_ranges;
  size_t remainder = merged.size() % num_ranges;
  size_t idx = 0;
  size_t total_size = 0;
  for (size_t i = 0; i < num_ranges; ++i) {
    size_t chunk = per_range + (i < remainder ? 1 : 0);
    Range r = Range(merged[idx].start, merged[idx + chunk - 1].limit);
    db->GetApproximateSizes(&r, 1, &size_bytes);
    total_size += size_bytes;
    std::cout << "Approx size in bytes " << size_bytes << " "
              << key_to_fen(r.start.ToString(), true) << " to "
              << key_to_fen(r.limit.ToString(), true) << std::endl;
    out.push_back(r);
    idx += chunk;
  }
  std::cout << "Total size bytes: " << total_size << std::endl;

  return out;
}

int main() {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t size = cdbdirect_size(handle);
  std::cout << "DB count: " << size << std::endl;

  DB *db = reinterpret_cast<DB *>(handle);

  const size_t num_threads = 4;
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
