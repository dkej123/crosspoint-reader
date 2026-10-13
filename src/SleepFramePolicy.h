#pragma once

// What to do with the Quick Resume frame file (/.crosspoint/sleep_frame.bin)
// when committing to deep sleep. Quick Resume wake restores that frame, so it
// must exist after every Quick Resume sleep — including a deferred sleep whose
// frame was already saved when the sleep screen was shown — and must not
// linger when another sleep screen mode is active.
struct SleepFrameAction {
  bool saveFrame;    // write the current framebuffer to the frame file now
  bool removeStale;  // delete an existing frame file
};

// quickResume: Quick Resume applies to this sleep.
// frameAlreadySaved: a deferred sleep already rendered + saved the frame.
constexpr SleepFrameAction sleepFrameAction(const bool quickResume, const bool frameAlreadySaved) {
  if (!quickResume) return {false, true};
  return {!frameAlreadySaved, false};
}
