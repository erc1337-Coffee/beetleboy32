#pragma once

#include <string>
#include <vector>

#include "services/config_store.h"

// What setup collects. The account is kept as the website's SSO cookies, never as a password: the
// device logs in with them the way the website does (keycloak_login).

struct WifiCredentials {
  std::string ssid;
  std::string password;

  bool valid() const { return !ssid.empty(); }
};

struct AccountCookies {
  std::string authSessionId;     // AUTH_SESSION_ID
  std::string keycloakSession;   // KEYCLOAK_SESSION
  std::string keycloakIdentity;  // KEYCLOAK_IDENTITY

  bool valid() const {
    return !authSessionId.empty() && !keycloakSession.empty() && !keycloakIdentity.empty();
  }
};

// What the setup page asks for: everything, or just the part that needs redoing (the other part
// is kept): a new Wi-Fi network, or the account after its login expired.
enum class SetupMode { Full, WifiOnly, AccountOnly };

// What the setup page sends (only the parts its SetupMode asked for). The account password signs in
// once (keycloak_password_login) and is never stored: only the SSO cookies it yields are.
struct SetupSubmission {
  WifiCredentials wifi;
  std::string email;
  std::string accountPassword;
};

struct DeviceConfig {
  static constexpr size_t MAX_NETWORKS = 8;

  WifiCredentials wifi;                   // the network joined last, or just set up
  std::vector<WifiCredentials> networks;  // every network joined so far, most recent first
  AccountCookies account;

  bool valid() const { return wifi.valid() && account.valid(); }
  // Puts it first in `networks` (replacing one with the same name), and makes it `wifi`.
  void rememberWifi(const WifiCredentials &joined);

  static DeviceConfig load(ConfigStore &store);
  void save(ConfigStore &store) const;

  // Written on its own, every time it rotates: Keycloak has already invalidated the previous one.
  static void saveRefreshToken(ConfigStore &store, const std::string &token);
  static std::string loadRefreshToken(ConfigStore &store);
};
