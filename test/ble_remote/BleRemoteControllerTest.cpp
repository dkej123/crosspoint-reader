// Requirements for the BLE page-turner remote (fork feature), checked against
// a fake HID host:
//  - the radio runs only while a book is open, the Bluetooth Remote toggle is
//    on and a remote is paired; the pairing screen may run it on its own;
//  - leaving the book or switching the toggle off shuts it down, even when the
//    host needs several attempts to stop;
//  - before deep sleep the stack is brought fully down within a bounded time;
//  - standard page-turner keys turn pages, and one physical press turns
//    exactly one page.
#include <gtest/gtest.h>

#include <deque>
#include <string>
#include <vector>

#include "src/ble/BleRemotePolicy.h"

using bleremote::RemoteConfig;
using bleremote::RemoteController;
using bleremote::RemoteKey;

namespace {

struct FakeHost {
  bool running = false;
  bool stopping = false;
  bool beginFails = false;
  int refuseEnds = 0;  // end() calls that leave the host "stopping"
  int beginCalls = 0;
  int endCalls = 0;
  std::vector<std::string> armed;
  std::deque<RemoteKey> keys;
  int rawPresses = 0;

  bool begin() {
    beginCalls++;
    if (beginFails) return false;
    running = true;
    return true;
  }
  bool end(uint32_t) {
    endCalls++;
    running = false;
    if (refuseEnds > 0) {
      refuseEnds--;
      stopping = true;
      return false;
    }
    stopping = false;
    return true;
  }
  bool isRunning() const { return running; }
  bool isStopping() const { return stopping; }
  void armReconnect(const char* addr) { armed.emplace_back(addr ? addr : ""); }
  void poll() {}
  bool popKey(RemoteKey& out) {
    if (keys.empty()) return false;
    out = keys.front();
    keys.pop_front();
    return true;
  }
  bool popRawPress() {
    if (rawPresses == 0) return false;
    rawPresses--;
    return true;
  }
  // One physical press of a keyboard-style remote: a decoded key plus its raw edge.
  void pressKey(RemoteKey k) {
    keys.push_back(k);
    rawPresses++;
  }
};

constexpr const char* kAddr = "AA:BB:CC:DD:EE:FF";
const RemoteConfig kOnPaired{true, kAddr};

class BleRemoteTest : public ::testing::Test {
 protected:
  FakeHost host;
  RemoteController<FakeHost> remote{host};

