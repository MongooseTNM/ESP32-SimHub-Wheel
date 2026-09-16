#pragma once

#include <cstdint>

namespace PedalsInput {

constexpr uint16_t HID_AXIS_MAX = 32767;

constexpr uint16_t absoluteDifference(const uint16_t left,
                                      const uint16_t right) {
  return left >= right ? left - right : right - left;
}

// Converts a calibrated raw ADC reading to a standard HID simulation-control
// axis. Output is always 0 at the released endpoint and 32767 at the pressed
// endpoint, including when the electrical direction is reversed.
inline uint16_t scaleRawToHid(const uint16_t raw,
                              const uint16_t releasedRaw,
                              const uint16_t pressedRaw,
                              const uint16_t endDeadZone) {
  const uint16_t range = absoluteDifference(releasedRaw, pressedRaw);
  if (range == 0 || endDeadZone * 2U >= range) return 0;

  int32_t travel = pressedRaw >= releasedRaw
                       ? static_cast<int32_t>(raw) - releasedRaw
                       : static_cast<int32_t>(releasedRaw) - raw;
  if (travel <= static_cast<int32_t>(endDeadZone)) return 0;
  if (travel >= static_cast<int32_t>(range - endDeadZone)) {
    return HID_AXIS_MAX;
  }

  const uint32_t usableTravel = range - endDeadZone * 2U;
  const uint32_t adjustedTravel =
      static_cast<uint32_t>(travel - endDeadZone);
  return static_cast<uint16_t>(
      (adjustedTravel * HID_AXIS_MAX + usableTravel / 2U) / usableTravel);
}

}  // namespace PedalsInput
