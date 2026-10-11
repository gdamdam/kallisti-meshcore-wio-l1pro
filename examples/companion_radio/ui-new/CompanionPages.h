#pragma once
#include "QuickReplies.h"
#include "Outbox.h"
#include "JoystickTextEditor.h"

class ActionsMenu : public UIScreen {
  UITask* _task;
  unsigned _selected = 0;
  bool _confirm = false, _yes = false, _hibernate = false, _clock = false;
public:
  explicit ActionsMenu(UITask* task) : _task(task) {}
  void reset() { _confirm = _yes = _clock = false; }
  void poll() override {
    if (_hibernate && !_task->isButtonPressed()) { _hibernate = false; _task->shutdown(); }
  }
  int render(DisplayDriver& d) override {
    d.setTextSize(1); d.setColor(UIColor::primary_txt);
    if (_clock) {
      d.setCursor(0, 0); d.print("CLOCK (UTC)");
      uint32_t time = _task->clockTime();
      if (time >= 946684800UL && time < 4102444800UL) {
        DateTime dt(time); char line[24];
        snprintf(line, sizeof(line), "%04u-%02u-%02u", unsigned(dt.year()), unsigned(dt.month()), unsigned(dt.day()));
        d.setCursor(0, 20); d.print(line);
        snprintf(line, sizeof(line), "%02u:%02u:%02u UTC", unsigned(dt.hour()), unsigned(dt.minute()), unsigned(dt.second()));
        d.setCursor(0, 34); d.print(line);
      } else { d.setCursor(0, 24); d.print("Time not set"); }
      d.setCursor(0, 56); d.print("OK/Menu: back"); return 1000;
    }
    d.setCursor(0, 0); d.print("ACTIONS");
    if (_confirm) {
      d.setCursor(0, 16); d.print(_selected == 1 ? "Send advert?" : (_selected == 2 ? "Hibernate now?" : "Clear msgs/out/draft?"));
      d.setCursor(0, 30); d.print(_yes ? "  No" : "> No");
      d.setCursor(0, 42); d.print(_yes ? "> Yes" : "  Yes");
    } else {
      char line[24];
      snprintf(line, sizeof(line), "%c Bluetooth: %s", _selected == 0 ? '>' : ' ', _task->isBluetoothEnabled() ? "on" : "off");
      char buzzer[24]; snprintf(buzzer,sizeof(buzzer),"%c Buzzer: %s",_selected==5?'>':' ',_task->isBuzzerQuiet()?"OFF":"ON");
      char auto_read[24]; snprintf(auto_read,sizeof(auto_read),"%c Auto-read: %s",_selected==6?'>':' ',_task->autoReadChannels()?"ON":"OFF");
      const char* rows[] = {line, _selected == 1 ? "> Send advert" : "  Send advert",
        _selected == 2 ? "> Hibernate" : "  Hibernate", _selected == 3 ? "> Clean storage" : "  Clean storage",
        _selected == 4 ? "> Channels" : "  Channels", buzzer, auto_read, _selected == 7 ? "> Show clock" : "  Show clock"};
      unsigned first = (_selected / 4) * 4;
      for (unsigned row = 0; row < 4 && first + row < 8; ++row) { d.setCursor(0, 12 + row * 11); d.print(rows[first + row]); }
    }
    d.setCursor(0, 56); d.print("Up/Dn OK Left/Right");
    return 500;
  }
  bool handleInput(char c) override {
    uint8_t key = static_cast<uint8_t>(c);
    if (_clock) { if (key == KEY_ENTER || key == KEY_CANCEL || key == KEY_LEFT) _clock = false; return true; }
    if (key == KEY_CANCEL && _confirm) { reset(); return true; }
    if (key == KEY_UP) { if (_confirm) _yes = false; else if (_selected) --_selected; return true; }
    if (key == KEY_DOWN) { if (_confirm) _yes = true; else if (_selected < 7) ++_selected; return true; }
    if (key != KEY_ENTER) return false;
    if (_confirm) {
      if (_yes) {
        if (_selected == 1) _task->showAlert(_task->sendAdvert() ? "Advert queued" : "Advert failed", 1500);
        else if (_selected == 2) _hibernate = true;
        else _task->cleanStorage();
      }
      reset();
    } else if (_selected == 0) {
      if (_task->isBluetoothEnabled()) _task->disableBluetooth(); else _task->enableBluetooth();
    } else if (_selected == 4) _task->gotoChannels();
    else if (_selected == 5) _task->toggleBuzzer();
    else if (_selected == 6) _task->toggleAutoReadChannels();
    else if (_selected == 7) _clock = true;
    else { _confirm = true; _yes = false; }
    return true;
  }
};

