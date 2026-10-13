// Requirements for "Check for updates" on the fork (releases tagged
// MAJOR.MINOR.PATCH.BUILD on dkej123/crosspoint-reader):
//  - never offer the version already running, nor an older one;
//  - a new fork build of the same upstream base is an update;
//  - a newer upstream base is an update regardless of the fork build number;
//  - development builds ("1.6.5-x4pro") can update to a release of that base;
//  - a release tag that cannot be understood is never offered.
#include <gtest/gtest.h>

#include "src/network/OtaVersion.h"

using ota::isVersionNewer;

TEST(OtaVersion, RunningVersionIsNeverOffered) {
  EXPECT_FALSE(isVersionNewer("1.6.5.1", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer("1.6.5", "1.6.5"));
}

TEST(OtaVersion, OlderVersionsAreNeverOffered) {
  EXPECT_FALSE(isVersionNewer("1.6.5.1", "1.6.5.2"));
  EXPECT_FALSE(isVersionNewer("1.6.4.9", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer("1.6.5", "1.6.5.1"));  // upstream base below a fork build of it
}

TEST(OtaVersion, NextForkBuildIsOffered) {
  EXPECT_TRUE(isVersionNewer("1.6.5.2", "1.6.5.1"));
  EXPECT_TRUE(isVersionNewer("1.6.5.1", "1.6.5"));
}

TEST(OtaVersion, NewerUpstreamBaseWinsOverForkBuildNumber) {
  EXPECT_TRUE(isVersionNewer("1.6.6.1", "1.6.5.9"));
  EXPECT_TRUE(isVersionNewer("1.7.0.1", "1.6.9.3"));
  EXPECT_TRUE(isVersionNewer("2.0.0", "1.9.9.9"));
}

TEST(OtaVersion, SegmentsCompareNumericallyNotAsText) {
  EXPECT_TRUE(isVersionNewer("1.6.10.1", "1.6.9.1"));
  EXPECT_TRUE(isVersionNewer("1.6.5.10", "1.6.5.9"));
}

TEST(OtaVersion, DevelopmentBuildUpdatesToReleaseOfItsBase) {
  EXPECT_TRUE(isVersionNewer("1.6.5.1", "1.6.5-x4pro"));
  EXPECT_TRUE(isVersionNewer("1.6.5.1", "1.6.5-dev-feat-x-05c6cf8"));
}

TEST(OtaVersion, ReleaseCandidateUpdatesToFinalReleaseOnly) {
  EXPECT_TRUE(isVersionNewer("1.6.5", "1.6.5-rc+abc1234"));
  EXPECT_FALSE(isVersionNewer("1.6.5-rc+def5678", "1.6.5"));
}

TEST(OtaVersion, UnparsableReleaseTagIsNeverOffered) {
  EXPECT_FALSE(isVersionNewer("", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer("nightly", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer("v1.6.6", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer("1.6", "1.6.5.1"));
  EXPECT_FALSE(isVersionNewer(nullptr, "1.6.5.1"));
}
