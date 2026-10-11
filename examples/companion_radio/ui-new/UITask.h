#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>
#include <helpers/sensors/LPPDataHelpers.h>

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif
#ifdef PIN_VIBRATION
  #include <helpers/ui/GenericVibration.h>
#endif

#include "../AbstractUITask.h"
#include "../NodePrefs.h"

#if defined(UI_RECENT_INBOX)
  #include "Outbox.h"
  #include "UIComposeState.h"
  #include "JoystickTextEditor.h"
#endif

class UITask : public AbstractUITask {
  DisplayDriver* _display;
  SensorManager* _sensors;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
#endif
#ifdef PIN_VIBRATION
  GenericVibration vibration;
#endif
  unsigned long _next_refresh, _auto_off;
  NodePrefs* _node_prefs;
  char _alert[80];
  unsigned long _alert_expiry;
  int _msgcount = 0;
  unsigned long ui_started_at, next_batt_chck;
  uint32_t _last_input_at = 0;
  int next_backlight_btn_check = 0;
#ifdef PIN_STATUS_LED
  int led_state = 0;
  int next_led_change = 0;
  int last_led_increment = 0;
#endif

#ifdef PIN_USER_BTN_ANA
  unsigned long _analogue_pin_read_millis = millis();
#endif

  UIScreen* splash;
  UIScreen* home = nullptr;
  UIScreen* msg_preview;
  UIScreen* curr;
#if defined(UI_RECENT_INBOX)
  UIScreen* quick_send = nullptr;
  UIScreen* outbox_screen = nullptr;
  Outbox* _outbox = nullptr;
  UIComposeState* _compose = nullptr;
  EditorDirectionInput _editor_input;
#endif

  void userLedHandler();

  // Button action handlers
  char checkDisplayOn(char c);
  char handleLongPress(char c);
  char handleDoubleClick(char c);
  char handleTripleClick(char c);

  void setCurrScreen(UIScreen* c);

public:

  UITask(mesh::MainBoard* board, MultiSerialInterface* serial) : AbstractUITask(board, serial), _display(NULL), _sensors(NULL) {
    next_batt_chck = _next_refresh = 0;
    ui_started_at = 0;
    curr = NULL;
  }
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  void gotoHomeScreen() { setCurrScreen(home); }
  void showAlert(const char* text, int duration_millis);
  int  getMsgCount() const { return _msgcount; }
#if defined(UI_RECENT_INBOX)
  bool editingText() const;
  void gotoInbox();
  void gotoQuickSend(bool position_only = false);
  void gotoOutbox();
  void gotoNextUnread();
  void retryOutbox(const Outbox::History::Entry& entry);
  bool toggleFavorite(const UISendTarget& target);
  bool hasDraft() const;
  void cleanStorage();
  bool autoReadChannels() const;
  uint32_t clockTime() const;
  void toggleAutoReadChannels();
  bool displayAwake() const { return _display && _display->isOn(); }
  void gotoChannels();
  const char* joinChannel(const char* name);
  bool removeChannel(const UISendTarget& target);
  UIScreen* channels_screen = nullptr;
  bool restoreDraft(UISendTarget& target, char* text, size_t size, unsigned& cursor);
  void saveDraft(const UISendTarget& target, const char* text, unsigned cursor);
  void discardDraft(const UISendTarget& target);
  const char* draftStatus() const;
  bool resolveTarget(UISendTarget& target);
  bool getSendTarget(bool channels, unsigned index, UISendTarget& target);
  bool sendAdvert();
  void onUIMessageAck(uint32_t ack, const uint8_t* key) override;
  bool inboxReplyTarget(uint8_t kind, uint8_t channel_idx, const uint8_t* target, char* name, size_t size);
  bool sendInboxReply(uint8_t kind, uint8_t channel_idx, const uint8_t* target, const char* text);
  bool prepareQuickReply(unsigned choice, char* text, size_t size, uint32_t& fix_time);
  bool quickReplyStillValid(uint32_t fix_time);
  const char* gpsReplyError();
  int getUnreadCount() const;
  void newMsgWithMetadata(uint8_t path_len, const char* from_name, const char* text,
                         int msgcount, const UIMessageMetadata& metadata) override;
#endif
  bool hasDisplay() const { return _display != NULL; }
  bool isButtonPressed() const;

  bool isBuzzerQuiet() { 
#ifdef PIN_BUZZER
    return buzzer.isQuiet();
#else
    return true;
#endif
  }

  void toggleBuzzer();
  bool getGPSState();
  void toggleGPS();


  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;

  void shutdown(bool restart = false);
};