class QuickSendScreen : public UIScreen {
  UITask* _task;
  enum Stage { Types, Targets, Presets, Edit, Confirm } _stage = Types;
  UISendTarget _targets[4], _target;
  unsigned _offset = 0, _count = 0, _selected = 0, _choice = 0, _type = 0;
  bool _channels = false, _positions = false, _yes = false, _retry = false, _resumed = false;
  char _text[80] = {}, _destination[40] = {};
  uint32_t _fix_time = 0;
  JoystickTextEditor _editor;
  bool loadPage(unsigned offset) {
    UISendTarget first;
    if (!_task->getSendTarget(_channels, offset, first)) return false;
    _targets[0] = first; _count = 1;
    while (_count < 4 && _task->getSendTarget(_channels, offset + _count, _targets[_count])) ++_count;
    _offset = offset; _selected = 0;
    return true;
  }
public:
  explicit QuickSendScreen(UITask* task) : _task(task) {}
  void open(bool positions) {
    _positions = positions; _stage = Types; _channels = false; _yes = _retry = _resumed = false; _type = 0;
    _choice = positions ? QuickReplies::SavedPosition : 0;
  }
  bool retry(const UISendTarget& target, const char* text) {
    _target = target; _retry = true; _positions = false; _fix_time = 0; _yes = false;
    if (!_task->inboxReplyTarget(target.kind, target.channel_idx, target.key, _destination, sizeof(_destination))) return false;
    if (!strncmp(text, "GPS position:", 13)) {
      if (!_task->prepareQuickReply(QuickReplies::GPSPosition, _text, sizeof(_text), _fix_time)) {
        _task->showAlert(_task->gpsReplyError(), 1500); return false;
      }
    } else {
      if (strlen(text) >= sizeof(_text)) return false;
      strcpy(_text, text);
    }
    _stage = Confirm; return true;
  }
  bool editingText() const { return _stage == Edit && _editor.editing(); }
  void cancelConfirmation() { if (_stage == Confirm) { if (_retry) { _stage = Types; _task->gotoOutbox(); } else _stage = _choice == QuickReplies::CustomText ? Edit : Presets; _yes = false; } }
  int render(DisplayDriver& d) override {
    if (_stage == Edit) return _editor.render(d, _task->draftStatus());
    d.setTextSize(1); d.setColor(UIColor::primary_txt);
    d.setCursor(0, 0); d.print(_stage == Confirm ? (_retry ? "Retry: may duplicate" : "Send message?") : _positions ? "Share position" : "QUICK SEND");
    if (_stage == Types) {
      d.setCursor(0, 18); d.print(_type == 0 ? "> Contacts" : "  Contacts");
      d.setCursor(0, 32); d.print(_type == 1 ? "> Channels" : "  Channels");
      if (!_positions && _task->hasDraft()) { d.setCursor(0, 44); d.print(_type == 2 ? "> Resume draft" : "  Resume draft"); }
    } else if (_stage == Targets) {
      for (unsigned row = 0; row < _count; row++) {
        char name[32], line[36]; d.translateUTF8ToBlocks(name, _targets[row].name, sizeof(name));
        snprintf(line, sizeof(line), "%c%c %s", row == _selected ? '>' : ' ', _targets[row].favorite ? '*' : ' ', name);
        d.drawTextEllipsized(0, 12 + row * 11, d.width(), line);
      }
    } else if (_stage == Presets) {
      unsigned first = _positions ? QuickReplies::SavedPosition : (_choice / 4) * 4;
      for (unsigned row = 0; row < 4 && first + row < (_positions ? QuickReplies::GPSPosition + 1 : QuickReplies::Count); row++) {
        char line[24]; snprintf(line, sizeof(line), "%c %s", first + row == _choice ? '>' : ' ', QuickReplies::label(first + row));
        d.setCursor(0, 12 + row * 11); d.print(line);
      }
    } else {
      char name[40]; d.translateUTF8ToBlocks(name, _destination, sizeof(name));
      d.drawTextEllipsized(0, 12, d.width(), name);
      size_t offset = 0; char line[22];
      for (unsigned row = 0; row < 4 && _text[offset]; row++) {
        size_t n = strlen(_text + offset); if (n > 21) n = 21;
        memcpy(line, _text + offset, n); line[n] = 0; offset += n;
        d.setCursor(0, 21 + row * 8); d.print(line);
      }
    }
    d.setCursor(0, 56);
    d.print(_stage == Confirm ? (_yes ? " No >Yes L/R OK" : ">No  Yes L/R OK") : _stage == Targets ? "OK:choose >:star <:bk" : "Up/Dn OK Left:back");
    return 500;
  }
  bool handleInput(char c) override {
    uint8_t key = static_cast<uint8_t>(c);
    if (_stage == Edit) {
      auto result = _editor.input(key);
      if (result == JoystickTextEditor::Discard) {
        _task->discardDraft(_target); _stage = _resumed ? Types : Presets;
        if (_resumed) { _type = 0; _channels = false; }
        _resumed = false;
      }
      else _task->saveDraft(_target, _editor.text(), _editor.cursor());
      if (result == JoystickTextEditor::Empty) _task->showAlert("Enter some text", 1500);
      else if (result == JoystickTextEditor::Preview) {
        if (_editor.copyForSend(_text, sizeof(_text)) && _task->inboxReplyTarget(_target.kind, _target.channel_idx, _target.key, _destination, sizeof(_destination))) {
          _fix_time = 0; _stage = Confirm; _yes = false;
        } else _task->showAlert("Target unavailable", 1500);
      }
      return true;
    }
    if (_stage == Confirm && (key == KEY_LEFT || key == KEY_RIGHT)) {
      _yes = key == KEY_RIGHT; return true;
    }
    if (_stage == Confirm && (key == KEY_UP || key == KEY_DOWN)) return true;
    if (key == KEY_CANCEL || key == KEY_LEFT) {
      if (_retry) { _stage = Types; _task->gotoOutbox(); return true; }
      if (_stage == Types) _task->gotoHomeScreen();
      else if (_stage == Confirm) _stage = _choice == QuickReplies::CustomText ? Edit : Presets;
      else if (_stage == Presets) {
        _stage = _resumed ? Types : Targets;
        if (_resumed) { _type = 0; _channels = false; }
        _resumed = false;
      }
      else _stage = Types;
      _yes = false; return true;
    }
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_RIGHT) {
      bool down = key != KEY_UP;
      if (_stage == Types) {
        unsigned last = !_positions && _task->hasDraft() ? 2 : 1;
        if (down && _type < last) ++_type; else if (!down && _type) --_type;
        _channels = _type == 1;
      }
      else if (_stage == Confirm) _yes = key == KEY_DOWN;
      else if (_stage == Presets) {
        unsigned first = _positions ? QuickReplies::SavedPosition : 0;
        if (!down && _choice > first) --_choice;
        else if (down && _choice + 1 < (_positions ? QuickReplies::GPSPosition + 1 : QuickReplies::Count)) ++_choice;
      } else if (key == KEY_RIGHT) {
        _task->toggleFavorite(_targets[_selected]);
        loadPage(0); // user-triggered reorder only; all other snapshots stay stable
      }
      else if (down) {
        if (_selected + 1 < _count) ++_selected; else loadPage(_offset + _count);
      } else if (_selected) --_selected;
      else if (_offset && loadPage(_offset - 4)) _selected = _count - 1;
      return true;
    }
    if (key != KEY_ENTER) return false;
    if (_stage == Types) {
      if (_type == 2) {
        char text[80]; unsigned cursor;
        if (_task->restoreDraft(_target, text, sizeof(text), cursor)) {
          _editor.restore(text, cursor); _choice = QuickReplies::CustomText; _resumed = true; _stage = Edit;
        } else _task->showAlert("No draft", 1500);
        return true;
      }
      if (loadPage(0)) _stage = Targets;
      else _task->showAlert(_channels ? "No channels" : "No contacts", 1500);
    } else if (_stage == Targets) {
      // Keep exactly the displayed identity, even if contacts change later.
      _target = _targets[_selected];
      if (_task->inboxReplyTarget(_target.kind, _target.channel_idx, _target.key, _destination, sizeof(_destination))) {
        _choice = _positions ? QuickReplies::SavedPosition : 0; _stage = Presets;
      } else _task->showAlert("Target unavailable", 1500);
    } else if (_stage == Presets) {
      if (!_task->inboxReplyTarget(_target.kind, _target.channel_idx, _target.key, _destination, sizeof(_destination)))
        _task->showAlert("Target unavailable", 1500);
      else if (_choice == QuickReplies::CustomText) {
        UISendTarget draft; char text[80]; unsigned cursor;
        if (_task->restoreDraft(draft, text, sizeof(text), cursor) && draft.kind == _target.kind && !memcmp(draft.key, _target.key, 32)) _editor.restore(text, cursor);
        else _editor.open();
        _stage = Edit;
      }
      else if (_task->prepareQuickReply(_choice, _text, sizeof(_text), _fix_time)) { _stage = Confirm; _yes = false; }
      else _task->showAlert(_choice == QuickReplies::GPSPosition ? _task->gpsReplyError() : "No saved position", 1500);
    } else {
      if (_yes) {
        bool ok = _task->quickReplyStillValid(_fix_time) &&
          _task->sendInboxReply(_target.kind, _target.channel_idx, _target.key, _text);
        if (ok && !_retry && _choice == QuickReplies::CustomText) _task->discardDraft(_target);
        _task->showAlert(ok ? "Message queued" : "Message not sent", 1500);
        _stage = Types; _type = 0; _channels = _resumed = false; // fresh selection after sending
      } else _stage = _choice == QuickReplies::CustomText ? Edit : Presets;
      if (_retry) { _stage = Types; _task->gotoOutbox(); }
      _yes = false;
    }
    return true;
  }
};

