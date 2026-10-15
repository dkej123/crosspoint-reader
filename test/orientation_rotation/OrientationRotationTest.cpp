// Requirement (Long-press behavior = Orientation): holding the right page-turn
// control rotates the reading orientation to the right (clockwise), holding
// the left one to the left (counter-clockwise), one quarter turn per hold.
// Settings order: Portrait, Landscape CW, Inverted, Landscape CCW.
#include <gtest/gtest.h>

#include "src/activities/reader/OrientationRotation.h"

namespace {
constexpr uint8_t PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3;
constexpr bool RIGHT = true, LEFT = false;
}  // namespace

TEST(OrientationRotation, RightHoldTurnsPortraitToLandscapeClockwise) {
  EXPECT_EQ(rotateOrientation(PORTRAIT, RIGHT), LANDSCAPE_CW);
}

TEST(OrientationRotation, LeftHoldTurnsPortraitToLandscapeCounterClockwise) {
  EXPECT_EQ(rotateOrientation(PORTRAIT, LEFT), LANDSCAPE_CCW);
}

TEST(OrientationRotation, RightHoldsWalkTheClockwiseCycle) {
  EXPECT_EQ(rotateOrientation(LANDSCAPE_CW, RIGHT), INVERTED);
  EXPECT_EQ(rotateOrientation(INVERTED, RIGHT), LANDSCAPE_CCW);
  EXPECT_EQ(rotateOrientation(LANDSCAPE_CCW, RIGHT), PORTRAIT);
}

TEST(OrientationRotation, LeftUndoesRight) {
  for (uint8_t o = 0; o < 4; o++) EXPECT_EQ(rotateOrientation(rotateOrientation(o, RIGHT), LEFT), o);
}

TEST(OrientationRotation, AlwaysAValidOrientation) {
  for (uint8_t o = 0; o < 4; o++) {
    EXPECT_LT(rotateOrientation(o, RIGHT), 4);
    EXPECT_LT(rotateOrientation(o, LEFT), 4);
  }
}

// X4 Pro: physical Left = side Up, Right = side Down. Inverted and Landscape
// CCW swap the nav because the physical right button is on the reader's left.
// The button on the reader's right rotates clockwise in every orientation.
TEST(OrientationRotation, ReadersRightIsClockwiseInEveryOrientation) {
  constexpr bool PHYSICAL_RIGHT = true, PHYSICAL_LEFT = false;
  EXPECT_TRUE(rotationClockwise(PHYSICAL_RIGHT, /*navSwapped=*/false));  // Portrait, Landscape CW
  EXPECT_FALSE(rotationClockwise(PHYSICAL_LEFT, /*navSwapped=*/false));
  EXPECT_TRUE(rotationClockwise(PHYSICAL_LEFT, /*navSwapped=*/true));  // Inverted, Landscape CCW
  EXPECT_FALSE(rotationClockwise(PHYSICAL_RIGHT, /*navSwapped=*/true));
}
