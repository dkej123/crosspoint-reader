#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "AutomaticWifiConnectionPolicy.h"

using P = AutomaticWifiConnectionPolicy;

TEST(AutomaticWifiConnectionPolicy, KeepsBackgroundConnectionAliveUntilFullDeadline) {
  EXPECT_FALSE(P::backgroundDeadlineExpired(P::BACKGROUND_TIMEOUT_MS / 2, 0));
  EXPECT_FALSE(P::backgroundDeadlineExpired(P::BACKGROUND_TIMEOUT_MS - 1, 0));
  EXPECT_TRUE(P::backgroundDeadlineExpired(P::BACKGROUND_TIMEOUT_MS, 0));
  EXPECT_EQ(P::remainingBackgroundTimeMs(7500, 0), P::BACKGROUND_TIMEOUT_MS - 7500);
  EXPECT_EQ(P::remainingBackgroundTimeMs(P::BACKGROUND_TIMEOUT_MS, 0), 0u);
}

TEST(AutomaticWifiConnectionPolicy, HandlesMillisRollover) {
  const uint32_t startedAt = std::numeric_limits<uint32_t>::max() - 4999;
  const uint32_t deadline = startedAt + P::BACKGROUND_TIMEOUT_MS;  // wraps

  EXPECT_FALSE(P::backgroundDeadlineExpired(deadline - 1, startedAt));
  EXPECT_TRUE(P::backgroundDeadlineExpired(deadline, startedAt));
  EXPECT_EQ(P::remainingBackgroundTimeMs(deadline - 1, startedAt), 1u);
}

// Observed on the X4 Pro: the home network joins in 4.2-4.3 s at -76 dBm and
// sometimes needs longer; a 6 s cap made the sleep sync give up.
TEST(AutomaticWifiConnectionPolicy, FirstNetworkGetsTimeForASlowJoinEvenWithFallback) {
  EXPECT_GE(P::connectionAttemptTimeoutMs(0, 0, true), 9000u);
}

TEST(AutomaticWifiConnectionPolicy, FallbackNetworkStillGetsAChance) {
  const uint32_t afterFirst = P::connectionAttemptTimeoutMs(0, 0, true);
  EXPECT_GE(P::connectionAttemptTimeoutMs(afterFirst, 0, false), 5000u);
}

TEST(AutomaticWifiConnectionPolicy, NoAttemptOutlivesTheBackgroundDeadline) {
  for (uint32_t now = 0; now <= P::BACKGROUND_TIMEOUT_MS; now += 500) {
    for (const bool fallback : {true, false}) {
      EXPECT_LE(now + P::connectionAttemptTimeoutMs(now, 0, fallback), P::BACKGROUND_TIMEOUT_MS);
    }
  }
}

TEST(AutomaticWifiConnectionPolicy, LastNetworkGetsAFullAttemptToo) {
  EXPECT_GE(P::connectionAttemptTimeoutMs(0, 0, false), 9000u);
  EXPECT_EQ(P::connectionAttemptTimeoutMs(P::BACKGROUND_TIMEOUT_MS, 0, false), 0u);
}

TEST(AutomaticWifiConnectionPolicy, KeepsTryingForOverAMinute) {
  // The user would rather wait than miss a sync.
  EXPECT_GE(P::BACKGROUND_TIMEOUT_MS, 60000u);
}

TEST(AutomaticWifiConnectionPolicy, AStuckJoinLeavesRoomForSeveralMoreAttempts) {
  // A join that hangs must not eat the whole budget: with one saved network,
  // at least three full attempts fit.
  uint32_t now = 0;
  int attempts = 0;
  while (P::remainingBackgroundTimeMs(now, 0) > 0 && attempts < 100) {
    now += P::connectionAttemptTimeoutMs(now, 0, false) + P::ROUND_PAUSE_MS;
    attempts++;
  }
  EXPECT_GE(attempts, 3);
}

TEST(AutomaticWifiConnectionPolicy, NoNewRoundWhenTooLittleTimeIsLeft) {
  EXPECT_TRUE(P::canStartAnotherRound(0, 0));
  EXPECT_FALSE(P::canStartAnotherRound(P::BACKGROUND_TIMEOUT_MS - P::ROUND_PAUSE_MS, 0));
  EXPECT_FALSE(P::canStartAnotherRound(P::BACKGROUND_TIMEOUT_MS, 0));
}
