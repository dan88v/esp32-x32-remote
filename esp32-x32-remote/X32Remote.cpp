#include "X32Remote.h"

#include <SPI.h>
#include <cstring>

#include "Config.h"

namespace {
constexpr uint8_t MENU_COUNT = 4;
constexpr const char* MENU_LABELS[MENU_COUNT] = {
    "Select Channel", "Set Local IP", "Set Mixer IP", "Lock Controls"};
constexpr const char* INITIAL_SUFFIXES[3] = {
    "/config/name", "/mix/fader", "/mix/on"};
constexpr size_t CHANNEL_METER_BYTES = 20;
constexpr size_t MAIN_METER_BYTES = 132;

uint8_t meter0[] = {B00000, B00000, B00000, B00000,
                    B00000, B00000, B11111, B11111};
uint8_t meter1[] = {B00000, B00000, B00000, B00000,
                    B11111, B11111, B11111, B11111};
uint8_t meter2[] = {B00000, B00000, B11111, B11111,
                    B11111, B11111, B11111, B11111};
uint8_t meter3[] = {B11111, B11111, B11111, B11111,
                    B11111, B11111, B11111, B11111};
uint8_t meterClip[] = {B00000, B00110, B01000, B01000,
                       B00110, B00000, B11111, B11111};
}  // namespace

static_assert(sizeof(float) == 4, "X32 meters require 32-bit floats");

X32Remote* X32Remote::instance_ = nullptr;

X32Remote::X32Remote()
    : lcd_(Config::LCD_ADDRESS, Config::LCD_COLUMNS, Config::LCD_ROWS),
      localIp_(Config::DEFAULT_LOCAL_IP[0], Config::DEFAULT_LOCAL_IP[1],
               Config::DEFAULT_LOCAL_IP[2], Config::DEFAULT_LOCAL_IP[3]),
      mixerIp_(Config::DEFAULT_MIXER_IP[0], Config::DEFAULT_MIXER_IP[1],
               Config::DEFAULT_MIXER_IP[2], Config::DEFAULT_MIXER_IP[3]) {
  memcpy(mac_, Config::MAC_ADDRESS, sizeof(mac_));
}

void X32Remote::begin() {
  instance_ = this;
  Serial.begin(Config::SERIAL_BAUD);

  lcd_.init();
  lcd_.backlight();
  lcd_.createChar(0, meter0);
  lcd_.createChar(1, meter1);
  lcd_.createChar(2, meter2);
  lcd_.createChar(3, meter3);
  lcd_.createChar(4, meterClip);

  preferences_.begin(Config::PREFERENCES_NAMESPACE, false);
  loadSettings();

  rotary_.begin(Config::ROTARY_PIN_A, Config::ROTARY_PIN_B,
                Config::CLICKS_PER_STEP);
  rotary_.setLeftRotationHandler(onRotaryTurn);
  rotary_.setRightRotationHandler(onRotaryTurn);

  button_.begin(Config::BUTTON_PIN);
  button_.setClickHandler(onButtonClick);
  button_.setLongClickTime(Config::LONG_PRESS_MS);
  button_.setLongClickHandler(onButtonLongClick);

  drawStatus("ESP32 X32 Remote", "Starting...");
  startEthernet();
  scheduleInitialQueries();
  refreshHomeView();
}

void X32Remote::update() {
  rotary_.loop();
  button_.loop();
  receiveOsc();

  const uint32_t now = millis();
  if (elapsed(now, lastNetworkPollAt_, Config::NETWORK_POLL_INTERVAL_MS)) {
    lastNetworkPollAt_ = now;
    pollNetwork(now);
  }
  if (cableConnected_ &&
      elapsed(now, lastRenewAt_, Config::XREMOTE_RENEW_INTERVAL_MS)) {
    lastRenewAt_ = now;
    renewSubscriptions();
  }
  processInitialQueries(now);
}

