#include <atomic>
#include <cstdint>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

#include "cdbdirect.h"

using namespace TERARKDB_NAMESPACE;

int main() {
  std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);

  std::uint64_t size = cdbdirect_size(handle);
  std::cout << "Approximate count: " << size << std::endl;

  DB *db = reinterpret_cast<DB *>(handle);

  Iterator *it = db->NewIterator(ReadOptions());
  it->SeekToFirst();

  std::uint64_t count = 0;
  while (it->Valid()) {
    count++;
    if (count % 100000 == 0) {
      std::cout << "Processed " << count << " / " << size << std::endl;
    }
    assert(it->status().ok());
    // Slice key = it->key();
    // Slice value = it->value();
    it->Next();
  }

  std::cout << "Exact count: " << count << std::endl;

  cdbdirect_finalize(handle);
  return 0;
}
