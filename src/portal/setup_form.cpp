#include "portal/setup_form.h"

#include "util/text.h"

bool parse_setup_form(const SetupForm &form, SetupMode mode, SetupSubmission &out, std::string &error) {
  SetupSubmission s;
  s.wifi.ssid = trim(form.ssid);
  s.wifi.password = form.password;  // passwords may legitimately have edge spaces
  s.email = trim(form.email);
  s.accountPassword = form.accountPassword;

  if (mode != SetupMode::AccountOnly && s.wifi.ssid.empty()) {
    error = "Pick a Wi-Fi network.";
    return false;
  }
  if (mode != SetupMode::WifiOnly && (s.email.empty() || s.accountPassword.empty())) {
    error = "Enter your remilia.net email and password.";
    return false;
  }
  out = std::move(s);
  return true;
}
