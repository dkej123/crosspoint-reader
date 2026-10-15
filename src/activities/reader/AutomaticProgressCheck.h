#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "AutomaticProgressCheckJob.h"
#include "KOReaderSyncClient.h"

/**
 * Runs an automatic KOReader progress check (pull) in a background FreeRTOS
 * task so the reader stays responsive while the network round-trip completes.
 *
 * Only the network fetch (WiFi connect + HTTP GET) runs off the main loop. The
 * position mapping and the prompt decision stay on the main loop, where the
 * reader owns the Epub and the renderer, so there is no shared-state race with
 * the render task.
 *
 * The task is never killed: it shares an AutomaticProgressCheckJob with this
 * object and finishes on its own. Destroying this object (leaving the reader,
 * sleeping) abandons the job, after which the task makes no further WiFi
 * changes and its result is dropped.
 */
class AutomaticProgressCheck {
 public:
  enum class Status : uint8_t {
    IDLE,            // no check started
    RUNNING,         // network fetch in flight
    DONE_OK,         // remote progress fetched
    DONE_NOT_FOUND,  // server has no progress for this document
    DONE_ERROR,      // network / auth / other failure
  };

  AutomaticProgressCheck() = default;
  ~AutomaticProgressCheck() { abandon(); }

  AutomaticProgressCheck(const AutomaticProgressCheck&) = delete;
  AutomaticProgressCheck& operator=(const AutomaticProgressCheck&) = delete;

  bool isRunning() const { return status() == Status::RUNNING; }

  /**
   * Spawns the background fetch for @p epubPath. Returns false when a check is
   * already running or no KOReader credentials are configured.
   */
  bool start(const std::string& epubPath);

  /** Non-blocking status poll for the main loop. */
  Status status() const;

  /** Completed remote progress; valid only when status() == DONE_OK. */
  const KOReaderProgress& remoteProgress() const;

  KOReaderSyncClient::Error error() const;

  /** Returns the device to IDLE (call after consuming a result). */
  void reset() { abandon(); }

  /** Stop caring about a running check; it winds down on its own. */
  void abandon();

  /**
   * Fork: waits (bounded) until no check task is alive. An abandoned task can
   * still be inside WiFi.begin() or a request, and it releases the radio after
   * its fetch; a sleep-time upload must not share the radio with it.
   * Returns false on timeout.
   */
  static bool waitUntilNoTaskRunning(uint32_t timeoutMs);

 private:
  static void taskTrampoline(void* arg);
  static std::atomic<uint8_t> liveTasks_;

  std::shared_ptr<AutomaticProgressCheckJob> job_;
};
