#pragma once

#include <cstdint>

// Host-testable core of the BLE page-turner remote (fork feature). No Arduino
// or NimBLE dependencies: the firmware adapts freeink::BleKeyboardHost to the
// Host concept below (src/ble/BleRemote.cpp); tests drive a fake host.
namespace bleremote {

// Keys a page-turner remote can send, as decoded by the HID host.
enum class RemoteKey : uint8_t {
  Other,
  PageDown,
  PageUp,
  Right,
  Left,
  Down,
  Up,
  Enter,
  Backspace,
  Space,
};

// +1 next page, -1 previous page, 0 not a page-turn key.
constexpr int8_t pageTurnForKey(const RemoteKey key) {
  switch (key) {
    case RemoteKey::PageDown:
    case RemoteKey::Right:
    case RemoteKey::Down:
    case RemoteKey::Enter:
    case RemoteKey::Space:
      return 1;
    case RemoteKey::PageUp:
    case RemoteKey::Left:
    case RemoteKey::Up:
    case RemoteKey::Backspace:
      return -1;
    case RemoteKey::Other:
      break;
  }
  return 0;
}

struct RemoteConfig {
  bool enabled = false;              // Settings -> Controls -> Bluetooth Remote
  const char* pairedAddr = nullptr;  // empty/null = no remote paired
};

// Host concept (all calls from the main loop task):
//   bool begin();                 bring the stack up; false on failure
//   bool end(uint32_t timeoutMs); tear down; false while a teardown is still in flight
//   bool isRunning() const;  bool isStopping() const;
//   void armReconnect(const char* addr);
//   void poll();
//   bool popKey(RemoteKey& out);  // decoded keyboard keys, one per press/repeat
//   bool popRawPress();           // one per raw button press edge (any button)
template <class Host>
class RemoteController {
 public:
  explicit RemoteController(Host& host) : host_(host) {}

  void setReaderOpen(const bool open) { readerOpen_ = open; }
  void setPairing(const bool pairing) { pairing_ = pairing; }
  bool readerOpen() const { return readerOpen_; }
  bool pairing() const { return pairing_; }

  // Whether the radio should be up for this config.
  bool wanted(const RemoteConfig& cfg) const {
    return pairing_ || (readerOpen_ && cfg.enabled && cfg.pairedAddr && cfg.pairedAddr[0]);
  }

  // Once per input frame. Returns the page turn decoded this frame.
  int8_t update(const RemoteConfig& cfg) {
    if (!wanted(cfg)) {
      if (host_.isRunning() || host_.isStopping()) host_.end(0);
      drain();
      return 0;
    }
    if (!host_.isRunning()) {
      if (host_.isStopping()) {
        host_.end(0);
        return 0;
      }
      if (!host_.begin()) return 0;
      if (!pairing_ && readerOpen_) host_.armReconnect(cfg.pairedAddr);
    }
    host_.poll();
    if (pairing_) return 0;  // the pairing screen reads the host itself

    int8_t turn = 0;
    bool anyKey = false;
    RemoteKey key;
    while (host_.popKey(key)) {
      anyKey = true;
      const int8_t t = pageTurnForKey(key);
      if (t != 0) turn = t;
    }
    bool rawPress = false;
    while (host_.popRawPress()) rawPress = true;

    // Every press also arrives as a raw edge; keyboard-style remotes add a
    // decoded key, consumer-control ones (volume, shutter) do not. The two are
    // queued from the BLE task and may straddle a frame, so a raw-only press
    // waits one frame for its key before it counts as "next page".
    if (anyKey) {
      rawOnlyPending_ = false;
    } else if (rawOnlyPending_) {
      rawOnlyPending_ = false;
      turn = 1;
    }
    if (rawPress && !anyKey) rawOnlyPending_ = true;
    return turn;
  }

  // Bring the stack fully down before deep sleep. Bounded: at most `attempts`
  // calls of end(timeoutMs). Returns true when nothing is left running.
  bool shutdown(const uint32_t timeoutMs, const int attempts) {
    readerOpen_ = false;
    pairing_ = false;
    for (int i = 0; i < attempts && (host_.isRunning() || host_.isStopping()); i++) {
      host_.end(timeoutMs);
    }
    drain();
    return !host_.isRunning() && !host_.isStopping();
  }

 private:
  void drain() {
    rawOnlyPending_ = false;
    RemoteKey key;
    while (host_.popKey(key)) {
    }
    while (host_.popRawPress()) {
    }
  }

  Host& host_;
  bool readerOpen_ = false;
  bool pairing_ = false;
  bool rawOnlyPending_ = false;
};

}  // namespace bleremote
