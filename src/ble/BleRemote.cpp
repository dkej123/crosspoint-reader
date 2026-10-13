#include "BleRemote.h"

#if FREEINK_CAP_BLE_HID_HOST

#include <BleKeyboardHost.h>
#include <Logging.h>

#include "BleRemotePolicy.h"
#include "CrossPointSettings.h"

namespace bleremote {
namespace {

RemoteKey toRemoteKey(const freeink::KeyEvent& ev) {
  switch (ev.special) {
    case freeink::SpecialKey::PageDown:
      return RemoteKey::PageDown;
    case freeink::SpecialKey::PageUp:
      return RemoteKey::PageUp;
    case freeink::SpecialKey::Right:
      return RemoteKey::Right;
    case freeink::SpecialKey::Left:
      return RemoteKey::Left;
    case freeink::SpecialKey::Down:
      return RemoteKey::Down;
    case freeink::SpecialKey::Up:
      return RemoteKey::Up;
    case freeink::SpecialKey::Enter:
      return RemoteKey::Enter;
    case freeink::SpecialKey::Backspace:
      return RemoteKey::Backspace;
    default:
      break;
  }
  return ev.ch == ' ' ? RemoteKey::Space : RemoteKey::Other;
}

// Adapts the SDK singleton to RemoteController's Host concept.
struct SdkHost {
  bool begin() {
    const bool ok = BleHid.begin("CrossPoint");
    if (ok) {
      LOG_INF("BLER", "BLE remote stack started");
    } else {
      LOG_ERR("BLER", "BLE begin failed");
    }
    return ok;
  }
  bool end(const uint32_t timeoutMs) {
    const bool done = BleHid.end(timeoutMs);
    if (done) LOG_INF("BLER", "BLE remote stack stopped");
    return done;
  }
  bool isRunning() const { return BleHid.isRunning(); }
  bool isStopping() const { return BleHid.isStopping(); }
  void armReconnect(const char* addr) { BleHid.armSelectedPeerReconnect(addr); }
  void poll() { BleHid.poll(); }
  bool popKey(RemoteKey& out) {
    freeink::KeyEvent ev;
    if (!BleHid.popKey(ev)) return false;
    out = toRemoteKey(ev);
    return true;
  }
  bool popRawPress() {
    freeink::RawButtonEvent raw;
    while (BleHid.popRawButton(raw)) {
      if (raw.pressed) return true;
    }
    return false;
  }
};

SdkHost host;
RemoteController<SdkHost> controller(host);
int8_t pendingTurn = 0;

RemoteConfig currentConfig() {
  RemoteConfig cfg;
  cfg.enabled = SETTINGS.bleRemoteEnabled != 0;
  cfg.pairedAddr = SETTINGS.bleRemoteAddr;
  return cfg;
}

}  // namespace

bool available() { return true; }

void startForReader() { controller.setReaderOpen(true); }

void stop() {
  controller.setReaderOpen(false);
  pendingTurn = 0;
}

void beginPairing() { controller.setPairing(true); }

void endPairing() { controller.setPairing(false); }

void poll() {
  const int8_t turn = controller.update(currentConfig());
  if (turn != 0) pendingTurn = turn;
}

int8_t takePageTurn() {
  const int8_t t = pendingTurn;
  pendingTurn = 0;
  return t;
}

bool shutdownForSleep() {
  pendingTurn = 0;
  const bool down = controller.shutdown(/*timeoutMs=*/500, /*attempts=*/4);
  if (!down) LOG_ERR("BLER", "BLE stack still up entering sleep");
  return down;
}

}  // namespace bleremote

#else  // !FREEINK_CAP_BLE_HID_HOST

namespace bleremote {
bool available() { return false; }
void startForReader() {}
void stop() {}
void beginPairing() {}
void endPairing() {}
void poll() {}
int8_t takePageTurn() { return 0; }
bool shutdownForSleep() { return true; }
}  // namespace bleremote

#endif
