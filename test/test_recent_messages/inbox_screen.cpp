#include <assert.h>
#include <stdio.h>
#include <ctime>
#include <string>
#include <vector>
#include <helpers/ui/UIScreen.h>
#include "../../examples/companion_radio/ui-new/InboxPersistence.h"
#include "../../examples/companion_radio/ui-new/QuickReplies.h"
#include "send-target-under-test.h"
#include "../../examples/companion_radio/ui-new/UIComposeState.h"
#include "../../examples/companion_radio/ui-new/Outbox.h"

ColorVal UIColor::window_bkg = 0, UIColor::title_bkg = 0, UIColor::title_txt = 1,
  UIColor::primary_txt = 1, UIColor::secondary_txt = 1, UIColor::warning_txt = 1,
  UIColor::popup_bkg = 0, UIColor::popup_txt = 1, UIColor::corp_blue = 1;
#define MAX_PACKET_PAYLOAD 184
#define UI_RECENT_INBOX 1
#define ENV_INCLUDE_GPS 1
#define UI_SENSORS_PAGE 0
#define AUTO_OFF_MILLIS 15000
static unsigned long now_millis = 1000;
static unsigned long millis() { return now_millis; }
struct UIMessageMetadata;
class UITask {
public:
  int home_calls = 0, _msgcount = 0;
  DisplayDriver* _display = nullptr;
  UIScreen *home = nullptr, *curr = nullptr, *msg_preview = nullptr;
  unsigned long _auto_off = 0, _next_refresh = 0;
  uint32_t _last_input_at = 0;
  unsigned long _alert_expiry = 0;
  void autoEntry(char c = 0);
  uint32_t clockTime() const { return 1790000000; }
  bool connected = false;
  UIComposeState* compose = nullptr;
  unsigned outbox_calls = 0, retry_calls = 0, quick_calls = 0, unread_calls = 0, gps_toggles = 0;
  bool position_only = false;
  void gotoQuickSend(bool positions = false) { ++quick_calls; position_only = positions; }
  void gotoNextUnread() { ++unread_calls; }
  void toggleGPS() { ++gps_toggles; }
  void gotoOutbox() { ++outbox_calls; }
  void retryOutbox(const Outbox::History::Entry&) { ++retry_calls; }
  bool toggleFavorite(const UISendTarget& target) { return compose && compose->toggle(target, millis()); }
  bool hasDraft() const { return compose && compose->hasDraft(); }
  bool restoreDraft(UISendTarget& target, char* text, size_t size, unsigned& cursor) { return compose && compose->restore(target,text,size,cursor); }
  void saveDraft(const UISendTarget& target,const char* text,unsigned cursor) { if(compose) compose->save(target,text,cursor,millis()); }
  void discardDraft(const UISendTarget& target) { if(compose) compose->discard(target,millis()); }
  const char* draftStatus() const { return compose ? compose->draftStatus() : nullptr; }
  UIScreen* quick_send = nullptr;
  bool bluetooth = true, button_pressed = false;
  unsigned advert_calls = 0, shutdown_calls = 0;
  bool isBluetoothEnabled() const { return bluetooth; }
  void enableBluetooth() { bluetooth = true; }
  void disableBluetooth() { bluetooth = false; }
  bool sendAdvert() { ++advert_calls; return true; }
  bool isButtonPressed() const { return button_pressed; }
  unsigned clean_calls = 0;
  void cleanStorage() { ++clean_calls; }
  bool auto_channels=true;
  bool autoReadChannels() const { return auto_channels; }
  void toggleAutoReadChannels() { auto_channels=!auto_channels; }
  bool displayAwake() const { return _display && _display->isOn(); }
  bool quiet=false;
  bool isBuzzerQuiet() const { return quiet; }
  void toggleBuzzer() { quiet=!quiet; }
  unsigned channel_calls=0, joins=0, removals=0;
  void gotoChannels() { ++channel_calls; }
  const char* join_error=nullptr;
  const char* joinChannel(const char*) { ++joins; return join_error; }
  bool removeChannel(const UISendTarget&) { ++removals; return true; }
  void shutdown() { ++shutdown_calls; }
  bool getSendTarget(bool channels, unsigned index, UISendTarget& target) {
    if (index >= (channels ? 3U : 6U)) return false;
    target.kind = channels ? 1 : 0; target.channel_idx = index;
    memset(target.key, 0, 32); target.key[0] = index + 1;
    snprintf(target.name, sizeof(target.name), "%s %u", channels ? "Channel" : "Contact", index);
    return true;
  }
  void setCurrScreen(UIScreen* screen) { curr = screen; }
  void gotoHomeScreen() { home_calls++; setCurrScreen(home); }
  bool hasConnection() const { return connected; }
  void showAlert(const char* text, int) { alert = text; }
  int sent = 0; bool target_available = true, send_ok = true, gps_valid = true;
  std::string sent_text, alert;
  uint8_t last_target[32] = {};
  bool inboxReplyTarget(uint8_t kind, uint8_t, const uint8_t*, char* name, size_t size) {
    snprintf(name, size, "DM: Test"); return target_available && kind != 2;
  }
  bool sendInboxReply(uint8_t, uint8_t, const uint8_t* key, const char* text) {
    memcpy(last_target, key, 32);
    sent++; sent_text = text; return send_ok;
  }
  bool prepareQuickReply(unsigned choice, char* text, size_t size, uint32_t& time) {
    return QuickReplies::build(choice, 10.5,-20.25, gps_valid, gps_valid, 10500000, -20250000,
                              0, millis(), text, size, time);
  }
  const char* gpsReplyError() { return "GPS: waiting for fix"; }
  bool quickReplyStillValid(uint32_t fix_time) { return !fix_time || gps_valid; }
  void gotoInbox();
  int getUnreadCount() const;
  void msgRead(int count);
  void newMsg(uint8_t, const char*, const char*, int);
  void newMsgWithMetadata(uint8_t, const char*, const char*, int, const UIMessageMetadata&);
  char checkDisplayOn(char);
};
struct Clock {
  uint32_t getCurrentTime() const { return 1700000000; }
} rtc_clock;
// Only the RTC adapter is substituted; the actual screen class is compiled below.
class DateTime {
  time_t value;
public:
  explicit DateTime(uint32_t t) : value(t) { }
  unsigned year() const { return std::gmtime(&value)->tm_year + 1900; }
  unsigned month() const { return std::gmtime(&value)->tm_mon + 1; }
  unsigned day() const { return std::gmtime(&value)->tm_mday; }
  unsigned hour() const { return std::gmtime(&value)->tm_hour; }
  unsigned minute() const { return std::gmtime(&value)->tm_min; }
  unsigned second() const { return std::gmtime(&value)->tm_sec; }
};
#include "../../examples/companion_radio/ui-new/CompanionPages.h"
#include "../../examples/companion_radio/ui-new/GPSPanel.h"
#include "fake_inbox_storage.h"
#include "screen-under-test.h"
#include "home-nav-under-test.h"
#include "auto-entry-under-test.h"

