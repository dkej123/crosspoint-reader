#include "AutomaticProgressCheck.h"

#include <Logging.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

#include "AutomaticWifiConnectionPolicy.h"
#include "KOReaderCredentialStore.h"
#include "KOReaderDocumentId.h"
#include "WifiCredentialStore.h"

namespace {

// Headless, bounded WiFi connect: try each saved credential once, then give up.
// Mirrors WifiSelectionActivity's auto-connect path but blocks (this runs in a
// dedicated task, not the activity loop). Bails out without touching the radio
// further once the job is abandoned.
bool connectHeadless(const std::atomic<bool>& abandoned) {
  if (WiFi.status() == WL_CONNECTED) return true;

  const uint32_t deadline = millis() + AutomaticWifiConnectionPolicy::BACKGROUND_TIMEOUT_MS;

  WiFi.persistent(false);  // Credentials are managed by WifiCredentialStore
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);  // Abort any SDK auto-connect and clear NVS SSID
  vTaskDelay(pdMS_TO_TICKS(100));

  for (size_t i = 0; i < WIFI_STORE.getCredentialCount(); i++) {
    if (abandoned.load(std::memory_order_acquire)) return false;
    const auto cred = WIFI_STORE.getCredentialAt(i);
    if (!cred) continue;

    const int32_t remaining = static_cast<int32_t>(deadline - millis());
    if (remaining <= 0) break;

    LOG_DBG("KOSync", "Automatic check: trying saved network %s", cred->ssid.c_str());
    if (cred->password.empty()) {
      WiFi.begin(cred->ssid.c_str());
    } else {
      WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
    }

    const uint32_t attemptTimeoutMs = std::min<uint32_t>(static_cast<uint32_t>(remaining),
                                                         AutomaticWifiConnectionPolicy::FALLBACK_ATTEMPT_TIMEOUT_MS);
    const uint32_t attemptStartedAt = millis();
    while (millis() - attemptStartedAt < attemptTimeoutMs) {
      if (WiFi.status() == WL_CONNECTED) {
        LOG_DBG("KOSync", "Automatic check: connected to %s", cred->ssid.c_str());
        return true;
      }
      if (abandoned.load(std::memory_order_acquire)) return false;
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    WiFi.disconnect();
  }
  return WiFi.status() == WL_CONNECTED;
}

struct DeviceSteps {
  const std::atomic<bool>& abandoned;
  bool connect(const std::atomic<bool>& abandoned) {
    // Load saved WiFi credentials from SD before the headless connect attempt.
    // (WifiSelectionActivity does this in onEnter(); a background check runs
    // without that activity, so it must load them itself.)
    WIFI_STORE.loadFromFile();
    const bool ok = connectHeadless(abandoned);
    if (!ok) LOG_DBG("KOSync", "Automatic check: WiFi unavailable or abandoned");
    return ok;
  }
  std::string documentHash(const std::string& epubPath) {
    const std::string hash = KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME
                                 ? KOReaderDocumentId::calculateFromFilename(epubPath)
                                 : KOReaderDocumentId::calculate(epubPath);
    if (hash.empty()) LOG_ERR("KOSync", "Automatic check: document hash failed");
    return hash;
  }
  KOReaderSyncClient::Error fetch(const std::string& hash, KOReaderProgress& out) {
    // Abandoned (reader left / sleep) ends the request within one poll.
    const auto result = KOReaderSyncClient::getProgress(hash, out, KOReaderSyncClient::DEFAULT_REQUEST_TIMEOUT_MS,
                                                        [this]() { return abandoned.load(std::memory_order_acquire); });
    if (result != KOReaderSyncClient::OK && result != KOReaderSyncClient::NOT_FOUND) {
      LOG_DBG("KOSync", "Automatic check: getProgress failed (%d)", static_cast<int>(result));
    }
    return result;
  }
  // Drop the radio; the reader resumes normal low-power operation.
  void releaseRadio() { WiFi.disconnect(true, false); }
};

}  // namespace

std::atomic<uint8_t> AutomaticProgressCheck::liveTasks_{0};

bool AutomaticProgressCheck::waitUntilNoTaskRunning(const uint32_t timeoutMs) {
  const uint32_t startedAt = millis();
  while (liveTasks_.load(std::memory_order_acquire) != 0) {
    if (millis() - startedAt >= timeoutMs) {
      LOG_ERR("KOSync", "Automatic check task still running after %lu ms", static_cast<unsigned long>(timeoutMs));
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return true;
}

void AutomaticProgressCheck::abandon() {
  if (job_) job_->abandoned.store(true, std::memory_order_release);
  job_.reset();
}

AutomaticProgressCheck::Status AutomaticProgressCheck::status() const {
  if (!job_) return Status::IDLE;
  switch (job_->status.load(std::memory_order_acquire)) {
    case AutomaticProgressCheckJob::Status::RUNNING:
      return Status::RUNNING;
    case AutomaticProgressCheckJob::Status::DONE_OK:
      return Status::DONE_OK;
    case AutomaticProgressCheckJob::Status::DONE_NOT_FOUND:
      return Status::DONE_NOT_FOUND;
    case AutomaticProgressCheckJob::Status::DONE_ERROR:
      break;
  }
  return Status::DONE_ERROR;
}

const KOReaderProgress& AutomaticProgressCheck::remoteProgress() const {
  static const KOReaderProgress kEmpty{};
  // Only read after status() reported DONE_OK (acquire), when the task is done writing.
  return job_ ? job_->remoteProgress : kEmpty;
}

KOReaderSyncClient::Error AutomaticProgressCheck::error() const { return job_ ? job_->error : KOReaderSyncClient::OK; }

bool AutomaticProgressCheck::start(const std::string& epubPath) {
  if (isRunning()) return false;
  if (!KOREADER_STORE.hasCredentials()) return false;

  auto job = std::make_shared<AutomaticProgressCheckJob>();
  job->epubPath = epubPath;
  // The task holds its own reference so the job outlives this object if the
  // reader is destroyed while the fetch is still in flight.
  auto* taskRef = new (std::nothrow) std::shared_ptr<AutomaticProgressCheckJob>(job);
  if (!taskRef) return false;

  liveTasks_.fetch_add(1, std::memory_order_acq_rel);
  if (xTaskCreatePinnedToCore(&taskTrampoline, "AutoProgressCheck", 8192, taskRef, 1, nullptr, 0) != pdPASS) {
    LOG_ERR("KOSync", "Failed to create automatic progress check task");
    liveTasks_.fetch_sub(1, std::memory_order_acq_rel);
    delete taskRef;
    return false;
  }
  job_ = std::move(job);
  return true;
}

void AutomaticProgressCheck::taskTrampoline(void* arg) {
  auto* ref = static_cast<std::shared_ptr<AutomaticProgressCheckJob>*>(arg);
  {
    DeviceSteps steps{(*ref)->abandoned};
    runAutomaticProgressCheck(**ref, steps);
  }
  delete ref;  // may free the job if the reader already let go
  liveTasks_.fetch_sub(1, std::memory_order_acq_rel);
  vTaskDelete(nullptr);  // never touches the job after this point
}
