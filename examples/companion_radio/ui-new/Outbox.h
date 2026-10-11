#pragma once
#include "InboxPersistence.h"

// Only UI-initiated sends are recorded. Separate files; no phone queue changes.
class Outbox {
public:
  enum State : uint8_t { Awaiting, Acknowledged, Unconfirmed, ChannelSent, Failed };
  using History = RecentMessages<80, 16>;
private:
  History _history;
  InboxPersistence<80, 16> _persistence;
public:
  explicit Outbox(InboxStorage* storage, uint32_t now) : _persistence(storage) {
    if (storage) _persistence.load(_history);
    // Millis deadlines and outstanding ACKs do not survive reboot.
    for (size_t i = 0; i < _history.count(); i++) {
      if (_history.at(i)->channel_idx == Awaiting || _history.at(i)->channel_idx > Failed) {
        _history.setChannelIndex(i, Unconfirmed); _persistence.changed(now);
      }
    }
  }
  static const char* status(const History::Entry& e) {
    switch (e.channel_idx) {
      case Awaiting: return "Waiting for ACK";
      case Acknowledged: return "Acknowledged";
      case ChannelSent: return "Sent / no receipt";
      case Failed: return "Not sent";
      default: return "Unconfirmed";
    }
  }
  bool clear(uint32_t now) { return _persistence.clear(_history, now); }
  History& history() { return _history; }
  bool pending() const { return _persistence.pending(); }
  bool failed() const { return _persistence.failed(); }
  void add(uint8_t kind, const uint8_t* key, const char* name, const char* text,
           bool queued, uint32_t ack, uint32_t timeout, uint32_t timestamp, uint32_t now) {
    State state = !queued ? Failed : kind == 1 ? ChannelSent : ack ? Awaiting : Unconfirmed;
    // Bound the deadline to the signed millis comparison window.
    if (timeout > 600000UL) timeout = 600000UL;
    uint8_t ack_bytes[4];
    for (unsigned i = 0; i < 4; i++) ack_bytes[i] = ack >> (8 * i);
    _history.add(kind, state, timestamp, now + timeout, ack_bytes, name, text);
    _history.setReplyTarget(0, key);
    _persistence.changed(now);
  }
  bool acknowledge(uint32_t ack, const uint8_t* key, uint32_t now) {
    if (!ack || !key) return false;
    for (size_t i = 0; i < _history.count(); i++) {
      const auto* e = _history.at(i);
      uint32_t stored = uint32_t(e->sender_prefix[0]) | uint32_t(e->sender_prefix[1]) << 8 |
                        uint32_t(e->sender_prefix[2]) << 16 | uint32_t(e->sender_prefix[3]) << 24;
      if (e->kind == 0 && e->has_reply_target && stored == ack &&
          !memcmp(e->reply_target, key, 32) && (e->channel_idx == Awaiting || e->channel_idx == Unconfirmed)) {
        _history.setChannelIndex(i, Acknowledged); _persistence.changed(now); return true;
      }
    }
    return false;
  }
  void tick(uint32_t now) {
    for (size_t i = 0; i < _history.count(); i++) {
      const auto* e = _history.at(i);
      if (e->channel_idx == Awaiting && int32_t(now - e->received_at) >= 0) {
        _history.setChannelIndex(i, Unconfirmed); _persistence.changed(now);
      }
    }
    _persistence.tick(_history, now);
  }
  bool flush(uint32_t now) { return _persistence.flush(_history, now); }
};
