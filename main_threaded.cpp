#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cdbdirect.h"

#include "external/threadpool.hpp"

int main(int argc, char *argv[]) {

  std::string filename = "caissa_sorted_100000.epd";

  if (argc > 1)
    filename = argv[1];

  size_t num_threads = std::thread::hardware_concurrency();

  // open file with fen/epd
  std::vector<std::vector<std::string>> fens_chunked;
  fens_chunked.resize(num_threads * 20);
  std::cout << "Loading: " << filename << std::endl;
  std::ifstream file(filename);
  assert(file.is_open());
  std::string line;
  size_t nfen = 0;
  while (std::getline(file, line)) {

    // Retain just the first 4 fields, no move counters etc
    // fens must also have `-` for the ep if no legal ep move is possible
    // (including pinned pawns).
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
    if (wordCount < 4)
      continue;
    nfen++;
    fens_chunked[nfen % fens_chunked.size()].push_back(fen);
  }

  file.close();

  // keep a list of unknown fens to store later
  std::mutex unknown_fens_vector_mutex;
  std::vector<std::string> unknown_fens_vector;
  auto add_unknown = [&unknown_fens_vector,
                      &unknown_fens_vector_mutex](const std::string &fen) {
    const std::lock_guard<std::mutex> lock(unknown_fens_vector_mutex);
    unknown_fens_vector.push_back(fen);
  };

  std::atomic<size_t> known_fens = 0;
  std::atomic<size_t> unknown_fens = 0;
  std::atomic<size_t> scored_moves = 0;

  // Create a thread pool
  ThreadPool pool(num_threads);

  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);
  std::uint64_t size = cdbdirect_size(handle);
  std::cout << "Opened DB with " << size << " stored positions." << std::endl;

  // start probing
  std::cout << "Probing " << nfen << " fens with " << num_threads << " threads."
            << std::endl;
  auto t_start = std::chrono::high_resolution_clock::now();
  for (const auto &chunk : fens_chunked)
    pool.enqueue([&handle, &known_fens, &unknown_fens, &scored_moves, &chunk,
                  &add_unknown]() {
      for (auto &fen : chunk) {

        std::vector<std::pair<std::string, int>> result =
            cdbdirect_get(handle, fen);

        size_t n_elements = result.size();
        int ply = result[n_elements - 1].second;

        if (ply > -2)
          known_fens++;
        else {
          unknown_fens++;
          add_unknown(fen);
        }

        scored_moves += n_elements - 1;
      }
    });

  // Wait for all threads to finish
  pool.wait();
  auto t_end = std::chrono::high_resolution_clock::now();

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "known fens:   " << std::right << std::setw(12) << known_fens
            << "  ( " << std::right << std::setw(5)
            << known_fens * 100.0 / nfen << "% )" << std::endl;
  std::cout << "unknown fens: " << std::right << std::setw(12) << unknown_fens
            << "  ( " << std::right << std::setw(5)
            << unknown_fens * 100.0 / nfen << "% )" << std::endl;
  std::cout << "scored moves: " << std::right << std::setw(12) << scored_moves
            << "  ( " << std::right
            << scored_moves / std::max((double)known_fens, 1.0)
            << " per known fen )" << std::endl;
  double elapsed_time_microsec =
      std::chrono::duration<double, std::micro>(t_end - t_start).count();
  std::cout << "Required probing time: "
            << elapsed_time_microsec / 1000000 << " sec." << std::endl;
  std::cout << "Required time per fen: "
            << elapsed_time_microsec / (known_fens + unknown_fens)
            << " microsec." << std::endl;

  // store unknown fens
  std::cout << "Storing: " << "unknown.epd" << std::endl;
  std::ofstream ufile("unknown.epd");
  assert(ufile.is_open());
  for (auto &fen : unknown_fens_vector)
    ufile << fen << "\n";
  ufile.close();

  // Close DB
  std::cout << "Closing DB" << std::endl;
  handle = cdbdirect_finalize(handle);

  return 0;
}
