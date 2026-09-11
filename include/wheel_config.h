#pragma once

#include <Arduino.h>

namespace WheelConfig {

// Change these constants to match the physical RPM strip.
constexpr uint8_t RPM_LED_DATA_PIN = 48;
constexpr uint16_t RPM_LED_COUNT = 12;
constexpr uint8_t RPM_LED_BRIGHTNESS = 10;  // 0-255

// Startup confirmation: flash the entire strip this many times.
constexpr uint8_t STARTUP_FLASH_COUNT = 2;
constexpr uint16_t STARTUP_FLASH_ON_MS = 120;
constexpr uint16_t STARTUP_FLASH_OFF_MS = 100;

// Active-low buttons: connect each GPIO to GND through a momentary switch.
constexpr uint8_t BUTTON_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint32_t BUTTON_DEBOUNCE_MS = 8;

// Changed states are transmitted/reported immediately. This lower periodic
// rate only refreshes the latest state as protection against a lost packet.
constexpr uint16_t INPUT_SAFETY_REFRESH_RATE_HZ = 25;

constexpr uint32_t SHIFT_FLASH_INTERVAL_MS = 80;

// Hold this wheel button during boot to erase the stored peer and re-enter
// automatic pairing. The index is zero-based in BUTTON_PINS.
constexpr uint8_t PAIRING_RESET_BUTTON_INDEX = 0;
constexpr uint32_t PAIRING_RESET_HOLD_MS = 2000;

// Clear RPM output if fresh telemetry stops arriving.
constexpr uint32_t TELEMETRY_STALE_TIMEOUT_MS = 500;

}  // namespace WheelConfig
