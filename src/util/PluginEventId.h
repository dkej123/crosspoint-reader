#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// Plugin event ids let a receiver drop re-sent events (the outbox is
// at-least-once). They must never repeat across boots, and every wake from deep
// sleep is a boot. esp_random() alone does not guarantee that: before the radio
// is on it is only pseudo-random and returned the same value on every wake,
// so each wake's first event reused the id of an earlier one and the receiver
// discarded it as already seen. A boot counter kept in NVS makes the id unique
// by construction; the random part only guards against a counter reset.
namespace pluginevents {

inline std::string makeEventId(const uint32_t entropy, const uint32_t bootCount, const uint32_t seq) {
  char id[40];
  snprintf(id, sizeof(id), "%08lx-%lu-%lu", static_cast<unsigned long>(entropy), static_cast<unsigned long>(bootCount),
           static_cast<unsigned long>(seq));
  return id;
}

}  // namespace pluginevents
