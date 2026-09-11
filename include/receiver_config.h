#pragma once

#include <Arduino.h>

namespace ReceiverConfig {

// Onboard WS2812 status LED. Change this to 21 for the other receiver board.
constexpr uint8_t STATUS_LED_DATA_PIN = 48;
constexpr uint8_t STATUS_LED_BRIGHTNESS = 10;  // 0-255

// Active-low button: hold GPIO 14 to GND during boot to erase pairing. After
// reset, the receiver will not pair again until it is power-cycled.
constexpr uint8_t PAIRING_RESET_BUTTON_PIN = 14;
constexpr uint32_t PAIRING_RESET_HOLD_MS = 2000;

}  // namespace ReceiverConfig
