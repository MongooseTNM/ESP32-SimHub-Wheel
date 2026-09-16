#pragma once

#include <cstdint>

namespace ShifterCalibration {

struct Position {
  uint16_t x;
  uint16_t y;
};

constexpr uint16_t average2(const uint16_t first, const uint16_t second) {
  return static_cast<uint16_t>((static_cast<uint32_t>(first) + second) / 2U);
}

constexpr uint16_t average3(const uint16_t first, const uint16_t second,
                            const uint16_t third) {
  return static_cast<uint16_t>(
      (static_cast<uint32_t>(first) + second + third) / 3U);
}

constexpr uint16_t midpoint(const uint16_t first, const uint16_t second) {
  return average2(first, second);
}

constexpr uint16_t orient(const uint16_t value, const bool reversed,
                          const uint16_t adcMaximum) {
  return reversed ? static_cast<uint16_t>(adcMaximum - value) : value;
}

struct Model {
  bool xReversed;
  bool yReversed;
  uint16_t xLeftCenter;
  uint16_t xCenterCenter;
  uint16_t xRightCenter;
  uint16_t yForwardCenter;
  uint16_t yNeutralCenter;
  uint16_t yBackCenter;
  uint16_t xLeftMaximum;
  uint16_t xRightMinimum;
  uint16_t yForwardMaximum;
  uint16_t yBackMinimum;

  constexpr Model(const Position neutral, const Position first,
                  const Position second, const Position third,
                  const Position fourth, const Position fifth,
                  const Position sixth, const uint16_t adcMaximum)
      : xReversed(average2(first.x, second.x) >
                  average2(fifth.x, sixth.x)),
        yReversed(average3(first.y, third.y, fifth.y) >
                  average3(second.y, fourth.y, sixth.y)),
        xLeftCenter(orient(average2(first.x, second.x), xReversed,
                           adcMaximum)),
        xCenterCenter(orient(average3(neutral.x, third.x, fourth.x),
                             xReversed, adcMaximum)),
        xRightCenter(orient(average2(fifth.x, sixth.x), xReversed,
                            adcMaximum)),
        yForwardCenter(orient(average3(first.y, third.y, fifth.y), yReversed,
                              adcMaximum)),
        yNeutralCenter(orient(neutral.y, yReversed, adcMaximum)),
        yBackCenter(orient(average3(second.y, fourth.y, sixth.y), yReversed,
                           adcMaximum)),
        xLeftMaximum(midpoint(xLeftCenter, xCenterCenter)),
        xRightMinimum(midpoint(xCenterCenter, xRightCenter)),
        yForwardMaximum(midpoint(yForwardCenter, yNeutralCenter)),
        yBackMinimum(midpoint(yNeutralCenter, yBackCenter)) {}
};

constexpr Model makeModel(const Position neutral, const Position first,
                          const Position second, const Position third,
                          const Position fourth, const Position fifth,
                          const Position sixth, const uint16_t adcMaximum) {
  return Model(neutral, first, second, third, fourth, fifth, sixth,
               adcMaximum);
}

}  // namespace ShifterCalibration
