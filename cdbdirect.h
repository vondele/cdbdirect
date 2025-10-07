#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

std::uintptr_t cdbdirect_initialize(const std::string &path);
std::uint64_t cdbdirect_size(std::uintptr_t handle);
std::uintptr_t cdbdirect_finalize(std::uintptr_t handle);
std::vector<std::pair<std::string, int>> cdbdirect_get(std::uintptr_t handle,
                                                       const std::string &fen);

std::vector<std::pair<std::string, int>>
value_to_scoredMoves(const std::string &value, bool natural_order);
std::string key_to_fen(const std::string &key, bool natural_order);
