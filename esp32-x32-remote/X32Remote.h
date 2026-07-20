#pragma once

#include <Arduino.h>
#include <Button2.h>
#include <ESPRotary.h>
#include <EthernetENC.h>
#include <EthernetUdp.h>
#include <LiquidCrystal_I2C.h>
#include <OSCMessage.h>
#include <Preferences.h>

#include "ChannelCatalog.h"

class X32Remote {
 public:
  X32Remote();

  void begin();
  void update();

 private:
  enum class UiState : uint8_t {
    Home,
    MainMenu,
    ChannelSelect,
    IpEdit,
    PinEdit,
  };

  enum class HomeView : uint8_t {
    Unknown,
    Channel,
    NoCable,
    Connecting,
    MixerOffline,
    Locked,
  };

  enum class IpEditTarget : uint8_t {
    Local,
    Mixer,
  };

  enum class PinEditMode : uint8_t {
    SetAndLock,
    Unlock,
  };

  struct ChannelState {
    float fader = 0.0F;
    bool on = true;
    bool hasFader = false;
    bool hasOn = false;
    char name[13] = {0};
  };

  static X32Remote* instance_;

  LiquidCrystal_I2C lcd_;
  EthernetUDP udp_;
  ESPRotary rotary_;
  Button2 button_;
  Preferences preferences_;

  UiState uiState_ = UiState::Home;
  HomeView homeView_ = HomeView::Unknown;
  IpEditTarget ipEditTarget_ = IpEditTarget::Local;
  PinEditMode pinEditMode_ = PinEditMode::SetAndLock;

  ChannelState channelState_[CHANNEL_COUNT];
  uint8_t selectedChannel_ = 0;
  uint8_t channelEdit_ = 0;
  uint8_t selectedMenuItem_ = 0;

  IPAddress localIp_;
  IPAddress mixerIp_;
  uint8_t ipEdit_[4] = {0, 0, 0, 0};
  uint8_t ipEditOctet_ = 0;

  uint8_t lockPin_[4] = {0, 0, 0, 0};
  uint8_t pinEdit_[4] = {0, 0, 0, 0};
  uint8_t pinEditDigit_ = 0;
  bool locked_ = false;
  bool pinError_ = false;

  uint8_t mac_[6];
  bool ethernetAvailable_ = false;
  bool cableConnected_ = false;
  bool mixerResponsive_ = false;
  uint8_t missedRenewals_ = 0;
  uint8_t initialQueryIndex_ = 0;

  uint32_t lastNetworkPollAt_ = 0;
  uint32_t lastRenewAt_ = 0;
  uint32_t lastInitialQueryAt_ = 0;

  static void onRotaryTurn(ESPRotary& rotary);
  static void onButtonClick(Button2& button);
  static void onButtonLongClick(Button2& button);
  static void onInfoMessage(OSCMessage& message);
  static void onStripMessage(OSCMessage& message, int addressOffset);
  static void onChannelMeterMessage(OSCMessage& message);
  static void onMainMeterMessage(OSCMessage& message);

  void loadSettings();
  void saveIp(const char* key, const IPAddress& ip);
  bool loadIp(const char* key, IPAddress& ip);

  void startEthernet();
  void restartEthernet();
  void pollNetwork(uint32_t now);
  void renewSubscriptions();
  void scheduleInitialQueries();
  void processInitialQueries(uint32_t now);

  void receiveOsc();
  void sendMessage(OSCMessage& message);
  void sendQuery(const char* path);
  void sendFader(float value);
  void sendChannelOn(bool on);
  void requestMeters();
  void markMixerResponsive();

  void handleRotation(int8_t delta);
  void handleClick();
  void handleLongClick();
  void adjustFader(int8_t delta);
  void toggleChannelOn();

  void openMainMenu();
  void activateMenuItem();
  void openChannelSelector();
  void confirmChannelSelection();
  void openIpEditor(IpEditTarget target);
  void confirmIpOctet();
  void commitIpEdit();
  void openPinEditor(PinEditMode mode);
  void confirmPinDigit();
  void commitPinEdit();
  void returnHome();

  void handleInfo(OSCMessage& message);
  void handleStrip(OSCMessage& message);
  void handleChannelMeter(OSCMessage& message);
  void handleMainMeter(OSCMessage& message);

  void refreshHomeView();
  void drawHome();
  void drawHomeFader();
  void drawHomeMute();
  void drawMainMenu();
  void drawChannelSelector();
  void drawIpEditor();
  void drawPinEditor();
  void drawStatus(const char* line1, const char* line2);

  void clearField(uint8_t column, uint8_t row, uint8_t width);
  void printCentered(const char* text, uint8_t row);
  void printRight(const char* text, uint8_t rightColumn, uint8_t row,
                  uint8_t maxWidth);

  void buildSelectedPath(const char* suffix, char* output,
                         size_t outputSize) const;
  static bool elapsed(uint32_t now, uint32_t previous, uint32_t interval);
  static float readBlobFloat(const uint8_t* blob, size_t blobLength,
                             size_t floatIndex, bool& ok);
  static void formatFaderDb(float value, char* output, size_t outputSize);
  void drawMeter(float value, uint8_t position);
  void clearMeter(uint8_t position);
};
