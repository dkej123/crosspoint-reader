#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Fork: read credentials saved by CrossInk on the same device.
//
// CrossInk stores *_obf passwords (wifi.json, koreader.json, opds.json) as
// base64(XOR(payload, hwKey)) like CrossPoint, but its payload is
//   "CPV1" + FNV-1a32(hwKey || plaintext) (uint32 LE) + plaintext
// where hwKey is the 6-byte factory MAC. CrossPoint's payload is the bare
// plaintext. After the XOR, a payload with the CrossInk prefix and a matching
// checksum is reduced to its plaintext; anything else is left untouched.
namespace obfuscation {

inline uint32_t crossInkChecksum(const uint8_t* key, const size_t keyLen, const std::string& plaintext) {
  uint32_t hash = 2166136261UL;
  const auto mix = [&hash](const uint8_t b) {
    hash ^= b;
    hash *= 16777619UL;
  };
  for (size_t i = 0; i < keyLen; i++) mix(key[i]);
  for (const char c : plaintext) mix(static_cast<uint8_t>(c));
  return hash;
}

// Returns true when `decoded` was a CrossInk payload and now holds the plaintext.
inline bool stripCrossInkValidated(std::string& decoded, const uint8_t* key, const size_t keyLen) {
  constexpr char kPrefix[] = {'C', 'P', 'V', '1'};
  constexpr size_t kHeader = sizeof(kPrefix) + 4;
  if (decoded.size() < kHeader || std::memcmp(decoded.data(), kPrefix, sizeof(kPrefix)) != 0) return false;
  const auto byte = [&decoded](const size_t i) { return static_cast<uint32_t>(static_cast<uint8_t>(decoded[i])); };
  const uint32_t stored = byte(4) | (byte(5) << 8) | (byte(6) << 16) | (byte(7) << 24);
  std::string plaintext = decoded.substr(kHeader);
  if (crossInkChecksum(key, keyLen, plaintext) != stored) return false;
  decoded = std::move(plaintext);
  return true;
}

}  // namespace obfuscation
