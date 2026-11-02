#include "account/profile.h"

#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "account/site_api.h"

static constexpr int AVATAR_SIZE = 96;

// The own avatar has one cache slot, tagged with its URL: picking another picture on the website
// changes the URL, so it's downloaded again and replaces the old one.
static constexpr const char *AVATAR_KEY = "own-avatar";
static constexpr const char *SAVED_PROFILE_KEY = "PROFILE";  // ConfigStore

static bool cached_avatar(AssetCache &cache, const std::string &url, OwnedImage &out) {
  std::string bytes;
  if (!cache.load(AVATAR_KEY, bytes)) return false;
  size_t end = bytes.find('\n');
  return end != std::string::npos && bytes.compare(0, end, url) == 0 &&
         OwnedImage::deserialize(bytes.substr(end + 1), out);
}

// Ids come as numbers or strings.
static std::string id_text(JsonVariantConst v) { return v.is<long>() ? std::to_string(v.as<long>()) : std::string(v | ""); }

static void read_editable(JsonVariantConst u, Profile &out) {
  out.pfpProject = u["pfp"]["project"] | "";
  out.pfpId = id_text(u["pfp"]["id"]);
  out.location = u["location"] | "";
  out.achievements.clear();
  for (JsonVariantConst a : u["achievementsDisplayed"].as<JsonArrayConst>()) out.achievements.push_back(a["id"] | 0);
  out.friends.clear();
  for (JsonVariantConst f : u["friendsDisplayed"].as<JsonArrayConst>()) out.friends.push_back(f["userHandle"] | "");
  out.pfps.clear();
  for (JsonPairConst project : u["property"].as<JsonObjectConst>()) {
    if (strcmp(project.key().c_str(), "Banners") == 0) continue;  // not pictures of a character
    for (JsonVariantConst token : project.value().as<JsonArrayConst>()) {
      std::string url = token["pfpUrl"] | "";
      if (!url.empty()) out.pfps.push_back({project.key().c_str(), id_text(token["id"]), url});
    }
  }
  std::sort(out.pfps.begin(), out.pfps.end(), [](const PfpChoice &a, const PfpChoice &b) {
    return a.project != b.project ? a.project < b.project : atol(a.id.c_str()) < atol(b.id.c_str());
  });
  out.editable = u["pfp"].is<JsonObjectConst>();
}

AuthResult fetch_profile(HttpClient &http, AssetCache &cache, const std::string &access_token, Profile &out,
                         std::string &detail) {
  AuthResult r;

  JsonDocument filter, doc;
  filter["userHandle"] = true;
  if ((r = site_get_json(http, access_token, "/api/profile/whoami", filter, doc, detail)) != AuthResult::Ok) return r;
  out.handle = doc["userHandle"] | "";

  filter.clear();
  doc.clear();
  for (const char *f : {"displayName", "bio", "theme", "color", "cover", "pfpUrl", "pfp", "location"})
    filter["user"][f] = true;
  filter["user"]["achievementsDisplayed"][0]["id"] = true;
  filter["user"]["friendsDisplayed"][0]["userHandle"] = true;
  filter["user"]["property"]["*"][0]["id"] = true;
  filter["user"]["property"]["*"][0]["pfpUrl"] = true;
  if ((r = site_get_json(http, access_token, "/api/profile/~" + out.handle, filter, doc, detail)) != AuthResult::Ok) return r;
  out.displayName = doc["user"]["displayName"] | out.handle.c_str();
  out.bio = doc["user"]["bio"] | "";
  out.theme.colored = strcmp(doc["user"]["theme"] | "default", "color") == 0;
  out.theme.hue = doc["user"]["color"] | 0;
  out.cover = doc["user"]["cover"] | "";
  out.pfpUrl = doc["user"]["pfpUrl"] | "";
  if (!out.pfpUrl.empty()) cached_avatar(cache, out.pfpUrl, out.avatar);
  read_editable(doc["user"], out);
  detail = "ok";
  return AuthResult::Ok;
}

static std::string profile_json(const Profile &p) {
  JsonDocument doc;
  doc["handle"] = p.handle;
  doc["displayName"] = p.displayName;
  doc["bio"] = p.bio;
  doc["colored"] = p.theme.colored;
  doc["hue"] = p.theme.hue;
  doc["cover"] = p.cover;
  doc["pfpUrl"] = p.pfpUrl;
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool load_saved_profile(ConfigStore &store, AssetCache &cache, Profile &out) {
  std::string json;
  JsonDocument doc;
  if (!store.get(SAVED_PROFILE_KEY, json) || deserializeJson(doc, json)) return false;
  out.handle = doc["handle"] | "";
  if (out.handle.empty()) return false;
  out.displayName = doc["displayName"] | out.handle.c_str();
  out.bio = doc["bio"] | "";
  out.theme.colored = doc["colored"] | false;
  out.theme.hue = doc["hue"] | 0;
  out.cover = doc["cover"] | "";
  out.pfpUrl = doc["pfpUrl"] | "";
  if (!out.pfpUrl.empty()) cached_avatar(cache, out.pfpUrl, out.avatar);
  return true;
}

void save_profile(ConfigStore &store, const Profile &profile) {
  std::string json = profile_json(profile), saved;
  if (store.get(SAVED_PROFILE_KEY, saved) && saved == json) return;  // spare the flash
  store.set(SAVED_PROFILE_KEY, json);
}

bool fetch_avatar(HttpClient &http, AssetCache &cache, const std::string &access_token, const std::string &url,
                  OwnedImage &out, std::string &detail) {
  HttpResult img = http.get(SITE + url, site_auth(access_token));
  if (img.status != 200 || !OwnedImage::fromWebp(img.body, AVATAR_SIZE, out)) {
    detail = img.status == 200 ? "not a WebP" : img.statusText();
    return false;
  }
  cache.store(AVATAR_KEY, url + "\n" + out.serialize());
  detail = "ok";
  return true;
}

AuthResult update_pfp(HttpClient &http, const std::string &access_token, const Profile &profile,
                      const std::string &project, const std::string &id, std::string &detail) {
  if (!profile.editable) {
    detail = "profile not loaded";
    return AuthResult::NetworkError;
  }
  JsonDocument body;
  body["pfp"]["project"] = project;
  body["pfp"]["id"] = id;
  body["bio"] = profile.bio;
  body["displayName"] = profile.displayName;
  body["userHandle"] = profile.handle;
  JsonArray achievements = body["achievementsDisplayed"].to<JsonArray>();
  for (int a : profile.achievements) achievements.add(a);
  JsonArray friends = body["friendsDisplayed"].to<JsonArray>();
  for (const std::string &f : profile.friends) friends.add(f);
  body["location"] = profile.location;
  std::string json;
  serializeJson(body, json);
  JsonDocument filter, doc;  // the response isn't used: the profile is read again
  return site_post_json(http, access_token, "/api/profile/updateProfile", json, filter, doc, detail);
}

AuthResult update_theme(HttpClient &http, const std::string &access_token, const ThemeSetting &theme,
                        const std::string &cover, std::string &detail) {
  static const char *const PATH = "/api/profile/updateThemeAndCover";
  JsonDocument body;
  body["theme"] = theme.colored ? "color" : "default";
  body["color"] = theme.hue;
  body["cover"] = cover;
  std::string json;
  serializeJson(body, json);
  JsonDocument filter, doc;  // the response isn't used
  return site_post_json(http, access_token, PATH, json, filter, doc, detail);
}
