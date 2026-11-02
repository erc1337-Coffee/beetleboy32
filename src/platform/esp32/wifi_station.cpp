#include "services/wifi_station.h"

#include <WiFi.h>

#include <algorithm>
#include <atomic>

#include "hal/hal.h"

static constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;  // per network
// A refused try is tried again right away, until the network's time is up (after a reset the router
// often refuses the first ones: it still has the board's last connection).
static constexpr uint32_t REJOIN_MS = 1000;
// Above ~15 dBm the router never answers the board (measured: no reply at 17-20 dBm, joins at 15 and
// below): the board's full-power bursts come out garbled. Some range for a link that works.
static constexpr wifi_power_t TX_POWER = WIFI_POWER_13dBm;
static constexpr uint32_t SCAN_MS_PER_CHANNEL = 120;  // ~1.6 s over the 13 channels, disconnected
static constexpr uint32_t SCAN_TIMEOUT_MS = 6000;
// Scans from the Add screen run while connected, hopping back to the network's channel in between:
// slower, and past what Arduino's scanComplete() allows (20x the time per channel), so they're
// awaited on the driver's done bit instead.
static constexpr uint32_t USER_SCAN_TIMEOUT_MS = 15000;

class EspWifiStation : public WifiStation {
 public:
  void connect(const std::vector<WifiCredentials> &networks) override {
    WiFi.persistent(false);  // credentials live in our ConfigStore, not the Wi-Fi driver's own NVS
    WiFi.setAutoReconnect(false);  // the app reconnects (and picks the network) itself
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // modem sleep delays every round trip (light sleep turns Wi-Fi off anyway)
    static bool watching = false;
    if (!watching) {
      watching = true;
      WiFi.onEvent([this](arduino_event_id_t, arduino_event_info_t info) {
        uint8_t reason = info.wifi_sta_disconnected.reason;
        if (reason == WIFI_REASON_ASSOC_LEAVE) return;  // our own disconnect
        lastReason_ = reason;
        dropped_ = true;
      }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    }
    WiFi.disconnect();
    if (userScan_) WiFi.scanDelete();  // the connection's own scan replaces it
    userScan_ = false;
    known_ = networks;
    tries_.clear();
    noneInRange_ = false;
    state_ = State::Connecting;
    startedAt_ = millis();
    if (known_.size() <= 1) {  // nothing to choose from: no scan
      tries_ = known_;
      join(0);
    } else {
      scanning_ = true;
      WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false, /*passive=*/false, SCAN_MS_PER_CHANNEL);
    }
  }

  void disconnect() override {
    WiFi.scanDelete();
    WiFi.disconnect(true);
    scanning_ = userScan_ = false;
    state_ = State::Idle;
  }

