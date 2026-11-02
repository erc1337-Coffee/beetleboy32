#pragma once

#include <map>
#include <string>
#include <vector>

#include "app/device_config.h"
#include "portal/portal_page.h"

struct HttpRequest {
  std::string method;  // "GET", "POST"
  std::string path;    // without query string
  std::string host;    // Host header
  std::map<std::string, std::string> form;
};

struct HttpResponse {
  int status = 200;
  std::string contentType = "text/html";
  std::string body;
  std::string location;  // set for redirects
  std::string cacheControl = "no-store";
};

// The setup web page: routes and form handling. Shared by every platform so the sim
// serves exactly what the device does; platforms only move bytes (ESP32 WebServer, sockets in the sim).
class Portal {
 public:
  // `captive`: redirect requests for any other host to the portal. Only meaningful when DNS
  // points every name at us (the device's setup network), which is what pops the page open on phones.
  // `mode`: what the form asks for; `kept_ssid`: the Wi-Fi network kept by an AccountOnly setup.
  void begin(const std::string &url, bool captive, SetupMode mode, const std::string &kept_ssid);
  void setNetworks(std::vector<ScannedNetwork> networks);  // raw scan: deduplicated, strongest first

  HttpResponse handle(const HttpRequest &request);
  bool takeSubmission(SetupSubmission &out);  // true once, shortly after a valid form was saved
  const std::string &url() const { return url_; }

 private:
  HttpResponse redirect() const;
  HttpResponse save(const HttpRequest &request);
  std::string formPage(const std::string &ssid, const std::string &email, const std::string &error) const;

  std::string url_;   // e.g. "http://192.168.4.1"
  std::string host_;  // e.g. "192.168.4.1"
  bool captive_ = false;
  SetupMode mode_ = SetupMode::Full;
  std::string keptSsid_;
  std::vector<ScannedNetwork> networks_;
  SetupSubmission submission_;  // holds the account password until taken
  bool pending_ = false;
  uint32_t savedAt_ = 0;
};
