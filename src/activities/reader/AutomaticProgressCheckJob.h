#pragma once

#include <atomic>
#include <string>
#include <utility>

#include "KOReaderSyncClient.h"

// State of one background automatic progress check (pull), shared between the
// reader that started it and the FreeRTOS task running it. The task owns a
// reference and finishes on its own; the reader never kills it. Leaving the
// reader only marks the job abandoned, after which the task makes no further
// WiFi changes (an automatic upload may be using the radio by then) and its
// result is ignored.
struct AutomaticProgressCheckJob {
  enum class Status : uint8_t { RUNNING, DONE_OK, DONE_NOT_FOUND, DONE_ERROR };

  std::atomic<Status> status{Status::RUNNING};
  std::atomic<bool> abandoned{false};
  std::string epubPath;
  KOReaderProgress remoteProgress;
  KOReaderSyncClient::Error error = KOReaderSyncClient::OK;
};

// The check's steps, in order, with abandonment honoured between them.
// Steps concept:
//   bool connect(const std::atomic<bool>& abandoned);  // may bail out early when abandoned
//   std::string documentHash(const std::string& epubPath);
//   KOReaderSyncClient::Error fetch(const std::string& hash, KOReaderProgress& out);
//   void releaseRadio();
template <class Steps>
void runAutomaticProgressCheck(AutomaticProgressCheckJob& job, Steps& steps) {
  using Status = AutomaticProgressCheckJob::Status;
  const auto finish = [&](const Status s, const KOReaderSyncClient::Error e) {
    job.error = e;
    job.status.store(s, std::memory_order_release);
  };

  if (job.abandoned.load(std::memory_order_acquire))
    return finish(Status::DONE_ERROR, KOReaderSyncClient::NETWORK_ERROR);
  if (!steps.connect(job.abandoned)) return finish(Status::DONE_ERROR, KOReaderSyncClient::NETWORK_ERROR);
  if (job.abandoned.load(std::memory_order_acquire))
    return finish(Status::DONE_ERROR, KOReaderSyncClient::NETWORK_ERROR);

  const std::string hash = steps.documentHash(job.epubPath);
  if (hash.empty() || job.abandoned.load(std::memory_order_acquire)) {
    if (!job.abandoned.load(std::memory_order_acquire)) steps.releaseRadio();
    return finish(Status::DONE_ERROR, KOReaderSyncClient::NETWORK_ERROR);
  }

  KOReaderProgress progress;
  const auto result = steps.fetch(hash, progress);
  // An abandoned job leaves the radio alone: whoever runs next owns it.
  if (job.abandoned.load(std::memory_order_acquire)) return finish(Status::DONE_ERROR, result);
  steps.releaseRadio();

  if (result == KOReaderSyncClient::OK) {
    job.remoteProgress = std::move(progress);
    return finish(Status::DONE_OK, result);
  }
  if (result == KOReaderSyncClient::NOT_FOUND) return finish(Status::DONE_NOT_FOUND, result);
  return finish(Status::DONE_ERROR, result);
}
