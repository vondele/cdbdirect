#include <cassert>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "cdbdirect.h"

int main() {

  std::uintptr_t handle = cdbdirect_initialize("/mnt/ssd/chess-20211203/data");

  // open file with fen/epd
  // std::ifstream file("caissa_sorted_100000.epd");
  std::ifstream file("UHO_Lichess_4852_v1.epd");
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

    // start fen manipulation and DB access.
    std::cout << "-------------------------------------------------------------"
              << "\n";
    std::cout << "Probing: " << fen << "\n";

    auto t_start = std::chrono::high_resolution_clock::now();
    std::vector<std::pair<std::string, int>> result =
        cdbdirect_get(handle, fen);
    auto t_end = std::chrono::high_resolution_clock::now();

    size_t n_elements = result.size();
    int ply = result[n_elements - 1].second;

    if (ply > -2) {
      for (auto &pair : result)
        if (pair.first != "a0a0")
          std::cout << "    " << pair.first << " : " << pair.second << "\n";

      if (ply >= 0)
        std::cout << "    Distance to startpos equal or less than " << ply
                  << "\n";
      else
        std::cout << "    Distance to startpos unknown"
                  << "\n";
    } else {
      std::cout << "Fen not found in DB!"
                << "\n";
    }

    double elapsed_time_microsec =
        std::chrono::duration<double, std::micro>(t_end - t_start).count();
    std::cout << "Required time: " << elapsed_time_microsec << " microsec."
              << std::endl;
  }

  file.close();

  handle = cdbdirect_finalize(handle);

  return 0;
}
