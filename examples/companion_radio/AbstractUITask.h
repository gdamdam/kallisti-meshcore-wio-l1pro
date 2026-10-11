#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    newContactMessage,
    ack
};

struct UIMessageMetadata {
  enum Kind : uint8_t { Contact, Channel, Signed };
  Kind kind;
  uint32_t timestamp;
  uint8_t channel_idx;
  const uint8_t* sender_prefix;  // four bytes for signed messages; callback lifetime only
  const uint8_t* reply_target;   // full public key or channel fingerprint; callback lifetime only
  UIMessageMetadata(Kind k, uint32_t t, uint8_t ch, const uint8_t* prefix, const uint8_t* target = nullptr)
    : kind(k), timestamp(t), channel_idx(ch), sender_prefix(prefix), reply_target(target) {}
};

struct UISendTarget {
  bool favorite = false;
  uint8_t kind = 0, channel_idx = 0;
  uint8_t key[32] = {};
  char name[32] = {};
};

class AbstractUITask {
protected:
  mesh::MainBoard* _board;
  MultiSerialInterface* _interfaceManager;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
    _connected = false;
  }

public:
  void setHasConnection(bool connected) { _connected = connected; }
  bool hasConnection() const { return _connected; }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }
  virtual void msgRead(int msgcount) = 0;
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) = 0;
  // Existing UIs retain their preview behavior through this default adapter.
  virtual void newMsgWithMetadata(uint8_t path_len, const char* from_name, const char* text,
                                 int msgcount, const UIMessageMetadata&) {
    newMsg(path_len, from_name, text, msgcount);
  }
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  virtual void onUIMessageAck(uint32_t, const uint8_t*) {}
  virtual void loop() = 0;
};