void X32Remote::loadSettings() {
  loadIp(Config::PREF_LOCAL_IP, localIp_);
  loadIp(Config::PREF_MIXER_IP, mixerIp_);
  const uint8_t saved =
      preferences_.getUChar(Config::PREF_CHANNEL, selectedChannel_);
  selectedChannel_ = saved < CHANNEL_COUNT ? saved : 0;
}

void X32Remote::saveIp(const char* key, const IPAddress& ip) {
  const uint8_t bytes[4] = {ip[0], ip[1], ip[2], ip[3]};
  preferences_.putBytes(key, bytes, sizeof(bytes));
}

bool X32Remote::loadIp(const char* key, IPAddress& ip) {
  if (preferences_.getBytesLength(key) != 4) return false;
  uint8_t bytes[4];
  if (preferences_.getBytes(key, bytes, sizeof(bytes)) != sizeof(bytes)) {
    return false;
  }
  ip = IPAddress(bytes[0], bytes[1], bytes[2], bytes[3]);
  return true;
}

void X32Remote::startEthernet() {
  Ethernet.init(Config::ETHERNET_CS_PIN);
  Ethernet.begin(mac_, localIp_);
  ethernetAvailable_ = Ethernet.hardwareStatus() != EthernetNoHardware;
  if (!ethernetAvailable_) {
    drawStatus("Ethernet error", "ENC28J60 absent");
    while (true) delay(1);
  }
  udp_.begin(Config::OSC_PORT);
  cableConnected_ = Ethernet.linkStatus() == LinkON;
  mixerResponsive_ = false;
  missedRenewals_ = 0;
  lastRenewAt_ = millis() - Config::XREMOTE_RENEW_INTERVAL_MS;
}

void X32Remote::restartEthernet() {
  udp_.stop();
  startEthernet();
  scheduleInitialQueries();
  homeView_ = HomeView::Unknown;
  refreshHomeView();
}

void X32Remote::pollNetwork(uint32_t) {
  const bool previous = cableConnected_;
  cableConnected_ = Ethernet.linkStatus() == LinkON;
  if (!cableConnected_) {
    mixerResponsive_ = false;
    missedRenewals_ = 0;
  } else if (!previous) {
    mixerResponsive_ = false;
    missedRenewals_ = 0;
    lastRenewAt_ = millis() - Config::XREMOTE_RENEW_INTERVAL_MS;
    scheduleInitialQueries();
  }
  refreshHomeView();
}

void X32Remote::renewSubscriptions() {
  OSCMessage remote("/xremote");
  sendMessage(remote);
  requestMeters();
  if (missedRenewals_ < UINT8_MAX) ++missedRenewals_;
  if (missedRenewals_ >= Config::MAX_MISSED_RENEWALS) {
    mixerResponsive_ = false;
  }
  refreshHomeView();
}

void X32Remote::scheduleInitialQueries() {
  initialQueryIndex_ = 0;
  lastInitialQueryAt_ = millis() - Config::INITIAL_QUERY_INTERVAL_MS;
}

void X32Remote::processInitialQueries(uint32_t now) {
  if (!cableConnected_ || initialQueryIndex_ >= 3 ||
      !elapsed(now, lastInitialQueryAt_, Config::INITIAL_QUERY_INTERVAL_MS)) {
    return;
  }
  char path[40];
  buildSelectedPath(INITIAL_SUFFIXES[initialQueryIndex_], path, sizeof(path));
  sendQuery(path);
  ++initialQueryIndex_;
  lastInitialQueryAt_ = now;
}

void X32Remote::receiveOsc() {
  OSCMessage message;
  int size = udp_.parsePacket();
  if (size <= 0) return;
  while (size-- > 0) {
    const int value = udp_.read();
    if (value < 0) break;
    message.fill(static_cast<uint8_t>(value));
  }
  if (message.hasError()) {
    if (Config::DEBUG_OSC) {
      Serial.print("OSC error: ");
      Serial.println(message.getError());
    }
    return;
  }
  markMixerResponsive();
  message.dispatch("/info", onInfoMessage);
  message.route("/ch", onStripMessage);
  message.route("/auxin", onStripMessage);
  message.route("/fxrtn", onStripMessage);
  message.route("/bus", onStripMessage);
  message.route("/mtx", onStripMessage);
  message.route("/main", onStripMessage);
  message.dispatch("/meters/6", onChannelMeterMessage);
  message.dispatch("/meters/5", onMainMeterMessage);
}

