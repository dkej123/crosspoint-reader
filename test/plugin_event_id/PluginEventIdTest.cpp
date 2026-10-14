#include <gtest/gtest.h>

#include <set>
#include <string>

#include "src/util/PluginEventId.h"

using pluginevents::makeEventId;

// The receiver (Home Assistant) drops any event whose id it has seen before,
// so a repeated id means a lost reading session.

TEST(PluginEventId, FirstEventOfEachBootIsDistinctEvenWhenRngRepeats) {
  // Deep-sleep wakes are boots; the pre-radio RNG returned the same value on
  // each one. The first event of every wake must still be new to the receiver.
  constexpr uint32_t sameRng = 0x3b9c2a9f;
  std::set<std::string> seen;
  for (uint32_t boot = 1; boot <= 1000; boot++) {
    EXPECT_TRUE(seen.insert(makeEventId(sameRng, boot, 1)).second) << "boot " << boot;
  }
}

TEST(PluginEventId, EventsWithinOneBootAreDistinct) {
  std::set<std::string> seen;
  for (uint32_t seq = 1; seq <= 1000; seq++) {
    EXPECT_TRUE(seen.insert(makeEventId(0x12345678, 7, seq)).second) << "seq " << seq;
  }
}

TEST(PluginEventId, NoCollisionBetweenBootAndSequenceCombinations) {
  // e.g. boot 1 / event 11 must not read like boot 11 / event 1.
  std::set<std::string> seen;
  for (uint32_t boot = 1; boot <= 30; boot++) {
    for (uint32_t seq = 1; seq <= 30; seq++) {
      EXPECT_TRUE(seen.insert(makeEventId(0, boot, seq)).second) << boot << "/" << seq;
    }
  }
}

TEST(PluginEventId, DiffersFromIdsOfOlderFirmware) {
  // Older builds sent "<rng>-<seq>"; the receiver still remembers those ids.
  EXPECT_NE(makeEventId(0x3b9c2a9f, 0, 1), "3b9c2a9f-1");
  EXPECT_NE(makeEventId(0x3b9c2a9f, 1, 1), "3b9c2a9f-1");
}

TEST(PluginEventId, LargestValuesAreNotTruncated) {
  EXPECT_EQ(makeEventId(0xffffffff, 0xffffffff, 0xffffffff), "ffffffff-4294967295-4294967295");
}
