#pragma once

#include <cstdint>

// Bluetooth LE page-turner remote (fork feature).
//
// Compiled in only with FREEINK_CAP_BLE_HID_HOST (env x4pro-dkej); elsewhere
// every call is a no-op and available() is false. The radio runs only while a
// reader is open AND Settings -> Controls -> Bluetooth Remote is on with a
// paired remote; leaving the reader or sleeping shuts NimBLE down again. The
// pairing screen brings the stack up on its own for the duration of the scan.
namespace bleremote {

bool available();

// Reader lifecycle: start/stop are requests, the state machine in poll()
// brings NimBLE up or down (end() may need several passes while a link drops).
void startForReader();
void stop();

// Pairing screen ownership: keeps the stack up outside a reader.
void beginPairing();
void endPairing();

// Called once per input frame (MappedInputManager::update()).
void poll();

// Page turn decoded during the last poll(): +1 next, -1 previous, 0 none.
int8_t takePageTurn();

}  // namespace bleremote
