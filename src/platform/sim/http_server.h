#pragma once

#include <stdint.h>

#include <functional>
#include <string>

#include "portal/portal.h"

// Minimal HTTP/1.1 server for the sim: one request per connection, form bodies only.
// Polled from the main loop like the ESP32 WebServer, so there are no threads.
class HttpServer {
 public:
  using Handler = std::function<HttpResponse(const HttpRequest &)>;

  ~HttpServer() { stop(); }
  bool begin(uint16_t port);  // listens on this computer only (loopback): the form takes passwords
  void stop();
  void poll(const Handler &handler);  // serves pending connections, never waits for new ones

 private:
  int fd_ = -1;
};

std::string lan_ip();  // this machine's LAN IPv4 address, or "localhost"
