// Requirement: waking from a Quick Resume sleep restores the saved frame, so
// after every Quick Resume sleep exactly that sleep's frame must be on disk —
// also when sleep was deferred for an automatic progress upload (the frame is
// rendered and saved when the sleep screen is shown, before the upload runs).
// With any other sleep screen mode no frame may be left behind.
#include <gtest/gtest.h>

#include "src/SleepFramePolicy.h"

namespace {

// Minimal model of the frame file across the two sleep paths in main.cpp.
struct FrameFile {
  bool exists = false;
  int writes = 0;
};

// enterDeepSleep -> (deferred: showSleepScreen + save when Quick Resume) -> commitDeepSleep
FrameFile sleepOnce(bool quickResume, bool deferred, bool staleFrameBefore) {
  FrameFile f{staleFrameBefore, 0};
  if (deferred && quickResume) {
    f.exists = true;  // enterDeepSleep saves right after showSleepScreen
    f.writes++;
  }
  const SleepFrameAction a = sleepFrameAction(quickResume, /*frameAlreadySaved=*/deferred);
  if (a.saveFrame) {
    f.exists = true;
    f.writes++;
  } else if (a.removeStale) {
    f.exists = false;
  }
  return f;
}

}  // namespace

TEST(SleepFramePolicy, QuickResumeSleepLeavesAFrame) { EXPECT_TRUE(sleepOnce(true, false, false).exists); }

TEST(SleepFramePolicy, DeferredQuickResumeSleepKeepsItsFrame) { EXPECT_TRUE(sleepOnce(true, true, false).exists); }

TEST(SleepFramePolicy, FrameIsWrittenOncePerSleep) {
  EXPECT_EQ(sleepOnce(true, false, false).writes, 1);
  EXPECT_EQ(sleepOnce(true, true, false).writes, 1);
}

TEST(SleepFramePolicy, OtherSleepModesLeaveNoStaleFrame) {
  EXPECT_FALSE(sleepOnce(false, false, true).exists);
  EXPECT_FALSE(sleepOnce(false, true, true).exists);
}
