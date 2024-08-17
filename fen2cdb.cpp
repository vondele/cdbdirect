/*

   The code in this file has been authored by noobpwnftw and is covered by the
original licenses a BSD-style license (ssdb) and the unlicense (chessdb). See
   https://github.com/noobpwnftw/ssdb/blob/124302059809644acc6726a72e234bdb275c56af/src/ssdb/t_hash.h
   https://github.com/noobpwnftw/chessdb/blob/7995300ce3bb68683e6da76e45a94f4c85735d69/extensions/PHP5/cboard/cboard.cpp

---
Copyright (c) 2013 SSDB Authors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the SSDB nor the names of its contributors may be used
to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

---
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>

*/

#include <cstring>
#include <string>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "fen2cdb.h"

#define CHESS_FEN_MAX_LENGTH 128
#define CHESS_BITSTR_MAX_LENGTH 93

char char2bithex(char ch) {
  switch (ch) {
  case '1':
    return '0';
  case '2':
    return '1';
  case '3':
    return '2';
  case 'p':
    return '3';
  case 'n':
    return '4';
  case 'b':
    return '5';
  case 'r':
    return '6';
  case 'q':
    return '7';
  case 'k':
    return '9';
  case 'P':
    return 'a';
  case 'N':
    return 'b';
  case 'B':
    return 'c';
  case 'R':
    return 'd';
  case 'Q':
    return 'e';
  case 'K':
    return 'f';
  default:
    return '8';
  }
}

char bithex2char(unsigned char ch) {
  switch (ch) {
  case '0':
    return '1';
  case '1':
    return '2';
  case '2':
    return '3';
  case '3':
    return 'p';
  case '4':
    return 'n';
  case '5':
    return 'b';
  case '6':
    return 'r';
  case '7':
    return 'q';
  case '9':
    return 'k';
  case 'a':
    return 'P';
  case 'b':
    return 'N';
  case 'c':
    return 'B';
  case 'd':
    return 'R';
  case 'e':
    return 'Q';
  case 'f':
    return 'K';
  }
  return 'x';
}

char extra2bithex(char ch) {
  switch (ch) {
  case '-':
    return '0';
  case 'K':
    return 'a';
  case 'Q':
    return 'b';
  case 'k':
    return 'c';
  case 'q':
    return 'd';
  case 'a':
    return '1';
  case 'b':
    return '2';
  case 'c':
    return '3';
  case 'd':
    return '4';
  case 'e':
    return '5';
  case 'f':
    return '6';
  case 'g':
    return '7';
  case 'h':
    return '8';
  case ' ':
    return '9';
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
    return 'e';
  default:
    return ch;
  }
}

char bithex2extra(unsigned char ch) {
  switch (ch) {
  case '0':
    return '-';
  case 'a':
    return 'K';
  case 'b':
    return 'Q';
  case 'c':
    return 'k';
  case 'd':
    return 'q';
  case '1':
    return 'a';
  case '2':
    return 'b';
  case '3':
    return 'c';
  case '4':
    return 'd';
  case '5':
    return 'e';
  case '6':
    return 'f';
  case '7':
    return 'g';
  case '8':
    return 'h';
  case '9':
    return ' ';
  }
  return 'x';
}

std::string cbfen2hexfen(const std::string &fen) {
  const char *fenstr = fen.data();
  size_t fenstr_len = fen.size();

  char bitstr[CHESS_BITSTR_MAX_LENGTH];
  size_t index = 0;
  size_t tmpindex = 0;
  while (index < fenstr_len) {
    char curCh = fenstr[index];
    if (curCh == ' ') {
      if (fenstr[index + 1] == 'b') {
        bitstr[tmpindex++] = '1';
      } else {
        bitstr[tmpindex++] = '0';
      }
      index += 3;
      while (index < fenstr_len) {
        bitstr[tmpindex++] = extra2bithex(fenstr[index++]);
        if (bitstr[tmpindex - 1] == 'e') {
          bitstr[tmpindex++] = extra2bithex(tolower(fenstr[index - 1]));
        }
      }
      break;
    } else if (curCh == '/') {
      index++;
    } else {
      bitstr[tmpindex++] = char2bithex(curCh);
      if (curCh >= '4' && curCh <= '8') {
        bitstr[tmpindex++] = curCh - 4;
      }
      index++;
    }
  }
  if (tmpindex % 2) {
    if (bitstr[tmpindex - 1] == '0') {
      bitstr[tmpindex - 1] = '\0';
    } else {
      bitstr[tmpindex++] = '0';
      bitstr[tmpindex] = '\0';
    }
  } else {
    bitstr[tmpindex] = '\0';
  }
  return std::string(bitstr);
}

