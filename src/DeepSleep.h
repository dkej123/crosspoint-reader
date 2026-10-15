#pragma once

// Starts deep sleep immediately unless the current activity defers it for
// required pre-sleep work.
void enterDeepSleep(bool fromTimeout = false);

// Completes a sleep request previously deferred by Activity::prepareDeferredSleep().
void completeDeferredDeepSleep(bool fromTimeout);

// Fork: true once a fresh power press asked to wake a deferred sleep. Blocking
// sync waits poll it, since they do not return to the main loop while running.
bool pollDeferredSleepWake();
