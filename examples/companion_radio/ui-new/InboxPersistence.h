#pragma once
#include "RecentMessages.h"

// Small streaming adapter: firmware uses QSPI LittleFS, host tests inject faults.
class InboxStorage {
public:
  virtual ~InboxStorage() {}
  virtual bool exists(unsigned slot) = 0;
  virtual bool open(unsigned slot, bool write) = 0;
  virtual size_t read(uint8_t* data, size_t size) = 0;
  virtual size_t write(const uint8_t* data, size_t size) = 0;
  virtual void close() = 0;
  virtual bool remove(unsigned slot) = 0;
};

// Explicit little-endian, versioned format; never serialize compiler padding.
// Two slots leave the last valid snapshot untouched until its replacement is
// closed and re-read successfully. No identity, preferences or queue files used.
template <size_t TextSize, size_t Capacity = 32>
class InboxPersistence {
  using History = RecentMessages<TextSize, Capacity>;
  using Entry = typename History::Entry;
  static constexpr size_t LegacySize = 47 + TextSize;
  static constexpr size_t RecordSize = LegacySize + 33;
  InboxStorage* _storage;
  int _active = -1;
  uint32_t _generation = 0, _first_change = 0, _last_change = 0, _attempt = 0;
  bool _dirty = false, _failed = false;
  static void put(uint8_t* p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++) p[i] = v >> (8 * i);
  }
  static uint32_t get(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
  }
  static uint32_t crc(uint32_t sum, const uint8_t* p, size_t n) {
    while (n--) {
      sum ^= *p++;
      for (unsigned i = 0; i < 8; i++) sum = (sum >> 1) ^ (0xEDB88320UL & (0U - (sum & 1)));
    }
    return sum;
  }
  static void encode(const Entry& e, uint8_t* p) {
    memset(p, 0, RecordSize);
    put(p, e.sent_at); put(p + 4, e.received_at);
    p[8] = e.kind; p[9] = e.channel_idx;
    memcpy(p + 10, e.sender_prefix, 4);
    p[14] = (e.read ? 1 : 0) | (e.truncated ? 2 : 0);
    // Zero unused bytes to avoid writing old RAM contents to flash.
    memcpy(p + 15, e.origin, strlen(e.origin));
    memcpy(p + 47, e.text, strlen(e.text));
    p[LegacySize] = e.has_reply_target ? 1 : 0;
    if (e.has_reply_target) memcpy(p + LegacySize + 1, e.reply_target, 32);
  }
  static bool validRecord(const uint8_t* p, bool legacy) {
    return p[8] <= 2 && p[14] <= 3 && p[46] == 0 && p[LegacySize - 1] == 0 && (legacy || p[LegacySize] <= 1);
  }
  bool inspect(unsigned slot, uint32_t& generation, History* load = nullptr) {
    if (!_storage || !_storage->open(slot, false)) return false;
    uint8_t header[20], record[RecordSize];
    bool ok = _storage->read(header, sizeof(header)) == sizeof(header);
    bool legacy = ok && memcmp(header, "WIB1", 4) == 0;
    size_t record_size = legacy ? LegacySize : RecordSize;
    ok = ok && (legacy || memcmp(header, "WIB2", 4) == 0) && get(header + 8) <= Capacity && get(header + 12) == record_size;
    uint32_t sum = crc(0xFFFFFFFFUL, header, ok ? 16 : 0);
    uint32_t count = ok ? get(header + 8) : 0;
    if (ok && load) load->clear();
    for (uint32_t i = 0; ok && i < count; i++) {
      ok = _storage->read(record, record_size) == record_size && validRecord(record, legacy);
      if (!ok) break;
      sum = crc(sum, record, record_size);
      if (load) {
        load->add(record[8], record[9], get(record), get(record + 4), record + 10,
                  reinterpret_cast<char*>(record + 15), reinterpret_cast<char*>(record + 47));
        load->setFlags(0, record[14] & 1, record[14] & 2);
        if (!legacy && record[LegacySize]) load->setReplyTarget(0, record + LegacySize + 1);
      }
    }
    uint8_t extra;
    ok = ok && (sum ^ 0xFFFFFFFFUL) == get(header + 16) && _storage->read(&extra, 1) == 0;
    if (ok) generation = get(header + 4);
    _storage->close();
    if (load) {
      load->select(0);
      if (!ok) load->clear();
    }
    return ok;
  }
public:
  explicit InboxPersistence(InboxStorage* storage = nullptr) : _storage(storage) {}
  bool pending() const { return _dirty; }
  bool failed() const { return _failed; }
  bool load(History& history) {
    if (!_storage) { _failed = true; return false; }
    uint32_t generations[2] = {};
    bool valid[2] = {inspect(0, generations[0]), inspect(1, generations[1])};
    if (!valid[0] && !valid[1]) {
      _failed = _storage->exists(0) || _storage->exists(1);
      return !_failed;
    }
    _active = valid[1] && (!valid[0] || int32_t(generations[1] - generations[0]) > 0) ? 1 : 0;
    if (!inspect(_active, _generation, &history)) { _failed = true; return false; }
    return true;
  }
  // Explicit cleanup removes both snapshots without allocating a replacement,
  // so it can reclaim space even when a normal copy-on-write save fails.
  bool clear(History& history, uint32_t now) {
    history.clear();
    changed(now);
    if (!_storage) { _failed = true; return false; }
    _storage->close();
    bool first = _storage->remove(0);
    bool second = _storage->remove(1);
    _failed = !(first && second);
    if (!_failed) { _active = -1; _generation = 0; _dirty = false; }
    return !_failed;
  }
  void changed(uint32_t now) {
    if (!_dirty) _first_change = now;
    _last_change = now;
    _dirty = true;
  }
  bool flush(const History& history, uint32_t now) {
    if (!_dirty) return true;
    _attempt = now;
    if (!_storage) { _failed = true; return false; }
    uint8_t header[20] = {}, record[RecordSize];
    memcpy(header, "WIB2", 4); put(header + 4, _generation + 1);
    put(header + 8, history.savedCount()); put(header + 12, RecordSize);
    uint32_t sum = crc(0xFFFFFFFFUL, header, 16);
    for (size_t i = history.count(); i > 0; i--) {
      if (history.at(i - 1)->temporary) continue;
      encode(*history.at(i - 1), record); sum = crc(sum, record, RecordSize);
    }
    put(header + 16, sum ^ 0xFFFFFFFFUL);
    unsigned target = _active == 0 ? 1 : 0;
    bool ok = _storage->open(target, true);
    if (ok) {
      ok = _storage->write(header, sizeof(header)) == sizeof(header);
      for (size_t i = history.count(); ok && i > 0; i--) {
        if (history.at(i - 1)->temporary) continue;
        encode(*history.at(i - 1), record);
        ok = _storage->write(record, RecordSize) == RecordSize;
      }
      _storage->close();
    }
    uint32_t generation = 0;
    ok = ok && inspect(target, generation) && generation == _generation + 1;
    _failed = !ok;
    if (ok) { _active = target; _generation = generation; _dirty = false; }
    return ok;
  }
  void tick(const History& history, uint32_t now) {
    if (!_dirty || (_failed && uint32_t(now - _attempt) < 30000)) return;
    if (uint32_t(now - _last_change) >= 2000 || uint32_t(now - _first_change) >= 5000) flush(history, now);
  }
};
