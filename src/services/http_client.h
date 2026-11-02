#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpResult {
  int status = 0;        // 0 = no response (network/TLS error, see `error`)
  std::string location;  // Location header, for redirects
  std::string body;
  std::string error;
  HttpHeaders cookies;   // Set-Cookie: name and value (attributes dropped), in order received

  // "HTTP 404", or the network error when there was no response.
  std::string statusText() const { return status ? "HTTP " + std::to_string(status) : error; }
};

// Outgoing HTTPS. Blocking: call it off the main loop. Never follows redirects, so callers can
// read Location themselves (the Keycloak silent login needs the code in it).
class HttpClient {
 public:
  virtual ~HttpClient() = default;
  virtual HttpResult get(const std::string &url, const HttpHeaders &headers) = 0;
  virtual HttpResult post(const std::string &url, const char *content_type, const std::string &body,
                          const HttpHeaders &headers) = 0;  // content_type nullptr: no such header

  HttpResult postForm(const std::string &url, const std::string &body, const HttpHeaders &headers) {
    return post(url, "application/x-www-form-urlencoded", body, headers);
  }
  HttpResult postJson(const std::string &url, const std::string &body, const HttpHeaders &headers) {
    return post(url, "application/json", body, headers);
  }
};

std::unique_ptr<HttpClient> create_http_client();  // per platform; safe to call from any thread
