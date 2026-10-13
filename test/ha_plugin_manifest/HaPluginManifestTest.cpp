// Requirements for the Home Assistant SD plugin (sd-card/plugins/homeassistant):
//  - the firmware accepts it as a background (events) plugin, not a catalog;
//  - its folder fits the event-subscription path limits under every SD root;
//  - reading sessions bring Wi-Fi up at sleep so they reach Home Assistant;
//  - every request body is valid JSON for any book path (quotes, backslashes,
//    non-ASCII) and also when a variable is not filled (sleep outside a book),
//    with the numeric session fields still numbers for HA's templates;
//  - requests target the webhook configured in config.json.
#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "src/util/PluginLocations.h"
#include "src/util/PluginTemplate.h"

namespace {

const std::string kDir = std::string(REPO_ROOT_DIR) + "/sd-card/plugins/homeassistant";
constexpr const char* kFolder = "homeassistant";

std::string readFile(const std::string& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

JsonDocument loadJson(const std::string& path) {
  JsonDocument doc;
  const auto err = deserializeJson(doc, readFile(path));
  EXPECT_EQ(err, DeserializationError::Ok) << path << ": " << err.c_str();
  return doc;
}

// Applies the firmware's substitution for the given event variables (plus the
// always-present event.ts / event.id and the plugin config).
std::string render(std::string tpl, const std::map<std::string, std::string>& vars) {
  JsonDocument cfg = loadJson(kDir + "/config.example.json");
  for (JsonPairConst kv : cfg.as<JsonObjectConst>()) {
    plugintemplate::substituteVar(tpl, std::string("cfg.") + kv.key().c_str(), kv.value().as<std::string>());
  }
  plugintemplate::substituteVar(tpl, "event.ts", "1792000000");
  plugintemplate::substituteVar(tpl, "event.id", "a1b2c3d4-17");
  for (const auto& [k, v] : vars) plugintemplate::substituteVar(tpl, "event." + k, v);
  return tpl;
}

std::map<std::string, std::string> session(const std::string& book) {
  return {{"book", book},
          {"document", "0123456789abcdef0123456789abcdef"},
          {"start_time", "1791990000"},
          {"end_time", "1791991500"},
          {"duration_seconds", "1500"},
          {"start_progress_bp", "3100"},
          {"end_progress_bp", "3412"},
          {"progress_scale", "10000"}};
}

JsonDocument parseBody(const std::string& body) {
  JsonDocument doc;
  const auto err = deserializeJson(doc, body);
  EXPECT_EQ(err, DeserializationError::Ok) << body;
  return doc;
}

const char* kAwkwardBooks[] = {
    "/Hugh Howey - Zmiana.epub", "/Books/Pan \"Tadeusz\".epub", "/Books/back\\slash.epub",
    "/Książki/Żółć – ćma.epub",  "/Books/tab\there.epub",
};

class HaPluginManifest : public ::testing::Test {
 protected:
  JsonDocument manifest = loadJson(kDir + "/device.json");
  std::string body(const char* event) const { return manifest["events"][event]["request"]["body"].as<std::string>(); }
};

}  // namespace

TEST_F(HaPluginManifest, IsABackgroundEventsPlugin) {
  const bool hasBrowseUrl = !manifest["browse"]["url"].isNull();
  const bool hasEvents = manifest["events"].is<JsonObjectConst>();
  EXPECT_EQ(PluginLocations::classifyDeviceManifest(hasBrowseUrl, hasEvents), PluginLocations::DeviceKind::Background);
}

TEST_F(HaPluginManifest, FolderFitsEventSubscriptionLimitsUnderEveryRoot) {
  EXPECT_LE(std::string(kFolder).size(), 23u);
  for (const char* root : PluginLocations::ROOTS) {
    EXPECT_LE(std::string(root).size() + 1 + std::string(kFolder).size(), 63u) << root;
  }
}

TEST_F(HaPluginManifest, ReadingSessionsAreDeliveredAtSleep) {
  EXPECT_TRUE(manifest["events"]["reader.session"]["connect"].as<bool>());
  EXPECT_FALSE(manifest["events"]["sleep.enter"].isNull());
}

TEST_F(HaPluginManifest, RequestsPostJsonToTheConfiguredWebhook) {
  JsonDocument cfg = loadJson(kDir + "/config.example.json");
  for (JsonPairConst ev : manifest["events"].as<JsonObjectConst>()) {
    const auto req = ev.value()["request"];
    EXPECT_EQ(req["method"].as<std::string>(), "POST") << ev.key().c_str();
    const std::string url = render(req["url"].as<std::string>(), {});
    EXPECT_EQ(url, cfg["ha_url"].as<std::string>() + "/api/webhook/" + cfg["webhook_id"].as<std::string>());
    EXPECT_NE(req["headers"]["Content-Type"].as<std::string>().find("application/json"), std::string::npos);
  }
}

TEST_F(HaPluginManifest, SessionBodyIsValidJsonForAnyBookPath) {
  for (const char* book : kAwkwardBooks) {
    JsonDocument doc = parseBody(render(body("reader.session"), session(book)));
    EXPECT_EQ(doc["book"].as<std::string>(), book);
    EXPECT_EQ(doc["event"].as<std::string>(), "reader.session");
  }
}

TEST_F(HaPluginManifest, SessionNumbersStayNumbers) {
  JsonDocument doc = parseBody(render(body("reader.session"), session("/a.epub")));
  for (const char* key : {"ts", "start_time", "end_time", "duration_seconds", "start_progress_bp", "end_progress_bp"}) {
    EXPECT_TRUE(doc[key].is<long long>()) << key;
  }
  EXPECT_EQ(doc["duration_seconds"].as<int>(), 1500);
  EXPECT_EQ(doc["document"].as<std::string>(), "0123456789abcdef0123456789abcdef");
  EXPECT_EQ(doc["id"].as<std::string>(), "a1b2c3d4-17");
}

TEST_F(HaPluginManifest, SleepBodyIsValidJsonInsideABook) {
  for (const char* book : kAwkwardBooks) {
    JsonDocument doc = parseBody(render(body("sleep.enter"), {{"book", book}, {"percent", "34"}}));
    EXPECT_EQ(doc["book"].as<std::string>(), book);
    EXPECT_EQ(doc["percent"].as<std::string>(), "34");
  }
}

TEST_F(HaPluginManifest, SleepBodyIsValidJsonOutsideABook) {
  // sleep.enter carries no book/percent when not sleeping from a reader; the
  // unfilled variables stay literal and must not break the JSON.
  parseBody(render(body("sleep.enter"), {}));
}
