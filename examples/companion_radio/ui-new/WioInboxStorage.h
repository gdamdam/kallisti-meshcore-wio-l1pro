#pragma once
#include "InboxPersistence.h"
#include "../DataStore.h"

// Only the Wio QSPI filesystem is used; never fall back to the identity FS.
class WioInboxStorage : public InboxStorage {
  FILESYSTEM* _fs;
  File _file;
  unsigned _dataset;
  const char* path(unsigned slot) const {
    if (_dataset == 1) return slot ? "/ui_outbox1" : "/ui_outbox0";
    if (_dataset == 2) return slot ? "/ui_fav1" : "/ui_fav0";
    if (_dataset == 4) return slot ? "/ui_settings1" : "/ui_settings0";
    if (_dataset == 3) return slot ? "/ui_draft1" : "/ui_draft0";
    return slot ? "/ui_inbox1" : "/ui_inbox0";
  }
public:
  explicit WioInboxStorage(FILESYSTEM& fs, unsigned dataset = 0) : _fs(&fs), _file(fs), _dataset(dataset) {}
  bool exists(unsigned slot) override { return _fs->exists(path(slot)); }
  bool open(unsigned slot, bool write) override {
    _file.close();
    if (write && _fs->exists(path(slot)) && !_fs->remove(path(slot))) return false;
    return _file.open(path(slot), write ? FILE_O_WRITE : FILE_O_READ);
  }
  size_t read(uint8_t* data, size_t size) override {
    // Transfers smaller than the 256-byte LittleFS page use its aligned cache.
    // Large 265-byte inbox records can otherwise bypass it at unaligned offsets,
    // which the nRF52 QSPI DMA driver rejects.
    size_t total = 0;
    while (total < size) {
      size_t chunk = size - total; if (chunk > 128) chunk = 128;
      int n = _file.read(data + total, chunk);
      if (n <= 0) break;
      total += size_t(n);
      if (size_t(n) != chunk) break;
    }
    return total;
  }
  size_t write(const uint8_t* data, size_t size) override {
    size_t total = 0;
    while (total < size) {
      size_t chunk = size - total; if (chunk > 128) chunk = 128;
      size_t n = _file.write(data + total, chunk);
      total += n;
      if (n != chunk) break;
    }
    return total;
  }
  bool remove(unsigned slot) override { _file.close(); return !_fs->exists(path(slot)) || _fs->remove(path(slot)); }
  void close() override { _file.close(); }
};
