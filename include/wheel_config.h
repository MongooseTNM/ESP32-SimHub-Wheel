#pragma once

#include <cstdint>

namespace WheelConfig {

// Change these constants to match the physical RPM strip.
constexpr uint8_t RPM_LED_DATA_PIN = 48;
constexpr uint16_t RPM_LED_COUNT = 12;
constexpr uint8_t RPM_LED_BRIGHTNESS = 10;  // 0-255

// Startup confirmation: flash the entire strip this many times.
constexpr uint8_t STARTUP_FLASH_COUNT = 2;
constexpr uint16_t STARTUP_FLASH_ON_MS = 120;
constexpr uint16_t STARTUP_FLASH_OFF_MS = 100;

// Active-low inputs: connect each switch contact between its GPIO and GND.
// Array order is HID button order: shifters 1-2, 5-way center 3, then the
// eight existing wheel switches as buttons 4-11.
constexpr uint8_t BUTTON_PINS[] = {10, 11, 12, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint8_t POV_UP_PIN = 13;
constexpr uint8_t POV_RIGHT_PIN = 14;
constexpr uint8_t POV_DOWN_PIN = 15;
constexpr uint8_t POV_LEFT_PIN = 16;
constexpr uint8_t POV_PINS[] = {POV_UP_PIN, POV_RIGHT_PIN, POV_DOWN_PIN,
                                POV_LEFT_PIN};
constexpr uint32_t BUTTON_DEBOUNCE_MS = 8;

// Changed states are transmitted/reported immediately. This lower periodic
// rate only refreshes the latest state as protection against a lost packet.
constexpr uint16_t INPUT_SAFETY_REFRESH_RATE_HZ = 25;

constexpr uint32_t SHIFT_FLASH_INTERVAL_MS = 80;

// Hold the existing switch on GPIO 2 during boot to erase the stored peer and
// re-enter automatic pairing. It is reported as HID button 4.
constexpr uint8_t PAIRING_RESET_PIN = 2;
constexpr uint32_t PAIRING_RESET_HOLD_MS = 2000;

// Clear RPM output if fresh telemetry stops arriving.
constexpr uint32_t TELEMETRY_STALE_TIMEOUT_MS = 500;

}  // namespace WheelConfig
