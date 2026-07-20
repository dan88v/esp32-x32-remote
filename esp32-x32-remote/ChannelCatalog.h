#pragma once

#include <Arduino.h>

struct ChannelDefinition {
  const char* shortName;
  const char* oscPrefix;
  uint8_t number;
  bool hasNumber;
};

constexpr size_t CHANNEL_COUNT = 72;
constexpr uint8_t MAIN_LR_CHANNEL_INDEX = 70;
constexpr uint8_t MONO_CHANNEL_INDEX = 71;
extern const ChannelDefinition CHANNELS[CHANNEL_COUNT];
