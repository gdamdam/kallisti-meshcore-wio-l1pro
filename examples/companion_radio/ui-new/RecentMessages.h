#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// UI-only history. Never dequeues or acknowledges companion protocol frames.
// TextSize includes the terminator; instantiate from the upstream packet limit.
template <size_t TextSize, size_t Capacity = 32>
class RecentMessages {
public:
  struct Entry {
    uint32_t sent_at, received_at;
    uint8_t kind, channel_idx, sender_prefix[4];
    bool read, truncated, has_reply_target, temporary;
    uint8_t reply_target[32];
    char origin[32];
    char text[TextSize];
  };

private:
  Entry _entries[Capacity];
  size_t _head = Capacity - 1;
  size_t _count = 0;
  size_t _selected = 0;

  static bool copy(char* dest, const char* src, size_t size) {
    size_t n = 0;
    while (n + 1 < size && src[n]) { dest[n] = src[n]; n++; }
    dest[n] = 0;
    return src[n] != 0;
  }

public:
  static_assert(Capacity > 0 && TextSize > 1, "Inbox must hold terminated text");
  void clear() { _head = Capacity - 1; _count = _selected = 0; }
  void setFlags(size_t index, bool read, bool truncated) {
    if (index < _count) {
      Entry& e = _entries[(_head + Capacity - index) % Capacity];
      e.read = read; e.truncated = truncated;
    }
  }
  // Outbox snapshots use this byte for delivery state in their separate files.
  void setChannelIndex(size_t index, uint8_t value) {
    if (index < _count) _entries[(_head + Capacity - index) % Capacity].channel_idx = value;
  }
  void setReplyTarget(size_t index, const uint8_t* target) {
    if (index >= _count) return;
    Entry& e = _entries[(_head + Capacity - index) % Capacity];
    e.has_reply_target = target != nullptr;
    memset(e.reply_target, 0, sizeof(e.reply_target));
    if (target) memcpy(e.reply_target, target, sizeof(e.reply_target));
  }
  bool erase(size_t index) {
    if (index >= _count) return false;
    for (size_t i = index; i + 1 < _count; i++)
      _entries[(_head + Capacity - i) % Capacity] = *at(i + 1);
    --_count;
    if (!_count) _selected = 0;
    else if (_selected > index) --_selected;
    else if (_selected >= _count) _selected = _count - 1;
    return true;
  }
  void setTemporary(size_t index, bool temporary) {
    if (index < _count) _entries[(_head + Capacity - index) % Capacity].temporary = temporary;
  }
  size_t savedCount() const { size_t n=0; for(size_t i=0;i<_count;i++) if(!at(i)->temporary) ++n; return n; }
  size_t count() const { return _count; }
  size_t selected() const { return _selected; }
  void select(size_t index) { if (index < _count) _selected = index; }
  const Entry* at(size_t index) const {
    return index < _count ? &_entries[(_head + Capacity - index) % Capacity] : nullptr;
  }
  void markRead(size_t index) {
    if (index < _count) _entries[(_head + Capacity - index) % Capacity].read = true;
  }
  size_t unreadCount() const {
    size_t unread = 0;
    for (size_t i = 0; i < _count; i++) if (!at(i)->read) unread++;
    return unread;
  }

  // Keep the selected record stable on arrival. Return true if it was evicted.
  bool add(uint8_t kind, uint8_t channel_idx, uint32_t sent_at, uint32_t received_at,
           const uint8_t* sender_prefix, const char* origin, const char* text) {
    bool evicted = _count == Capacity && _selected == Capacity - 1;
    if (_count && _selected + 1 < Capacity) _selected++;
    _head = (_head + 1) % Capacity;
    if (_count < Capacity) _count++;
    Entry& entry = _entries[_head];
    entry.sent_at = sent_at;
    entry.received_at = received_at;
    entry.kind = kind;
    entry.channel_idx = channel_idx;
    memset(entry.sender_prefix, 0, sizeof(entry.sender_prefix));
    if (sender_prefix) memcpy(entry.sender_prefix, sender_prefix, sizeof(entry.sender_prefix));
    entry.read = false;
    entry.temporary = false;
    entry.has_reply_target = false;
    memset(entry.reply_target, 0, sizeof(entry.reply_target));
    entry.truncated = copy(entry.origin, origin, sizeof(entry.origin));
    entry.truncated |= copy(entry.text, text, sizeof(entry.text));
    return evicted;
  }
};

// Fixed-font OLED helpers. Preserve newlines, replace unsupported UTF-8 glyphs
// with the same CP437 block used by upstream, and bound every rendered line.
namespace InboxText {
inline void toDisplay(char* dest, const char* src, size_t size) {
  if (!size) return;
  size_t n = 0;
  for (size_t i = 0; src[i] && n + 1 < size; i++) {
    uint8_t c = static_cast<uint8_t>(src[i]);
    if (c == '\n' || (c >= 32 && c <= 126)) dest[n++] = c;
    else if (c == '\t') dest[n++] = ' ';
    else if (c >= 128) {
      dest[n++] = '\xDB';
      while (src[i + 1] && (static_cast<uint8_t>(src[i + 1]) & 0xC0) == 0x80) i++;
    }
  }
  dest[n] = 0;
}

// Returns the next source offset. Caller provides columns + 1 bytes in line.
inline size_t nextLine(const char* text, size_t offset, char* line, size_t columns) {
  size_t n = 0, space = 0;
  while (n < columns && text[offset + n] && text[offset + n] != '\n') {
    if (text[offset + n] == ' ') space = n;
    n++;
  }
  size_t next = offset + n;
  if (text[next] == '\n') next++;
  else if (n == columns && text[next] == ' ') {
    while (text[next] == ' ') next++;
  } else if (n == columns && text[next] && space) {
    n = space;
    next = offset + space + 1;
  }
  memcpy(line, text + offset, n);
  line[n] = 0;
  return next;
}
}
