#include "UITask.h"
#if defined(UI_RECENT_INBOX)
  #include "InboxPersistence.h"
  #include "QuickReplies.h"
  #include "WioInboxStorage.h"
  extern DataStore store;
#endif
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#if defined(UI_RECENT_INBOX)
  #include "CompanionPages.h"
  #include "GPSPanel.h"
#endif
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(UIColor::corp_blue);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

#if defined(UI_RECENT_INBOX)
    display.setColor(UIColor::primary_txt);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 22, "KALLISTI");

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 42, _version_info);

    const char* build = strchr(FIRMWARE_VERSION, '-');
    if (build && build[1]) {
      char label[16];
      snprintf(label, sizeof(label), "build %.8s", build + 1);
      display.setColor(UIColor::secondary_txt);
      display.drawTextCentered(display.width()/2, 55, label);
    }
#else
    const char* website = "https://meshcore.io";
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 22, website);
    display.drawTextCentered(display.width()/2, 35, _version_info);
    display.setColor(UIColor::secondary_txt);
    display.drawTextCentered(display.width()/2, 48, FIRMWARE_BUILD_DATE);
#endif

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    RADIO,
#if defined(UI_RECENT_INBOX)
    QUICK_SEND,
    OUTBOX,
#else
    BLUETOOTH,
    ADVERT,
#endif
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
#if defined(UI_RECENT_INBOX)
    ACTIONS,
#else
    SHUTDOWN,
#endif
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
#if defined(UI_RECENT_INBOX)
  ActionsMenu _actions;
#endif
  AdvertPath recent[UI_RECENT_LIST_SIZE];


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(UIColor::title_txt);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);

    // show muted icon if buzzer is muted
#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(UIColor::warning_txt);
      display.drawXbm(iconX - 9, iconY + 1, muted_icon, 8, 8);
    }
#endif
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
#if defined(UI_RECENT_INBOX)
  bool canAutoRead() const { return _page != HomePage::ACTIONS; }
  void leave() { _actions.reset(); }
#endif
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _shutdown_init(false),
#if defined(UI_RECENT_INBOX)
       _actions(task),
#endif
       sensors_lpp(200) {  }

  void poll() override {
#if defined(UI_RECENT_INBOX)
    _actions.poll();
#endif
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
#if defined(UI_RECENT_INBOX)
    if (_page == HomePage::ACTIONS) return _actions.render(display);
#endif
    display.setColor(UIColor::title_bkg);
    display.fillRect(0, 0, display.width(), 12);
    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(UIColor::title_txt);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 2);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    if (UIColor::title_bkg == UIColor::window_bkg) {
      display.setColor(UIColor::title_txt);
    } else {
      display.setColor(UIColor::title_bkg);
    }
#if defined(UI_RECENT_INBOX) && ENV_INCLUDE_GPS == 1
    if (_page != HomePage::GPS) {
#endif
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 4, 4);
      } else {
        display.fillRect(x, y, 2, 2);
      }
    }

#if defined(UI_RECENT_INBOX) && ENV_INCLUDE_GPS == 1
    }
#endif
    if (_page == HomePage::FIRST) {
      display.setColor(UIColor::primary_txt);
#if defined(UI_RECENT_INBOX)
      display.setTextSize(1);
      snprintf(tmp, sizeof(tmp), "> Messages  %d unread", _task->getUnreadCount());
      display.drawTextCentered(display.width() / 2, 22, tmp);
      display.drawTextCentered(display.width() / 2, 33, "OK:inbox Down:unread");
#else
      display.setTextSize(2);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 22, tmp);
#endif

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 54, tmp);
      #endif
      if (_task->hasConnection()) {
        display.setColor(UIColor::warning_txt);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Connected >");

      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(UIColor::warning_txt);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(UIColor::primary_txt);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
#if !defined(UI_RECENT_INBOX)
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(UIColor::corp_blue);
      display.drawXbm((display.width() - 32) / 2, 18,
          _task->isBluetoothEnabled() ? bluetooth_on : bluetooth_off,
          32, 32);
      display.setColor(UIColor::secondary_txt);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, "toggle: " PRESS_LABEL);
    } else if (_page == HomePage::ADVERT) {
      display.setColor(UIColor::corp_blue);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.setColor(UIColor::secondary_txt);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
#else
    } else if (_page == HomePage::QUICK_SEND) {
      display.setColor(UIColor::primary_txt);
      display.drawTextCentered(display.width() / 2, 25, "QUICK SEND");
      display.drawTextCentered(display.width() / 2, 39, "Presets / position");
      display.drawTextCentered(display.width() / 2, 54, "Enter: choose target");
    } else if (_page == HomePage::OUTBOX) {
      display.setColor(UIColor::primary_txt);
      display.drawTextCentered(display.width() / 2, 25, "OUTBOX");
      display.drawTextCentered(display.width() / 2, 39, "Sent / ACK status");
      display.drawTextCentered(display.width() / 2, 54, "Enter: review");
#endif
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
#if defined(UI_RECENT_INBOX)
      LocationProvider* gps = sensors.getLocationProvider();
      bool enabled = gps && _task->getGPSState() && gps->isEnabled();
      GPSPanelData data = {gps != nullptr, enabled, enabled && gps->hasFix(),
        gps ? gps->satellitesCount() : 0, gps ? gps->getLatitude() : 0,
        gps ? gps->getLongitude() : 0, gps ? gps->getAltitude() : 0,
        gps ? gps->getFixAgeMillis() : UINT32_MAX, gps && gps->hasAltitude()};
      renderGPSPanel(display, data);
#else
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.setColor(UIColor::primary_txt);
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        display.setColor(UIColor::primary_txt);
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "sat");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, "pos");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.setColor(UIColor::secondary_txt);
#if defined(UI_RECENT_INBOX)
        display.setCursor(0, 56); display.print("OK:GPS Down:share");