void X32Remote::sendMessage(OSCMessage& message) {
  if (!cableConnected_) {
    message.empty();
    return;
  }
  if (Config::DEBUG_OSC) {
    message.send(Serial);
    Serial.println();
  }
  udp_.beginPacket(mixerIp_, Config::OSC_PORT);
  message.send(udp_);
  udp_.endPacket();
  message.empty();
}

void X32Remote::sendQuery(const char* path) {
  OSCMessage message(path);
  sendMessage(message);
}

void X32Remote::sendFader(float value) {
  char path[40];
  buildSelectedPath("/mix/fader", path, sizeof(path));
  OSCMessage message(path);
  message.add(value);
  sendMessage(message);
  sendQuery(path);
}

void X32Remote::sendChannelOn(bool on) {
  char path[40];
  buildSelectedPath("/mix/on", path, sizeof(path));
  OSCMessage message(path);
  message.add(on ? 1 : 0);
  sendMessage(message);
  sendQuery(path);
}

void X32Remote::requestMeters() {
  OSCMessage message("/meters");
  if (selectedChannel_ == MAIN_LR_CHANNEL_INDEX) {
    message.add("/meters/5");
    message.add(3);
    message.add(3);
    message.add(10);
  } else {
    message.add("/meters/6");
    message.add(static_cast<int>(selectedChannel_));
    message.add(0);
    message.add(10);
  }
  sendMessage(message);
}

void X32Remote::markMixerResponsive() {
  const bool changed = !mixerResponsive_;
  mixerResponsive_ = true;
  missedRenewals_ = 0;
  if (changed) {
    homeView_ = HomeView::Unknown;
    refreshHomeView();
  }
}

void X32Remote::onRotaryTurn(ESPRotary& rotary) {
  if (instance_) {
    instance_->handleRotation(
        rotary.getDirection() == rotary_direction::left ? 1 : -1);
  }
}
void X32Remote::onButtonClick(Button2&) {
  if (instance_) instance_->handleClick();
}
void X32Remote::onButtonLongClick(Button2&) {
  if (instance_) instance_->handleLongClick();
}
void X32Remote::onInfoMessage(OSCMessage& message) {
  if (instance_) instance_->handleInfo(message);
}
void X32Remote::onStripMessage(OSCMessage& message, int) {
  if (instance_) instance_->handleStrip(message);
}
void X32Remote::onChannelMeterMessage(OSCMessage& message) {
  if (instance_) instance_->handleChannelMeter(message);
}
void X32Remote::onMainMeterMessage(OSCMessage& message) {
  if (instance_) instance_->handleMainMeter(message);
}

void X32Remote::handleRotation(int8_t delta) {
  if (uiState_ == UiState::Home) {
    if (!locked_) adjustFader(delta);
  } else if (uiState_ == UiState::MainMenu) {
    int next = selectedMenuItem_ + delta;
    selectedMenuItem_ = constrain(next, 0, MENU_COUNT - 1);
    drawMainMenu();
  } else if (uiState_ == UiState::ChannelSelect) {
    int next = channelEdit_ + delta;
    if (next < 0) next = CHANNEL_COUNT - 1;
    if (next >= static_cast<int>(CHANNEL_COUNT)) next = 0;
    channelEdit_ = next;
    drawChannelSelector();
  } else if (uiState_ == UiState::IpEdit) {
    int next = ipEdit_[ipEditOctet_] + delta;
    ipEdit_[ipEditOctet_] = constrain(next, 0, 254);
    drawIpEditor();
  } else if (uiState_ == UiState::PinEdit) {
    int next = pinEdit_[pinEditDigit_] + delta;
    pinEdit_[pinEditDigit_] = constrain(next, 0, 9);
    pinError_ = false;
    drawPinEditor();
  }
}

