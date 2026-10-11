// Standalone host test: c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined
//   test/test_recent_messages/recent_messages.cpp -o /tmp/recent_messages && /tmp/recent_messages
#include "../../examples/companion_radio/ui-new/RecentMessages.h"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>

static std::vector<std::string> lines(const char* text, size_t columns) {
  std::vector<std::string> result;
  char line[22];
  size_t offset = 0;
  while (text[offset]) {
    size_t next = InboxText::nextLine(text, offset, line, columns);
    assert(next > offset && strlen(line) <= columns);
    result.push_back(line);
    offset = next;
  }
  return result;
}

int main() {
  RecentMessages<185> history;
  assert(history.count() == 0 && history.unreadCount() == 0);
  assert(history.at(0) == nullptr);
  history.select(99);
  history.markRead(99);
  uint8_t prefix[] = {0x12, 0x34, 0x56, 0x78};
  history.add(2, 0xFF, 123, 456, prefix, "Room relay", "First message");
  prefix[0] = 0;
  assert(history.at(0)->sender_prefix[0] == 0x12);
  assert(history.at(0)->sent_at == 123 && history.at(0)->received_at == 456);
  history.markRead(0);
  history.markRead(0);
  assert(history.unreadCount() == 0 && history.count() == 1);
  for (unsigned i = 1; i < 32; i++) {
    assert(!history.add(1, 7, i, i, nullptr, "Channel at receipt", "body"));
  }
  assert(history.count() == 32 && history.selected() == 31);
  assert(!strcmp(history.at(history.selected())->text, "First message"));
  assert(history.unreadCount() == 31);
  assert(history.add(0, 0xFF, 0, 0, nullptr, "DM", "replacement"));
  assert(history.count() == 32 && history.unreadCount() == 32);
  assert(history.selected() == 31);
  history.select(2);
  auto timestamp = history.at(2)->sent_at;
  history.markRead(2);
  assert(!history.add(0, 0xFF, 0, 0, nullptr, "DM", "new arrival"));
  assert(history.selected() == 3 && history.at(3)->sent_at == timestamp);
  assert(history.at(3)->read && history.unreadCount() == 31);
  std::string full(184, 'X'), too_long(300, 'Z');
  history.add(0, 0xFF, 0, 0, nullptr, "DM", full.c_str());
  assert(!history.at(0)->truncated && strlen(history.at(0)->text) == 184);
  history.add(0, 0xFF, 0, 0, nullptr, "DM", too_long.c_str());
  assert(history.at(0)->truncated && strlen(history.at(0)->text) == 184);
  for (unsigned i = 0; i < 1000; i++) {
    history.add(1, i % 40, i, i, nullptr, "ch", "text");
    assert(history.count() == 32 && history.at(31));
  }
  assert(history.unreadCount() == 32);
  for (size_t i = 0; i < 32; i++) history.markRead(i);
  assert(history.unreadCount() == 0);
  history.add(0, 0xFF, 0, 0, nullptr, "DM", "new");
  assert(history.unreadCount() == 1);
  RecentMessages<185> rebooted;
  assert(rebooted.count() == 0);

  char display[400];
  InboxText::toDisplay(display, "line 1\r\nline 2\t\xF0\x9F\x91\x8D", sizeof(display));
  assert(!strcmp(display, "line 1\nline 2 \xDB"));
  char tiny[2];
  InboxText::toDisplay(tiny, "abcdef", sizeof(tiny));
  assert(!strcmp(tiny, "a"));
  assert(lines("", 21).empty());
  auto wrapped = lines("one two three\n\nlast", 7);
  assert((wrapped == std::vector<std::string>{"one two", "three", "", "last"}));
  auto exact = lines("1234567\nnext", 7);
  assert((exact == std::vector<std::string>{"1234567", "next"}));
  auto long_word = lines(full.c_str(), 21);
  std::string reconstructed;
  for (const auto& line : long_word) reconstructed += line;
  assert(reconstructed == full && long_word.back().size() == 16);
  printf("RecentMessages tests passed; 32-entry buffer: %zu bytes\n", sizeof(history));
}
