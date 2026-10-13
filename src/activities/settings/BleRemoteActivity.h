#pragma once

#include <string>

#include "activities/UiListActivity.h"

// Settings -> Controls -> Pair Bluetooth Remote (fork feature).
// Scans for BLE HID devices, connects/bonds the chosen one and stores its
// address as the reader's page-turner. The BLE stack runs only while this
// screen is open (bleremote::beginPairing/endPairing).
class BleRemoteActivity final : public UiListActivity {
 public:
  BleRemoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  static constexpr int FIXED_ROWS = 2;  // status/rescan, forget
  static constexpr int MAX_DEVICES = 24;
  static constexpr int MAX_ROWS = FIXED_ROWS + MAX_DEVICES;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void startScan();

  bool scanRequested_ = false;
  bool scanStarted_ = false;
  std::string connectingAddr_;
  std::string status_;
  unsigned long lastRefreshMs_ = 0;
  uint8_t lastDeviceCount_ = 0;

  std::string rowLabels_[MAX_ROWS];
  std::string rowValues_[MAX_ROWS];
  freeink::ui::ListItem rowItems_[MAX_ROWS]{};
};