std::string cbhexfen2fen(const std::string &hexfen) {
  const char *fenstr = hexfen.data();
  size_t fenstr_len = hexfen.size();
  size_t index = 0;
  char fen[CHESS_FEN_MAX_LENGTH];
  size_t tmpidx = 0;
  for (int sq = 0; sq < 64; sq++) {
    if (sq != 0 && (sq % 8) == 0) {
      fen[tmpidx++] = '/';
    }
    char tmpch = '0';
    if (index < fenstr_len) {
      tmpch = fenstr[index++];
    }
    if (tmpch == '1') {
      sq += 1;
    } else if (tmpch == '2') {
      sq += 2;
    }
    if (tmpch == '8') {
      tmpch = fenstr[index++];
      fen[tmpidx++] = tmpch + 4;
      sq += tmpch - '0' + 3;
    } else {
      fen[tmpidx++] = bithex2char(tmpch);
    }
  }
  fen[tmpidx] = '\0';

  if (fenstr[index++] != '0') {
    strcat(fen, " b ");
  } else {
    strcat(fen, " w ");
  }
  tmpidx += 3;
  do {
    if (fenstr[index] == 'e') {
      index++;
      fen[tmpidx++] = toupper(bithex2extra(fenstr[index++]));
    } else {
      fen[tmpidx++] = bithex2extra(fenstr[index++]);
    }
  } while (fen[tmpidx - 1] != ' ');
  if (index < fenstr_len) {
    fen[tmpidx++] = bithex2extra(fenstr[index++]);
    if (fen[tmpidx - 1] != '-')
      fen[tmpidx++] = fenstr[index];
    fen[tmpidx] = '\0';
  } else {
    fen[tmpidx++] = '-';
    fen[tmpidx] = '\0';
  }
  return std::string(fen);
}

const char MoveToBW[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '8', '7', '6', '5', '4', '3', '2', '1',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,
};

std::string cbgetBWfen(const std::string &orig) {
  const char *fenstr = orig.data();
  size_t fenstr_len = orig.size();
  char fen[CHESS_FEN_MAX_LENGTH];
  char tmp[CHESS_FEN_MAX_LENGTH];
  size_t index = 0;
  size_t tmpidx = 0;
  fen[0] = '\0';
  while (index < fenstr_len) {
    if (fenstr[index] == ' ') {
      tmp[tmpidx] = '\0';
      strcat(tmp, "/");
      strcat(tmp, fen);
      strcpy(fen, tmp);
      if (fenstr[index + 1] == 'w') {
        strcat(fen, " b ");
      } else {
        strcat(fen, " w ");
      }
      index += 3;
      tmpidx = 0;
      char tmp2[4];
      int tmpidx2 = 0;
      do {
        if (isupper(fenstr[index]))
          tmp2[tmpidx2++] = tolower(fenstr[index++]);
        else
          tmp[tmpidx++] = toupper(fenstr[index++]);
      } while (fenstr[index] != ' ');
      tmp[tmpidx] = '\0';
      tmp2[tmpidx2++] = ' ';
      tmp2[tmpidx2] = '\0';
      strcat(fen, tmp);
      strcat(fen, tmp2);
      index++;

      while (index < fenstr_len) {
        char tmp = MoveToBW[int(fenstr[index])];
        if (tmp)
          fen[index] = tmp;
        else
          fen[index] = fenstr[index];
        index++;
      }
      fen[index] = '\0';
      break;
    } else if (fenstr[index] == '/') {
      tmp[tmpidx] = '\0';
      if (strlen(fen) > 0) {
        strcat(tmp, "/");
        strcat(tmp, fen);
      }
      strcpy(fen, tmp);
      tmpidx = 0;
      index++;
    } else {
      tmp[tmpidx] = fenstr[index++];
      if (isupper(tmp[tmpidx])) {
        tmp[tmpidx] = tolower(tmp[tmpidx]);
      } else {
        tmp[tmpidx] = toupper(tmp[tmpidx]);
      }
      tmpidx++;
    }
  }
  return std::string(fen);
}

