#include "account/site_api.h"

#include <ctype.h>

#include "util/text.h"

const char *const SITE = "https://www.remilia.net";

HttpHeaders site_auth(const std::string &access_token) { return {{"Cookie", "authToken=" + access_token}}; }

// Some limits come back with another status but say so in the message.
static bool says_rate_limited(const std::string &detail) {
  const std::string message = lowercase(detail);
  return message.find("rate limit") != std::string::npos || message.find("too many requests") != std::string::npos;
}

static AuthResult failure(const std::string &path, const HttpResult &r, std::string &detail) {
  if (r.status == 0) {
    detail = path + ": " + r.statusText();
    return AuthResult::NetworkError;
  }
  JsonDocument doc;
  const char *message = nullptr;
  if (!deserializeJson(doc, r.body)) message = doc["message"] | (doc["error"] | (const char *)nullptr);
  detail = path + ": " + (message ? std::string(message) : r.statusText());
  if (r.status == 429 || says_rate_limited(detail)) return AuthResult::RateLimited;
  return r.status == 401 || r.status == 403 ? AuthResult::Rejected : AuthResult::NetworkError;
}

AuthResult site_get_json(HttpClient &http, const std::string &access_token, const std::string &path,
                         const JsonDocument &filter, JsonDocument &doc, std::string &detail) {
  HttpResult r = http.get(SITE + path, site_auth(access_token));
  if (r.status != 200) return failure(path, r, detail);
  if (DeserializationError e = deserializeJson(doc, r.body, DeserializationOption::Filter(filter))) {
    // IncompleteInput = the body was cut short; InvalidInput = not JSON (an HTML error page...).
    std::string start = r.body.substr(0, 40);
    for (char &c : start)
      if (!isprint((unsigned char)c)) c = ' ';
    detail = path + ": bad JSON (" + e.c_str() + ", " + std::to_string(r.body.size()) + " bytes: " + start + ")";
    return AuthResult::NetworkError;
  }
  return AuthResult::Ok;
}

AuthResult site_post_json(HttpClient &http, const std::string &access_token, const std::string &path,
                          const std::string &body, const JsonDocument &filter, JsonDocument &doc,
                          std::string &detail) {
  HttpResult r = body.empty() ? http.post(SITE + path, nullptr, body, site_auth(access_token))  // no type, like the website
                              : http.postJson(SITE + path, body, site_auth(access_token));
  if (r.status < 200 || r.status >= 300) return failure(path, r, detail);
  if (deserializeJson(doc, r.body, DeserializationOption::Filter(filter))) doc.clear();
  detail = "ok";
  return AuthResult::Ok;
}
