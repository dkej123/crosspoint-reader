#pragma once

#include <algorithm>
#include <cstdint>

class AutomaticWifiConnectionPolicy {
 public:
  // Fork: the user prefers a long wait at sleep over a missed sync. Saved
  // networks are retried in rounds until this runs out (WifiSelectionActivity).
  static constexpr uint32_t BACKGROUND_TIMEOUT_MS = 100000;
  // Joins take 4-5 s at -76 dBm and sometimes longer; 6 s was too short.
  static constexpr uint32_t FALLBACK_ATTEMPT_TIMEOUT_MS = 10000;
  // A join that has not finished by now is stuck; a fresh attempt does better.
  static constexpr uint32_t ATTEMPT_TIMEOUT_MS = 15000;
  // Pause between rounds so an absent network does not spin the radio.
  static constexpr uint32_t ROUND_PAUSE_MS = 1000;

  static bool backgroundDeadlineExpired(const uint32_t now, const uint32_t startedAt) {
    return remainingBackgroundTimeMs(now, startedAt) == 0;
  }

  static uint32_t remainingBackgroundTimeMs(const uint32_t now, const uint32_t startedAt) {
    const uint32_t elapsed = static_cast<uint32_t>(now - startedAt);
    return elapsed >= BACKGROUND_TIMEOUT_MS ? 0 : BACKGROUND_TIMEOUT_MS - elapsed;
  }

  // Whether another round over the saved networks still fits.
  static bool canStartAnotherRound(const uint32_t now, const uint32_t startedAt) {
    return remainingBackgroundTimeMs(now, startedAt) > ROUND_PAUSE_MS + 2000;
  }

  static uint32_t connectionAttemptTimeoutMs(const uint32_t now, const uint32_t startedAt,
                                             const bool hasFallbackNetwork) {
    const uint32_t remaining = remainingBackgroundTimeMs(now, startedAt);
    return std::min(remaining, hasFallbackNetwork ? FALLBACK_ATTEMPT_TIMEOUT_MS : ATTEMPT_TIMEOUT_MS);
  }
};