  // Frames until the stack is up (one update normally suffices).
  void openBook(const RemoteConfig& cfg = kOnPaired) {
    remote.setReaderOpen(true);
    remote.update(cfg);
  }
  int sumTurns(int frames, const RemoteConfig& cfg = kOnPaired) {
    int sum = 0;
    for (int i = 0; i < frames; i++) sum += remote.update(cfg);
    return sum;
  }
};

// --- when the radio may run ------------------------------------------------

TEST_F(BleRemoteTest, RadioStaysOffOutsideABook) {
  for (int i = 0; i < 5; i++) remote.update(kOnPaired);
  EXPECT_EQ(host.beginCalls, 0);
  EXPECT_FALSE(host.running);
}

TEST_F(BleRemoteTest, RadioStartsWhenBookOpenToggleOnAndPaired) {
  openBook();
  EXPECT_TRUE(host.running);
  ASSERT_EQ(host.armed.size(), 1u);
  EXPECT_EQ(host.armed[0], kAddr);  // reconnects to the remote the user paired
}

TEST_F(BleRemoteTest, RadioStaysOffWhenToggleIsOff) {
  openBook(RemoteConfig{false, kAddr});
  EXPECT_EQ(host.beginCalls, 0);
}

TEST_F(BleRemoteTest, RadioStaysOffWithoutAPairedRemote) {
  openBook(RemoteConfig{true, ""});
  remote.update(RemoteConfig{true, nullptr});
  EXPECT_EQ(host.beginCalls, 0);
}

TEST_F(BleRemoteTest, LeavingTheBookStopsTheRadio) {
  openBook();
  remote.setReaderOpen(false);
  remote.update(kOnPaired);
  EXPECT_FALSE(host.running);
  EXPECT_FALSE(host.stopping);
}

TEST_F(BleRemoteTest, SwitchingToggleOffWhileReadingStopsTheRadio) {
  openBook();
  remote.update(RemoteConfig{false, kAddr});
  EXPECT_FALSE(host.running);
}

TEST_F(BleRemoteTest, StopKeepsRetryingUntilTheHostIsDown) {
  openBook();
  host.refuseEnds = 3;
  remote.setReaderOpen(false);
  for (int i = 0; i < 10; i++) remote.update(kOnPaired);
  EXPECT_FALSE(host.running);
  EXPECT_FALSE(host.stopping);
}

TEST_F(BleRemoteTest, ReopeningWhileStoppingWaitsForTeardownBeforeRestart) {
  openBook();
  host.refuseEnds = 1;
  remote.setReaderOpen(false);
  remote.update(kOnPaired);  // end() refused -> stopping
  ASSERT_TRUE(host.stopping);
  remote.setReaderOpen(true);
  remote.update(kOnPaired);  // must finish teardown, not begin() on top of it
  EXPECT_EQ(host.beginCalls, 1);
  remote.update(kOnPaired);  // now it may start again
  EXPECT_TRUE(host.running);
  EXPECT_EQ(host.beginCalls, 2);
}

TEST_F(BleRemoteTest, FailedStartIsRetriedOnALaterFrame) {
  host.beginFails = true;
  openBook();
  EXPECT_FALSE(host.running);
  host.beginFails = false;
  remote.update(kOnPaired);
  EXPECT_TRUE(host.running);
}

TEST_F(BleRemoteTest, PairingScreenRunsTheRadioWithoutBookOrPairedRemote) {
  remote.setPairing(true);
  remote.update(RemoteConfig{false, ""});
  EXPECT_TRUE(host.running);
  EXPECT_TRUE(host.armed.empty());  // pairing must not reconnect the old remote
  remote.setPairing(false);
  remote.update(RemoteConfig{false, ""});
  EXPECT_FALSE(host.running);
}

TEST_F(BleRemoteTest, PairingScreenDoesNotTurnPages) {
  remote.setPairing(true);
  remote.update(kOnPaired);
  host.pressKey(RemoteKey::PageDown);
  EXPECT_EQ(sumTurns(3), 0);
}

// --- deep sleep -------------------------------------------------------------

TEST_F(BleRemoteTest, ShutdownForSleepBringsStackFullyDown) {
  openBook();
  host.refuseEnds = 2;
  EXPECT_TRUE(remote.shutdown(500, 4));
  EXPECT_FALSE(host.running);
  EXPECT_FALSE(host.stopping);
}

TEST_F(BleRemoteTest, ShutdownForSleepIsBoundedWhenHostNeverStops) {
  openBook();
  host.refuseEnds = 1000;
  EXPECT_FALSE(remote.shutdown(500, 4));
  EXPECT_LE(host.endCalls, 4);
}

TEST_F(BleRemoteTest, ShutdownWhenNothingRunsIsANoOp) {
  EXPECT_TRUE(remote.shutdown(500, 4));
  EXPECT_EQ(host.endCalls, 0);
}

// --- page turns ---------------------------------------------------------------

TEST_F(BleRemoteTest, ForwardKeysTurnToNextPage) {
  for (RemoteKey k : {RemoteKey::PageDown, RemoteKey::Right, RemoteKey::Down, RemoteKey::Enter, RemoteKey::Space}) {
    openBook();
    host.pressKey(k);
    EXPECT_EQ(sumTurns(3), 1) << static_cast<int>(k);
  }
}

TEST_F(BleRemoteTest, BackwardKeysTurnToPreviousPage) {
  for (RemoteKey k : {RemoteKey::PageUp, RemoteKey::Left, RemoteKey::Up, RemoteKey::Backspace}) {
    openBook();
    host.pressKey(k);
    EXPECT_EQ(sumTurns(3), -1) << static_cast<int>(k);
  }
}

TEST_F(BleRemoteTest, UnrelatedKeyboardKeyDoesNotTurn) {
  openBook();
  host.pressKey(RemoteKey::Other);
  EXPECT_EQ(sumTurns(3), 0);
}

TEST_F(BleRemoteTest, ConsumerControlButtonTurnsForwardExactlyOnce) {
  openBook();
  host.rawPresses = 1;  // volume/shutter remote: raw edge, no decodable key
  EXPECT_EQ(sumTurns(4), 1);
}

TEST_F(BleRemoteTest, KeyAndItsRawEdgeSplitAcrossFramesTurnOnce) {
  openBook();
  host.rawPresses = 1;  // raw edge lands in this frame...
  int sum = remote.update(kOnPaired);
  host.keys.push_back(RemoteKey::PageDown);  // ...its decoded key in the next
  sum += sumTurns(3);
  EXPECT_EQ(sum, 1);
}

TEST_F(BleRemoteTest, TwoSeparatePressesTurnTwice) {
  openBook();
  host.pressKey(RemoteKey::PageDown);
  int sum = sumTurns(2);
  host.pressKey(RemoteKey::PageDown);
  sum += sumTurns(2);
  EXPECT_EQ(sum, 2);

  host.rawPresses = 1;
  sum = sumTurns(2);
  host.rawPresses = 1;
  sum += sumTurns(2);
  EXPECT_EQ(sum, 2);
}

TEST_F(BleRemoteTest, PressesQueuedAfterLeavingTheBookAreNotReplayedLater) {
  openBook();
  remote.setReaderOpen(false);
  host.pressKey(RemoteKey::PageDown);  // arrives while the reader is gone
  remote.update(kOnPaired);
  openBook();
  EXPECT_EQ(sumTurns(3), 0);
}

}  // namespace
