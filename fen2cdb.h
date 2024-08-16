#pragma once

#include <cassert>
#include <deque>
#include <string>

using Bytes = std::string;
using StrPair = std::pair<std::string, std::string>;
using BytesPair = std::pair<Bytes, Bytes>;

std::string cbfen2hexfen(const std::string &fen);
std::string cbhexfen2fen(const std::string &hexfen);
std::string hex2bin(const std::string &hex);
std::string bin2hex(const std::string &bin);
std::string cbgetBWfen(const std::string &orig);
std::string cbgetBWmove(const std::string &move);
int get_hash_values(const Bytes &slice, std::deque<StrPair> &values);
