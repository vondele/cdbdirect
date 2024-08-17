#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cdbdirect.h"

#include "external/threadpool.hpp"

inline std::vector<std::vector<std::string>>
split_chunks(const std::vector<std::string> &fens, int target_chunks) {
  const int chunks_size = (fens.size() + target_chunks - 1) / target_chunks;

  auto begin = fens.begin();
  auto end = fens.end();

  std::vector<std::vector<std::string>> chunks;

  while (begin != end) {
    auto next =
        std::next(begin, std::min(chunks_size,
                                  static_cast<int>(std::distance(begin, end))));
    chunks.push_back(std::vector<std::string>(begin, next));
    begin = next;
  }

  return chunks;
}

int main() {

  std::uintptr_t handle = cdbdirect_initialize("/mnt/ssd/chess-20240814/data");

  // open file with fen/epd
  std::vector<std::string> fens;
  // std::string filename = "grob_popular_T60t7_cdb.epd";
  std::string filename = "caissa_sorted_100000.epd";

  std::cout << "Loading: " << filename << std::endl;
  std::ifstream file(filename);
  assert(file.is_open());
  std::string line;
  while (std::getline(file, line)) {

    // Retain just the first 4 fields, no move counters etc
    std::istringstream iss(line);

    std::string word;
    std::string fen;
    int wordCount = 0;

    while (iss >> word && wordCount < 4) {
      if (wordCount > 0)
        fen += " ";
      fen += word;
      wordCount++;
    }
    fens.push_back(fen);
  }

  file.close();

  size_t num_threads = std::thread::hardware_concurrency();

  auto fens_chunked = split_chunks(fens, num_threads * 20);

  std::atomic<size_t> known_fens = 0;
  std::atomic<size_t> unknown_fens = 0;
  std::atomic<size_t> scored_moves = 0;

  // Create a thread pool
  ThreadPool pool(num_threads);

  std::cout << "Probing " << fens.size() << " fens with " << num_threads
            << " threads." << std::endl;
  auto t_start = std::chrono::high_resolution_clock::now();
  for (const auto &chunk : fens_chunked)
    pool.enqueue(
        [&handle, &known_fens, &unknown_fens, &scored_moves, &chunk]() {
          for (auto &fen : chunk) {

            std::vector<std::pair<std::string, int>> result =
                cdbdirect_get(handle, fen);

            size_t n_elements = result.size();
            int ply = result[n_elements - 1].second;

            if (ply > -2)
              known_fens++;
            else
              unknown_fens++;

            scored_moves += n_elements - 1;
          }
        });

  // Wait for all threads to finish
  pool.wait();
  auto t_end = std::chrono::high_resolution_clock::now();

  std::cout << "known fens:   " << known_fens << std::endl;
  std::cout << "unknown fens: " << unknown_fens << std::endl;
  std::cout << "scored moves: " << scored_moves << std::endl;
  double elapsed_time_microsec =
      std::chrono::duration<double, std::micro>(t_end - t_start).count();
  std::cout << "Required probing time:         "
            << elapsed_time_microsec / 1000000 << " sec." << std::endl;
  std::cout << "Required time per fen: "
            << elapsed_time_microsec / (known_fens + unknown_fens)
            << " microsec." << std::endl;

  handle = cdbdirect_finalize(handle);

  return 0;
}
