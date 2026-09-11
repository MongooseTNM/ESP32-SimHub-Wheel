#pragma once

#include <cstddef>
#include <cstdint>

#include "wheel_config.h"

namespace WheelInput {

constexpr size_t BUTTON_COUNT =
    sizeof(WheelConfig::BUTTON_PINS) / sizeof(WheelConfig::BUTTON_PINS[0]);
constexpr size_t POV_DIRECTION_COUNT = 4;
constexpr size_t CONTACT_COUNT = BUTTON_COUNT + POV_DIRECTION_COUNT;
constexpr uint16_t BUTTON_MASK =
    static_cast<uint16_t>((1UL << BUTTON_COUNT) - 1UL);

// Values expected by ESP32 USBHIDGamepad: 0 is neutral and 1-8 rotate
// clockwise in 45-degree increments starting at north.
enum class Pov : uint8_t {
  Neutral = 0,
  Up = 1,
  UpRight = 2,
  Right = 3,
  DownRight = 4,
  Down = 5,
  DownLeft = 6,
  Left = 7,
  UpLeft = 8,
};

inline Pov povFromDirections(const bool up, const bool right, const bool down,
                             const bool left) {
  const int8_t vertical = static_cast<int8_t>(down) - static_cast<int8_t>(up);
  const int8_t horizontal =
      static_cast<int8_t>(right) - static_cast<int8_t>(left);

  if (vertical < 0) {
    if (horizontal > 0) return Pov::UpRight;
    if (horizontal < 0) return Pov::UpLeft;
    return Pov::Up;
  }
  if (vertical > 0) {
    if (horizontal > 0) return Pov::DownRight;
    if (horizontal < 0) return Pov::DownLeft;
    return Pov::Down;
  }
  if (horizontal > 0) return Pov::Right;
  if (horizontal < 0) return Pov::Left;
  return Pov::Neutral;
}

static_assert(BUTTON_COUNT == 11, "Wheel must expose HID buttons 1-11");
static_assert(CONTACT_COUNT <= 16, "Contact state must fit in a 16-bit mask");

}  // namespace WheelInput
