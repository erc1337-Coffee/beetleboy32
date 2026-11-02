#include "portal/portal.h"

#include <algorithm>

#include "assets/web_assets.h"
#include "hal/hal.h"
#include "portal/portal_page.h"
#include "portal/setup_form.h"
#include "util/secret.h"

static constexpr uint32_t SUBMIT_GRACE_MS = 1000;  // let the "Saved" page reach the phone first

void Portal::begin(const std::string &url, bool captive, SetupMode mode, const std::string &kept_ssid) {
  mode_ = mode;
  keptSsid_ = kept_ssid;
  url_ = url;
  size_t scheme = url.find("://");
  host_ = scheme == std::string::npos ? url : url.substr(scheme + 3);
  captive_ = captive;
  pending_ = false;
}

HttpResponse Portal::handle(const HttpRequest &req) {
  // Phones probe e.g. captive.apple.com/hotspot-detect.html; anything not addressed to us goes to the form.
  if (captive_ && req.host != host_) return redirect();
  if (req.path == "/" && req.method == "GET") {
    HttpResponse res;
    res.body = formPage("", "", "");
    return res;
  }
  if (req.path == "/save" && req.method == "POST") return save(req);
  if (req.path == PORTAL_BG_PATH) {
    HttpResponse res;
    res.contentType = "image/jpeg";
    res.cacheControl = "max-age=86400";
    res.body.assign((const char *)web_portal_bg_jpg, web_portal_bg_jpg_size);
    return res;
  }
  return redirect();
}

HttpResponse Portal::save(const HttpRequest &req) {
  auto field = [&](const char *name) {
    auto it = req.form.find(name);
    return it == req.form.end() ? std::string() : it->second;
  };
  SetupForm form;
  form.ssid = field("ssid");
  form.password = field("password");
  form.email = field("email");
  form.accountPassword = field("account_password");

  HttpResponse res;
  std::string error;
  bool ok = parse_setup_form(form, mode_, submission_, error);
  wipe_secret(form.accountPassword);
  if (!ok) {
    res.status = 400;
    res.body = formPage(form.ssid, form.email, error);
    return res;
  }
  res.body = portal_done_page(mode_ == SetupMode::AccountOnly ? keptSsid_ : submission_.wifi.ssid);
  pending_ = true;
  savedAt_ = hal_millis();
  return res;
}

void Portal::setNetworks(std::vector<ScannedNetwork> networks) {
  networks_.clear();
  for (auto &n : networks) {
    if (n.ssid.empty()) continue;  // hidden network
    auto same = std::find_if(networks_.begin(), networks_.end(), [&](auto &m) { return m.ssid == n.ssid; });
    if (same == networks_.end()) networks_.push_back(n);
    else if (n.rssi > same->rssi) *same = n;  // several access points: keep the strongest
  }
  std::stable_sort(networks_.begin(), networks_.end(), [](auto &a, auto &b) { return a.rssi > b.rssi; });
}

bool Portal::takeSubmission(SetupSubmission &out) {
  if (!pending_ || hal_millis() - savedAt_ < SUBMIT_GRACE_MS) return false;
  pending_ = false;
  out = submission_;
  wipe_secret(submission_.accountPassword);
  return true;
}

std::string Portal::formPage(const std::string &ssid, const std::string &email, const std::string &error) const {
  PortalFormView view;
  view.mode = mode_;
  view.keptSsid = keptSsid_;
  view.networks = networks_;
  view.ssid = ssid;
  view.email = email;
  view.error = error;
  return portal_form_page(view);
}

HttpResponse Portal::redirect() const {
  HttpResponse res;
  res.status = 302;
  res.contentType = "text/plain";
  res.location = url_ + "/";
  return res;
}
