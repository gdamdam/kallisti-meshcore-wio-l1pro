#include <assert.h>
#include <stdio.h>
#include <vector>
#include <Arduino.h>
unsigned long button_millis = 100;
int button_level = HIGH;
#include <helpers/ui/MomentaryButton.cpp>

static void click(MomentaryButton& button, std::vector<int>& events) {
  button_millis += 50; button_level = LOW;
  int event = button.check(); if (event) events.push_back(event);
  button_millis += 50; button_level = HIGH;
  event = button.check(); if (event) events.push_back(event);
}
static void settle(MomentaryButton& button, std::vector<int>& events) {
  button_millis += 281;
  int event = button.check(); if (event) events.push_back(event);
}
int main() {
  MomentaryButton menu(13, 1000, true, false, true);
  std::vector<int> events;
  click(menu, events);
  assert(events.empty());
  settle(menu, events);
  assert(events.size() == 1 && events[0] == BUTTON_EVENT_CLICK);
  events.clear();
  click(menu, events); click(menu, events); click(menu, events);
  assert(events.empty());
  settle(menu, events);
  assert(events.size() == 1 && events[0] == BUTTON_EVENT_TRIPLE_CLICK);
  events.clear();
  // Two successive double-click gestures each produce exactly one event.
  for(int i=0;i<2;i++) {
    click(menu,events); click(menu,events); assert(events.empty()); settle(menu,events);
    assert(events.size()==1 && events[0]==BUTTON_EVENT_DOUBLE_CLICK); events.clear();
  }
  MomentaryButton up(25, 0, true, false, false);
  click(up, events);
  assert(events.size() == 1 && events[0] == BUTTON_EVENT_CLICK);
  puts("Button tests passed: delayed Menu back, double/triple-click events, up/down click");
}