#else
        display.drawTextLeftAlign(0, y, "alt");
        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
#endif
        y = y + 12;
      }
#endif
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.setColor(UIColor::secondary_txt);
        display.print(name);
        display.setColor(UIColor::primary_txt);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
#if !defined(UI_RECENT_INBOX)
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(UIColor::corp_blue);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.setColor(UIColor::warning_txt);
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.setColor(UIColor::secondary_txt);
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
#endif
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    uint8_t key = static_cast<uint8_t>(c);
#if defined(UI_RECENT_INBOX)
    if (_page == HomePage::ACTIONS) {
      if (_actions.handleInput(c)) return true;
      _actions.reset();
    }
    if (key == KEY_ENTER && _page == HomePage::QUICK_SEND) { _task->gotoQuickSend(); return true; }
    if (key == KEY_ENTER && _page == HomePage::OUTBOX) { _task->gotoOutbox(); return true; }
#if ENV_INCLUDE_GPS == 1
    if (static_cast<uint8_t>(c) == KEY_DOWN && _page == HomePage::GPS) { _task->gotoQuickSend(true); return true; }
#endif
    if (static_cast<uint8_t>(c) == KEY_CANCEL) {
      _page = HomePage::FIRST;
      return true;
    }
    if (static_cast<uint8_t>(c) == KEY_DOWN && _page == HomePage::FIRST) { _task->gotoNextUnread(); return true; }
    if (key == KEY_ENTER && _page == HomePage::FIRST) {
      _task->gotoInbox();
      return true;
    }
