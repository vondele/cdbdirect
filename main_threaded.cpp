#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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

  // keep a list of known fens to store later
  std::mutex known_fens_vector_mutex;
  std::vector<std::pair<std::string, std::pair<int, int>>> known_fens_vector;
  auto add_known = [&known_fens_vector, &known_fens_vector_mutex](
                       const std::string &fen, int e, int p) {
    const std::lock_guard<std::mutex> lock(known_fens_vector_mutex);
    known_fens_vector.push_back(std::pair<std::string, std::pair<int, int>>(
        fen, std::pair<int, int>(e, p)));
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
                  &add_known]() {
      for (auto &fen : chunk) {

        std::vector<std::pair<std::string, int>> result =
            cdbdirect_get(handle, fen);

        size_t n_elements = result.size();
        int ply = result[n_elements - 1].second;

        if (ply > -2) {
          known_fens++;
          add_known(fen, result[0].second, ply);
        } else
          unknown_fens++;

        scored_moves += n_elements - 1;
      }
    });

  // Wait for all threads to finish
  pool.wait();
  auto t_end = std::chrono::high_resolution_clock::now();

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "known fens:   " << std::right << std::setw(12) << known_fens
            << "  ( " << std::right << std::setw(5) << known_fens * 100.0 / nfen
            << "% )" << std::endl;
  std::cout << "unknown fens: " << std::right << std::setw(12) << unknown_fens
            << "  ( " << std::right << std::setw(5)
            << unknown_fens * 100.0 / nfen << "% )" << std::endl;
  std::cout << "scored moves: " << std::right << std::setw(12) << scored_moves
            << "  ( " << std::right
            << scored_moves / std::max((double)known_fens, 1.0)
            << " per known fen )" << std::endl;
  double elapsed_time_microsec =
      std::chrono::duration<double, std::micro>(t_end - t_start).count();
  std::cout << "Required probing time: " << elapsed_time_microsec / 1000000
            << " sec." << std::endl;
  std::cout << "Required time per fen: "
            << elapsed_time_microsec / (known_fens + unknown_fens)
            << " microsec." << std::endl;

  std::unordered_map<std::string, std::pair<int, int>> eval_map;
  eval_map.reserve(known_fens_vector.size());
  for (auto &tuple : known_fens_vector) {
    eval_map[tuple.first] = tuple.second;
  }

  std::ofstream ofile("cdbdirect.epd");
  assert(ofile.is_open());
  file.open(filename);
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string word, fen;
    int wordCount = 0;
    while (iss >> word && wordCount < 4) {
      if (wordCount > 0)
        fen += " ";
      fen += word;
      wordCount++;
    }
    if (wordCount < 4)
      continue;

    auto it = eval_map.find(fen);
    if (it == eval_map.end()) {
      ofile << line << std::endl;
      continue;
    }

    ofile << line << " ; cdb eval: ";
    int s = it->second.first;
    if (std::abs(s) > 25000)
      ofile << (s > 0 ? "M" : "-M") << 30000 - std::abs(s);
    else
      ofile << s;
    int ply = it->second.second;
    if (ply >= 0)
      ofile << ", ply: " << ply;
    ofile << ";\n";
  }
  ofile.close();
  std::cout << "Known evals written to cdbdirect.epd." << std::endl;

  // Close DB
  std::cout << "Closing DB" << std::endl;
  handle = cdbdirect_finalize(handle);

  return 0;
}
