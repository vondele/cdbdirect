#include "cdbdirect.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t db_size = cdbdirect_size(handle);
  std::cout << "DB count: " << db_size << std::endl;

  // setup of a function that will be called for each entry in the db,
  // multithreaded
  size_t max_entries = std::min(1'000'000'000UL, db_size);
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
          auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
              end - start);
          std::cout << "Counted               " << peek << " of " << max_entries
                    << " entries so far..."
                    << "\n";
          std::cout << "  Have min ply:       " << count_have_minply.load()
                    << "\n";
          std::cout << "  Have single move:   " << count_have_single.load()
                    << "\n";
          std::cout << "  Total scored moves: " << count_moves.load() << "\n";
          std::cout << "  Time (s):           " << elapsed.count() / 1000.0
                    << "\n";
          std::cout << "  nps:                " << peek * 1000 / elapsed.count()
                    << "\n";
          std::cout << "  ETA (s):            "
                    << (max_entries - peek) * elapsed.count() / (peek * 1000)
                    << "\n";
          /*
          std::cout << fen << "\n";
          for (auto &e : scored) {
            std::cout << e.first << " " << e.second << "\n";
          }
          */
          std::cout << std::endl;
        }
        return peek < max_entries; // continue iteration as long as true
      };

  // evaluate all entries in the db, using multiple threads
  const size_t num_threads = std::thread::hardware_concurrency();
  cdbdirect_apply(handle, num_threads, evaluate_entry);

  // Final status update
  std::cout << "Final count:          " << count_total << std::endl;
  std::cout << "  Have min ply:       " << count_have_minply.load() << "\n";
  std::cout << "  Have single move:   " << count_have_single.load() << "\n";
  std::cout << "  Total scored moves: " << count_moves.load() << "\n";

  cdbdirect_finalize(handle);
  return 0;
}