void X32Remote::handleClick() {
  switch (uiState_) {
    case UiState::Home:
      locked_ ? openPinEditor(PinEditMode::Unlock) : openMainMenu();
      break;
    case UiState::MainMenu: activateMenuItem(); break;
    case UiState::ChannelSelect: confirmChannelSelection(); break;
    case UiState::IpEdit: confirmIpOctet(); break;
    case UiState::PinEdit: confirmPinDigit(); break;
  }
}

void X32Remote::handleLongClick() {
  if (uiState_ == UiState::Home) {
    locked_ ? openPinEditor(PinEditMode::Unlock) : toggleChannelOn();
  } else {
    returnHome();
  }
}

void X32Remote::adjustFader(int8_t delta) {
  if (!cableConnected_ || !mixerResponsive_) return;
  ChannelState& state = channelState_[selectedChannel_];
  if (!state.hasFader) {
    char path[40];
    buildSelectedPath("/mix/fader", path, sizeof(path));
    sendQuery(path);
    return;
  }
  state.fader = constrain(state.fader + delta * Config::FADER_STEP, 0.0F, 1.0F);
  drawHomeFader();
  sendFader(state.fader);
}

void X32Remote::toggleChannelOn() {
  if (!cableConnected_ || !mixerResponsive_) return;
  ChannelState& state = channelState_[selectedChannel_];
  if (!state.hasOn) {
    char path[40];
    buildSelectedPath("/mix/on", path, sizeof(path));
    sendQuery(path);
    return;
  }
  state.on = !state.on;
  drawHomeMute();
  sendChannelOn(state.on);
}

void X32Remote::openMainMenu() {
  selectedMenuItem_ = 0;
  uiState_ = UiState::MainMenu;
  drawMainMenu();
}

void X32Remote::activateMenuItem() {
  if (selectedMenuItem_ == 0) openChannelSelector();
  else if (selectedMenuItem_ == 1) openIpEditor(IpEditTarget::Local);
  else if (selectedMenuItem_ == 2) openIpEditor(IpEditTarget::Mixer);
  else openPinEditor(PinEditMode::SetAndLock);
}

void X32Remote::openChannelSelector() {
  channelEdit_ = selectedChannel_;
  uiState_ = UiState::ChannelSelect;
  drawChannelSelector();
}

void X32Remote::confirmChannelSelection() {
  selectedChannel_ = channelEdit_;
  preferences_.putUChar(Config::PREF_CHANNEL, selectedChannel_);
  scheduleInitialQueries();
  returnHome();
}

void X32Remote::openIpEditor(IpEditTarget target) {
  ipEditTarget_ = target;
  const IPAddress& source = target == IpEditTarget::Local ? localIp_ : mixerIp_;
  for (uint8_t i = 0; i < 4; ++i) ipEdit_[i] = source[i];
  ipEditOctet_ = 0;
  uiState_ = UiState::IpEdit;
  drawIpEditor();
}

void X32Remote::confirmIpOctet() {
  if (ipEditOctet_ < 3) {
    ++ipEditOctet_;
    drawIpEditor();
  } else {
    commitIpEdit();
  }
}

void X32Remote::commitIpEdit() {
  const IPAddress updated(ipEdit_[0], ipEdit_[1], ipEdit_[2], ipEdit_[3]);
  if (ipEditTarget_ == IpEditTarget::Local) {
    localIp_ = updated;
    saveIp(Config::PREF_LOCAL_IP, localIp_);
    returnHome();
    restartEthernet();
  } else {
    mixerIp_ = updated;
    saveIp(Config::PREF_MIXER_IP, mixerIp_);
    mixerResponsive_ = false;
    missedRenewals_ = 0;
    lastRenewAt_ = millis() - Config::XREMOTE_RENEW_INTERVAL_MS;
    scheduleInitialQueries();
    returnHome();
  }
}

