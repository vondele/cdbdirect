#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

std::uintptr_t cdbdirect_initialize(const std::string &path);
std::uint64_t cdbdirect_size(std::uintptr_t handle);
std::uintptr_t cdbdirect_finalize(std::uintptr_t handle);
std::vector<std::pair<std::string, int>> cdbdirect_get(std::uintptr_t handle,
                                                       const std::string &fen);
void cdbdirect_apply(
    std::uintptr_t handle, size_t num_threads,
    const std::function<bool(const std::string &,
                             const std::vector<std::pair<std::string, int>> &)>
        &evaluate_entry);
