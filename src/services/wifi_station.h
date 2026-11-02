#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/device_config.h"

// Connection to the user's Wi-Fi. Driven from the main loop via update(); never touches LVGL.
class WifiStation {
 public:
  enum class State { Idle, Connecting, Connected, Failed };
  struct ScanResult {
    std::string ssid;
    int rssi;  // dBm
    bool secured;
  };

  virtual ~WifiStation() = default;
  // Joins one of `networks` (in order of preference): the strongest in range, or the first one if
  // none shows up (it may be hidden). Failed when none of them could be joined.
  virtual void connect(const std::vector<WifiCredentials> &networks) = 0;
  virtual void disconnect() = 0;
  virtual void update() = 0;

  virtual State state() const = 0;
  virtual const char *failureReason() const = 0;  // human readable; meaningful in State::Failed
  virtual std::string ssid() const = 0;           // the network joined; meaningful once Connected

  // Looks for the networks around, in the background (connected or not); not while connecting.
  virtual void startScan() = 0;
  virtual bool scanning() const = 0;
  virtual std::vector<ScanResult> scanResults() const = 0;  // the last scan: one per name, strongest first

  // Live link, after Connected: false once the network drops (the state stays Connected).
  virtual bool linkUp() const = 0;
  virtual std::string ip() const = 0;  // "" without a link
  virtual int rssi() const = 0;        // dBm; 0 without a link
};

std::unique_ptr<WifiStation> create_wifi_station();  // per platform
