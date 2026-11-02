#include "services/wifi_station.h"

#include <SDL.h>

#include "hal/hal.h"
#include "http_server.h"  // lan_ip()

static constexpr uint32_t CONNECT_MS = 1500;
static constexpr uint32_t SCAN_MS = 1200;
static constexpr int LINK_RSSI = -54;  // dBm

// Connects after a short delay to the first network whose password isn't "wrong", so both paths
// can be exercised. Key D drops (and restores) the link, like walking away from every known network.
class FakeWifiStation : public WifiStation {
 public:
  void connect(const std::vector<WifiCredentials> &networks) override {
    networks_ = networks;
    state_ = State::Connecting;
    startedAt_ = hal_millis();
  }

  void disconnect() override { state_ = State::Idle; }

  void update() override {
    bool d = SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D];
    if (d && !dWasDown_) {
      dropped_ = !dropped_;
      hal_log("[sim] wi-fi link %s (D)\n", dropped_ ? "dropped" : "restored");
    }
    dWasDown_ = d;
    if (state_ != State::Connecting || hal_millis() - startedAt_ < CONNECT_MS) return;
    state_ = State::Failed;
    if (dropped_) {  // reconnecting while dropped fails, like out of range
      reason_ = "no known network nearby";
      return;
    }
    reason_ = networks_.empty() ? "no Wi-Fi saved" : "sign-in refused";
    for (const WifiCredentials &n : networks_) {
      if (n.password == "wrong") continue;
      state_ = State::Connected;
      ssid_ = n.ssid;
      break;
    }
  }

  bool linkUp() const override { return state_ == State::Connected && !dropped_; }
  std::string ip() const override { return linkUp() ? lan_ip() : ""; }
  int rssi() const override { return linkUp() ? LINK_RSSI : 0; }

  State state() const override { return state_; }
  const char *failureReason() const override { return reason_; }
  std::string ssid() const override { return state_ == State::Connected ? ssid_ : ""; }

  // A made-up neighborhood, long enough to scroll.
  void startScan() override { scanAt_ = hal_millis(); }
  bool scanning() const override { return scanAt_ && hal_millis() - scanAt_ < SCAN_MS; }
  std::vector<ScanResult> scanResults() const override {
    if (!scanAt_ || scanning()) return {};
    return {{"Home Wi-Fi", -48, true}, {"Cafe Guest", -61, false}, {"Bbox-1A2B3C4D", -66, true},
            {"Neighbor", -72, true},   {"Office", -79, true},       {"FreeWifi_secure", -85, true}};
  }

 private:
  std::vector<WifiCredentials> networks_;
  std::string ssid_;
  State state_ = State::Idle;
  uint32_t startedAt_ = 0;
  const char *reason_ = "";
  bool dropped_ = false, dWasDown_ = false;
  uint32_t scanAt_ = 0;
};

std::unique_ptr<WifiStation> create_wifi_station() { return std::make_unique<FakeWifiStation>(); }
