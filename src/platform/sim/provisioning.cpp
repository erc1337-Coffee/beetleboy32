#include "services/provisioning.h"

#include <SDL.h>

#include "hal/hal.h"
#include "http_server.h"
#include "portal/portal.h"

static constexpr uint16_t PORT = 8080;
static constexpr uint32_t READY_MS = 800;  // as if the setup network took a moment to come up

// The setup page is real: the shared Portal is served on this computer, to open in a browser. Only
// joining the device's own network is faked.
class DesktopProvisioning : public Provisioning {
 public:
  void start(const SetupNetwork &network, SetupMode mode, const std::string &kept_ssid) override {
    url_ = "http://localhost:" + std::to_string(PORT);
    portal_.begin(url_, /*captive=*/false, mode, kept_ssid);  // no DNS takeover on a computer
    portal_.setNetworks({{"SimHomeWifi", -48, false}, {"Neighbours-5G", -71, false}, {"CoffeeShop", -83, true}});
    running_ = server_.begin(PORT);
    joined_ = false;
    startedAt_ = hal_millis();
    hal_log("[sim] setup network %s / %s (simulated)\n", network.ssid.c_str(), network.password.c_str());
    hal_log("[sim] setup page: %s  (opening it counts as joining)\n", url_.c_str());
    hal_log("[sim] Wi-Fi password \"wrong\" fails to connect. Keys: J = join/leave, S/W = quick submit\n");
  }

  void stop() override {
    server_.stop();
    running_ = false;
  }

  void update() override {
    if (!running_) return;
    if (pressed(SDL_SCANCODE_J)) joined_ = !joined_;
    if (pressed(SDL_SCANCODE_S)) quickSubmit("correct horse");
    if (pressed(SDL_SCANCODE_W)) quickSubmit("wrong");
    server_.poll([this](const HttpRequest &req) {
      joined_ = true;  // on the device, the phone has necessarily joined before it can load the page
      return portal_.handle(req);
    });
  }

  bool ready() const override { return running_ && hal_millis() - startedAt_ > READY_MS; }
  bool clientConnected() const override { return running_ && joined_; }
  const char *portalUrl() const override { return url_.c_str(); }

  bool takeSubmission(SetupSubmission &out) override {
    if (portal_.takeSubmission(out)) return true;
    if (!hasQuick_) return false;
    hasQuick_ = false;
    out = quick_;
    return true;
  }

 private:
  // Skips the form: canned Wi-Fi + account values (the account is fake: sign-in will say so).
  void quickSubmit(const char *wifi_password) {
    quick_.wifi = {"SimHomeWifi", wifi_password};
    quick_.email = "sim@example.com";
    quick_.accountPassword = "not-a-real-password";
    hasQuick_ = true;
  }

  // Key went down since the last call.
  bool pressed(SDL_Scancode key) {
    bool down = SDL_GetKeyboardState(nullptr)[key];
    bool edge = down && !wasDown_[key];
    wasDown_[key] = down;
    return edge;
  }

  HttpServer server_;
  Portal portal_;
  std::string url_;
  bool running_ = false;
  bool joined_ = false;
  uint32_t startedAt_ = 0;
  bool hasQuick_ = false;
  SetupSubmission quick_;
  bool wasDown_[SDL_NUM_SCANCODES] = {};
};

std::unique_ptr<Provisioning> create_provisioning() { return std::make_unique<DesktopProvisioning>(); }
