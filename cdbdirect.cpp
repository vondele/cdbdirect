#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "rocksdb/db.h"
#include "rocksdb/options.h"

#include "fen2cdb.h"

#define ROCKSDB_NAMESPACE terarkdb
using ROCKSDB_NAMESPACE::DB;
using ROCKSDB_NAMESPACE::Options;
using ROCKSDB_NAMESPACE::ReadOptions;
using ROCKSDB_NAMESPACE::Status;

int main() {

  DB *db;
  Options options;

  // open DB
  std::string kDBPath = "/mnt/ssd/rocks/chess/data";
  Status s = DB::OpenForReadOnly(options, kDBPath, &db);
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());

  // open file with fen/epd
  std::ifstream file("caissa_sorted_100000.epd");
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
    auto t_start = std::chrono::high_resolution_clock::now();
    std::cout << "-------------------------------------------------------------"
              << "\n";
    std::cout << "Probing: " << fen << "\n";

    // The fen or its black-white mirrored equivalent is to be probed,
    // depending on their hexfen order
    std::string inputfen = fen;
    std::string hexfen = cbfen2hexfen(inputfen);
    std::string BWfen = cbgetBWfen(inputfen);
    std::string BWhexfen = cbfen2hexfen(BWfen);
    bool natural_order = hexfen < BWhexfen;

    // generate the binary fen with prefix 'h' has key
    std::string key = 'h' + hex2bin(natural_order ? hexfen : BWhexfen);

    // get value (prefix binary fen by 'h')
    std::string value;
    auto t_start_db = std::chrono::high_resolution_clock::now();
    s = db->Get(ReadOptions(), key, &value);
    auto t_end_db = std::chrono::high_resolution_clock::now();

    // If we have a hit decode the answer
    if (s.ok()) {
      // decode the value to scoredMoves
      std::deque<StrPair> scoredMoves;
      get_hash_values(value, scoredMoves);

      // sort moves
      std::sort(scoredMoves.begin(), scoredMoves.end(),
                [](const StrPair &a, const StrPair &b) {
                  return std::stoi(a.second) < std::stoi(b.second);
                });

      // output
      int ply = -1;
      for (auto &pair : scoredMoves) {
        if (pair.first != "a0a0") {
          // output moves + score adjusting the sign.
          std::cout << "    "
                    << (natural_order ? pair.first : cbgetBWmove(pair.first))
                    << " : " << -std::stoi(pair.second) << "\n";
        } else {
          // the special move a0a0 encodes distance to root
          ply = std::stoi(pair.second);
        }
      }

      if (ply >= 0)
        std::cout << "    Distance to startpos equal or less than " << ply << "\n";
      else
        std::cout << "    Distance to startpos unknown"
                  << "\n";
    } else {
      std::cout << "Fen not found in DB!"
                << "\n";
    }

    // Timing info
    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_time_microsec =
        std::chrono::duration<double, std::micro>(t_end - t_start).count();
    double elapsed_time_microsec_db =
        std::chrono::duration<double, std::micro>(t_end_db - t_start_db)
            .count();
    std::cout << "Required time: " << elapsed_time_microsec << " ("
              << elapsed_time_microsec_db << ") microsec." << std::endl;
  }

  file.close();

  // safely close the DB.
  delete db;

  return 0;
}
