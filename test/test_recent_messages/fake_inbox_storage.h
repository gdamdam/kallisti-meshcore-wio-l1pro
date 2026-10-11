#pragma once
#include <vector>
#include <algorithm>
#include "../../examples/companion_radio/ui-new/InboxPersistence.h"
class FakeInboxStorage : public InboxStorage {
public:
  std::vector<uint8_t> files[2];
  bool present[2] = {false, false};
  int slot = 0, limit = -1, writes = 0;
  size_t pos = 0;
  bool refuse = false, refuse_remove = false;
  bool exists(unsigned s) override { return present[s]; }
  bool open(unsigned s, bool write) override {
    slot = s; pos = 0;
    if (write) {
      if (refuse) return false;
      present[s] = true; files[s].clear(); writes++;
    }
    return present[s];
  }
  size_t read(uint8_t* p, size_t n) override {
    n = std::min(n, files[slot].size() - pos);
    if (n) memcpy(p, files[slot].data() + pos, n);
    pos += n; return n;
  }
  size_t write(const uint8_t* p, size_t n) override {
    if (limit >= 0) n = std::min(n, size_t(limit));
    if (limit >= 0) limit -= n;
    files[slot].insert(files[slot].end(), p, p + n);
    return n;
  }
  bool remove(unsigned s) override { if(refuse_remove) return false; present[s]=false; files[s].clear(); return true; }
  void close() override {}
};
