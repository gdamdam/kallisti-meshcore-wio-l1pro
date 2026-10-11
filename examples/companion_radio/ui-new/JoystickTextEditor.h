#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class JoystickTextEditor {
  char _text[80] = {};
  unsigned _cursor = 0, _option = 0;
  bool _options = false;
  static const char* alphabet() { return " abcdefghijklmnopqrstuvwxyz0123456789.,!?'-:/@()"; }
public:
  enum Result { Continue, Preview, Discard, Empty };
  void open() { _text[0] = 0; _cursor = _option = 0; _options = false; }
  void restore(const char* text, unsigned cursor) {
    open();
    size_t n = 0;
    while (text[n] && n < sizeof(_text) - 1) {
      char c = text[n]; if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
      _text[n++] = strchr(alphabet(), c) ? c : ' ';
    }
    _text[n] = 0; _cursor = cursor > n ? n : cursor;
    if (_cursor > sizeof(_text) - 2) _cursor = sizeof(_text) - 2;
  }
  bool editing() const { return !_options; }
  const char* text() const { return _text; }
  unsigned cursor() const { return _cursor; }
  void cycle(int direction) {
    size_t length = strlen(_text), count = strlen(alphabet());
    char current = _cursor < length ? _text[_cursor] : ' ';
    const char* found = strchr(alphabet(), current);
    size_t index = found ? size_t(found - alphabet()) : 0;
    index = (index + count + direction) % count;
    _text[_cursor] = alphabet()[index];
    if (_cursor == length) _text[_cursor + 1] = 0;
  }
  void remove() {
    size_t length = strlen(_text);
    if (_cursor < length) memmove(_text + _cursor, _text + _cursor + 1, length - _cursor);
    else if (_cursor) { _text[--_cursor] = 0; }
  }
  void insert() {
    size_t length = strlen(_text);
    if (length == sizeof(_text) - 1) return;
    memmove(_text + _cursor + 1, _text + _cursor, length - _cursor + 1);
    _text[_cursor] = ' ';
  }
  bool copyForSend(char* dest, size_t size) const {
    size_t length = strlen(_text);
    while (length && _text[length - 1] == ' ') --length;
    bool non_space = false;
    for (size_t i = 0; i < length; i++) if (_text[i] != ' ') non_space = true;
    if (!non_space || length >= size) return false;
    memcpy(dest, _text, length); dest[length] = 0; return true;
  }
  Result input(uint8_t key) {
    if (_options) {
      if (key == KEY_CANCEL || key == KEY_LEFT) _options = false;
      else if (key == KEY_UP && _option) --_option;
      else if (key == KEY_DOWN && _option < 4) ++_option;
      else if (key == KEY_ENTER) {
        if (_option == 1) { char text[80]; if (!copyForSend(text, sizeof(text))) return Empty; _options = false; return Preview; }
        if (_option == 2) remove();
        if (_option == 3) insert();
        if (_option == 4) { _options = false; return Discard; }
        _options = false;
      }
      return Continue;
    }
    if (key == KEY_CANCEL) { _options = true; _option = 0; }
    else if (key == KEY_UP) cycle(-1);
    else if (key == KEY_DOWN) cycle(1);
    else if (key == KEY_LEFT && _cursor) --_cursor;
    else if (key == KEY_RIGHT && _cursor < strlen(_text) && _cursor < sizeof(_text) - 2) ++_cursor;
    else if (key == KEY_ENTER) { _options = true; _option = 0; }
    return Continue;
  }
  int render(DisplayDriver& d, const char* status = nullptr, bool channel_name = false) {
    d.setTextSize(1); d.setColor(UIColor::primary_txt);
    d.setCursor(0, 0); d.print(channel_name ? "Channel name (no #)" : "Custom text (a-z)");
    if (_options) {
      static const char* const names[] = {"Continue editing", "Preview send", "Delete character", "Insert space", "Discard draft"};
      unsigned first = (_option / 4) * 4;
      for (unsigned row = 0; row < 4 && first + row < 5; row++) {
        char label[24]; snprintf(label, sizeof(label), "%c %s", first + row == _option ? '>' : ' ', channel_name && first + row == 1 ? "Preview join" : channel_name && first + row == 4 ? "Cancel" : names[first + row]);
        d.setCursor(0, 12 + row * 10); d.print(label);
      }
    } else {
      size_t length = strlen(_text);
      for (unsigned row = 0; row < 4; row++) {
        char line[22]; unsigned n = 0;
        while (n < 21 && row * 21 + n < length) { line[n] = _text[row * 21 + n]; ++n; }
        line[n] = 0; d.setCursor(0, 12 + row * 10); d.print(line);
      }
      unsigned x = (_cursor % 21) * 6, y = 12 + (_cursor / 21) * 10;
      d.drawRect(x, y + 8, 6, 1);
    }
    d.setCursor(0, 56); d.print(status ? status : _options ? "Up/Dn OK Left:back" : "Up/Dn:a-z <>:move OK");
    return 100;
  }
};

// Capture editor direction presses and their release events, so a held press
// cannot leak an extra click into the editor menu or a send confirmation.
class EditorDirectionInput {
  int _direction = 0;
  bool _up = false, _down = false, _wake_only = false;
  uint32_t _started = 0, _last = 0;
public:
  struct Event { int direction; bool suppress_up, suppress_down; };
  Event update(bool editing, bool up, bool down, bool awake, uint32_t now) {
    Event e = {0, _up, _down};
    if (editing && up) _up = e.suppress_up = true;
    if (editing && down) _down = e.suppress_down = true;
    if (!up) _up = false;
    if (!down) _down = false;
    int direction = editing && up != down ? (up ? 1 : -1) : 0;
    if (direction != _direction) {
      _direction = direction; _started = _last = now; _wake_only = !awake;
      e.direction = direction; // first event also wakes a sleeping display
    } else if (direction && !_wake_only && uint32_t(now - _started) >= 600 &&
               uint32_t(now - _last) >= (uint32_t(now - _started) >= 1800 ? 60U : 150U)) {
      _last = now; e.direction = direction;
    }
    return e;
  }
};
