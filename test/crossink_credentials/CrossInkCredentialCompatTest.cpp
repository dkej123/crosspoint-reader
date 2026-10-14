// Requirement: after switching this device from CrossInk to the fork, the
// Wi-Fi / KOSync / OPDS passwords CrossInk saved on the SD card are read back
// as the original passwords, while passwords saved by CrossPoint itself are
// read exactly as before.
//
// CrossInk payload (from CrossInk's ObfuscationUtils, implemented here
// independently): "CPV1" + FNV-1a 32-bit over (6-byte MAC key, then
// plaintext), stored little-endian, followed by the plaintext.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "lib/Serialization/CrossInkCredentialCompat.h"

namespace {

const uint8_t kDeviceKey[6] = {0x98, 0xC3, 0x77, 0xBD, 0xB5, 0xF0};
const uint8_t kOtherKey[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

std::string crossInkPayload(const std::string& password, const uint8_t* key) {
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < 6; i++) h = (h ^ key[i]) * 0x01000193u;
  for (unsigned char c : password) h = (h ^ c) * 0x01000193u;
  std::string p = "CPV1";
  for (int i = 0; i < 4; i++) p += static_cast<char>((h >> (8 * i)) & 0xFF);
  return p + password;
}

std::string readBack(std::string decoded, const uint8_t* deviceKey = kDeviceKey) {
  obfuscation::stripCrossInkValidated(decoded, deviceKey, 6);
  return decoded;
}

}  // namespace

TEST(CrossInkCredentials, PasswordsSavedByCrossInkReadAsOriginal) {
  for (const std::string& pw :
       std::vector<std::string>{"hunter2", "WiFi P@ssw0rd!", "zażółć gęślą jaźń", std::string(63, 'x'), "a"}) {
    EXPECT_EQ(readBack(crossInkPayload(pw, kDeviceKey)), pw) << pw;
  }
}

TEST(CrossInkCredentials, EmptyPasswordSavedByCrossInkReadsEmpty) {
  EXPECT_EQ(readBack(crossInkPayload("", kDeviceKey)), "");
}

TEST(CrossInkCredentials, PasswordsSavedByCrossPointAreUnchanged) {
  for (const std::string& pw : std::vector<std::string>{"hunter2", "CPV1", "CPV1short", "CPV1xxxxpassword", ""}) {
    EXPECT_EQ(readBack(pw), pw) << pw;
  }
}

TEST(CrossInkCredentials, PayloadFromAnotherDeviceIsNotTrusted) {
  const std::string foreign = crossInkPayload("hunter2", kOtherKey);
  EXPECT_EQ(readBack(foreign), foreign);
}

TEST(CrossInkCredentials, CorruptedChecksumIsNotTrusted) {
  std::string p = crossInkPayload("hunter2", kDeviceKey);
  p[5] ^= 0x01;
  EXPECT_EQ(readBack(p), p);
}
