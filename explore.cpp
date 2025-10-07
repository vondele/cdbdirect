#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

#include "cdbdirect.h"

using namespace TERARKDB_NAMESPACE;

void IterateRange(
    DB *db, const RangeStorage &range,
    const std::function<bool(const std::string &,
                             const std::vector<std::pair<std::string, int>> &)>
        &evaluate_entry) {

  const Comparator *cmp = db->GetOptions().comparator;
  std::unique_ptr<Iterator> it(db->NewIterator(ReadOptions()));

  for (it->Seek(range.start);
       it->Valid() && (cmp->Compare(it->key(), range.limit) < 0); it->Next()) {

    auto fen = key_to_fen(it->key().ToString(), true);
    auto scored = value_to_scoredMoves(it->value().ToString(), true);
    if (!evaluate_entry(fen, scored))
      break;
  }
}

std::vector<RangeStorage> BuildRangesFromSSTs(DB *db, size_t num_threads) {

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

int main() {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t db_size = cdbdirect_size(handle);
  std::cout << "DB count: " << db_size << std::endl;

  DB *db = reinterpret_cast<DB *>(handle);

  const size_t num_threads = std::thread::hardware_concurrency();
  auto ranges = BuildRangesFromSSTs(db, num_threads);
  std::atomic<size_t> count_total(0);
  std::atomic<size_t> count_have_minply(0);
  std::atomic<size_t> count_have_single(0);
  std::atomic<size_t> count_moves(0);
  auto start = std::chrono::steady_clock::now();

  auto evaluate_entry =
      [&](const std::string &fen,
          const std::vector<std::pair<std::string, int>> &scored) {
        // count entries
        size_t peek = count_total.fetch_add(1, std::memory_order_relaxed);
        if (scored.back().second > -1)
          count_have_minply.fetch_add(1, std::memory_order_relaxed);
        if (scored.size() == 2)
          count_have_single.fetch_add(1, std::memory_order_relaxed);
        count_moves.fetch_add(scored.size() - 1, std::memory_order_relaxed);

        // status update
        if (peek % 10000000 == 0 && peek > 0) {
          auto end = std::chrono::steady_clock::now();
          auto elapsed =
              std::chrono::duration_cast<std::chrono::seconds>(end - start);
          std::cout << "Counted               " << peek << " entries so far..."
                    << "\n";
          std::cout << "  Have min ply:       " << count_have_minply.load()
                    << "\n";
          std::cout << "  Have single move:   " << count_have_single.load()
                    << "\n";
          std::cout << "  Total scored moves: " << count_moves.load() << "\n";
          std::cout << "  Time (s):           " << elapsed.count() << "\n";
          std::cout << "  nps:                " << peek / elapsed.count()
                    << "\n";
          std::cout << "  ETA (s):            "
                    << (db_size - peek) * elapsed.count() / peek << "\n";
          /*
          std::cout << fen << "\n";
          for (auto &e : scored) {
            std::cout << e.first << " " << e.second << "\n";
          }
          */
          std::cout << std::endl;
        }
        return true; // continue iteration
      };

  std::vector<std::thread> workers;
  for (auto &r : ranges) {
    workers.emplace_back(IterateRange, db, r, evaluate_entry);
  }
  for (auto &t : workers)
    t.join();

  std::cout << "Exact count:          " << count_total << std::endl;
  std::cout << "  Have min ply:       " << count_have_minply.load() << "\n";
  std::cout << "  Have single move:   " << count_have_single.load() << "\n";
  std::cout << "  Total scored moves: " << count_moves.load() << "\n";

  cdbdirect_finalize(handle);
  return 0;
}
