// Requirement (Long-press behavior = Orientation), X4 Pro: page buttons sit on
// opposite long edges (3 = left, 6 = right when held upright). Holding the
// right button turns the device a quarter turn to the right (clockwise, as the
// user holds it), the left one to the left. A button keeps its meaning after a
// turn: the user names it by its place on the case.
//
// Settings indices describe the drawn content: 0 Portrait, 1 Landscape CW,
// 2 Inverted, 3 Landscape CCW. Content drawn clockwise is read with the device
// turned counter-clockwise, so in device degrees (clockwise, as held):
// Portrait 0, Landscape CCW 90, Inverted 180, Landscape CW 270.
#include <gtest/gtest.h>

#include "src/activities/reader/OrientationRotation.h"

namespace {
constexpr uint8_t PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3;
constexpr bool RIGHT = true, LEFT = false;

int deviceDegrees(const uint8_t o) {
  switch (o) {
    case PORTRAIT: return 0;
    case LANDSCAPE_CCW: return 90;
    case INVERTED: return 180;
    case LANDSCAPE_CW: return 270;
  }
  return -1;
}
}  // namespace

TEST(OrientationRotation, RightButtonTurnsTheDeviceAQuarterToTheRightFromEveryOrientation) {
  for (uint8_t o = 0; o < 4; o++) {
    EXPECT_EQ(deviceDegrees(turnDevice(o, RIGHT)), (deviceDegrees(o) + 90) % 360) << "from " << deviceDegrees(o);
  }
}

TEST(OrientationRotation, LeftButtonTurnsTheDeviceAQuarterToTheLeftFromEveryOrientation) {
  for (uint8_t o = 0; o < 4; o++) {
    EXPECT_EQ(deviceDegrees(turnDevice(o, LEFT)), (deviceDegrees(o) + 270) % 360) << "from " << deviceDegrees(o);
  }
}

// Reported 15.10.2026: at 180 deg the right button went to 270, pressed again
// it went back to 180 instead of on to 0.
TEST(OrientationRotation, PressingTheSameButtonAgainKeepsTurningTheSameWay) {
  uint8_t o = INVERTED;
  o = turnDevice(o, RIGHT);
  EXPECT_EQ(deviceDegrees(o), 270);
  o = turnDevice(o, RIGHT);
  EXPECT_EQ(deviceDegrees(o), 0);
}

TEST(OrientationRotation, FourRightTurnsVisitEveryOrientationAndComeBack) {
  uint8_t o = PORTRAIT;
  bool seen[4] = {};
  for (int i = 0; i < 4; i++) {
    o = turnDevice(o, RIGHT);
    seen[o] = true;
  }
  EXPECT_EQ(o, PORTRAIT);
  for (bool s : seen) EXPECT_TRUE(s);
}

TEST(OrientationRotation, LeftUndoesRight) {
  for (uint8_t o = 0; o < 4; o++) EXPECT_EQ(turnDevice(turnDevice(o, RIGHT), LEFT), o);
}
