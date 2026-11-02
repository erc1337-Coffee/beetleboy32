#include "account/pokes.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "account/site_api.h"

static constexpr const char *NOTIFICATIONS_PATH = "/api/notifications?states=pending%2Cunread&limit=10";
// Every state (the ones read too), a page at a time ("&page=N" added).
static constexpr const char *FEED_PATH = "/api/notifications?states=all&limit=100";
// Users without a PFP of their own get a generated one, linked in full from this host (a PNG). It's
// public: fetched without the login token.
static constexpr const char *DEFAULT_PFP_HOST = "https://pfp.remilia.net/";

// {"global": {...}, "notifications": [{_id, type ("poke", "page_view"...), content.from, updatedAt...}],
//  "pagination": {"page", "limit", "hasMore"}}
static AuthResult read_pokes(HttpClient &http, const std::string &access_token, const std::string &path,
                             std::vector<Poke> &out, bool &more, std::string &detail) {
  JsonDocument filter, doc;
  JsonObject f = filter["notifications"][0].to<JsonObject>();
  for (const char *field : {"_id", "type", "updatedAt", "createdAt"}) f[field] = true;
  for (const char *field : {"userHandle", "displayName", "pfpUrl"}) f["content"]["from"][field] = true;
  filter["pagination"]["hasMore"] = true;
  AuthResult r = site_get_json(http, access_token, path, filter, doc, detail);
  if (r != AuthResult::Ok) return r;
  out.clear();
  more = doc["pagination"]["hasMore"] | false;
  for (JsonVariantConst n : doc["notifications"].as<JsonArrayConst>()) {
    if (strcmp(n["type"] | "", "poke") != 0) continue;
    JsonVariantConst from = n["content"]["from"];
    Poke p;
    p.id = n["_id"] | "";
    p.handle = from["userHandle"] | "";
    p.displayName = from["displayName"] | "";  // may be "": the handle shows then
    p.pfpUrl = from["pfpUrl"] | "";
    p.at = n["updatedAt"] | (n["createdAt"] | (int64_t)0);
    if (!p.handle.empty()) out.push_back(p);
  }
  detail = "ok";
  return AuthResult::Ok;
}

AuthResult fetch_pokes(HttpClient &http, const std::string &access_token, std::vector<Poke> &out, std::string &detail) {
  bool more;
  return read_pokes(http, access_token, NOTIFICATIONS_PATH, out, more, detail);
}

AuthResult fetch_poke_feed(HttpClient &http, const std::string &access_token, int page, std::vector<Poke> &out,
                           bool &more, std::string &detail) {
  return read_pokes(http, access_token, std::string(FEED_PATH) + "&page=" + std::to_string(page), out, more, detail);
}

AuthResult fetch_poke_status(HttpClient &http, const std::string &access_token, Poke &poke, PokeStatus &out,
                             std::string &detail) {
  JsonDocument filter, doc;
  filter["viewerContext"]["canPoke"] = true;
  filter["viewerContext"]["pokeCooldownSeconds"] = true;
  filter["user"]["displayName"] = true;
  filter["user"]["pfpUrl"] = true;
  AuthResult r = site_get_json(http, access_token, "/api/profile/~" + poke.handle, filter, doc, detail);
  if (r != AuthResult::Ok) return r;
  out.canPoke = doc["viewerContext"]["canPoke"] | false;
  out.cooldownS = doc["viewerContext"]["pokeCooldownSeconds"] | 0L;
  if (poke.displayName.empty()) poke.displayName = doc["user"]["displayName"] | "";
  if (poke.pfpUrl.empty()) poke.pfpUrl = doc["user"]["pfpUrl"] | "";
  detail = "ok";
  return AuthResult::Ok;
}

bool fetch_poke_pfp(HttpClient &http, const std::string &access_token, const std::string &url, int size,
                    OwnedImage &out) {
  if (url.compare(0, strlen(DEFAULT_PFP_HOST), DEFAULT_PFP_HOST) == 0) {
    HttpResult r = http.get(url, {});
    return r.status == 200 && OwnedImage::fromPng(r.body, size, out);
  }
  if (url.empty() || url[0] != '/') return false;  // a site path; nothing else gets the token
  HttpResult r = http.get(SITE + url, site_auth(access_token));
  return r.status == 200 && OwnedImage::fromWebp(r.body, size, out);
}

AuthResult mark_notification_read(HttpClient &http, const std::string &access_token, const std::string &id,
                                  std::string &detail) {
  JsonDocument body, filter, doc;  // {"success":true,"modifiedCount":1}: not used
  body["ids"][0] = id;
  std::string json;
  serializeJson(body, json);
  return site_post_json(http, access_token, "/api/notifications/read", json, filter, doc, detail);
}

AuthResult poke_user(HttpClient &http, const std::string &access_token, const std::string &handle, std::string &detail) {
  JsonDocument filter, doc;  // {"data":{"target_handle","poked_at"}}: not used
  return site_post_json(http, access_token, "/api/users/" + handle + "/poke", "", filter, doc, detail);
}

std::string poke_wait_text(long s) {
  char text[24];
  if (s >= 3600) {
    snprintf(text, sizeof text, "%ldh %ldm", s / 3600, s % 3600 / 60);
  } else if (s >= 60) {
    snprintf(text, sizeof text, "%ldm", (s + 59) / 60);
  } else {
    snprintf(text, sizeof text, "%lds", s);
  }
  return text;
}