void X32Remote::openPinEditor(PinEditMode mode) {
  pinEditMode_ = mode;
  memset(pinEdit_, 0, sizeof(pinEdit_));
  pinEditDigit_ = 0;
  pinError_ = false;
  uiState_ = UiState::PinEdit;
  drawPinEditor();
}

void X32Remote::confirmPinDigit() {
  pinError_ = false;
  if (pinEditDigit_ < 3) {
    ++pinEditDigit_;
    drawPinEditor();
  } else {
    commitPinEdit();
  }
}

void X32Remote::commitPinEdit() {
  if (pinEditMode_ == PinEditMode::SetAndLock) {
    memcpy(lockPin_, pinEdit_, sizeof(lockPin_));
    locked_ = true;
    returnHome();
  } else if (memcmp(lockPin_, pinEdit_, sizeof(lockPin_)) == 0) {
    locked_ = false;
    returnHome();
  } else {
    memset(pinEdit_, 0, sizeof(pinEdit_));
    pinEditDigit_ = 0;
    pinError_ = true;
    drawPinEditor();
  }
}

void X32Remote::returnHome() {
  lcd_.noCursor();
  uiState_ = UiState::Home;
  homeView_ = HomeView::Unknown;
  refreshHomeView();
}

void X32Remote::handleInfo(OSCMessage&) { markMixerResponsive(); }

void X32Remote::handleStrip(OSCMessage& message) {
  ChannelState& state = channelState_[selectedChannel_];
  char path[40];
  buildSelectedPath("/mix/fader", path, sizeof(path));
  if (message.fullMatch(path) && message.isFloat(0)) {
    state.fader = constrain(message.getFloat(0), 0.0F, 1.0F);
    state.hasFader = true;
    if (uiState_ == UiState::Home && homeView_ == HomeView::Channel) {
      drawHomeFader();
    }
    return;
  }
  buildSelectedPath("/mix/on", path, sizeof(path));
  if (message.fullMatch(path) && message.isInt(0)) {
    state.on = message.getInt(0) != 0;
    state.hasOn = true;
    if (uiState_ == UiState::Home && homeView_ == HomeView::Channel) {
      drawHomeMute();
    }
    return;
  }
  buildSelectedPath("/config/name", path, sizeof(path));
  if (message.fullMatch(path) && message.isString(0)) {
    char name[16] = {0};
    message.getString(0, name, sizeof(name));
    strncpy(state.name, name, sizeof(state.name) - 1);
    state.name[sizeof(state.name) - 1] = '\0';
    if (uiState_ == UiState::Home && homeView_ == HomeView::Channel) drawHome();
  }
}

void X32Remote::handleChannelMeter(OSCMessage& message) {
  if (selectedChannel_ == MAIN_LR_CHANNEL_INDEX) return;
  const int length = message.getBlobLength(0);
  if (length <= 0 || length > static_cast<int>(CHANNEL_METER_BYTES)) return;
  uint8_t blob[CHANNEL_METER_BYTES] = {0};
  if (message.getBlob(0, blob, sizeof(blob)) != length) return;
  bool ok = false;
  const float value = readBlobFloat(blob, length, 1, ok);
  if (ok && uiState_ == UiState::Home && homeView_ == HomeView::Channel) {
    drawMeter(0.0F, 0);
    drawMeter(value, 1);
  }
}

void X32Remote::handleMainMeter(OSCMessage& message) {
  if (selectedChannel_ != MAIN_LR_CHANNEL_INDEX) return;
  const int length = message.getBlobLength(0);
  if (length <= 0 || length > static_cast<int>(MAIN_METER_BYTES)) return;
  uint8_t blob[MAIN_METER_BYTES] = {0};
  if (message.getBlob(0, blob, sizeof(blob)) != length) return;
  bool leftOk = false, rightOk = false;
  const float left = readBlobFloat(blob, length, 25, leftOk);
  const float right = readBlobFloat(blob, length, 26, rightOk);
  if (leftOk && rightOk && uiState_ == UiState::Home &&
      homeView_ == HomeView::Channel) {
    drawMeter(left, 0);
    drawMeter(right, 1);
  }
}

