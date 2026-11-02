#pragma once

#include <ArduinoJson.h>

#include <string>

#include "account/keycloak.h"
#include "services/http_client.h"

// remilia.net API calls, authenticated like the website: the access token as the authToken cookie.
// All blocking. Failures: Rejected for 401/403, RateLimited for 429 (or a "rate limit" message),
// NetworkError otherwise; `detail` says why.

extern const char *const SITE;  // "https://www.remilia.net"

HttpHeaders site_auth(const std::string &access_token);

// GET + JSON parse; `filter` keeps only the fields read (some responses are ~120KB).
AuthResult site_get_json(HttpClient &http, const std::string &access_token, const std::string &path,
                         const JsonDocument &filter, JsonDocument &doc, std::string &detail);

// POST a JSON body ("": none, no Content-Type). Any 2xx is success; `doc` gets the filtered response,
// or stays empty if it isn't JSON. On failure `detail` carries the body's `message` or `error` when
// there is one.
AuthResult site_post_json(HttpClient &http, const std::string &access_token, const std::string &path,
                          const std::string &body, const JsonDocument &filter, JsonDocument &doc,
                          std::string &detail);
