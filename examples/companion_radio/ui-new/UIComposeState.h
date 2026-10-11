#pragma once
#include "InboxPersistence.h"

// UI-only favorites overrides and one recoverable custom-text draft.
class UIComposeState {
  RecentMessages<2, 32> _favorites;
  InboxPersistence<2, 32> _favorite_disk;
  RecentMessages<2, 1> _settings;
  InboxPersistence<2, 1> _settings_disk;
  RecentMessages<80, 1> _draft;
  InboxPersistence<80, 1> _draft_disk;
  static bool matches(uint8_t kind, const uint8_t* key, uint8_t stored_kind, const uint8_t* stored_key) {
    return kind == stored_kind && !memcmp(key, stored_key, 32);
  }
public:
  UIComposeState(InboxStorage* favorites, InboxStorage* draft, InboxStorage* settings = nullptr) : _favorite_disk(favorites), _settings_disk(settings), _draft_disk(draft) {
    if (favorites) _favorite_disk.load(_favorites);
    // Older firmware kept false overrides forever; app favorites now always win.
    for (size_t i = _favorites.count(); i > 0; --i) {
      if (!_favorites.at(i - 1)->read || !_favorites.at(i - 1)->has_reply_target) {
        _favorites.erase(i - 1); _favorite_disk.changed(0);
      }
    }
    if (draft) _draft_disk.load(_draft);
    if (settings) _settings_disk.load(_settings);
  }
  bool autoReadChannels() const { return !_settings.count() || _settings.at(0)->text[0] != '0'; }
  bool toggleAutoRead(uint32_t now) {
    bool enabled = !autoReadChannels();
    _settings.clear(); _settings.add(0,0,0,0,nullptr,"auto read",enabled ? "1" : "0");
    _settings_disk.changed(now); return _settings_disk.flush(_settings,now);
  }
  bool favorite(const UISendTarget& target) const {
    if (target.favorite) return true; // app favorites cannot be hidden by a local override
    for (size_t i = 0; i < _favorites.count(); i++) {
      const auto* e = _favorites.at(i);
      if (e->has_reply_target && matches(target.kind, target.key, e->kind, e->reply_target)) return e->read;
    }
    return target.favorite;
  }
  bool toggle(const UISendTarget& target, uint32_t now) {
    if (target.favorite) return true; // never remove a phone-app favorite
    bool value = !favorite(target);
    size_t index = _favorites.count();
    for (size_t i = 0; i < _favorites.count(); i++) {
      const auto* e = _favorites.at(i);
      if (matches(target.kind, target.key, e->kind, e->reply_target)) { index = i; break; }
    }
    if (index == _favorites.count()) {
      if (_favorites.count() == 32) return false; // never silently evict a favorite override
      _favorites.add(target.kind, target.channel_idx, 0, 0, nullptr, target.name, "");
      _favorites.setReplyTarget(0, target.key); index = 0;
    }
    if (value) _favorites.setFlags(index, true, false);
    else _favorites.erase(index);
    _favorite_disk.changed(now);
    return _favorite_disk.flush(_favorites, now);
  }
  bool hasDraft() const { return _draft.count() != 0; }
  bool restore(UISendTarget& target, char* text, size_t size, unsigned& cursor) const {
    const auto* e = _draft.at(0);
    if (!e || !e->has_reply_target || strlen(e->text) >= size) return false;
    target.kind = e->kind; target.channel_idx = e->channel_idx;
    memcpy(target.key, e->reply_target, 32); memcpy(target.name, e->origin, 32);
    strcpy(text, e->text); cursor = e->sent_at;
    return true;
  }
  void save(const UISendTarget& target, const char* text, unsigned cursor, uint32_t now) {
    const auto* old = _draft.at(0);
    if (old && matches(target.kind, target.key, old->kind, old->reply_target) &&
        old->channel_idx == target.channel_idx && old->sent_at == cursor && !strcmp(old->text, text)) return;
    _draft.clear(); _draft.add(target.kind, target.channel_idx, cursor, 0, nullptr, target.name, text);
    _draft.setReplyTarget(0, target.key); _draft_disk.changed(now);
  }
  bool discard(const UISendTarget& target, uint32_t now) {
    const auto* e = _draft.at(0);
    if (!e || !matches(target.kind, target.key, e->kind, e->reply_target)) return true;
    _draft.clear(); _draft_disk.changed(now); return _draft_disk.flush(_draft, now);
  }
  bool clearDraft(uint32_t now) { return _draft_disk.clear(_draft, now); }
  const char* draftStatus() const {
    return _draft_disk.failed() ? "Draft storage error" : _draft_disk.pending() ? "Saving draft..." : nullptr;
  }
  void tick(uint32_t now) { _settings_disk.tick(_settings,now); _favorite_disk.tick(_favorites, now); _draft_disk.tick(_draft, now); }
  bool flush(uint32_t now) {
    bool settings = _settings_disk.flush(_settings,now);
    bool favorites = _favorite_disk.flush(_favorites, now);
    bool draft = _draft_disk.flush(_draft, now); return favorites && draft && settings;
  }
};
