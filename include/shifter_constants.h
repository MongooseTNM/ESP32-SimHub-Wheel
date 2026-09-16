#pragma once

#include "shifter_config.h"

namespace ShifterConstants {

constexpr uint32_t HID_REFRESH_INTERVAL_US =
    1000000UL / ShifterConfig::HID_REFRESH_RATE_HZ;

constexpr bool positionFitsAdc(const ShifterCalibration::Position position) {
  return position.x <= ShifterConfig::ADC_MAX &&
         position.y <= ShifterConfig::ADC_MAX;
}

static_assert(ShifterConfig::X_AXIS_PIN != ShifterConfig::Y_AXIS_PIN &&
                  ShifterConfig::X_AXIS_PIN != ShifterConfig::REVERSE_PIN &&
                  ShifterConfig::Y_AXIS_PIN != ShifterConfig::REVERSE_PIN,
              "Shifter input pins must be unique");
static_assert(ShifterConfig::X_LEFT_MAX < ShifterConfig::X_RIGHT_MIN,
              "X thresholds must leave a center gate");
static_assert(ShifterConfig::Y_FORWARD_MAX < ShifterConfig::Y_BACK_MIN,
              "Y thresholds must leave a neutral band");
static_assert(ShifterConfig::X_RIGHT_MIN <= ShifterConfig::ADC_MAX &&
                  ShifterConfig::Y_BACK_MIN <= ShifterConfig::ADC_MAX,
              "Shifter thresholds must fit the ADC range");
static_assert(positionFitsAdc(ShifterConfig::NEUTRAL) &&
                  positionFitsAdc(ShifterConfig::GEAR_1) &&
                  positionFitsAdc(ShifterConfig::GEAR_2) &&
                  positionFitsAdc(ShifterConfig::GEAR_3) &&
                  positionFitsAdc(ShifterConfig::GEAR_4) &&
                  positionFitsAdc(ShifterConfig::GEAR_5) &&
                  positionFitsAdc(ShifterConfig::GEAR_6),
              "Every calibration reading must fit the ADC range");
static_assert(ShifterConfig::X_LEFT_CENTER <
                      ShifterConfig::X_CENTER_CENTER &&
                  ShifterConfig::X_CENTER_CENTER <
                      ShifterConfig::X_RIGHT_CENTER,
              "X calibration must distinguish left, center, and right gates");
static_assert(ShifterConfig::Y_FORWARD_CENTER <
                      ShifterConfig::Y_NEUTRAL_CENTER &&
                  ShifterConfig::Y_NEUTRAL_CENTER <
                      ShifterConfig::Y_BACK_CENTER,
              "Y calibration must distinguish forward, neutral, and back rows");
static_assert(
    ShifterConfig::AXIS_HYSTERESIS * 2U <
            ShifterConfig::X_CENTER_CENTER - ShifterConfig::X_LEFT_CENTER &&
        ShifterConfig::AXIS_HYSTERESIS * 2U <
            ShifterConfig::X_RIGHT_CENTER - ShifterConfig::X_CENTER_CENTER &&
        ShifterConfig::AXIS_HYSTERESIS * 2U <
            ShifterConfig::Y_NEUTRAL_CENTER -
                ShifterConfig::Y_FORWARD_CENTER &&
        ShifterConfig::AXIS_HYSTERESIS * 2U <
            ShifterConfig::Y_BACK_CENTER - ShifterConfig::Y_NEUTRAL_CENTER,
    "Calibration positions are too close for the configured hysteresis");
static_assert(ShifterConfig::ADC_FILTER_DIVISOR > 0,
              "ADC filter divisor must be nonzero");
static_assert(ShifterConfig::GEAR_STABILITY_MS > 0,
              "Gear stability time must be nonzero");
static_assert(ShifterConfig::HID_REFRESH_RATE_HZ > 0 &&
                  ShifterConfig::HID_REFRESH_RATE_HZ <= 1000,
              "HID refresh rate must be between 1 and 1000 Hz");
static_assert(1000000UL % ShifterConfig::HID_REFRESH_RATE_HZ == 0,
              "HID refresh rate must divide evenly into one second");
static_assert(ShifterConfig::DIAGNOSTICS_INTERVAL_MS >= 50,
              "Diagnostics interval must be at least 50 ms");

}  // namespace ShifterConstants
