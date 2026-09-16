#pragma once

#include "pedals_config.h"
#include "pedals_input.h"

namespace PedalsConstants {

constexpr uint32_t HID_REFRESH_INTERVAL_US =
    1000000UL / PedalsConfig::HID_REFRESH_RATE_HZ;

constexpr uint16_t GAS_RANGE = PedalsInput::absoluteDifference(
    PedalsConfig::GAS_RELEASED_RAW, PedalsConfig::GAS_PRESSED_RAW);
constexpr uint16_t BRAKE_RANGE = PedalsInput::absoluteDifference(
    PedalsConfig::BRAKE_RELEASED_RAW, PedalsConfig::BRAKE_PRESSED_RAW);
constexpr uint16_t CLUTCH_RANGE = PedalsInput::absoluteDifference(
    PedalsConfig::CLUTCH_RELEASED_RAW, PedalsConfig::CLUTCH_PRESSED_RAW);

static_assert(PedalsConfig::GAS_PIN != PedalsConfig::BRAKE_PIN &&
                  PedalsConfig::GAS_PIN != PedalsConfig::CLUTCH_PIN &&
                  PedalsConfig::BRAKE_PIN != PedalsConfig::CLUTCH_PIN,
              "Gas, brake, and clutch pins must be different");
static_assert(PedalsConfig::GAS_RELEASED_RAW <= PedalsConfig::ADC_MAX &&
                  PedalsConfig::GAS_PRESSED_RAW <= PedalsConfig::ADC_MAX &&
                  PedalsConfig::BRAKE_RELEASED_RAW <= PedalsConfig::ADC_MAX &&
                  PedalsConfig::BRAKE_PRESSED_RAW <= PedalsConfig::ADC_MAX &&
                  PedalsConfig::CLUTCH_RELEASED_RAW <= PedalsConfig::ADC_MAX &&
                  PedalsConfig::CLUTCH_PRESSED_RAW <= PedalsConfig::ADC_MAX,
              "Pedal calibration readings must fit the ADC range");
static_assert(GAS_RANGE > 0 && BRAKE_RANGE > 0 && CLUTCH_RANGE > 0,
              "Each pedal needs different released and pressed readings");
static_assert(PedalsConfig::END_DEAD_ZONE * 2U < GAS_RANGE &&
                  PedalsConfig::END_DEAD_ZONE * 2U < BRAKE_RANGE &&
                  PedalsConfig::END_DEAD_ZONE * 2U < CLUTCH_RANGE,
              "Pedal dead zone must be less than half each calibrated range");
static_assert(PedalsConfig::ADC_FILTER_DIVISOR > 0,
              "ADC filter divisor must be nonzero");
static_assert(PedalsConfig::HID_REFRESH_RATE_HZ > 0 &&
                  PedalsConfig::HID_REFRESH_RATE_HZ <= 1000,
              "HID refresh rate must be between 1 and 1000 Hz");
static_assert(1000000UL % PedalsConfig::HID_REFRESH_RATE_HZ == 0,
              "HID refresh rate must divide evenly into one second");
static_assert(PedalsConfig::DIAGNOSTICS_INTERVAL_MS >= 50,
              "Diagnostics interval must be at least 50 ms");

}  // namespace PedalsConstants
