#pragma once

#include <memory>
#include <string>

#include "app/device_config.h"

// The device's own Wi-Fi network, used only during setup.
struct SetupNetwork {
  std::string ssid;
  std::string password;
};

// Setup mode: host a Wi-Fi network plus a captive portal where the user enters Wi-Fi and
// account details. Driven from the main loop via update(); never touches LVGL.
class Provisioning {
 public:
  virtual ~Provisioning() = default;
  // `mode`: what the setup page asks for; `kept_ssid`: the Wi-Fi network an AccountOnly setup keeps.
  virtual void start(const SetupNetwork &network, SetupMode mode, const std::string &kept_ssid) = 0;
  virtual void stop() = 0;
  virtual void update() = 0;

  virtual bool ready() const = 0;            // network is up and joinable
  virtual bool clientConnected() const = 0;  // a phone/laptop has joined
  virtual const char *portalUrl() const = 0;
  virtual bool takeSubmission(SetupSubmission &out) = 0;  // true once, after the form was submitted
};

std::unique_ptr<Provisioning> create_provisioning();  // per platform