void X32Remote::refreshHomeView() {
  if (uiState_ != UiState::Home) return;
  HomeView desired = HomeView::Channel;
  if (locked_) desired = HomeView::Locked;
  else if (!ethernetAvailable_ || !cableConnected_) desired = HomeView::NoCable;
  else if (!mixerResponsive_ && missedRenewals_ == 0) desired = HomeView::Connecting;
  else if (!mixerResponsive_) desired = HomeView::MixerOffline;
  if (desired == homeView_) return;
  homeView_ = desired;
  if (desired == HomeView::Channel) drawHome();
  else if (desired == HomeView::NoCable) drawStatus("Ethernet", "No cable");
  else if (desired == HomeView::Connecting) drawStatus("Connecting", "to mixer...");
  else if (desired == HomeView::Locked) drawStatus("CONTROLS LOCKED", "Press to unlock");
  else {
    char ip[17];
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", mixerIp_[0], mixerIp_[1],
             mixerIp_[2], mixerIp_[3]);
    drawStatus("Mixer offline", ip);
  }
}

void X32Remote::drawHome() {
  lcd_.noCursor();
  lcd_.clear();
  lcd_.setCursor(0, 0);
  lcd_.print(CHANNELS[selectedChannel_].shortName);
  printRight(channelState_[selectedChannel_].name, 13, 0, 9);
  drawHomeFader();
  drawHomeMute();
  clearMeter(0);
  clearMeter(1);
}

void X32Remote::drawHomeFader() {
  clearField(0, 1, 7);
  lcd_.setCursor(0, 1);
  const ChannelState& state = channelState_[selectedChannel_];
  if (!state.hasFader) {
    lcd_.print("--.-");
    return;
  }
  char value[8];
  formatFaderDb(state.fader, value, sizeof(value));
  lcd_.print(value);
}

void X32Remote::drawHomeMute() {
  clearField(10, 1, 4);
  const ChannelState& state = channelState_[selectedChannel_];
  if (state.hasOn && !state.on) {
    lcd_.setCursor(10, 1);
    lcd_.print("MUTE");
  }
}

void X32Remote::drawMainMenu() {
  lcd_.noCursor();
  lcd_.clear();
  printCentered("<MAIN MENU>", 0);
  printCentered(MENU_LABELS[selectedMenuItem_], 1);
}

void X32Remote::drawChannelSelector() {
  lcd_.noCursor();
  lcd_.clear();
  printCentered("Select channel", 0);
  printCentered(CHANNELS[channelEdit_].shortName, 1);
}

void X32Remote::drawIpEditor() {
  lcd_.noCursor();
  lcd_.clear();
  printCentered(ipEditTarget_ == IpEditTarget::Local ? "Set Local IP"
                                                      : "Set Mixer IP", 0);
  char value[16];
  snprintf(value, sizeof(value), "%u.%u.%u.%u", ipEdit_[0], ipEdit_[1],
           ipEdit_[2], ipEdit_[3]);
  printCentered(value, 1);
  uint8_t cursor = (Config::LCD_COLUMNS - strlen(value)) / 2;
  for (uint8_t i = 0; i < ipEditOctet_; ++i) {
    cursor += 2;
    if (ipEdit_[i] >= 10) ++cursor;
    if (ipEdit_[i] >= 100) ++cursor;
  }
  lcd_.setCursor(cursor, 1);
  lcd_.cursor();
}

void X32Remote::drawPinEditor() {
  lcd_.clear();
  printCentered(pinError_ ? "Wrong PIN"
                          : pinEditMode_ == PinEditMode::SetAndLock
                                ? "Set lock PIN"
                                : "Enter PIN", 0);
  for (uint8_t i = 0; i < 4; ++i) {
    lcd_.setCursor(4 + i * 2, 1);
    lcd_.print(pinEdit_[i]);
  }
  lcd_.setCursor(4 + pinEditDigit_ * 2, 1);
  lcd_.cursor();
}

