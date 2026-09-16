#pragma once

#include <cstdint>

#include "shifter_calibration.h"

namespace ShifterConfig {

// All three input signals must stay between 0 V and 3.3 V. The two axis pins
// are ADC inputs. The reverse switch input is active-high and uses the ESP32's
// internal pull-down resistor.
constexpr uint8_t REVERSE_PIN = 4;
constexpr uint8_t X_AXIS_PIN = 5;
constexpr uint8_t Y_AXIS_PIN = 6;

constexpr uint16_t ADC_MAX = 4095;

// Enter one stable raw X/Y reading for neutral and each forward gear. The
// firmware averages related positions, detects both axis directions, and
// calculates all four gate boundaries automatically. These example values
// produce the same 1400/2700 default thresholds as the previous configuration;
// replace them with measurements from your own shifter.
using ShifterCalibration::Position;
constexpr Position NEUTRAL = {1830, 1567};
constexpr Position GEAR_1 = {777, 3200};
constexpr Position GEAR_2 = {965, 70};
constexpr Position GEAR_3 = {1757, 3243};
constexpr Position GEAR_4 = {1767, 19};
constexpr Position GEAR_5 = {2375, 3550};
constexpr Position GEAR_6 = {2455, 75};

// Calculated calibration model. Normally no values below this comment need to
// be changed for calibration.
constexpr auto CALIBRATION = ShifterCalibration::makeModel(
    NEUTRAL, GEAR_1, GEAR_2, GEAR_3, GEAR_4, GEAR_5, GEAR_6, ADC_MAX);
constexpr bool X_REVERSED = CALIBRATION.xReversed;
constexpr bool Y_REVERSED = CALIBRATION.yReversed;
constexpr uint16_t X_LEFT_CENTER = CALIBRATION.xLeftCenter;
constexpr uint16_t X_CENTER_CENTER = CALIBRATION.xCenterCenter;
constexpr uint16_t X_RIGHT_CENTER = CALIBRATION.xRightCenter;
constexpr uint16_t Y_FORWARD_CENTER = CALIBRATION.yForwardCenter;
constexpr uint16_t Y_NEUTRAL_CENTER = CALIBRATION.yNeutralCenter;
constexpr uint16_t Y_BACK_CENTER = CALIBRATION.yBackCenter;
constexpr uint16_t X_LEFT_MAX = CALIBRATION.xLeftMaximum;
constexpr uint16_t X_RIGHT_MIN = CALIBRATION.xRightMinimum;
constexpr uint16_t Y_FORWARD_MAX = CALIBRATION.yForwardMaximum;
constexpr uint16_t Y_BACK_MIN = CALIBRATION.yBackMinimum;

// Hysteresis keeps a classified gate stable near a boundary. It must be less
// than half the neutral span between that axis's two boundaries.
constexpr uint16_t AXIS_HYSTERESIS = 80;

// Low-pass filtering and candidate stability suppress ADC and switch noise.
// A larger filter divisor is smoother but slower. A new gear must remain the
// candidate for GEAR_STABILITY_MS before it is reported.
constexpr uint8_t ADC_FILTER_DIVISOR = 4;
constexpr uint16_t GEAR_STABILITY_MS = 20;

// Changed states are sent immediately after stability filtering. The periodic
// report refresh is a safeguard for hosts; diagnostics are printed separately.
constexpr uint16_t HID_REFRESH_RATE_HZ = 100;
constexpr uint16_t DIAGNOSTICS_INTERVAL_MS = 250;

}  // namespace ShifterConfig
