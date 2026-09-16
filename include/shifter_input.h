#pragma once

#include <cstdint>

namespace ShifterInput {

enum class AxisZone : uint8_t { Low, Center, High };

enum class Gear : uint8_t {
  Neutral = 0,
  First = 1,
  Second = 2,
  Third = 3,
  Fourth = 4,
  Fifth = 5,
  Sixth = 6,
  Reverse = 7,
};

// Schmitt-trigger style classification. A zone must cross its boundary plus
// hysteresis before leaving it, preventing rapid toggling around a threshold.
inline AxisZone classifyAxis(const uint16_t value, const AxisZone previous,
                             const uint16_t lowMaximum,
                             const uint16_t highMinimum,
                             const uint16_t hysteresis) {
  switch (previous) {
    case AxisZone::Low:
      return value <= lowMaximum + hysteresis ? AxisZone::Low
                                              : AxisZone::Center;
    case AxisZone::High:
      return value >= highMinimum - hysteresis ? AxisZone::High
                                               : AxisZone::Center;
    case AxisZone::Center:
    default:
      if (value + hysteresis < lowMaximum) return AxisZone::Low;
      if (value > highMinimum + hysteresis) return AxisZone::High;
      return AxisZone::Center;
  }
}

inline Gear gearFromZones(const AxisZone column, const AxisZone row,
                          const bool reverseActive) {
  if (row == AxisZone::Center) return Gear::Neutral;

  if (row == AxisZone::Low) {
    if (column == AxisZone::Low) return Gear::First;
    if (column == AxisZone::Center) return Gear::Third;
    return Gear::Fifth;
  }

  if (column == AxisZone::Low) return Gear::Second;
  if (column == AxisZone::Center) return Gear::Fourth;
  return reverseActive ? Gear::Reverse : Gear::Sixth;
}

inline uint16_t buttonMaskForGear(const Gear gear) {
  const uint8_t button = static_cast<uint8_t>(gear);
  return button == 0 || button > 7
             ? 0
             : static_cast<uint16_t>(1U << (button - 1U));
}

inline const char *gearName(const Gear gear) {
  switch (gear) {
    case Gear::First:
      return "1";
    case Gear::Second:
      return "2";
    case Gear::Third:
      return "3";
    case Gear::Fourth:
      return "4";
    case Gear::Fifth:
      return "5";
    case Gear::Sixth:
      return "6";
    case Gear::Reverse:
      return "R";
    case Gear::Neutral:
    default:
      return "N";
  }
}

}  // namespace ShifterInput
