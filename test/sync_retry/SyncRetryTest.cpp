#include <gtest/gtest.h>

#include <vector>

#include "KOReaderSync/AutomaticWifiConnectionPolicy.h"
#include "KOReaderSync/SyncRetryPolicy.h"

// Goal: progress reaches kosync at sleep whenever the network recovers within
// the budget, without ever overrunning the budget or hammering on errors that
// a retry cannot fix.

namespace {

enum class R { Ok, Network, Server5xx, Auth };

syncretry::Outcome classify(const R r) {
  switch (r) {
    case R::Ok:
      return syncretry::Outcome::Ok;
    case R::Network:
    case R::Server5xx:
      return syncretry::Outcome::Transient;
    case R::Auth:
      return syncretry::Outcome::Permanent;
  }
  return syncretry::Outcome::Permanent;
}

// Simulated time: each attempt consumes its duration (or its whole timeout if
// the request hangs), waits consume their delay.
struct FakeClock {
  uint32_t budget = syncretry::OPERATION_BUDGET_MS;
  uint32_t now = 0;
  uint32_t remaining() const { return now >= budget ? 0 : budget - now; }
  void wait(const uint32_t ms) { now += ms; }
};

struct Step {
  R result;
  bool hangs;  // no response: the attempt runs until its timeout
};

struct Server {
  std::vector<Step> steps;
  size_t calls = 0;
  std::vector<uint32_t> timeouts;
  FakeClock* clock = nullptr;
  R operator()(const uint32_t timeoutMs) {
    timeouts.push_back(timeoutMs);
    const Step s = calls < steps.size() ? steps[calls] : steps.back();
    calls++;
    clock->now += s.hangs ? timeoutMs : 400;
    return s.hangs ? R::Network : s.result;
  }
};

R runAgainst(Server& server, FakeClock& clock) {
  server.clock = &clock;
  return syncretry::run(server, classify, clock, R::Network);
}

}  // namespace

TEST(SyncRetry, RequestLostOnceStillSucceeds) {
  // What happened on 2026-10-14: handshake OK, then no response.
  FakeClock clock;
  Server server{{{R::Network, true}, {R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Ok);
  EXPECT_EQ(server.calls, 2u);
}

TEST(SyncRetry, LostRequestDoesNotConsumeTheWholeBudget) {
  // Even with 10 s already spent on Wi-Fi, a hung request must leave room for
  // another attempt.
  FakeClock clock;
  clock.now = 10000;
  Server server{{{R::Network, true}, {R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Ok);
}

TEST(SyncRetry, ServerErrorsAreRetried) {
  FakeClock clock;
  Server server{{{R::Server5xx, false}, {R::Server5xx, false}, {R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Ok);
  EXPECT_EQ(server.calls, 3u);
}

TEST(SyncRetry, AuthFailureIsNotRetried) {
  FakeClock clock;
  Server server{{{R::Auth, false}, {R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Auth);
  EXPECT_EQ(server.calls, 1u);
}

TEST(SyncRetry, SuccessIsNotRepeated) {
  FakeClock clock;
  Server server{{{R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Ok);
  EXPECT_EQ(server.calls, 1u);
}

TEST(SyncRetry, PersistentOutageEndsWithinBudgetAndReportsFailure) {
  FakeClock clock;
  Server server{{{R::Network, true}}};
  EXPECT_EQ(runAgainst(server, clock), R::Network);
  EXPECT_LE(clock.now, syncretry::OPERATION_BUDGET_MS);
  EXPECT_GE(server.calls, 2u);
}

TEST(SyncRetry, NoAttemptStartsAfterBudgetIsSpent) {
  FakeClock clock;
  clock.now = syncretry::OPERATION_BUDGET_MS;
  Server server{{{R::Ok, false}}};
  EXPECT_EQ(runAgainst(server, clock), R::Network);
  EXPECT_EQ(server.calls, 0u);
}

TEST(SyncRetry, NoAttemptIsGivenMoreTimeThanRemains) {
  FakeClock clock;
  Server server{{{R::Network, true}}};
  runAgainst(server, clock);
  uint32_t used = 0;
  for (const uint32_t t : server.timeouts) used += t;
  EXPECT_LE(used, syncretry::OPERATION_BUDGET_MS);
}

TEST(SyncRetry, BudgetCountsFromOperationStartAcrossMillisRollover) {
  const uint32_t startedAt = 0xFFFFFFFFu - 999;  // 1 s before rollover
  EXPECT_EQ(syncretry::remainingMs(startedAt + 1000, startedAt), syncretry::OPERATION_BUDGET_MS - 1000);
  EXPECT_EQ(syncretry::remainingMs(startedAt + syncretry::OPERATION_BUDGET_MS, startedAt), 0u);
}

TEST(SyncRetry, SlowestWifiJoinStillLeavesTimeForLookupsAndUpload) {
  // Wi-Fi may use its whole background budget; the sync must then still fit
  // two lookups and the upload, plus one retry of a lost request.
  const uint32_t left = syncretry::OPERATION_BUDGET_MS - AutomaticWifiConnectionPolicy::BACKGROUND_TIMEOUT_MS;
  EXPECT_GE(left, 3 * 500u + syncretry::ATTEMPT_TIMEOUT_MS + syncretry::RETRY_DELAY_MS + syncretry::MIN_ATTEMPT_MS);
}