class OutboxScreen : public UIScreen {
  UITask* _task;
  Outbox* _outbox;
  bool _detail = false;
  unsigned _scroll = 0, _lines = 0;
public:
  OutboxScreen(UITask* task, Outbox* outbox) : _task(task), _outbox(outbox) {}
  void open() { _outbox->history().select(0); _detail = false; _scroll = 0; }
  int render(DisplayDriver& d) override {
    auto& h = _outbox->history();
    d.setTextSize(1); d.setColor(UIColor::primary_txt);
    d.setCursor(0, 0); d.print("OUTBOX");
    if (!h.count()) { d.setCursor(0, 24); d.print("No UI sends yet"); }
    else if (!_detail) {
      size_t first = (h.selected() / 4) * 4;
      for (unsigned row = 0; row < 4 && first + row < h.count(); row++) {
        const auto* e = h.at(first + row); char name[32], line[40];
        d.translateUTF8ToBlocks(name, e->origin, sizeof(name));
        char status = e->channel_idx == Outbox::Acknowledged ? '+' : e->channel_idx == Outbox::Failed ? '!' : e->channel_idx == Outbox::ChannelSent ? '#' : '?';
        snprintf(line, sizeof(line), "%c%c %s", h.selected() == first + row ? '>' : ' ', status, name);
        d.drawTextEllipsized(0, 12 + row * 11, d.width(), line);
      }
    } else {
      const auto* e = h.at(h.selected()); char time[32] = "unknown", text[220], name[32];
      if (e->sent_at >= 946684800UL && e->sent_at < 4102444800UL) {
        DateTime dt(e->sent_at);
        snprintf(time, sizeof(time), "%02u-%02u-%02u %02u:%02u", unsigned(dt.year() % 100), unsigned(dt.month()), unsigned(dt.day()), unsigned(dt.hour()), unsigned(dt.minute()));
      }
      d.translateUTF8ToBlocks(name, e->origin, sizeof(name));
      snprintf(text, sizeof(text), "%s\n%s\n%s\n%s", name, Outbox::status(*e), time, e->text);
      size_t offset = 0; char line[22]; _lines = 0;
      while (text[offset]) {
        offset = InboxText::nextLine(text, offset, line, 21);
        if (_lines >= _scroll && _lines < _scroll + 5) { d.setCursor(0, 12 + (_lines - _scroll) * 8); d.print(line); }
        ++_lines;
      }
    }
    d.setCursor(0, 56);
    d.print(_outbox->failed() ? "Outbox storage error" : _outbox->pending() ? "Saving..." : _detail ? "Up/Dn >:retry <:back" : "Up/Dn OK Left:back");
    return 500;
  }
  bool handleInput(char c) override {
    uint8_t key = static_cast<uint8_t>(c); auto& h = _outbox->history();
    if (key == KEY_CANCEL || key == KEY_LEFT) {
      if (_detail) { _detail = false; _scroll = 0; } else _task->gotoHomeScreen();
      return true;
    }
    if (!h.count()) return false;
    if (key == KEY_UP) { if (_detail) { if (_scroll) --_scroll; } else if (h.selected()) h.select(h.selected() - 1); }
    else if (key == KEY_DOWN) { if (_detail) { if (_scroll + 5 < _lines) ++_scroll; } else h.select(h.selected() + 1); }
    else if (key == KEY_ENTER) { _detail = true; _scroll = 0; }
    else if (key == KEY_RIGHT && _detail) _task->retryOutbox(*h.at(h.selected()));
    else return false;
    return true;
  }
};

