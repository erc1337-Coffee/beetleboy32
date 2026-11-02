#include "app/device_config.h"

#include <ArduinoJson.h>

// Env-style names; max 15 chars (NVS limit), hence KC_ rather than the full cookie names.
namespace key {
constexpr const char *WIFI_SSID = "WIFI_SSID";
constexpr const char *WIFI_PASS = "WIFI_PASSWORD";
constexpr const char *WIFI_NETWORKS = "WIFI_NETWORKS";  // [{"ssid","password"}], most recent first
constexpr const char *AUTH_SESSION_ID = "KC_AUTH_SESSION";
constexpr const char *KEYCLOAK_SESSION = "KC_SESSION";
constexpr const char *KEYCLOAK_IDENTITY = "KC_IDENTITY";
constexpr const char *REFRESH_TOKEN = "KC_REFRESH";
}  // namespace key

DeviceConfig DeviceConfig::load(ConfigStore &store) {
  DeviceConfig c;
  store.get(key::WIFI_SSID, c.wifi.ssid);
  store.get(key::WIFI_PASS, c.wifi.password);
  std::string networks;
  JsonDocument doc;
  if (store.get(key::WIFI_NETWORKS, networks) && !deserializeJson(doc, networks)) {
    for (JsonObjectConst n : doc.as<JsonArrayConst>())
      c.networks.push_back({n["ssid"] | "", n["password"] | ""});
  } else if (c.wifi.valid()) {
    c.networks.push_back(c.wifi);  // no list saved: the one network starts it
  }
  store.get(key::AUTH_SESSION_ID, c.account.authSessionId);
  store.get(key::KEYCLOAK_SESSION, c.account.keycloakSession);
  store.get(key::KEYCLOAK_IDENTITY, c.account.keycloakIdentity);
  return c;
}

void DeviceConfig::save(ConfigStore &store) const {
  store.set(key::WIFI_SSID, wifi.ssid);
  store.set(key::WIFI_PASS, wifi.password);
  JsonDocument doc;
  for (const WifiCredentials &n : networks) {
    JsonObject o = doc.add<JsonObject>();
    o["ssid"] = n.ssid;
    o["password"] = n.password;
  }
  std::string json;
  serializeJson(doc, json);
  store.set(key::WIFI_NETWORKS, json);
  store.set(key::AUTH_SESSION_ID, account.authSessionId);
  store.set(key::KEYCLOAK_SESSION, account.keycloakSession);
  store.set(key::KEYCLOAK_IDENTITY, account.keycloakIdentity);
}

void DeviceConfig::rememberWifi(const WifiCredentials &joined) {
  wifi = joined;
  for (auto it = networks.begin(); it != networks.end(); ++it) {
    if (it->ssid == joined.ssid) {
      networks.erase(it);
      break;
    }
  }
  networks.insert(networks.begin(), joined);
  if (networks.size() > MAX_NETWORKS) networks.pop_back();
}

void DeviceConfig::saveRefreshToken(ConfigStore &store, const std::string &token) {
  store.set(key::REFRESH_TOKEN, token);
}

std::string DeviceConfig::loadRefreshToken(ConfigStore &store) {
  std::string token;
  store.get(key::REFRESH_TOKEN, token);
  return token;
}