#endif
    if (key == KEY_LEFT || key == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (key == KEY_NEXT || key == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
#if !defined(UI_RECENT_INBOX)
    if (key == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isBluetoothEnabled()) {  // toggle Bluetooth on/off
        _task->disableBluetooth();
      } else {
        _task->enableBluetooth();
      }
      return true;
    }
    if (key == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#endif
#if ENV_INCLUDE_GPS == 1
    if (key == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (key == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
#if !defined(UI_RECENT_INBOX)
    if (key == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
#endif
    return false;
  }
};

#if defined(UI_RECENT_INBOX)
class InboxScreen : public UIScreen {
  static constexpr size_t SavedCapacity = 64, TemporaryCapacity = 8;
  UITask* _task;
  RecentMessages<MAX_PACKET_PAYLOAD + 1, SavedCapacity + TemporaryCapacity> _history;
  InboxPersistence<MAX_PACKET_PAYLOAD + 1, SavedCapacity + TemporaryCapacity> _persistence;
  bool _detail = false, _auto_view = false;
  uint32_t _auto_page_at = 0;
  bool _options = false, _confirm = false, _yes = false;
  unsigned _option = 0;
  bool _reply = false, _reply_confirm = false, _reply_yes = false;
  unsigned _reply_choice = 0;
  JoystickTextEditor _editor;
  bool _editing = false;
  UISendTarget _reply_to;
  uint32_t _reply_fix_time = 0;
  char _reply_text[80] = {}, _reply_target[40] = {};

  bool replyTarget() {
    if (_reply) return _task->inboxReplyTarget(_reply_to.kind, _reply_to.channel_idx, _reply_to.key, _reply_target, sizeof(_reply_target));
    const auto* e = _history.at(_history.selected());
    return e && e->has_reply_target && _task->inboxReplyTarget(e->kind, e->channel_idx, e->reply_target, _reply_target, sizeof(_reply_target));
  }
  unsigned _scroll = 0;
  unsigned _line_count = 0;

  static void formatTime(char* dest, size_t size, uint32_t timestamp) {
    // DateTime supports 2000..2099. Sender clocks can be unset or incorrect.
    if (timestamp < 946684800UL || timestamp >= 4102444800UL) {
      snprintf(dest, size, "unknown");
    } else {
      DateTime dt(timestamp);
      snprintf(dest, size, "%02u-%02u-%02u %02u:%02u:%02u", unsigned(dt.year() % 100),
               unsigned(dt.month()), unsigned(dt.day()), unsigned(dt.hour()),
               unsigned(dt.minute()), unsigned(dt.second()));
    }
  }

public:
  InboxScreen(UITask* task, InboxStorage* storage = nullptr) : _task(task), _persistence(storage) {
    if (storage) _persistence.load(_history);
  }
  void tick() { _persistence.tick(_history, millis()); }
  bool startAutoView() {
    if (!_task->autoReadChannels()) return false;
    for (size_t i = _history.count(); i > 0; --i) {
      const auto* e = _history.at(i - 1);
      if (e->temporary && !e->read) {
        open(); _history.select(i - 1); _detail = _auto_view = true;
        _auto_page_at = millis(); _line_count = 0; return true;
      }
    }
    return false;
  }
  void poll() override {
    if (!_auto_view || !_task->displayAwake()) return;
    if (uint32_t(millis() - _auto_page_at) < 3000 || !_line_count) return;
    _auto_page_at = millis();
    if (_scroll + 5 < _line_count) { _scroll += 4; return; }
    _history.erase(_history.selected()); _auto_view = _detail = false;
    _task->gotoHomeScreen();
  }
  void removeReadTemporary() {
    const auto* e = _history.at(_history.selected());
    if (_detail && e && e->temporary && e->read) _history.erase(_history.selected());
  }
  void markSelectedRead() {
    const auto* e = _history.at(_history.selected());
    if (e && !e->read) {
      bool saved = !e->temporary; _history.markRead(_history.selected());
      if (saved) _persistence.changed(millis());
    }
  }
  void flush() {
    if (!_persistence.flush(_history, millis())) _task->showAlert("Inbox save failed", 2000);
  }
  bool nextUnread() {
    size_t previous = _history.selected();
    bool was_detail = _detail;
    bool removes = _detail && _history.at(previous) && _history.at(previous)->temporary && _history.at(previous)->read;
    removeReadTemporary();
    if (removes) _detail = false;
    size_t count = _history.count();
    size_t first = was_detail ? previous + (removes ? 0 : 1) : 0;
    for (size_t i = 0; i < count; i++) {
      size_t index = (first + i) % count;
      if (!_history.at(index)->read) {
        _history.select(index); markSelectedRead();
        _detail = true; _scroll = 0; _options = false; return true;
      }
    }
    return false;
  }
  bool editingText() const { return _reply && _editing && _editor.editing(); }
  int unreadCount() const { return _history.unreadCount(); }
  void open() {
    _auto_view = false;
    _history.select(0);
    _options = _confirm = _reply = _reply_confirm = _editing = false;
    _detail = false;
    _scroll = 0;
  }
  bool clearStorage() {
    open(); return _persistence.clear(_history, millis());
  }
  bool full() const { return _history.savedCount() >= SavedCapacity; }
  void add(const char* origin, const char* text, const UIMessageMetadata& metadata) {
    // A new arrival cancels destructive menus so their target cannot change.
    _options = _confirm = false;
    if (_reply_confirm && _reply_choice == QuickReplies::CustomText) _editing = true;
    _reply_confirm = false; // keep draft and frozen target; require confirmation again
    bool temporary = metadata.kind == UIMessageMetadata::Channel;
    size_t group_count = temporary ? _history.count() - _history.savedCount() : _history.savedCount();
    if (group_count >= (temporary ? TemporaryCapacity : SavedCapacity)) {
      size_t victim = _history.count();
      for (size_t i = _history.count(); i > 0; --i) {
        const auto* e = _history.at(i - 1);
        if (e->temporary != temporary) continue;
        if (victim == _history.count()) victim = i - 1;
        if (e->read) { victim = i - 1; break; }
      }
      if (victim == _history.selected()) { _detail = _auto_view = false; _scroll = 0; }
      if (victim < _history.count()) _history.erase(victim);
    }
    if (!temporary) _persistence.changed(millis());
    if (_history.add(metadata.kind, metadata.channel_idx, metadata.timestamp,
                     rtc_clock.getCurrentTime(), metadata.sender_prefix, origin, text)) {
      // The selected oldest record expired. Never silently substitute its body.
      _detail = false;
      _scroll = 0;
    }
    _history.setReplyTarget(0, metadata.reply_target);
    _history.setTemporary(0, temporary);
  }

  int render(DisplayDriver& display) override {
    if (_reply && _editing) return _editor.render(display, _task->draftStatus());
    display.setTextSize(1);
    display.setColor(UIColor::primary_txt);
    char title[32];
    if (_reply) snprintf(title, sizeof(title), "%s", _reply_confirm ? "Send reply?" : "Quick reply");
    else if (_detail) snprintf(title, sizeof(title), "Message %u/%u", unsigned(_history.selected() + 1), unsigned(_history.count()));
    else if (full()) snprintf(title, sizeof(title), "FULL 64/64: delete");
    else snprintf(title, sizeof(title), "Messages %u unread", unsigned(_history.unreadCount()));
    display.setCursor(0, 0);
    display.print(title);
    display.drawRect(0, 9, display.width(), 1);

    if (_reply) {
      if (_reply_confirm) {
        char target[40]; display.translateUTF8ToBlocks(target, _reply_target, sizeof(target));
        display.drawTextEllipsized(0, 12, display.width(), target);
        size_t offset = 0; char line[22];
        for (unsigned row = 0; row < 4 && _reply_text[offset]; row++) {
          size_t n = strlen(_reply_text + offset); if (n > 21) n = 21;
          memcpy(line, _reply_text + offset, n); line[n] = 0; offset += n;
          display.setCursor(0, 21 + row * 8); display.print(line);
        }
      } else {
        size_t first = (_reply_choice / 4) * 4;
        for (size_t row = 0; row < 4 && first + row < QuickReplies::Count; row++) {
          char label[24]; snprintf(label, sizeof(label), "%c %s", first + row == _reply_choice ? '>' : ' ', QuickReplies::label(first + row));
          display.setCursor(0, 12 + row * 11); display.print(label);
        }
      }
      display.setCursor(0, 56); display.print(_reply_confirm ? (_reply_yes ? " No >Yes L/R OK" : ">No  Yes L/R OK") : "Up/Dn OK Left:back");
      return 250;
    }
    if (_options) {
      display.setCursor(0, 13);
      if (_confirm) {
        display.print(_option == 1 ? "Delete this message?" : "Clear entire inbox?");
        display.setCursor(0, 26); display.print(_yes ? "  No" : "> No");
        display.setCursor(0, 39); display.print(_yes ? "> Yes" : "  Yes");
      } else {
        const char* labels[] = {"Cancel", "Delete message", "Clear inbox", _history.at(_history.selected())->has_reply_target ? "Reply" : "Reply unavailable", "Next unread", _history.at(_history.selected())->temporary ? "Keep message" : "Already saved"};
        unsigned first = (_option / 4) * 4;
        for (unsigned row = 0; row < 4 && first + row < 6; row++) {
          char line[24]; snprintf(line, sizeof(line), "%c %s", first + row == _option ? '>' : ' ', labels[first + row]);
          display.setCursor(0, 12 + row * 11); display.print(line);
        }
      }
      display.setCursor(0, 56); display.print("Enter: OK Left: back");
      return 250;
    }
    if (!_history.count()) {
      display.setCursor(0, 22);
      display.print("No recent messages");
      display.setCursor(0, 56);
      display.print(_persistence.failed() ? "Inbox storage error" : _persistence.pending() ? "Saving..." : "Menu: back");
      return 1000;
    }

    if (!_detail) {
      size_t first = (_history.selected() / 4) * 4;
      for (size_t row = 0; row < 4 && first + row < _history.count(); row++) {
        size_t index = first + row;
        const auto* entry = _history.at(index);
        char origin[32], label[40];
        display.translateUTF8ToBlocks(origin, entry->origin, sizeof(origin));
        char kind = entry->kind == UIMessageMetadata::Channel ? '#' :
                    entry->kind == UIMessageMetadata::Signed ? 'S' : 'D';
        snprintf(label, sizeof(label), "%c%c%c %s", index == _history.selected() ? '>' : ' ',
                 entry->read ? ' ' : '*', kind, origin);
        display.drawTextEllipsized(0, 12 + row * 11, display.width(), label);
      }
      snprintf(title, sizeof(title), "%u/%u OK:read >:menu", unsigned(_history.selected() + 1), unsigned(_history.count()));
    } else {
      const auto* entry = _history.at(_history.selected());
      char sent[32], received[32], origin[32], body[MAX_PACKET_PAYLOAD + 1];
      formatTime(sent, sizeof(sent), entry->sent_at);
      formatTime(received, sizeof(received), entry->received_at);
      display.translateUTF8ToBlocks(origin, entry->origin, sizeof(origin));
      InboxText::toDisplay(body, entry->text, sizeof(body));
      // Scroll metadata with the body so long names/timestamps remain reachable.
      char text[400], sender[32] = "";
      const char* kind = "DM from";
      if (entry->kind == UIMessageMetadata::Channel) kind = "Channel";
      if (entry->kind == UIMessageMetadata::Signed) {
        kind = "Signed via";
        snprintf(sender, sizeof(sender), "Sender: %02X%02X%02X%02X\n",
                 unsigned(entry->sender_prefix[0]), unsigned(entry->sender_prefix[1]),
                 unsigned(entry->sender_prefix[2]), unsigned(entry->sender_prefix[3]));
      }
      snprintf(text, sizeof(text), "%s: %s\n%ssnt: %s\nrcv: %s\n%s\n%s",
               kind, origin, sender, sent, received, entry->temporary ? "Temporary >:Keep" : entry->truncated ? "[truncated]" : "", body);
      // SH1106's size-1 font is 6 pixels wide, 8 high; draw five bounded lines.
      char line[22];
      size_t offset = 0;
      _line_count = 0;
      while (text[offset]) {
        offset = InboxText::nextLine(text, offset, line, 21);
        if (_line_count >= _scroll && _line_count < _scroll + 5) {
          display.setCursor(0, 12 + (_line_count - _scroll) * 8);
          display.print(line);
        }
        _line_count++;
      }
      snprintf(title, sizeof(title), "%u/%u Up/Dn >:menu", _scroll + 1, _line_count);
    }
    display.setCursor(0, 56);
    display.print(_persistence.failed() ? "Inbox storage error" : _persistence.pending() ? "Saving..." : title);
    return 1000;
  }

  bool handleInput(char c) override {
    // Key constants exceed signed-char range on some platforms.
    uint8_t key = static_cast<uint8_t>(c);
    if (_auto_view) { _auto_view = false; markSelectedRead(); }
    if (_reply) {
      if (_editing) {
        auto result = _editor.input(key);
        if (result == JoystickTextEditor::Discard) { _task->discardDraft(_reply_to); _editing = false; }
        else _task->saveDraft(_reply_to, _editor.text(), _editor.cursor());
        if (result == JoystickTextEditor::Empty) _task->showAlert("Enter some text", 1500);
        else if (result == JoystickTextEditor::Preview) {
          if (replyTarget() && _editor.copyForSend(_reply_text, sizeof(_reply_text))) {
            _reply_fix_time = 0; _editing = false; _reply_confirm = true; _reply_yes = false;
          } else _task->showAlert("Target unavailable", 1500);
        }
        return true;
      }
      if (_reply_confirm && (key == KEY_LEFT || key == KEY_RIGHT)) {
        _reply_yes = key == KEY_RIGHT;
        return true;
      }
      if (_reply_confirm && (key == KEY_UP || key == KEY_DOWN || key == KEY_PREV || key == KEY_NEXT)) return true;
      if (key == KEY_CANCEL || key == KEY_LEFT) {
        if (_reply_confirm) { _reply_confirm = false; _editing = _reply_choice == QuickReplies::CustomText; }
        else { _reply = false; _options = true; }
      } else if (key == KEY_UP || key == KEY_PREV) {
        if (_reply_confirm) _reply_yes = false;
        else if (_reply_choice) --_reply_choice;
      } else if (key == KEY_DOWN || key == KEY_NEXT) {
        if (_reply_confirm) _reply_yes = true;
        else if (_reply_choice + 1 < QuickReplies::Count) ++_reply_choice;
      } else if (key == KEY_ENTER) {
        if (_reply_confirm) {
          if (_reply_yes) {
            bool ok = replyTarget() && _task->quickReplyStillValid(_reply_fix_time) &&
              _task->sendInboxReply(_reply_to.kind, _reply_to.channel_idx, _reply_to.key, _reply_text);
            if (ok && _reply_choice == QuickReplies::CustomText) _task->discardDraft(_reply_to);
            _task->showAlert(ok ? "Reply queued" : "Reply not sent", 2000);
            _reply = false;
          }
          _reply_confirm = false;
          if (_reply && _reply_choice == QuickReplies::CustomText) _editing = true;
        } else if (_reply_choice == QuickReplies::CustomText && replyTarget()) {
          UISendTarget draft; char text[80]; unsigned cursor;
          if (_task->restoreDraft(draft, text, sizeof(text), cursor) && draft.kind == _reply_to.kind && !memcmp(draft.key, _reply_to.key, 32)) _editor.restore(text, cursor);
          else _editor.open();
          _editing = true;
        } else if (replyTarget() && _task->prepareQuickReply(_reply_choice, _reply_text, sizeof(_reply_text), _reply_fix_time)) {
          _reply_confirm = true; _reply_yes = false;
        } else _task->showAlert(_reply_choice == QuickReplies::GPSPosition ? _task->gpsReplyError() :
                               _reply_choice == QuickReplies::SavedPosition ? "No saved position" : "Reply unavailable", 2000);
      }
      return true;
    }
    if (_options) {
      if (key == KEY_CANCEL || key == KEY_LEFT) {
        if (_confirm) _confirm = false;
        else _options = false;
      } else if (key == KEY_UP || key == KEY_PREV) {
        if (_confirm) _yes = false;
        else if (_option) --_option;
      } else if (key == KEY_DOWN || key == KEY_NEXT) {
        if (_confirm) _yes = true;
        else if (_option < 5) ++_option;
      } else if (key == KEY_ENTER) {
        if (_confirm) {
          if (_yes) {
            if (_option == 1) _history.erase(_history.selected());
            else if (!clearStorage()) _task->showAlert("Inbox cleanup failed", 2000);
            if (_option == 1) { _persistence.changed(millis()); flush(); }
            _detail = false; _scroll = 0;
          }
          _options = _confirm = false;
        } else if (_option == 0) _options = false;
        else if (_option == 5) {
          if (_history.at(_history.selected())->temporary) {
            if (full()) _task->showAlert("Saved full: delete", 2000);
            else { _history.setTemporary(_history.selected(), false); _persistence.changed(millis()); flush(); _task->showAlert(_persistence.failed() ? "Keep failed: retry" : "Message kept", 2000); }
          }
          _options = false;
        }
        else if (_option == 4) { _options = false; if (!nextUnread()) _task->showAlert("No unread messages", 1500); }
        else if (_option == 3) {
          if (replyTarget()) {
            const auto* e = _history.at(_history.selected());
            _reply_to.kind = e->kind; _reply_to.channel_idx = e->channel_idx;
            memcpy(_reply_to.key, e->reply_target, sizeof(_reply_to.key));
            memcpy(_reply_to.name, e->origin, sizeof(_reply_to.name));
            _editing = false;
            _options = false; _reply = true; _reply_confirm = _reply_yes = false; _reply_choice = 0;
          } else _task->showAlert("No reply destination", 2000);
        } else { _confirm = true; _yes = false; }
      }
      return true;
    }
    if (key == KEY_RIGHT && _history.count()) {
      _options = true; _option = 0; _yes = _confirm = false;
      return true;
    }
    if (key == KEY_CANCEL || key == KEY_LEFT) {
      if (_detail) { removeReadTemporary(); _detail = false; _scroll = 0; }
      else _task->gotoHomeScreen();
      return true;
    }
    if (!_history.count()) return false;
    if (key == KEY_UP || key == KEY_PREV) {
      if (_detail) { if (_scroll) _scroll--; }
      else if (_history.selected()) _history.select(_history.selected() - 1);
      return true;
    }
    if (key == KEY_DOWN || key == KEY_NEXT) {
      if (_detail) { if (_scroll + 5 < _line_count) _scroll++; }
      else _history.select(_history.selected() + 1);
      return true;
    }
    if (key == KEY_ENTER && _detail) { if (!nextUnread()) _task->showAlert("No unread messages", 1500); return true; }
    if (key == KEY_ENTER && !_detail) {
      _detail = true;
      _scroll = 0;
      markSelectedRead();
      return true;
    }
    return false;
  }
};
#else
class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[78];
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  int head = MAX_UNREAD_MSGS - 1; // index of latest unread message
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    head = (head + 1) % MAX_UNREAD_MSGS;
    if (num_unread < MAX_UNREAD_MSGS) num_unread++;

    auto p = &unread[head];
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(UIColor::corp_blue);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = &unread[head];

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(UIColor::secondary_txt);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    display.setCursor(0, 25);
    display.setColor(UIColor::primary_txt);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
      num_unread--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      }
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // clear unread queue
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

#endif // UI_RECENT_INBOX

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
#if defined(UI_RECENT_INBOX)
  auto* inbox_fs = store.getSecondaryFS();
  // QSPI was mounted in setup before UITask::begin; do not format on failure.
  msg_preview = new InboxScreen(this, inbox_fs ? new WioInboxStorage(*inbox_fs) : nullptr);
  _outbox = new Outbox(inbox_fs ? new WioInboxStorage(*inbox_fs, true) : nullptr, millis());
  _compose = new UIComposeState(inbox_fs ? new WioInboxStorage(*inbox_fs, 2) : nullptr,
                                inbox_fs ? new WioInboxStorage(*inbox_fs, 3) : nullptr,
                                inbox_fs ? new WioInboxStorage(*inbox_fs, 4) : nullptr);
  quick_send = new QuickSendScreen(this);
  channels_screen = new ChannelsScreen(this);
  outbox_screen = new OutboxScreen(this, _outbox);
#else
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
#endif
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
#if !defined(UI_RECENT_INBOX)
  if (msgcount == 0) {
    gotoHomeScreen();
  }
#endif
}

#if defined(UI_RECENT_INBOX)
bool UITask::inboxReplyTarget(uint8_t kind, uint8_t channel_idx, const uint8_t* target, char* name, size_t size) {
  return the_mesh.inboxReplyTarget(kind, channel_idx, target, name, size);
}
bool UITask::sendInboxReply(uint8_t kind, uint8_t channel_idx, const uint8_t* target, const char* text) {
  char name[40] = "Unavailable target";
  the_mesh.inboxReplyTarget(kind, channel_idx, target, name, sizeof(name));
  uint32_t ack = 0, timeout = 0;
  bool ok = the_mesh.sendInboxReply(kind, channel_idx, target, text, &ack, &timeout);
  if (_outbox) _outbox->add(kind, target, name, text, ok, ack, timeout, rtc_clock.getCurrentTime(), millis());
  return ok;
}
bool UITask::prepareQuickReply(unsigned choice, char* text, size_t size, uint32_t& fix_time) {
  auto* gps = _sensors ? _sensors->getLocationProvider() : nullptr;
  bool enabled = gps && getGPSState() && gps->isEnabled();
  bool valid = enabled && gps->hasFix();
  return QuickReplies::build(choice, _node_prefs->node_lat, _node_prefs->node_lon, enabled, valid,
                            valid ? gps->getLatitude() : 0, valid ? gps->getLongitude() : 0,
                            valid ? gps->getFixAgeMillis() : UINT32_MAX, millis(), text, size, fix_time);
}
const char* UITask::gpsReplyError() {
  auto* gps = _sensors ? _sensors->getLocationProvider() : nullptr;
  if (!gps) return "GPS unavailable";
  const char* problem = QuickReplies::gpsProblem(getGPSState() && gps->isEnabled(),
      gps->hasFix(), gps->getFixAgeMillis());
  return problem ? problem : "GPS: invalid position";
}
bool UITask::quickReplyStillValid(uint32_t fix_time) {
  if (!fix_time) return true;
  auto* gps = _sensors ? _sensors->getLocationProvider() : nullptr;
  return gps && getGPSState() && gps->isEnabled() && gps->isValid() &&
         QuickReplies::previewFresh(fix_time, millis());
}

bool UITask::editingText() const {
  if (curr == channels_screen) return static_cast<ChannelsScreen*>(channels_screen)->editingText();
  if (curr == quick_send) return static_cast<QuickSendScreen*>(quick_send)->editingText();
  if (curr == msg_preview) return static_cast<InboxScreen*>(msg_preview)->editingText();
  return false;
}
void UITask::gotoQuickSend(bool position_only) {
  static_cast<QuickSendScreen*>(quick_send)->open(position_only);
  setCurrScreen(quick_send);
}
void UITask::gotoChannels() { static_cast<ChannelsScreen*>(channels_screen)->open(); setCurrScreen(channels_screen); }
const char* UITask::joinChannel(const char* name) { return the_mesh.joinUIChannel(name); }
bool UITask::removeChannel(const UISendTarget& target) { return the_mesh.removeUIChannel(target); }
void UITask::gotoOutbox() {
  static_cast<OutboxScreen*>(outbox_screen)->open();
  setCurrScreen(outbox_screen);
}
bool UITask::getSendTarget(bool channels, unsigned index, UISendTarget& target) {
  // Stable page snapshots in the picker prevent reordering under the cursor.
  for (unsigned pass = 0; pass < 2; pass++) {
    UISendTarget candidate;
    for (unsigned i = 0; the_mesh.getUISendTarget(channels, i, candidate); i++) {
      bool favorite = _compose ? _compose->favorite(candidate) : candidate.favorite;
      if (favorite != (pass == 0)) continue;
      if (index) { --index; continue; }
      target = candidate; target.favorite = favorite; return true;
    }
  }
  return false;
}
bool UITask::toggleFavorite(const UISendTarget& target) {
  UISendTarget current = target;
  if (!resolveTarget(current)) { showAlert("Target unavailable", 1500); return false; }
  if (current.favorite) { showAlert("App favorite retained", 1500); return true; }
  bool ok = _compose && _compose->toggle(current, millis());
  showAlert(ok ? "Favorite updated" : "Favorite save failed", 1500); return ok;
}
bool UITask::autoReadChannels() const { return _compose && _compose->autoReadChannels(); }
uint32_t UITask::clockTime() const { return rtc_clock.getCurrentTime(); }
void UITask::toggleAutoReadChannels() {
  bool ok = _compose && _compose->toggleAutoRead(millis());
  showAlert(!ok ? "Setting save failed" : autoReadChannels() ? "Channel auto-read ON" : "Channel auto-read OFF", 2000);
}
void UITask::cleanStorage() {
  bool inbox = static_cast<InboxScreen*>(msg_preview)->clearStorage();
  bool outbox = _outbox && _outbox->clear(millis());
  bool draft = _compose && _compose->clearDraft(millis());
  static_cast<QuickSendScreen*>(quick_send)->open(false);
  showAlert(inbox && outbox && draft ? "Storage cleaned" : "Cleanup failed: retry", 2500);
}
bool UITask::hasDraft() const { return _compose && _compose->hasDraft(); }
bool UITask::resolveTarget(UISendTarget& target) {
  UISendTarget candidate;
  for (unsigned i = 0; the_mesh.getUISendTarget(target.kind == UIMessageMetadata::Channel, i, candidate); i++) {
    if (target.kind == candidate.kind && !memcmp(target.key, candidate.key, 32)) { target = candidate; return true; }
  }
  return false;
}
bool UITask::restoreDraft(UISendTarget& target, char* text, size_t size, unsigned& cursor) {
  return _compose && _compose->restore(target, text, size, cursor);
}
void UITask::saveDraft(const UISendTarget& target, const char* text, unsigned cursor) {
  if (_compose) _compose->save(target, text, cursor, millis());
}
void UITask::discardDraft(const UISendTarget& target) {
  if (_compose && !_compose->discard(target, millis())) showAlert("Draft save failed", 1500);
}
const char* UITask::draftStatus() const { return _compose ? _compose->draftStatus() : nullptr; }
void UITask::retryOutbox(const Outbox::History::Entry& entry) {
  if (entry.channel_idx == Outbox::Awaiting || entry.channel_idx == Outbox::Acknowledged) {
    showAlert(entry.channel_idx == Outbox::Awaiting ? "Still awaiting ACK" : "Already acknowledged", 1500); return;
  }
  UISendTarget target; target.kind = entry.kind; memcpy(target.key, entry.reply_target, 32);
  if (!entry.has_reply_target || !resolveTarget(target)) { showAlert("Target unavailable", 1500); return; }
  if (static_cast<QuickSendScreen*>(quick_send)->retry(target, entry.text)) setCurrScreen(quick_send);
}
void UITask::gotoNextUnread() {
  auto* inbox = static_cast<InboxScreen*>(msg_preview);
  inbox->open();
  if (inbox->nextUnread()) setCurrScreen(msg_preview);
  else showAlert("No unread messages", 1200);
}
bool UITask::sendAdvert() { return the_mesh.advert(); }
void UITask::onUIMessageAck(uint32_t ack, const uint8_t* key) {
  if (_outbox && _outbox->acknowledge(ack, key, millis())) _next_refresh = 0;
}

void UITask::gotoInbox() {
  static_cast<InboxScreen*>(msg_preview)->open();
  setCurrScreen(msg_preview);
}

int UITask::getUnreadCount() const {
  return static_cast<InboxScreen*>(msg_preview)->unreadCount();
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  UIMessageMetadata metadata = { UIMessageMetadata::Contact, 0, 0xFF, nullptr };
  newMsgWithMetadata(path_len, from_name, text, msgcount, metadata);
}

void UITask::newMsgWithMetadata(uint8_t, const char* from_name, const char* text,
                              int msgcount, const UIMessageMetadata& metadata) {
  if (quick_send) static_cast<QuickSendScreen*>(quick_send)->cancelConfirmation();
  _msgcount = msgcount;  // retain transport count for existing status/LED behavior
  static_cast<InboxScreen*>(msg_preview)->add(from_name, text, metadata);
  if (static_cast<InboxScreen*>(msg_preview)->full()) showAlert("Inbox full: delete", 2500);
  else if (curr == home) showAlert("New message in inbox", 1000);
  // Retain the current screen/selection, including when the OLED is asleep.
  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) _display->turnOn();
    if (_display->isOn()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
      _next_refresh = 0;
    }
  }
}
#else
void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}
#endif

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
#if defined(UI_RECENT_INBOX)
  if (home && curr == home && c != home) static_cast<HomeScreen*>(home)->leave();
