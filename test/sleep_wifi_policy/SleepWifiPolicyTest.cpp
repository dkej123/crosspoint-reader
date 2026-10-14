// Requirement: going to sleep must deliver queued plugin events (e.g. the Home
// Assistant webhook) whenever a network is saved — also when the device never
// visited the Wi-Fi screen since boot, when the last connected network was
// never recorded, or when it has since been forgotten. It prefers the network
// last used, and does nothing only when no network is saved at all.
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "src/network/SleepWifiPolicy.h"

namespace {

struct Cred {
  std::string ssid;
};

struct FakeStore {
  std::vector<Cred> saved;
  std::string last;
  std::optional<Cred> findCredential(const std::string& ssid) const {
    for (const auto& c : saved)
      if (!ssid.empty() && c.ssid == ssid) return c;
    return std::nullopt;
  }
  std::string getLastConnectedSsid() const { return last; }
  size_t getCredentialCount() const { return saved.size(); }
  std::optional<Cred> getCredentialAt(size_t i) const {
    return i < saved.size() ? std::optional<Cred>(saved[i]) : std::nullopt;
  }
};

}  // namespace

TEST(SleepWifi, PrefersTheLastConnectedNetwork) {
  FakeStore s{{{"Pixel"}, {"MikroTik"}}, "MikroTik"};
  EXPECT_EQ(sleepJoinCredential(s)->ssid, "MikroTik");
}

TEST(SleepWifi, JoinsASavedNetworkWhenLastConnectedWasNeverRecorded) {
  FakeStore s{{{"MikroTik"}, {"Pixel"}}, ""};
  ASSERT_TRUE(sleepJoinCredential(s).has_value());
}

TEST(SleepWifi, JoinsASavedNetworkWhenLastConnectedWasForgotten) {
  FakeStore s{{{"MikroTik"}}, "OldCafe"};
  ASSERT_TRUE(sleepJoinCredential(s).has_value());
  EXPECT_EQ(sleepJoinCredential(s)->ssid, "MikroTik");
}

TEST(SleepWifi, SkipsOnlyWhenNoNetworkIsSaved) {
  FakeStore s{{}, "MikroTik"};
  EXPECT_FALSE(sleepJoinCredential(s).has_value());
}