  // Polled rather than event-driven: Wi-Fi events arrive on another task.
  void update() override {
    if (userScan_) pollUserScan();
    if (state_ != State::Connecting) return;
    if (scanning_) {
      int found = WiFi.scanComplete();
      if (found == WIFI_SCAN_RUNNING && millis() - startedAt_ < SCAN_TIMEOUT_MS) return;
      scanning_ = false;
      pickFromScan(found);
      join(0);
      return;
    }
    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      state_ = State::Connected;
      ssid_ = tries_[try_].ssid;
      // Syncs in the background (hal.cpp saves it to the RTC). configTime() would reset TZ: keep ours.
      const char *tz = getenv("TZ");
      configTzTime(tz ? tz : "UTC0", "pool.ntp.org", "time.google.com");
    } else if (millis() - startedAt_ > CONNECT_TIMEOUT_MS) {
      const uint8_t reason = lastReason_;
      const char *why = reason == WIFI_REASON_NO_AP_FOUND ? "not found"
                        : reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                  reason == WIFI_REASON_HANDSHAKE_TIMEOUT
                            ? "sign-in refused"
                            : "timed out";
      hal_log("[wifi] \"%s\": %s (%u tries, reason %u)\n", tries_[try_].ssid.c_str(), why, (unsigned)joins_,
              (unsigned)reason);
      if (try_ + 1 < tries_.size()) {
        join(try_ + 1);
      } else {
        fail(noneInRange_ ? "no known network nearby" : why);
      }
    } else if (dropped_ && millis() - joinedAt_ >= REJOIN_MS) {
      begin();
    }
  }

  State state() const override { return state_; }
  const char *failureReason() const override { return reason_; }
  std::string ssid() const override { return state_ == State::Connected ? ssid_ : ""; }

  void startScan() override {
    if (state_ == State::Connecting || userScan_) return;
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
    userScan_ = true;
    userScanAt_ = millis();
    int started = WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false, /*passive=*/false, SCAN_MS_PER_CHANNEL);
    if (started != WIFI_SCAN_RUNNING) hal_log("[wifi] scan didn't start (%d)\n", started);
  }
  bool scanning() const override { return userScan_; }
  std::vector<ScanResult> scanResults() const override { return scanResults_; }

  bool linkUp() const override { return state_ == State::Connected && WiFi.status() == WL_CONNECTED; }
  std::string ip() const override { return linkUp() ? WiFi.localIP().toString().c_str() : ""; }
  int rssi() const override { return linkUp() ? WiFi.RSSI() : 0; }

 private:
  // The known networks the scan saw, strongest first; if it saw none, the most recent one anyway.
  void pickFromScan(int found) {
    std::vector<std::pair<int, size_t>> seen;  // rssi, index in known_
    for (int i = 0; i < found; i++) {
      for (size_t k = 0; k < known_.size(); k++) {
        if (WiFi.SSID(i) != known_[k].ssid.c_str()) continue;
        bool already = false;
        for (auto &s : seen) {
          if (s.second == k) {
            s.first = std::max(s.first, (int)WiFi.RSSI(i));  // several access points, one name
            already = true;
          }
        }
        if (!already) seen.push_back({WiFi.RSSI(i), k});
      }
    }
    WiFi.scanDelete();
    std::stable_sort(seen.begin(), seen.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &s : seen) tries_.push_back(known_[s.second]);
    noneInRange_ = tries_.empty();
    if (noneInRange_ && !known_.empty()) tries_.push_back(known_[0]);
    hal_log("[wifi] %d networks around, %u known\n", found < 0 ? 0 : found, (unsigned)seen.size());
  }

  void pollUserScan() {
    const bool done = WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT;
    if (!done && millis() - userScanAt_ < USER_SCAN_TIMEOUT_MS) return;
    int found = done ? WiFi.scanComplete() : WIFI_SCAN_FAILED;  // once done, scanComplete() returns the count
    userScan_ = false;
    scanResults_.clear();
    if (found < 0) hal_log("[wifi] scan failed (%d) after %u ms\n", found, (unsigned)(millis() - userScanAt_));
    for (int i = 0; i < found; i++) {
      std::string name = WiFi.SSID(i).c_str();
      if (name.empty()) continue;
      bool known = false;
      for (ScanResult &r : scanResults_) {
        if (r.ssid != name) continue;
        r.rssi = std::max(r.rssi, (int)WiFi.RSSI(i));
        known = true;
      }
      if (!known) scanResults_.push_back({name, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    }
    WiFi.scanDelete();
    std::sort(scanResults_.begin(), scanResults_.end(), [](const ScanResult &a, const ScanResult &b) { return a.rssi > b.rssi; });
  }

  void join(size_t i) {
    if (i >= tries_.size()) {
      fail("no Wi-Fi saved");
      return;
    }
    try_ = i;
    startedAt_ = millis();
    joins_ = 0;
    lastReason_ = 0;
    if (i > 0) WiFi.disconnect();
    begin();
  }

  void begin() {
    dropped_ = false;
    joins_++;
    joinedAt_ = millis();
    WiFi.setTxPower(TX_POWER);  // here: right after WiFi.mode() the driver isn't started and refuses it
    WiFi.begin(tries_[try_].ssid.c_str(), tries_[try_].password.c_str());
  }

  void fail(const char *reason) {
    reason_ = reason;
    state_ = State::Failed;
    WiFi.disconnect(true);
  }

  std::vector<WifiCredentials> known_, tries_;
  size_t try_ = 0;
  unsigned joins_ = 0;          // tries on the current network
  uint32_t joinedAt_ = 0;       // when the last try started
  std::atomic<bool> dropped_{false};     // the last try failed (set from the Wi-Fi task)
  std::atomic<uint8_t> lastReason_{0};   // the driver's why (0: none yet)
  bool scanning_ = false, noneInRange_ = false;
  bool userScan_ = false;
  uint32_t userScanAt_ = 0;
  std::vector<ScanResult> scanResults_;
  State state_ = State::Idle;
  uint32_t startedAt_ = 0;
  const char *reason_ = "";
  std::string ssid_;
};

std::unique_ptr<WifiStation> create_wifi_station() { return std::make_unique<EspWifiStation>(); }
