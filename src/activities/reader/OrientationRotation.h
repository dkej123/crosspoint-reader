#pragma once

#include <cstdint>

// Long-press page-turn rotation (Long-press behavior = Orientation).
//
// The X4 Pro has one page button on each long edge: "right" (side Down) on the
// right edge and "left" (side Up) on the left edge when held upright. The user
// names them by that fixed place on the case, so a button keeps its meaning in
// every orientation: holding the right one turns the device a quarter turn to
// the right (clockwise, as the user holds it), the left one to the left.
//
// Settings indices describe how the content is drawn: 0 Portrait,
// 1 Landscape CW, 2 Inverted, 3 Landscape CCW. Content drawn clockwise is read
// with the device turned counter-clockwise, so the device's clockwise turns
// walk 0 -> 3 -> 2 -> 1 -> 0 (0, 90, 180, 270 degrees).
constexpr uint8_t turnDevice(const uint8_t current, const bool toTheRight, const uint8_t count = 4) {
  return toTheRight ? static_cast<uint8_t>((current + count - 1) % count) : static_cast<uint8_t>((current + 1) % count);
}