class Display : public DisplayDriver {
public:
  std::vector<std::string> rows;
  int x = 0, y = 0;
  unsigned filled_rects = 0;
  Display() : DisplayDriver(128, 64) { }
  bool on = true;
  bool isOn() override { return on; }
  void turnOn() override { on = true; }
  void turnOff() override { on = false; }
  void clear() override { rows.clear(); }
  void startFrame(ColorVal = 0) override { clear(); }
  void endFrame() override { }
  void setTextSize(int s) override { assert(s == 1); }
  void setColor(ColorVal) override { }
  void setCursor(int px, int py) override { x = px; y = py; }
  void print(const char* s) override {
    assert(x >= 0 && y >= 0 && y + 8 <= height());
    assert(x + int(strlen(s)) * 6 <= width());
    rows.push_back(s);
  }
  void fillRect(int, int, int, int) override { ++filled_rects; }
  void drawRect(int, int, int, int) override { }
  void drawXbm(int, int, const uint8_t*, int, int) override { }
  uint16_t getTextWidth(const char* s) override { return strlen(s) * 6; }
  std::string text() const {
    std::string result;
    for (const auto& row : rows) result += row + "\n";
    return result;
  }
};
static std::string render(InboxScreen& screen, Display& display) {
  display.clear(); screen.render(display); return display.text();
}
static std::string scrollAll(InboxScreen& screen, Display& display) {
  std::string result;
  for (int i = 0; i < 90; i++) {
    result += render(screen, display);
    screen.handleInput(static_cast<char>(KEY_DOWN));
  }
  return result;
}
int main() {

  {
    UITask task; Display display; FakeInboxStorage storage; InboxScreen inbox(&task,&storage); HomeNav home(&task);
    task.home=&home;task.curr=&home;task.msg_preview=&inbox;task._display=&display;home._page=HomeNav::ACTIONS;
    for(int i=0;i<3;i++){home.handleInput(KEY_DOWN);}home.handleInput(KEY_ENTER);home.handleInput(KEY_DOWN);
    UIMessageMetadata channel(UIMessageMetadata::Channel,1700000000,0,nullptr);inbox.add("Public","auto channel",channel);
    now_millis+=6000;task.autoEntry();assert(task.curr==&home && !home.canAutoRead());
    home.leave();display.clear();home._actions.render(display);assert(display.text().find("Clear msgs/out/draft?")==std::string::npos);
    home._page=HomeNav::FIRST;task.checkDisplayOn(KEY_DOWN);task.autoEntry();assert(task.curr==&home);
    now_millis+=5001;task.autoEntry(KEY_DOWN);assert(task.curr==&home);task.autoEntry();assert(task.curr==&inbox);
    // Clock remains in ACTIONS, updates once per second and exits predictably.
    ActionsMenu actions(&task);for(int i=0;i<7;i++){actions.handleInput(KEY_DOWN);}actions.handleInput(KEY_ENTER);
    display.clear();assert(actions.render(display)==1000);assert(display.text().find("CLOCK (UTC)")!=std::string::npos);
    assert(display.text().find("2026-")!=std::string::npos);actions.handleInput(KEY_CANCEL);
    display.clear();actions.render(display);assert(display.text().find("Show clock")!=std::string::npos);
  }
  {
    UITask task;Display display;FakeInboxStorage favorites,drafts;UIComposeState compose(&favorites,&drafts);task.compose=&compose;
    UISendTarget target;strcpy(target.name,"Saved recipient");target.key[0]=1;compose.save(target,"draft",0,1);
    QuickSendScreen send(&task);send.open(false);send.handleInput(KEY_DOWN);send.handleInput(KEY_DOWN);send.handleInput(KEY_ENTER);
    send.handleInput(KEY_CANCEL);for(int i=0;i<4;i++){send.handleInput(KEY_DOWN);}send.handleInput(KEY_ENTER);
    display.clear();send.render(display);assert(display.text().find("> Contacts")!=std::string::npos);
    send.handleInput(KEY_ENTER);display.clear();send.render(display);assert(display.text().find("Contact 0")!=std::string::npos);
    // Channel join failure retains the name in the editor for correction/retry.
    ChannelsScreen channels(&task);channels.open();channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);
    channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);
    task.join_error="Channel slots full";channels.handleInput(KEY_RIGHT);channels.handleInput(KEY_ENTER);
    display.clear();channels.render(display);assert(display.text().find("Channel name")!=std::string::npos && display.text().find("a")!=std::string::npos);
    channels.handleInput(KEY_UP); // a -> space, then Up -> unsupported ')' character
    channels.handleInput(KEY_UP);channels.handleInput(KEY_ENTER);channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);
    assert(task.alert=="Use a-z 0-9 or -");display.clear();channels.render(display);assert(display.text().find("Channel name")!=std::string::npos);
  }

  {
    Display d;
    GPSPanelData gps = {true,true,false,3,10500000,-20250000,123000,2000,true};
    renderGPSPanel(d,gps); assert(d.text().find("ACQUIRING")!=std::string::npos);
    assert(d.text().find("Lat")==std::string::npos);
    gps.valid=true; d.clear(); renderGPSPanel(d,gps);
    assert(d.text().find("GPS FIX")!=std::string::npos && d.text().find("20.25000 W")!=std::string::npos);
    assert(d.text().find("Alt 123m Fix 2s")!=std::string::npos);
    gps.fix_age_ms+=40000; d.clear(); renderGPSPanel(d,gps); assert(d.text().find("STALE")!=std::string::npos);
    gps.fix_age_ms=0; gps.altitude_valid=false; d.clear();renderGPSPanel(d,gps);
    assert(d.text().find("GPS FIX")!=std::string::npos && d.text().find("Alt -- Fix 0s")!=std::string::npos);
    gps.enabled=false; d.clear(); renderGPSPanel(d,gps); assert(d.text().find("GPS OFF")!=std::string::npos);
    assert(d.text().find("Lat")==std::string::npos);
  }
  {
    UITask task; Display d; FakeInboxStorage storage; InboxScreen inbox(&task,&storage);
    UIMessageMetadata from(UIMessageMetadata::Contact,1790000000,255,nullptr);
    for(int i=0;i<64;i++) { char text[16]; snprintf(text,sizeof(text),"msg%d",i); inbox.add("test",text,from); }
    inbox.open(); for(int i=0;i<20;i++) inbox.handleInput(KEY_DOWN);
    inbox.handleInput(KEY_ENTER); // msg43 read, older unread messages remain protected
    assert(scrollAll(inbox,d).find("snt: 26-")!=std::string::npos);
    inbox.add("test","replacement",from); inbox.flush();
    RecentMessages<185,72> h; InboxPersistence<185,72> disk(&storage); assert(disk.load(h) && h.count()==64);
    bool oldest=false,read_survived=false;
    for(size_t i=0;i<h.count();i++) { oldest |= !strcmp(h.at(i)->text,"msg0"); read_survived |= !strcmp(h.at(i)->text,"msg43"); }
    assert(oldest && !read_survived && !strcmp(h.at(0)->text,"replacement"));
    inbox.add("test","next",from); inbox.flush();
    RecentMessages<185,72> h2; InboxPersistence<185,72> disk2(&storage); assert(disk2.load(h2));
    assert(!strcmp(h2.at(63)->text,"msg1")); // all unread: evict oldest
  }
  {
    UITask task; Display d; ChannelsScreen channels(&task); channels.open();
    channels.handleInput(KEY_ENTER); channels.handleInput(KEY_ENTER); assert(!task.joins);
    channels.handleInput(KEY_ENTER); channels.handleInput(KEY_RIGHT); channels.handleInput(KEY_ENTER); assert(task.joins==1);
    channels.handleInput(KEY_DOWN); channels.handleInput(KEY_ENTER); channels.handleInput(KEY_DOWN);
    assert(channels.editingText()); channels.handleInput(KEY_ENTER); channels.handleInput(KEY_DOWN); channels.handleInput(KEY_ENTER);
    d.clear();channels.render(d);assert(d.text().find("#a")!=std::string::npos);
    channels.handleInput(KEY_RIGHT);channels.handleInput(KEY_ENTER);assert(task.joins==2);
    channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);channels.handleInput(KEY_ENTER);
    channels.handleInput(KEY_DOWN);channels.handleInput(KEY_ENTER);assert(!task.removals);
    channels.handleInput(KEY_ENTER);channels.handleInput(KEY_ENTER);channels.handleInput(KEY_RIGHT);channels.handleInput(KEY_ENTER);assert(task.removals==1);
  }
  {
    UITask task; Display display; task._display=&display; FakeInboxStorage storage;
    InboxScreen inbox(&task,&storage);
    UIMessageMetadata dm(UIMessageMetadata::Contact,1700000000,255,nullptr);
    UIMessageMetadata channel(UIMessageMetadata::Channel,1700000000,0,nullptr);
    for(int i=0;i<64;i++) inbox.add("Friend","saved DM",dm);
    inbox.flush(); int writes=storage.writes;
    for(int i=0;i<80;i++) inbox.add("Public","temporary channel",channel);
    inbox.flush(); assert(storage.writes==writes && inbox.unreadCount()==72);
    RecentMessages<185,72> saved;InboxPersistence<185,72> disk(&storage);
    assert(disk.load(saved) && saved.count()==64);
    for(size_t i=0;i<saved.count();i++) assert(saved.at(i)->kind==0);
    // At capacity Keep cannot silently evict a DM.
    inbox.open();inbox.handleInput(KEY_RIGHT);for(int i=0;i<5;i++){inbox.handleInput(KEY_DOWN);}inbox.handleInput(KEY_ENTER);
    assert(task.alert=="Saved full: delete");
    // Auto-view progresses through all pages before discarding the temporary item.
    task.auto_channels=false;assert(!inbox.startAutoView());task.auto_channels=true;
    assert(inbox.startAutoView());unsigned unread=inbox.unreadCount();
    for(int i=0;i<20 && task.home_calls==0;i++){render(inbox,display);now_millis+=3001;inbox.poll();}
    assert(task.home_calls==1 && inbox.unreadCount()==int(unread)-1);
    inbox.flush();assert(storage.writes==writes);
    assert(inbox.clearStorage());
    // A kept channel persists; reading an unkept one removes it on leaving.
    inbox.add("Field","important",channel);inbox.open();inbox.handleInput(KEY_RIGHT);
    for(int i=0;i<5;i++){inbox.handleInput(KEY_DOWN);}inbox.handleInput(KEY_ENTER);
    assert(task.alert=="Message kept");
    inbox.add("Public","discard after read",channel);inbox.open();inbox.handleInput(KEY_ENTER);inbox.handleInput(KEY_LEFT);
    inbox.flush();InboxScreen restarted(&task,&storage);restarted.open();
    assert(restarted.unreadCount()==1);restarted.handleInput(KEY_ENTER);
    assert(scrollAll(restarted,display).find("important")!=std::string::npos);
  }
  // Exercise the actual delete/clear UI and verify the results across reboot.
  {
    UITask task; Display display; FakeInboxStorage favorites,drafts,storage;
    UIComposeState compose(&favorites,&drafts);task.compose=&compose;
    InboxScreen inbox(&task,&storage); uint8_t original[32]={4},replacement[32]={5};
    UIMessageMetadata from(UIMessageMetadata::Contact,1700000000,255,nullptr,original);
    UIMessageMetadata later(UIMessageMetadata::Contact,1700000001,255,nullptr,replacement);
    inbox.add("Original","reply to me",from);inbox.open();inbox.handleInput(KEY_RIGHT);
    for(int i=0;i<3;i++){inbox.handleInput(KEY_DOWN);}inbox.handleInput(KEY_ENTER);
    for(int i=0;i<9;i++){inbox.handleInput(KEY_DOWN);}inbox.handleInput(KEY_ENTER);
    inbox.handleInput(KEY_DOWN);
    for(int i=0;i<70;i++)inbox.add("Other","new",later); // original inbox entry is gone
    assert(render(inbox,display).find("Custom text")!=std::string::npos);
    inbox.handleInput(KEY_ENTER);inbox.handleInput(KEY_DOWN);inbox.handleInput(KEY_ENTER);
    inbox.handleInput(KEY_RIGHT);inbox.handleInput(KEY_ENTER);
    assert(task.sent==1 && !memcmp(task.last_target,original,32) && task.sent_text=="a");
  }

  {
    UITask task; Display display; FakeInboxStorage favorites, drafts, storage;
    UIComposeState compose(&favorites, &drafts); task.compose=&compose;
    ActionsMenu actions(&task);
    actions.render(display); actions.handleInput(KEY_ENTER); assert(!task.bluetooth);
    actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER); actions.handleInput(KEY_ENTER);
    assert(task.advert_calls==0);
    actions.handleInput(KEY_ENTER); actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER);
    assert(task.advert_calls==1);
    actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER); actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER);
    task.button_pressed=true; actions.poll(); assert(task.shutdown_calls==0);
    task.button_pressed=false; actions.poll(); actions.poll(); assert(task.shutdown_calls==1);
    InboxScreen inbox(&task); task.msg_preview=&inbox;
    HomeNav home(&task);
    display.filled_rects=0; home.render(display); assert(display.filled_rects==HomeNav::Count);
    home._page=HomeNav::GPS; display.filled_rects=0; home.render(display); assert(display.filled_rects==0);
    home._page=HomeNav::FIRST;
    home.handleInput(KEY_DOWN); assert(task.unread_calls==1);
    home.handleInput(KEY_ENTER); assert(task.curr==&inbox);
    for(unsigned i=0;i<HomeNav::QUICK_SEND;i++) home.handleInput(KEY_RIGHT);
    home.handleInput(KEY_ENTER); assert(task.quick_calls==1 && !task.position_only);
    home.handleInput(KEY_RIGHT); home.handleInput(KEY_ENTER); assert(task.outbox_calls==1);
    home.handleInput(KEY_RIGHT); home.handleInput(KEY_DOWN); assert(task.position_only);
    home.handleInput(KEY_ENTER); assert(task.gps_toggles==1);
    home.handleInput(KEY_CANCEL); assert(home._page==HomeNav::FIRST);
    home.handleInput(KEY_LEFT); assert(home._page==HomeNav::Count-1);
    assert(home._page==HomeNav::ACTIONS);
    actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER); actions.handleInput(KEY_ENTER);
    assert(task.clean_calls==0); // default No
    actions.handleInput(KEY_ENTER); actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER);
    assert(task.clean_calls==1);
    actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER); assert(task.channel_calls==1);
    actions.handleInput(KEY_DOWN); actions.handleInput(KEY_ENTER); assert(task.quiet);
    actions.handleInput(KEY_ENTER); assert(!task.quiet);
    QuickSendScreen send(&task); task.quick_send=&send; send.open(false);
    display.clear(); send.render(display); assert(display.text().find("Contacts")!=std::string::npos);
    send.handleInput(KEY_ENTER);
    for(int i=0;i<4;i++) send.handleInput(KEY_DOWN);
    display.clear(); send.render(display); assert(display.text().find("Contact 4")!=std::string::npos);
    send.handleInput(KEY_RIGHT); UISendTarget target; task.getSendTarget(false,4,target); assert(compose.favorite(target));
    send.handleInput(KEY_ENTER);
    for(int i=0;i<9;i++) send.handleInput(KEY_DOWN);
    send.handleInput(KEY_ENTER); assert(send.editingText());
    send.handleInput(KEY_DOWN);send.handleInput(KEY_RIGHT);send.handleInput(KEY_DOWN);send.handleInput(KEY_DOWN);
    assert(task.hasDraft()); compose.flush(millis());
    UIComposeState reboot(&favorites,&drafts); task.compose=&reboot;
    send.open(false);send.handleInput(KEY_DOWN);send.handleInput(KEY_DOWN);send.handleInput(KEY_ENTER);
    display.clear();send.render(display);assert(display.text().find("ab")!=std::string::npos);
    send.handleInput(KEY_ENTER);send.handleInput(KEY_DOWN);send.handleInput(KEY_ENTER);
    display.clear();send.render(display);assert(display.text().find("ab")!=std::string::npos && task.sent==0);
    send.handleInput(KEY_DOWN); // vertical navigation must not authorize a send
    display.clear(); send.render(display); assert(display.text().find(">No")!=std::string::npos);
    send.handleInput(KEY_RIGHT);send.handleInput(KEY_LEFT);
    display.clear(); send.render(display); assert(display.text().find(">No")!=std::string::npos);
    send.handleInput(KEY_RIGHT);send.handleInput(KEY_ENTER);assert(task.sent==1 && task.sent_text=="ab" && !task.hasDraft());
    task.getSendTarget(false,0,target); assert(send.retry(target,"hello again"));
    send.handleInput(KEY_ENTER);assert(task.sent==1); // default No on retry
    assert(send.retry(target,"hello again"));send.handleInput(KEY_RIGHT);send.handleInput(KEY_ENTER);
    assert(task.sent==2 && task.sent_text=="hello again");
    std::string long_text(76,'x');long_text += "end";
    assert(send.retry(target,long_text.c_str()));display.clear();send.render(display);
    assert(display.text().find("end")!=std::string::npos);send.handleInput(KEY_CANCEL);
    JoystickTextEditor visual_editor;visual_editor.restore(long_text.c_str(),78);
    display.clear();visual_editor.render(display);assert(display.text().find("end")!=std::string::npos);
    task.gps_valid=false;assert(!send.retry(target,"GPS position: 1.000000, 2.000000"));assert(task.sent==2);
    Outbox outbox(&storage,0);outbox.add(0,target.key,target.name,"review me",true,1,1000,1700000000,0);
    OutboxScreen review(&task,&outbox);review.open();review.handleInput(KEY_ENTER);
    display.clear();review.render(display);assert(display.text().find("Waiting for ACK")!=std::string::npos);
    review.handleInput(KEY_RIGHT);assert(task.retry_calls==1);
    UIMessageMetadata incoming(UIMessageMetadata::Contact,1700000000,255,nullptr,target.key);
    inbox.add("One","first",incoming);inbox.add("Two","second",incoming);inbox.open();
    assert(inbox.nextUnread() && inbox.unreadCount()==1);assert(inbox.nextUnread() && inbox.unreadCount()==0);assert(!inbox.nextUnread());
  }

  {
    UITask task; Display display; FakeInboxStorage storage;
    InboxScreen inbox(&task, &storage);
    UIMessageMetadata metadata = {UIMessageMetadata::Contact, 1700000000, 0xFF, nullptr};
    inbox.add("One", "one", metadata); inbox.add("Two", "two", metadata);
    inbox.flush(); inbox.open();
    inbox.handleInput(KEY_RIGHT); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    assert(render(inbox, display).find("Delete this message?") != std::string::npos);
    inbox.handleInput(KEY_ENTER); // default No preserves both
    InboxScreen unchanged(&task, &storage); assert(unchanged.unreadCount()==2);
    inbox.handleInput(KEY_RIGHT); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    InboxScreen deleted(&task, &storage); assert(deleted.unreadCount()==1);
    assert(render(deleted, display).find("One") != std::string::npos);
    inbox.handleInput(KEY_RIGHT); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_DOWN);
    inbox.handleInput(KEY_ENTER); assert(render(inbox, display).find("Clear entire inbox?") != std::string::npos);
    inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    InboxScreen cleared(&task, &storage); assert(cleared.unreadCount()==0);
    assert(render(cleared, display).find("No recent messages") != std::string::npos);
    inbox.add("Three", "three", metadata); inbox.flush(); inbox.open();
    inbox.handleInput(KEY_RIGHT); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    inbox.handleInput(KEY_DOWN); inbox.add("Four", "four", metadata);
    assert(render(inbox, display).find("Delete this message?") == std::string::npos);
    inbox.handleInput(KEY_ENTER); // new arrival canceled confirmation; opens selected message
    inbox.flush(); InboxScreen arrived(&task, &storage); assert(arrived.unreadCount()==1);
  }
  {
    UITask task; Display display; FakeInboxStorage storage;
    InboxScreen inbox(&task, &storage);
    uint8_t key[32] = {7};
    UIMessageMetadata metadata(UIMessageMetadata::Contact, 1700000000, 0xFF, nullptr, key);
    inbox.add("Test", "hello", metadata); inbox.flush(); inbox.open();
    auto menu = [&]() {
      inbox.handleInput(KEY_RIGHT);
      for (int i=0;i<3;i++) inbox.handleInput(KEY_DOWN);
      inbox.handleInput(KEY_ENTER);
      assert(render(inbox,display).find("Quick reply")!=std::string::npos);
    };
    menu(); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_DOWN); inbox.handleInput(KEY_ENTER);
    assert(render(inbox,display).find("STATS")!=std::string::npos);
    inbox.handleInput(KEY_ENTER); assert(task.sent==0); // default No
    inbox.handleInput(KEY_ENTER); inbox.handleInput(KEY_RIGHT); inbox.handleInput(KEY_ENTER);
    assert(task.sent==1 && task.sent_text=="STATS" && task.alert=="Reply queued");
    inbox.handleInput(KEY_ENTER); assert(task.sent==1); // repeated Enter doesn't send again
    menu(); inbox.handleInput(KEY_ENTER); inbox.handleInput(KEY_RIGHT);
    inbox.add("Other", "new arrival", metadata); inbox.handleInput(KEY_ENTER);
    assert(task.sent==1); // arrival cancels send confirmation
    inbox.handleInput(KEY_CANCEL); inbox.handleInput(KEY_CANCEL); inbox.handleInput(KEY_CANCEL);
    menu(); inbox.handleInput(KEY_ENTER); inbox.handleInput(KEY_RIGHT);
    task.target_available=false; inbox.handleInput(KEY_ENTER); assert(task.sent==1);
    task.target_available=true;
    menu(); for(int i=0;i<8;i++) inbox.handleInput(KEY_DOWN);
    inbox.handleInput(KEY_ENTER); assert(render(inbox,display).find("GPS position")!=std::string::npos);
    inbox.handleInput(KEY_RIGHT); task.gps_valid=false; inbox.handleInput(KEY_ENTER);
    assert(task.sent==1 && task.alert=="Reply not sent");
    menu(); inbox.handleInput(KEY_ENTER); inbox.handleInput(KEY_RIGHT); task.send_ok=false;
    inbox.handleInput(KEY_ENTER); assert(task.sent==2 && task.alert=="Reply not sent");
    InboxScreen reboot(&task,&storage); reboot.open(); reboot.handleInput(KEY_RIGHT);
    for(int i=0;i<3;i++) reboot.handleInput(KEY_DOWN);
    reboot.handleInput(KEY_ENTER); assert(render(reboot,display).find("Quick reply")!=std::string::npos);
  }
  UITask task;
  Display display;
  InboxScreen screen(&task);
  assert(render(screen, display).find("No recent messages") != std::string::npos);
  screen.handleInput(static_cast<char>(KEY_DOWN));
  screen.handleInput(KEY_ENTER);
  assert(screen.unreadCount() == 0);
  screen.handleInput(KEY_CANCEL);
  assert(task.home_calls == 1);
  UIMessageMetadata dm = { UIMessageMetadata::Contact, 0, 0xFF, nullptr };
  std::string body(140, 'X'); body += " END_OF_BODY";
  screen.add("Alpha", body.c_str(), dm);
  screen.open();
  assert(render(screen, display).find(">*D Alpha") != std::string::npos);
  screen.handleInput(KEY_ENTER);
  assert(screen.unreadCount() == 0);
  assert(render(screen, display).find("DM from: Alpha") != std::string::npos);
  UIMessageMetadata channel = { UIMessageMetadata::Channel, 1700000000, 3, nullptr };
  screen.add("Field", "Bob: hello", channel);
  auto stable = render(screen, display);
  assert(stable.find("Message 2/2") != std::string::npos);
  assert(stable.find("DM from: Alpha") != std::string::npos);
  assert(screen.unreadCount() == 1);
  auto all = scrollAll(screen, display);
  assert(all.find("END_OF_BODY") != std::string::npos);
  screen.handleInput(KEY_CANCEL);
  assert(render(screen, display).find("> D Alpha") != std::string::npos);
  screen.handleInput(static_cast<char>(KEY_UP));
  assert(render(screen, display).find(">*# Field") != std::string::npos);
  screen.handleInput(KEY_ENTER);
  all = scrollAll(screen, display);
  assert(all.find("Bob: hello") != std::string::npos && all.find("Channel: Field") != std::string::npos);
  assert(screen.unreadCount() == 0);
  screen.handleInput(KEY_CANCEL);
  screen.handleInput(KEY_CANCEL);
  screen.open();
  assert(render(screen, display).find("Messages 0 unread") != std::string::npos);
  uint8_t prefix[] = {0xAB, 0xCD, 0x12, 0x34};
  UIMessageMetadata room = { UIMessageMetadata::Signed, 0xFFFFFFFF, 0xFF, prefix };
  screen.add("Relay", "signed body", room);
  screen.open(); screen.handleInput(KEY_ENTER);
  all = scrollAll(screen, display);
  assert(all.find("Sender: ABCD1234") != std::string::npos);
  assert(all.find("snt: unknown") != std::string::npos);
  // Evict a viewed record: switch to the list, do not silently mark replacement read.
  for (int i = 0; i < 64; i++) screen.add("New", "new text", dm);
  assert(render(screen, display).find("FULL 64/64: delete") != std::string::npos);
  assert(screen.unreadCount() == 64);
  // A maximum-size newline-heavy payload must not overflow the footer either.
  InboxScreen multiline(&task);
  std::string many_lines(180, '\n'); many_lines += "END";
  multiline.add("Name\nwith control", many_lines.c_str(), dm);
  multiline.open(); multiline.handleInput(KEY_ENTER);
  assert(scrollAll(multiline, display).find("snt: unknown") != std::string::npos);
  for (int i = 0; i < 200; i++) multiline.handleInput(static_cast<char>(KEY_DOWN));
  assert(render(multiline, display).find("END") != std::string::npos);

  // Compile and exercise the actual task receive/sync/wake methods as well.
  UITask receiving;
  InboxScreen inbox(&receiving);
  receiving._display = &display;
  receiving.msg_preview = &inbox;
  receiving.newMsgWithMetadata(0xFF, "Alpha", "stored body", 7, dm);
  receiving.gotoInbox();
  inbox.handleInput(KEY_ENTER);
  auto before_sync = render(inbox, display);
  receiving.msgRead(0);
  assert(receiving.curr == &inbox && receiving._msgcount == 0);
  assert(render(inbox, display) == before_sync && receiving.getUnreadCount() == 0);
  display.turnOff();
  char wake = receiving.checkDisplayOn(static_cast<char>(KEY_DOWN));
  assert(wake == 0 && display.isOn());
  assert(render(inbox, display) == before_sync);
  display.turnOff(); receiving.connected = true;
  receiving.newMsgWithMetadata(0xFF, "Field", "new body", 1, channel);
  assert(!display.isOn() && receiving.curr == &inbox && receiving.getUnreadCount() == 1);
  receiving.connected = false;
  receiving.newMsgWithMetadata(0xFF, "Field", "another body", 2, channel);
  assert(display.isOn() && receiving.curr == &inbox && receiving.getUnreadCount() == 2);
  assert(receiving._auto_off == now_millis + AUTO_OFF_MILLIS);
  assert(render(inbox, display).find("DM from: Alpha") != std::string::npos);
  printf("InboxScreen tests passed (actual renderer/input, simulated OLED/RTC)\n");
}
