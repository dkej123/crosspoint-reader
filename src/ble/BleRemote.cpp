#include "BleRemote.h"

#if FREEINK_CAP_BLE_HID_HOST

#include <BleKeyboardHost.h>
#include <Logging.h>

#include "CrossPointSettings.h"

namespace bleremote {
namespace {

bool readerWants = false;
bool pairingWants = false;
int8_t pendingTurn = 0;

bool wanted() { return pairingWants || (readerWants && SETTINGS.bleRemoteEnabled && SETTINGS.bleRemoteAddr[0]); }

int8_t turnForKey(const freeink::KeyEvent& ev) {
  switch (ev.special) {
    case freeink::SpecialKey::PageDown:
    case freeink::SpecialKey::Right:
    case freeink::SpecialKey::Down:
    case freeink::SpecialKey::Enter:
      return 1;
    case freeink::SpecialKey::PageUp:
    case freeink::SpecialKey::Left:
    case freeink::SpecialKey::Up:
    case freeink::SpecialKey::Backspace:
      return -1;
    default:
      break;
  }
  if (ev.ch == ' ' || ev.ch == 'n' || ev.ch == 'j') return 1;
  if (ev.ch == 'p' || ev.ch == 'k') return -1;
  return 0;
}

}  // namespace

bool available() { return true; }

void startForReader() { readerWants = true; }

void stop() {
  readerWants = false;
  pendingTurn = 0;
}

void beginPairing() { pairingWants = true; }

void endPairing() { pairingWants = false; }

void poll() {
  auto& hid = BleHid;
  const bool want = wanted();

  if (!want) {
    // end() refuses to tear down while a NimBLE wait or disconnect is in
    // flight; keep retrying on later frames until the stack is down.
    if (hid.isRunning() || hid.isStopping()) {
      if (hid.end(0)) LOG_INF("BLER", "BLE remote stack stopped");
    }
    pendingTurn = 0;
    return;
  }

  if (!hid.isRunning()) {
    if (hid.isStopping()) {
      hid.end(0);
      return;
    }
    if (!hid.begin("CrossPoint")) {
      LOG_ERR("BLER", "BLE begin failed");
      return;
    }
    LOG_INF("BLER", "BLE remote stack started (reader=%d pairing=%d)", readerWants, pairingWants);
    if (readerWants && !pairingWants && SETTINGS.bleRemoteAddr[0]) {
      hid.armSelectedPeerReconnect(SETTINGS.bleRemoteAddr);
    }
  }

  hid.poll();
  if (pairingWants) return;  // the pairing screen consumes keys itself

  int8_t turn = 0;
  bool decodedKey = false;
  freeink::KeyEvent ev;
  while (hid.popKey(ev)) {
    const int8_t t = turnForKey(ev);
    if (t != 0) {
      turn = t;
      decodedKey = true;
    }
  }
  // Cheap "TikTok"/camera-shutter remotes send consumer-control usages the
  // keyboard decode ignores; any such undecoded press turns forward.
  freeink::RawButtonEvent raw;
  bool rawPress = false;
  while (hid.popRawButton(raw)) {
    if (raw.pressed) rawPress = true;
  }
  if (!decodedKey && rawPress) turn = 1;
  if (turn != 0) pendingTurn = turn;
}

int8_t takePageTurn() {
  const int8_t t = pendingTurn;
  pendingTurn = 0;
  return t;
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
}  // namespace bleremote

#endif
