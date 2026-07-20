#pragma once

#include <Arduino.h>

namespace Config {

// LCD
constexpr uint8_t LCD_ADDRESS = 0x27;
constexpr uint8_t LCD_COLUMNS = 16;
constexpr uint8_t LCD_ROWS = 2;

// Rotary encoder and push button
constexpr uint8_t ROTARY_PIN_A = 27;
constexpr uint8_t ROTARY_PIN_B = 26;
constexpr uint8_t BUTTON_PIN = 25;
constexpr uint8_t CLICKS_PER_STEP = 4;
constexpr uint16_t LONG_PRESS_MS = 500;

// ENC28J60 Ethernet controller
constexpr uint8_t ETHERNET_CS_PIN = 5;
constexpr uint8_t MAC_ADDRESS[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
constexpr uint16_t OSC_PORT = 10023;
constexpr uint8_t DEFAULT_LOCAL_IP[4] = {10, 0, 1, 2};
constexpr uint8_t DEFAULT_MIXER_IP[4] = {10, 0, 1, 1};

// Runtime behaviour
constexpr float FADER_STEP = 0.01F;
constexpr uint32_t NETWORK_POLL_INTERVAL_MS = 250;
constexpr uint32_t XREMOTE_RENEW_INTERVAL_MS = 5000;
constexpr uint32_t INITIAL_QUERY_INTERVAL_MS = 150;
constexpr uint8_t MAX_MISSED_RENEWALS = 2;

// ESP32 NVS preferences
constexpr char PREFERENCES_NAMESPACE[] = "x32-control";
constexpr char PREF_LOCAL_IP[] = "local_ip";
constexpr char PREF_MIXER_IP[] = "mixer_ip";
constexpr char PREF_CHANNEL[] = "channel";

// Set to true for verbose OSC diagnostics on the serial monitor.
constexpr bool DEBUG_OSC = false;
constexpr uint32_t SERIAL_BAUD = 115200;

}  // namespace Config
