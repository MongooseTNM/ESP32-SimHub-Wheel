#pragma once

#include <Arduino.h>

#include "wheel_config.h"

namespace WheelConstants {

constexpr size_t BUTTON_COUNT =
    sizeof(WheelConfig::BUTTON_PINS) / sizeof(WheelConfig::BUTTON_PINS[0]);
constexpr uint32_t INPUT_SAFETY_REFRESH_INTERVAL_US =
    1000000UL / WheelConfig::INPUT_SAFETY_REFRESH_RATE_HZ;

static_assert(WheelConfig::INPUT_SAFETY_REFRESH_RATE_HZ > 0 &&
                  WheelConfig::INPUT_SAFETY_REFRESH_RATE_HZ <= 1000,
              "Input safety refresh rate must be between 1 and 1000 Hz");
static_assert(1000000UL % WheelConfig::INPUT_SAFETY_REFRESH_RATE_HZ == 0,
              "Input safety refresh rate must divide evenly into one second");

}  // namespace WheelConstants