std::string cbgetBWmove(const std::string &move) {
  const char *movestr = move.data();
  size_t movestr_len = move.size();
  char BWmove[6];
  BWmove[4] = '\0';
  for (size_t i = 0; i < movestr_len; i++) {
    if (i < 4) {
      char tmp = MoveToBW[int(movestr[i])];
      if (tmp)
        BWmove[i] = tmp;
      else
        BWmove[i] = movestr[i];
    } else {
      BWmove[i] = movestr[i];
      BWmove[5] = '\0';
    }
  }
  return std::string(BWmove);
}

std::string hex2bin(const std::string &hex) {
  std::string bin;
  bin.reserve(hex.size() / 2);

  for (size_t i = 0; i < hex.size(); i += 2) {
    // Take two hex digits and convert to a byte
    std::stringstream ss;
    ss << std::hex << hex.substr(i, 2);
    unsigned int byte;
    ss >> byte;
    bin.push_back(static_cast<char>(byte));
  }

  return bin;
}

std::string bin2hex(const std::string &bin) {
  std::stringstream ss;

  // Convert each byte in the binary string to a 2-digit hexadecimal string
  for (unsigned char c : bin) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
  }

  return ss.str();
}

const char SQ_File[90] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c',
    'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c',
    'd', 'e', 'f', 'g', 'h', 'i', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
};
const char SQ_Rank[90] = {
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '1', '1', '1', '1', '1', '1',
    '1', '1', '1', '2', '2', '2', '2', '2', '2', '2', '2', '2', '3', '3', '3',
    '3', '3', '3', '3', '3', '3', '4', '4', '4', '4', '4', '4', '4', '4', '4',
    '5', '5', '5', '5', '5', '5', '5', '5', '5', '6', '6', '6', '6', '6', '6',
    '6', '6', '6', '7', '7', '7', '7', '7', '7', '7', '7', '7', '8', '8', '8',
    '8', '8', '8', '8', '8', '8', '9', '9', '9', '9', '9', '9', '9', '9', '9',
};

int decode_hash_value(const Bytes &slice, std::string *key,
                      std::string *value) {
  if (slice.size() < 2 * sizeof(int16_t)) {
    return -1;
  }
  int16_t encoded = *(int16_t *)slice.data();
  int src = encoded >> 8;
  int dst = encoded & 0x7F;
  if (encoded & 0x80) {
    key->resize(5);
    (*key)[0] = SQ_File[src];
    (*key)[1] = SQ_Rank[src];
    (*key)[2] = SQ_File[dst];
    if (SQ_Rank[src] == '7')
      (*key)[3] = '8';
    else if (SQ_Rank[src] == '2')
      (*key)[3] = '1';
    else
      return -1;

    switch (SQ_Rank[dst]) {
    case '0':
      (*key)[4] = 'q';
      break;
    case '1':
      (*key)[4] = 'r';
      break;
    case '2':
      (*key)[4] = 'b';
      break;
    case '3':
      (*key)[4] = 'n';
      break;
    default:
      return -1;
    }
  } else {
    key->resize(4);
    (*key)[0] = SQ_File[src];
    (*key)[1] = SQ_Rank[src];
    (*key)[2] = SQ_File[dst];
    (*key)[3] = SQ_Rank[dst];
  }
  int16_t val = *(int16_t *)(slice.data() + sizeof(int16_t));
  *value = std::to_string(val);
  return 0;
}

int get_hash_value(const Bytes &slice, const Bytes &field, std::string *value) {
  if (slice.empty() || slice.size() % (2 * sizeof(int16_t)) != 0) {
    return 0;
  }
  for (size_t i = 0; i < slice.size(); i += 2 * sizeof(int16_t)) {
    std::string elem_field, elem_value;
    if (decode_hash_value(Bytes(slice.data() + i, 2 * sizeof(int16_t)),
                          &elem_field, &elem_value) == 0 &&
        Bytes(elem_field) == Bytes(field)) {
      *value = elem_value;
      return 1;
    }
  }
  return 0;
}

int get_hash_values(const Bytes &slice, std::vector<StrPair> &values) {
  if (slice.empty() || slice.size() % (2 * sizeof(int16_t)) != 0) {
    return 0;
  }
  for (size_t i = 0; i < slice.size(); i += 2 * sizeof(int16_t)) {
    std::string elem_field, elem_value;
    if (decode_hash_value(Bytes(slice.data() + i, 2 * sizeof(int16_t)),
                          &elem_field, &elem_value) == 0) {
      values.push_back(std::make_pair(elem_field, elem_value));
    }
  }
  return 0;
}
