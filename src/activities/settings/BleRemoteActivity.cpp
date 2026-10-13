#include "BleRemoteActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ble/BleRemote.h"
#include "components/UITheme.h"

#if FREEINK_CAP_BLE_HID_HOST
#include <BleKeyboardHost.h>
#endif

namespace fui = freeink::ui;

namespace {
constexpr int IDX_STATUS = 0;
constexpr int IDX_FORGET = 1;
constexpr unsigned long REFRESH_MS = 1000;
constexpr uint32_t SCAN_MS = 10000;
}  // namespace

BleRemoteActivity::BleRemoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BleRemote", renderer, mappedInput) {}

const char* BleRemoteActivity::headerTitle() const { return tr(STR_BLE_REMOTE_PAIR); }

void BleRemoteActivity::onEnter() {
  bleremote::beginPairing();
  scanRequested_ = true;
  status_ = tr(STR_BLE_REMOTE_SCANNING);
  UiListActivity::onEnter();
}

void BleRemoteActivity::onExit() {
#if FREEINK_CAP_BLE_HID_HOST
  if (BleHid.isRunning()) {
    BleHid.stopScan();
    BleHid.releaseScanResults();
  }
#endif
  bleremote::endPairing();
  UiListActivity::onExit();
}

void BleRemoteActivity::startScan() {
#if FREEINK_CAP_BLE_HID_HOST
  if (!BleHid.isRunning()) return;  // stack comes up on the next input frame
  BleHid.startScan(SCAN_MS);
  scanRequested_ = false;
  scanStarted_ = true;
  status_ = tr(STR_BLE_REMOTE_SCANNING);
  requestUpdate();
#endif
}

void BleRemoteActivity::loop() {
#if FREEINK_CAP_BLE_HID_HOST
  if (scanRequested_) startScan();

  char buf[48];
  if (!connectingAddr_.empty()) {
    uint32_t passkey = 0;
    if (BleHid.takePairingPasskey(passkey)) {
      snprintf(buf, sizeof(buf), "%s %06u", tr(STR_BLE_REMOTE_PASSKEY), static_cast<unsigned>(passkey));
      status_ = buf;
      requestUpdate();
    }
    if (BleHid.takeConnectFailure(buf, sizeof(buf))) {
      LOG_ERR("BLER", "Pairing %s failed: %s", connectingAddr_.c_str(), buf);
      status_ = std::string(tr(STR_BLE_REMOTE_FAILED)) + ": " + buf;
      connectingAddr_.clear();
      requestUpdate();
    } else if (BleHid.isConnected() && connectingAddr_ == BleHid.connectedAddr()) {
      strncpy(SETTINGS.bleRemoteAddr, connectingAddr_.c_str(), sizeof(SETTINGS.bleRemoteAddr) - 1);
      SETTINGS.bleRemoteAddr[sizeof(SETTINGS.bleRemoteAddr) - 1] = '\0';
      SETTINGS.bleRemoteEnabled = 1;
      SETTINGS.saveToFile();
      LOG_INF("BLER", "Paired remote %s (%s)", connectingAddr_.c_str(), BleHid.connectedName());
      status_ = std::string(tr(STR_BLE_REMOTE_PAIRED)) + ": " + BleHid.connectedName();
      connectingAddr_.clear();
      requestUpdate();
    }
  }

  const unsigned long now = millis();
  if (scanStarted_ && now - lastRefreshMs_ >= REFRESH_MS) {
    lastRefreshMs_ = now;
    if (BleHid.deviceCount() != lastDeviceCount_ || !BleHid.isScanning()) {
      lastDeviceCount_ = BleHid.deviceCount();
      if (!BleHid.isScanning() && connectingAddr_.empty() && status_ == tr(STR_BLE_REMOTE_SCANNING)) {
        status_ = tr(STR_BLE_REMOTE_RESCAN);
      }
      requestUpdate();
    }
  }
#endif
  UiListActivity::loop();
}

int BleRemoteActivity::listCount() const {
#if FREEINK_CAP_BLE_HID_HOST
  const int devices = BleHid.isRunning() ? std::min<int>(BleHid.deviceCount(), MAX_DEVICES) : 0;
  return FIXED_ROWS + devices;
#else
  return FIXED_ROWS;
#endif
}

void BleRemoteActivity::activateIndex(const int index) {
#if FREEINK_CAP_BLE_HID_HOST
  if (index == IDX_STATUS) {
    if (!BleHid.isScanning() && connectingAddr_.empty()) scanRequested_ = true;
    return;
  }
  if (index == IDX_FORGET) {
    if (SETTINGS.bleRemoteAddr[0]) {
      if (BleHid.isRunning()) BleHid.forget(SETTINGS.bleRemoteAddr);
      SETTINGS.bleRemoteAddr[0] = '\0';
      SETTINGS.bleRemoteEnabled = 0;
      SETTINGS.saveToFile();
      status_ = tr(STR_BLE_REMOTE_FORGOTTEN);
      requestUpdate();
    }
    return;
  }
  const int dev = index - FIXED_ROWS;
  if (dev < 0 || dev >= BleHid.deviceCount() || !connectingAddr_.empty()) return;
  const auto& d = BleHid.device(static_cast<uint8_t>(dev));
  BleHid.stopScan();
  connectingAddr_.assign(d.addr, strnlen(d.addr, sizeof(d.addr)));
  status_ = std::string(tr(STR_BLE_REMOTE_CONNECTING)) + "...";
  if (!BleHid.connect(connectingAddr_.c_str())) {
    status_ = tr(STR_BLE_REMOTE_FAILED);
    connectingAddr_.clear();
  }
  requestUpdate();
#else
  (void)index;
#endif
}

void BleRemoteActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const int count = listCount();
  rowLabels_[IDX_STATUS] = status_;
  rowValues_[IDX_STATUS].clear();
  rowLabels_[IDX_FORGET] = tr(STR_BLE_REMOTE_FORGET);
  rowValues_[IDX_FORGET] = SETTINGS.bleRemoteAddr[0] ? SETTINGS.bleRemoteAddr : tr(STR_NOT_SET);
#if FREEINK_CAP_BLE_HID_HOST
  for (int i = FIXED_ROWS; i < count; i++) {
    // The scan task may update an entry while we read it: copy bounded.
    const auto& d = BleHid.device(static_cast<uint8_t>(i - FIXED_ROWS));
    rowLabels_[i] = d.name[0] ? std::string(d.name, strnlen(d.name, sizeof(d.name)))
                              : std::string(d.addr, strnlen(d.addr, sizeof(d.addr)));
    if (d.hid) rowLabels_[i] += " [HID]";
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%d dBm", d.rssi);
    const bool current = std::strncmp(d.addr, SETTINGS.bleRemoteAddr, sizeof(d.addr)) == 0;
    rowValues_[i] = current ? std::string("* ") + rssi : rssi;
  }
#endif
  for (int i = 0; i < count; i++) {
    rowItems_[i].label = rowLabels_[i].c_str();
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(count);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
