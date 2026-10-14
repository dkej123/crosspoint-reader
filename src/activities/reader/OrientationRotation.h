#pragma once

#include <cstdint>

// Long-press page-turn rotation (Long-press behavior = Orientation): holding the
// right (next-page) control rotates the reading orientation one step clockwise,
// holding the left (previous-page) control one step counter-clockwise.
// Orientations are numbered in clockwise order: Portrait 0, Landscape CW 1,
// Inverted 2, Landscape CCW 3.
constexpr uint8_t rotateOrientation(const uint8_t current, const bool clockwise, const uint8_t count = 4) {
  return clockwise ? static_cast<uint8_t>((current + 1) % count) : static_cast<uint8_t>((current + count - 1) % count);
}
