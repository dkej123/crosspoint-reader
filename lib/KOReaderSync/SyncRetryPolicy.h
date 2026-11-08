#pragma once

#include <algorithm>
#include <cstdint>

// Fork: retries for the automatic upload at sleep. A single kosync request can
// get lost between the reader, Cloudflare and the tunnel (seen: TLS handshake
// OK, then no response until the whole background budget ran out). Each
// attempt is therefore capped well below the budget, and network/5xx failures
// are retried until the budget is spent. Failures a retry cannot fix (auth,
// bad JSON, low memory, no credentials) are returned at once.
namespace syncretry {

enum class Outcome : uint8_t { Ok, Transient, Permanent };

// Whole automatic-upload operation at sleep, Wi-Fi connect included.
constexpr uint32_t OPERATION_BUDGET_MS = 120000;
// Normal requests finish in ~0.5 s; a lost one must not eat the whole budget.
constexpr uint32_t ATTEMPT_TIMEOUT_MS = 5000;
constexpr uint32_t RETRY_DELAY_MS = 500;
// Not worth starting an attempt with less time than this left.
constexpr uint32_t MIN_ATTEMPT_MS = 1000;

inline uint32_t remainingMs(const uint32_t now, const uint32_t startedAt) {
  const uint32_t elapsed = now - startedAt;
  return elapsed >= OPERATION_BUDGET_MS ? 0 : OPERATION_BUDGET_MS - elapsed;
}

inline uint32_t attemptTimeoutMs(const uint32_t remaining) { return std::min(remaining, ATTEMPT_TIMEOUT_MS); }

// Runs attempt(timeoutMs) until it succeeds, fails permanently or time runs out.
// Clock: uint32_t remaining() (ms left in the operation budget), void wait(ms).
// classify(result) -> Outcome. Returns the last attempt's result, or
// `outOfTime` when no attempt could start.
template <class Result, class Attempt, class Classify, class Clock>
Result run(Attempt&& attempt, Classify&& classify, Clock& clock, const Result outOfTime) {
  Result last = outOfTime;
  while (true) {
    const uint32_t remaining = clock.remaining();
    if (remaining < MIN_ATTEMPT_MS) return last;
    last = attempt(attemptTimeoutMs(remaining));
    if (classify(last) != Outcome::Transient) return last;
    if (clock.remaining() < RETRY_DELAY_MS + MIN_ATTEMPT_MS) return last;
    clock.wait(RETRY_DELAY_MS);
  }
}

}  // namespace syncretry