// Channel management keeps a frozen identity for deletion, just like sending.
class ChannelsScreen : public UIScreen {
  UITask* _task;
  enum Stage { Menu, Edit, List, Join, Remove } _stage = Menu;
  unsigned _choice = 0, _index = 0;
  bool _yes = false;
  UISendTarget _target;
  char _name[32] = {};
  JoystickTextEditor _editor;
public:
  explicit ChannelsScreen(UITask* task) : _task(task) {}
  void open() { _stage = Menu; _choice = 0; _yes = false; }
  bool editingText() const { return _stage == Edit && _editor.editing(); }
  int render(DisplayDriver& d) override {
    if (_stage == Edit) return _editor.render(d, nullptr, true);
    d.setTextSize(1); d.setColor(UIColor::primary_txt);
    d.setCursor(0,0); d.print("CHANNELS");
    if (_stage == Menu) {
      const char* rows[] = {"Join Public", "Join #channel", "Remove channel"};
      for (unsigned i=0;i<3;i++) { char line[24]; snprintf(line,sizeof(line),"%c %s",i==_choice?'>':' ',rows[i]);d.setCursor(0,16+i*12);d.print(line); }
    } else {
      d.setCursor(0,16);d.print(_stage==Join ? "Join channel?" : _stage==Remove ? "Remove channel?" : "Choose to remove");
      char name[32]; d.translateUTF8ToBlocks(name,_stage==Join?_name:_target.name,sizeof(name));
      d.drawTextEllipsized(0,30,d.width(),name);
    }
    d.setCursor(0,56); d.print(_stage==Join||_stage==Remove ? (_yes?" No >Yes L/R OK":">No  Yes L/R OK") : "Up/Dn OK Menu:back");
    return 500;
  }
  bool handleInput(char c) override {
    uint8_t key=static_cast<uint8_t>(c);
    if (_stage==Edit) {
      auto result=_editor.input(key);
      if(result==JoystickTextEditor::Discard) _stage=Menu;
      if(result==JoystickTextEditor::Empty) _task->showAlert("Enter channel name",1500);
      if(result==JoystickTextEditor::Preview) {
        char name[80];
        if(!_editor.copyForSend(name,sizeof(name)) || strlen(name)>30) { _task->showAlert("Max 30 characters",1500);return true; }
        for (const char* p = name; *p; ++p) {
          if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) {
            _task->showAlert("Use a-z 0-9 or -", 1500); return true;
          }
        }
        _name[0]='#';strcpy(_name+1,name);_yes=false;_stage=Join;
      }
      return true;
    }
    if(key==KEY_CANCEL) { if(_stage==Menu) _task->gotoHomeScreen();else _stage=Menu;_yes=false;return true; }
    if(_stage==Join||_stage==Remove) {
      if(key==KEY_LEFT||key==KEY_RIGHT) _yes=key==KEY_RIGHT;
      if(key==KEY_ENTER) {
        if(_yes) {
          if(_stage==Join) {
            const char* error=_task->joinChannel(_name);_task->showAlert(error?error:"Channel joined",2000);
            if (error && _name[0] == '#') { _stage=Edit; _yes=false; return true; }
          }
          else _task->showAlert(_task->removeChannel(_target)?"Channel removed":"Channel changed/error",2000);
        }
        _stage=Menu;_yes=false;
      }
      return true;
    }
    if(key==KEY_LEFT) { if(_stage==Menu)_task->gotoHomeScreen();else _stage=Menu;return true; }
    if(key==KEY_UP||key==KEY_DOWN) {
      if(_stage==Menu) { if(key==KEY_UP&&_choice)--_choice;else if(key==KEY_DOWN&&_choice<2)++_choice; }
      else { unsigned next=_index;if(key==KEY_UP&&next)--next;else if(key==KEY_DOWN)++next;UISendTarget target;if(_task->getSendTarget(true,next,target)){_target=target;_index=next;} }
      return true;
    }
    if(key==KEY_ENTER) {
      if(_stage==List) { _stage=Remove;_yes=false; }
      else if(_choice==0) { strcpy(_name,"Public");_stage=Join;_yes=false; }
      else if(_choice==1) { _editor.open();_stage=Edit; }
      else if(_task->getSendTarget(true,0,_target)) { _index=0;_stage=List; }
      else _task->showAlert("No channels",1500);
    }
    return true;
  }
};
