#pragma once

#include <string>

// Which saved network the sleep-time plugin delivery joins (upstream PR #3867).
// The last connected network when it is still saved; otherwise the first saved
// one, because lastConnectedSsid is only set by the Wi-Fi selection screen and
// may be empty or stale. Empty result = no saved network, skip the join.
// Store concept: findCredential(ssid), getLastConnectedSsid(), getCredentialCount(),
// getCredentialAt(index) — all returning/accepting like WifiCredentialStore.
template <class Store>
auto sleepJoinCredential(const Store& store) -> decltype(store.findCredential(std::string())) {
  auto cred = store.findCredential(store.getLastConnectedSsid());
  if (!cred && store.getCredentialCount() > 0) cred = store.getCredentialAt(0);
  return cred;
}