void X32Remote::drawStatus(const char* line1, const char* line2) {
  lcd_.noCursor();
  lcd_.clear();
  printCentered(line1, 0);
  printCentered(line2, 1);
}

void X32Remote::clearField(uint8_t column, uint8_t row, uint8_t width) {
  lcd_.setCursor(column, row);
  while (width--) lcd_.print(' ');
}

void X32Remote::printCentered(const char* text, uint8_t row) {
  if (!text) return;
  const size_t length = min(strlen(text), static_cast<size_t>(Config::LCD_COLUMNS));
  lcd_.setCursor((Config::LCD_COLUMNS - length) / 2, row);
  for (size_t i = 0; i < length; ++i) lcd_.print(text[i]);
}

void X32Remote::printRight(const char* text, uint8_t rightColumn, uint8_t row,
                           uint8_t maxWidth) {
  if (!text || !text[0]) return;
  const size_t length = min(strlen(text), static_cast<size_t>(maxWidth));
  lcd_.setCursor(rightColumn + 1 - length, row);
  for (size_t i = 0; i < length; ++i) lcd_.print(text[i]);
}

void X32Remote::buildSelectedPath(const char* suffix, char* output,
                                  size_t outputSize) const {
  const ChannelDefinition& channel = CHANNELS[selectedChannel_];
  if (channel.hasNumber) {
    snprintf(output, outputSize, "%s%02u%s", channel.oscPrefix,
             channel.number, suffix);
  } else {
    snprintf(output, outputSize, "%s%s", channel.oscPrefix, suffix);
  }
}

bool X32Remote::elapsed(uint32_t now, uint32_t previous, uint32_t interval) {
  return static_cast<uint32_t>(now - previous) >= interval;
}

float X32Remote::readBlobFloat(const uint8_t* blob, size_t blobLength,
                               size_t floatIndex, bool& ok) {
  const size_t offset = floatIndex * sizeof(float);
  if (!blob || offset + sizeof(float) > blobLength) {
    ok = false;
    return 0.0F;
  }
  float value;
  memcpy(&value, blob + offset, sizeof(value));
  ok = true;
  return value;
}

void X32Remote::formatFaderDb(float value, char* output, size_t outputSize) {
  value = constrain(value, 0.0F, 1.0F);
  if (value <= 0.0F) {
    snprintf(output, outputSize, "-Inf");
    return;
  }
  float db;
  if (value < 0.0625F) db = -90.0F + value / 0.0625F * 30.0F;
  else if (value < 0.25F) db = -60.0F + (value - 0.0625F) / 0.1875F * 30.0F;
  else if (value < 0.5F) db = -30.0F + (value - 0.25F) / 0.25F * 20.0F;
  else db = -10.0F + (value - 0.5F) / 0.5F * 20.0F;
  if (fabs(db) < 0.05F) db = 0.0F;
  snprintf(output, outputSize, db > 0.0F ? "+%.1f" : "%.1f", db);
}

void X32Remote::clearMeter(uint8_t position) {
  const uint8_t column = 14 + position;
  lcd_.setCursor(column, 0); lcd_.print(' ');
  lcd_.setCursor(column, 1); lcd_.print(' ');
}

void X32Remote::drawMeter(float value, uint8_t position) {
  clearMeter(position);
  const uint8_t column = 14 + position;
  if (value >= 0.95F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(3));
    lcd_.setCursor(column, 0); lcd_.write(static_cast<uint8_t>(4));
  } else if (value >= 0.50F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(3));
    lcd_.setCursor(column, 0); lcd_.write(static_cast<uint8_t>(0));
  } else if (value >= 0.25F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(3));
  } else if (value >= 0.13F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(2));
  } else if (value >= 0.03F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(1));
  } else if (value >= 0.002F) {
    lcd_.setCursor(column, 1); lcd_.write(static_cast<uint8_t>(0));
  }
}