#endif
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){
#if defined(UI_RECENT_INBOX)
  static_cast<InboxScreen*>(msg_preview)->flush();
  if (_outbox && !_outbox->flush(millis())) showAlert("Outbox save failed", 2000);
  if (_compose && !_compose->flush(millis())) showAlert("Draft save failed", 2000);
#endif

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    // Power off board including radio, display, GPS and components
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
#if defined(UI_RECENT_INBOX)
  static_cast<InboxScreen*>(msg_preview)->tick();
  if (_outbox) _outbox->tick(millis());
  if (_compose) _compose->tick(millis());
#endif
  char c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
#if defined(UI_RECENT_INBOX)
    c = checkDisplayOn(handleLongPress(KEY_ENTER));
#else
    c = handleLongPress(KEY_ENTER);
#endif  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
#if defined(UI_RECENT_INBOX)
    c = checkDisplayOn(handleLongPress(KEY_LEFT));
#else
    c = handleLongPress(KEY_LEFT);
#endif
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
#if defined(UI_RECENT_INBOX)
    c = checkDisplayOn(handleLongPress(KEY_RIGHT));
#else
    c = handleLongPress(KEY_RIGHT);
#endif
  }
#if defined(UI_RECENT_INBOX)
  auto editor_event = _editor_input.update(editingText(), joystick_up.isPressed(), joystick_down.isPressed(), _display && _display->isOn(), millis());
  ev = joystick_up.check();
  if (ev == BUTTON_EVENT_CLICK && !editor_event.suppress_up) c = checkDisplayOn(KEY_UP);
  ev = joystick_down.check();
  if (ev == BUTTON_EVENT_CLICK && !editor_event.suppress_down) c = checkDisplayOn(KEY_DOWN);
  if (editor_event.direction) c = checkDisplayOn(editor_event.direction > 0 ? KEY_UP : KEY_DOWN);
