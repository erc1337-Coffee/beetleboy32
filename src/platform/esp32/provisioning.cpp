#include "services/provisioning.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <vector>

#include "hal/hal.h"
#include "portal/portal.h"

static const IPAddress PORTAL_IP(192, 168, 4, 1);
static constexpr int SCAN_TRIES = 4;  // a scan right after the radio starts can fail or see nothing

// Setup network + captive portal: every DNS query resolves to us, and every HTTP request goes
// through the shared Portal, which redirects unknown hosts/URLs to the form.
class SoftApProvisioning : public Provisioning {
 public:
  SoftApProvisioning() : server_(80) {
    server_.onNotFound([this] { serve(); });  // no fixed routes: Portal does the routing
  }

  void start(const SetupNetwork &network, SetupMode mode, const std::string &kept_ssid) override {
    network_ = network;
    portal_.begin("http://192.168.4.1", /*captive=*/true, mode, kept_ssid);

    // Scan before the AP is up: scanning while it runs makes the radio hop channels
    // and drops the phone. Async, so the UI keeps running.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    scanTries_ = 0;
    startScan();
  }

  void stop() override {
    if (phase_ == Phase::Running) {
      server_.stop();
      dns_.stop();
      WiFi.softAPdisconnect(true);
    }
    WiFi.scanDelete();
    phase_ = Phase::Idle;
  }

  void update() override {
    switch (phase_) {
      case Phase::Idle:
        break;
      case Phase::Scanning: {
        int found = WiFi.scanComplete();
        if (found == WIFI_SCAN_RUNNING) return;
        if (found <= 0 && scanTries_ < SCAN_TRIES) {
          hal_log("[setup] scan %d: %s, again\n", scanTries_, found < 0 ? "failed" : "nothing");
          WiFi.scanDelete();
          startScan();
          return;
        }
        std::vector<ScannedNetwork> networks;
        for (int i = 0; i < found; i++)
          networks.push_back({WiFi.SSID(i).c_str(), (int)WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN});
        WiFi.scanDelete();
        portal_.setNetworks(std::move(networks));
        startAccessPoint();
        break;
      }
      case Phase::Running:
        dns_.processNextRequest();
        server_.handleClient();
        break;
    }
  }

  bool ready() const override { return phase_ == Phase::Running; }
  bool clientConnected() const override { return ready() && WiFi.softAPgetStationNum() > 0; }
  const char *portalUrl() const override { return portal_.url().c_str(); }
  bool takeSubmission(SetupSubmission &out) override { return portal_.takeSubmission(out); }

 private:
  enum class Phase { Idle, Scanning, Running };

  void startScan() {
    scanTries_++;
    WiFi.scanNetworks(/*async=*/true);
    phase_ = Phase::Scanning;
  }

  void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(PORTAL_IP, PORTAL_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(network_.ssid.c_str(), network_.password.c_str());
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(53, "*", PORTAL_IP);
    server_.begin();
    phase_ = Phase::Running;
    hal_log("[setup] network %s up\n", network_.ssid.c_str());
  }

  void serve() {
    HttpRequest req;
    req.method = server_.method() == HTTP_POST ? "POST" : "GET";
    req.path = server_.uri().c_str();
    req.host = server_.hostHeader().c_str();
    for (int i = 0; i < server_.args(); i++) req.form[server_.argName(i).c_str()] = server_.arg(i).c_str();

    HttpResponse res = portal_.handle(req);
    if (!res.location.empty()) server_.sendHeader("Location", res.location.c_str());
    server_.sendHeader("Cache-Control", res.cacheControl.c_str());
    server_.send_P(res.status, res.contentType.c_str(), res.body.data(), res.body.size());  // may be binary
  }

  WebServer server_;
  DNSServer dns_;
  Portal portal_;
  Phase phase_ = Phase::Idle;
  SetupNetwork network_;
  int scanTries_ = 0;
};

std::unique_ptr<Provisioning> create_provisioning() { return std::make_unique<SoftApProvisioning>(); }
