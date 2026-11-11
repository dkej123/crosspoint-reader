// RadioPort over the SDK's BleKeyboardHost, with the radio start on a FreeRTOS task. The only
// file of the module that includes the SDK or the chip's headers.
#if defined(FREEINK_CAP_BLE_HID_HOST) && FREEINK_CAP_BLE_HID_HOST

#include <BleKeyboardHost.h>
#include <NimBLEDevice.h>
#include <esp_attr.h>
#include <esp_timer.h>

#include <atomic>
#include <cstring>

#include "RadioPort.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace bleturner::port {
namespace {

// Looked up once: every call below is then a plain member call.
freeink::BleKeyboardHost& radioHost = freeink::BleKeyboardHost::getInstance();
freeink::BleKeyboardHost& hid() { return radioHost; }

// Outlives ESP.restart: the one heap restart already spent since the radio last came up.
RTC_NOINIT_ATTR Memo memo;

#ifdef BLE_PAGE_TURNER_STACK_PROBE
std::atomic<uint32_t> startStackMinimum{0};
#endif

void startTaskEntry(void*) {
  detail::startTask();
#ifdef BLE_PAGE_TURNER_STACK_PROBE
  startStackMinimum.store(uxTaskGetStackHighWaterMark(nullptr), std::memory_order_release);
#endif
  vTaskDelete(nullptr);
}

Peer peerOf(const char* addr, const char* name) { return Peer{addr, name}; }

// Slave latency for our own battery remote (xteink_bt_remote in the Embeded monorepo).
//
// Why: the SDK connects with setConnectionParams(12, 24, 0, 800) = 15-30 ms, latency 0
// (BleKeyboardHost.cpp, commit ea75c71 "Increase BLE link supervision timeout to 8 seconds").
// That commit was about the 8 s timeout (long chapter renders block the core and dropped the
// link); latency 0 is just NimBLE's default, never discussed. With it the remote must answer
// every event, 30-60 radio wake-ups a second while idle: 1-3 mA, ~2.5 weeks on 1000 mAh.
//
// What latency 30 changes: the remote may skip up to 30 quiet events (wakes every ~0.5-0.9 s).
// It is an allowance, not a rule: the reader still comes to every event, so a key press goes
// out at the very next one, 15-30 ms as before. Only reader-to-remote traffic slows down
// (keyboard LEDs), which a page turner does not use. Timeout 8 s vs ~0.9 s worst gap: ~9x.
// The reader's own consumption is unchanged.
//
// Why not via the remote: the SDK rejects every peripheral update request
// (onConnParamsUpdateRequest -> false, commit 2e71363: some keyboards drop the link after
// such a negotiation). So the central asks itself (an LL connection update, not covered by that
// rule), 2 s after the link came up so encryption and subscription have settled, and only for
// this remote by name, so other keyboards keep the SDK's tested behaviour.
//
// The same one-number change upstream (latency 0 -> 30 in setConnectionParams) would help every
// battery remote; kept here so freeink-sdk stays an unmodified submodule.
constexpr const char* kLowPowerRemoteName = "XteinkRemote";
constexpr uint16_t kLowPowerLatency = 30;
constexpr uint32_t kLatencyAskDelayMs = 2000;
uint32_t linkUpAtMs = 0;
bool linkSeen = false;
bool latencyAsked = false;

void askRemoteLatency() {
  if (!hid().isConnected()) {
    linkSeen = latencyAsked = false;
    return;
  }
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  if (!linkSeen) {
    linkSeen = true;
    linkUpAtMs = now;
    return;
  }
  if (latencyAsked || now - linkUpAtMs < kLatencyAskDelayMs) return;
  latencyAsked = true;
  const char* name = hid().connectedName();
  if (name == nullptr || strcmp(name, kLowPowerRemoteName) != 0) return;
  for (NimBLEClient* client : NimBLEDevice::getConnectedClients()) {
    client->updateConnParams(/*minInterval=*/12, /*maxInterval=*/24, kLowPowerLatency, /*timeout=*/800);
  }
}

}  // namespace

bool compiledIn() { return true; }
bool begin() { return hid().begin("FreeInk"); }
bool end(const uint32_t timeoutMs) { return hid().end(timeoutMs); }
bool running() { return hid().isRunning(); }
bool stopping() { return hid().isStopping(); }
bool connected() { return hid().isConnected(); }
bool connecting() { return hid().isConnecting(); }
// The SDK reports no advertisement count: a connection attempt in flight is the only activity.
ConnectionActivity connectionActivity() { return {hid().isConnecting(), 0}; }
bool scanning() { return hid().isScanning(); }
void poll() {
  hid().poll();
  askRemoteLatency();
}

bool popRaw(RawEdge& out) {
  freeink::RawButtonEvent ev;
  if (!hid().popRawButton(ev)) return false;
  out.reportId = ev.reportId;
  out.byteIndex = ev.byteIndex;
  out.value = ev.value;
  out.pressed = ev.pressed;
  out.keycode = ev.keycode;
  out.mods = ev.mods;
  out.wasRest = ev.wasRest;
  out.atMs = ev.atMs;
  return true;
}

bool popKey(KeyPress& out) {
  freeink::KeyEvent ev;
  if (!hid().popKey(ev)) return false;
  out.keycode = ev.keycode;
  out.mods = ev.mods;
  out.pressed = ev.pressed;
  return true;
}

// The SDK reconnects one selected remote, so the pick policy cannot ask for "the first bonded
// remote to come up": the chosen remote is armed, and nothing when none is chosen.
bool armBondedReconnect(uint8_t, const char* priorityAddr) {
  return priorityAddr != nullptr && priorityAddr[0] != '\0' && hid().armSelectedPeerReconnect(priorityAddr);
}
Peer linked() { return peerOf(hid().connectedAddr(), hid().connectedName()); }

void scan(const uint32_t durationMs) {
  if (durationMs == 0) {
    hid().stopScan();
  } else {
    hid().startScan(durationMs);
  }
}

bool connect(const char* addr) { return hid().connect(addr); }
void disconnect() { hid().disconnect(); }
void forget(const char* addr) { hid().forget(addr); }
bool takeConnectFailure(char* out, const size_t outLen) { return hid().takeConnectFailure(out, outLen); }
uint8_t bondCount() { return hid().pairedCount(); }
Peer bond(const uint8_t index) { return peerOf(hid().paired(index).addr, hid().paired(index).name); }
uint8_t foundCount() { return hid().deviceCount(); }
Peer found(const uint8_t index) { return peerOf(hid().device(index).addr, hid().device(index).name); }

bool spawnStart() { return xTaskCreate(startTaskEntry, "ble-start", 3072, nullptr, 2, nullptr) == pdPASS; }
uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }
void sleepMs(const uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
Memo& restartMemo() { return memo; }

#ifdef BLE_PAGE_TURNER_PROBE
void inject(const uint8_t* frame, const size_t len) { hid().onReportIngest(frame, len); }
unsigned rawOverflows() { return hid().rawOverflows(); }
#endif
#ifdef BLE_PAGE_TURNER_STACK_PROBE
uint32_t startStackLeft() { return startStackMinimum.load(std::memory_order_acquire); }
#endif

}  // namespace bleturner::port

#endif
