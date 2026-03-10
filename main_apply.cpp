#include "cdbdirect.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char *argv[]) {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t db_size = cdbdirect_size(handle);
  std::cout << "DB count: " << db_size << std::endl;

  size_t max_entries = 1'000'000'000UL;
  if (argc > 1) { // either pass max_entries as integer
    size_t cli = std::stoull(argv[1]);
    if (cli > 1 || std::string(argv[1]) == "1") {
      max_entries = cli;
      if (max_entries == 1)
        std::cout << "Use '" << argv[0] << " 1.0' to analyse the whole DB."
                  << std::endl;
    } else { // or pass a fraction of total DB, like 0.5 or 1.0
      double fraction = std::stod(argv[1]);
      max_entries = std::size_t(fraction * db_size);
    }
  }
  max_entries = std::clamp(max_entries, 0UL, db_size);
  std::cout << "Analyse the first " << max_entries << " DB entries ..."
            << std::endl;

  // setup of a function that will be called for each entry in the db,
  // multithreaded
  std::atomic<size_t> count_total(0);
  std::atomic<size_t> count_have_minply(0);
  std::atomic<size_t> count_have_single(0);
  std::atomic<size_t> count_moves(0);
  auto start = std::chrono::steady_clock::now();

  std::array<std::atomic<size_t>, 65536> min_ply_histogram = {};
  std::array<std::atomic<size_t>, 65536> score_histogram = {};

  auto evaluate_entry = [&](const std::string &fen,
                            const std::vector<std::pair<std::string, int>>
                                &scored) {
    // distribution of min ply
    int index_min_Ply = std::clamp(scored.back().second, 0, 65535);
    min_ply_histogram[index_min_Ply].fetch_add(1, std::memory_order_relaxed);
    // distribution of scores
    int index_score = std::clamp(scored.front().second + 32768, 0, 65535);
    score_histogram[index_score].fetch_add(1, std::memory_order_relaxed);

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
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      std::cout << "Counted               " << peek << " of " << max_entries
                << " entries so far..."
                << "\n";
      std::cout << "  Have min ply:       " << count_have_minply.load() << "\n";
      std::cout << "  Have single move:   " << count_have_single.load() << "\n";
      std::cout << "  Total scored moves: " << count_moves.load() << "\n";
      std::cout << "  Time (s):           " << elapsed.count() / 1000.0 << "\n";
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

  std::ofstream file_ply("min_ply_histogram.txt");
  for (size_t i = 0; i < min_ply_histogram.size(); ++i) {
    file_ply << i << " " << min_ply_histogram[i].load() << "\n";
  }
  file_ply.close();

  std::ofstream file_score("score_histogram.txt");
  for (size_t i = 0; i < score_histogram.size(); ++i) {
    file_score << int(i) - 32768 << " " << score_histogram[i].load() << "\n";
  }
  file_score.close();

  cdbdirect_finalize(handle);
  return 0;
}
