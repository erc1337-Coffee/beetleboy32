#pragma once

#include <string>
#include <vector>

#include "app/device_config.h"

// HTML for the captive portal. No external assets: there's no internet while the phone is on the
// setup network, so the background is served by the device (PORTAL_BG_PATH) and the font is the
// phone's own monospace.

static constexpr const char *PORTAL_BG_PATH = "/bg.jpg";

// One network from the scan.
struct ScannedNetwork {
  std::string ssid;
  int rssi = -100;    // dBm
  bool open = false;  // no password
};

struct PortalFormView {
  SetupMode mode = SetupMode::Full;      // which sections to show
  std::string keptSsid;                  // AccountOnly: the Wi-Fi network being kept, shown for reference
  std::vector<ScannedNetwork> networks;  // strongest first; tapping one picks it
  std::string ssid, email, error;        // refilled after a rejected submit (never the passwords)
};
std::string portal_form_page(const PortalFormView &view);
std::string portal_done_page(const std::string &ssid);
