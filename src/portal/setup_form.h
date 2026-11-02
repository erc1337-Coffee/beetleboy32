#pragma once

#include <string>

#include "app/device_config.h"

// Raw fields from the setup page.
struct SetupForm {
  std::string ssid;
  std::string password;         // Wi-Fi
  std::string email;            // remilia.net account
  std::string accountPassword;
};

// Checks the fields `mode` asks for. On failure returns false and sets `error` to a message for
// the user.
bool parse_setup_form(const SetupForm &form, SetupMode mode, SetupSubmission &out, std::string &error);
