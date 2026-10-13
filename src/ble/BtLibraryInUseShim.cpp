// Weak fallback for `_btLibraryInUse` in BLE builds (fork, from upstream PR #3197).
//
// arduino-esp32's esp32-hal-bt-mem.h emits a constructor referencing
// `_btLibraryInUse` whenever BLE hardware is present, but the core only defines
// it when an IDF host stack is enabled. NimBLE-Arduino brings its own host, so
// the final link can miss the symbol. Weak: a core-provided strong definition
// still wins when one exists.
#if FREEINK_CAP_BLE_HID_HOST
extern "C" __attribute__((weak)) bool _btLibraryInUse = false;
#endif
