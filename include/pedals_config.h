#pragma once

#include <cstdint>

namespace PedalsConfig {

// Connect each potentiometer's two outside terminals to 3.3 V and GND, then
// connect its center/wiper terminal to the matching ADC pin. Never apply more
// than 3.3 V to an ESP32-S3 GPIO. All pedal grounds must connect to ESP32 GND.
constexpr uint8_t GAS_PIN = 9;
constexpr uint8_t BRAKE_PIN = 10;
constexpr uint8_t CLUTCH_PIN = 11;

constexpr uint16_t ADC_MAX = 4095;

// Replace these six values with stable raw readings from your actual pedals.
// Released may be greater than pressed; the firmware detects either direction.
constexpr uint16_t GAS_RELEASED_RAW = 4095;
constexpr uint16_t GAS_PRESSED_RAW = 870;
constexpr uint16_t BRAKE_RELEASED_RAW = 4095;
constexpr uint16_t BRAKE_PRESSED_RAW = 660;
constexpr uint16_t CLUTCH_RELEASED_RAW = 4095;
constexpr uint16_t CLUTCH_PRESSED_RAW = 0;

// Raw ADC counts ignored at both ends of each calibrated range. This ensures a
// fully released 0% and fully pressed 100% despite small mechanical variations.
constexpr uint16_t END_DEAD_ZONE = 40;

// A larger divisor provides smoother input at the cost of response time.
constexpr uint8_t ADC_FILTER_DIVISOR = 2;

// Reports are sent immediately when an axis changes and periodically at this
// rate as a host-side safety refresh.
constexpr uint16_t HID_REFRESH_RATE_HZ = 100;
constexpr uint16_t DIAGNOSTICS_INTERVAL_MS = 250;

}  // namespace PedalsConfig