#endif
  ev = back_btn.check();
#if defined(UI_RECENT_INBOX)
  if (ev == BUTTON_EVENT_CLICK) c = checkDisplayOn(KEY_CANCEL);
  if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    checkDisplayOn(KEY_SELECT); toggleBuzzer(); c = 0;
  }
#endif
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(UI_HAS_ROTARY_INPUT)
  RotaryInputEvent rotaryEv = rotary_input.poll();
  if (c == 0 && _display != NULL && _display->isOn()) {
    if (rotaryEv == RotaryInputEvent::Next) {
      c = KEY_NEXT;
    } else if (rotaryEv == RotaryInputEvent::Prev) {
      c = KEY_PREV;
    }
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

#if defined(UI_RECENT_INBOX)
  // Process control events before considering an automatic screen change.
  if (!c && curr == home && displayAwake() && uint32_t(millis() - _last_input_at) >= 5000 &&
      static_cast<HomeScreen*>(home)->canAutoRead() && static_cast<InboxScreen*>(msg_preview)->startAutoView()) {
    _alert_expiry = 0; _auto_off = millis() + 30000; setCurrScreen(msg_preview);
  }
#endif
  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(UIColor::popup_bkg);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(UIColor::popup_txt);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(UIColor::warning_txt);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  _last_input_at = millis();
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
