#include "account/beetle.h"

#include <string.h>

#include <algorithm>

#include "account/site_api.h"
#include "assets/item_icons.h"
#include "hal/hal.h"
#include "util/text.h"

static constexpr int64_t HUNT_CYCLE_MS = 90LL * 60 * 1000;

static uint32_t ms_left(uint32_t cooldown, uint32_t fetched_at, uint32_t now) {
  uint32_t elapsed = now - fetched_at;
  return cooldown > elapsed ? cooldown - elapsed : 0;
}

long BeetleState::count(const std::string &key) const {
  auto it = items().find(key);
  return it == items().end() ? 0 : it->second;
}

const std::map<std::string, long> &BeetleState::items() const {
  static const std::map<std::string, long> NONE;
  return inventory ? *inventory : NONE;
}

uint32_t BeetleState::cheeseMsLeft(uint32_t now) const { return ms_left(cheeseCooldown, fetchedAt, now); }
uint32_t BeetleState::trapMsLeft(uint32_t now) const { return ms_left(trapCooldown, fetchedAt, now); }
uint32_t BeetleState::junkMsLeft(uint32_t now) const { return ms_left(junkCooldown, fetchedAt, now); }

int BeetleState::huntsRemaining(int64_t epoch_ms) const {
  // The server can keep reporting 3 used after the cycle is over: 90 min after the last hunt,
  // the count starts again. Without a clock, trust the count.
  bool cycle_over = lastHuntDate > 0 && epoch_ms > 0 && epoch_ms - lastHuntDate >= HUNT_CYCLE_MS;
  int used = cycle_over ? 0 : huntsUsed;
  return used < MAX_HUNTS ? MAX_HUNTS - used : 0;
}

bool BeetleState::beetlesReady(uint32_t now, int64_t epoch_ms) const {
  return trapMsLeft(now) == 0 || huntsRemaining(epoch_ms) > 0;
}

uint32_t BeetleState::beetlesMsLeft(uint32_t now, int64_t epoch_ms) const {
  if (beetlesReady(now, epoch_ms)) return 0;
  uint32_t trap = trapMsLeft(now);
  if (epoch_ms <= 0 || lastHuntDate <= 0) return trap;  // hunt refill time unknown
  int64_t refill = lastHuntDate + HUNT_CYCLE_MS - epoch_ms;
  return refill < (int64_t)trap ? (uint32_t)(refill > 0 ? refill : 0) : trap;
}

// Only the fields BeetleState reads.
template <typename Filter>
static void user_filter(Filter f) {
  f["level"] = true;
  f["beetle_count"] = true;
  f["inventory"] = true;  // ~150 keys: crafting needs them all
  f["hammers"][0]["hammer"] = true;
  f["hammers"][0]["break_rate"] = true;
  f["crafting_slot3_unlocked"] = true;
  f["beetleHuntsUsed"] = true;
  f["lastBeetleHuntDate"] = true;
  f["cooldowns"]["claimUBC"] = true;
  f["cooldowns"]["catchBeetle"] = true;
  f["cooldowns"]["junkFaucet"] = true;
}

static void read_user(JsonVariantConst u, BeetleState &s) {
  s.level = u["level"] | 0;
  s.beetles = u["beetle_count"] | 0L;
  s.cheese = u["inventory"]["cheese"] | 0L;
  auto inventory = std::make_shared<std::map<std::string, long>>();
  for (JsonPairConst item : u["inventory"].as<JsonObjectConst>()) (*inventory)[item.key().c_str()] = item.value() | 0L;
  s.inventory = std::move(inventory);
  s.hammers.clear();
  for (JsonVariantConst h : u["hammers"].as<JsonArrayConst>()) {
    std::string key = h["hammer"] | "";
    if (!key.empty()) s.hammers.push_back({key, h["break_rate"] | 0});
  }
  s.slot3Unlocked = u["crafting_slot3_unlocked"] | false;
  s.huntsUsed = u["beetleHuntsUsed"] | 0;
  s.lastHuntDate = u["lastBeetleHuntDate"] | (int64_t)0;
  s.cheeseCooldown = u["cooldowns"]["claimUBC"] | 0u;  // missing = ready
  s.trapCooldown = u["cooldowns"]["catchBeetle"] | 0u;
  s.junkCooldown = u["cooldowns"]["junkFaucet"] | 0u;
  s.fetchedAt = hal_millis();
  s.loaded = true;
}

// beetleCard has only been seen as one object, but may be a list: take either.
static void read_items(JsonVariantConst v, bool secondary, std::vector<BeetleItem> &out) {
  if (v.is<JsonArrayConst>()) {
    for (JsonVariantConst item : v.as<JsonArrayConst>()) read_items(item, secondary, out);
  } else if (v.is<JsonObjectConst>()) {
    BeetleItem item;
    item.key = v["beetle"] | "";
    item.name = v["beetle_name"] | item.key.c_str();
    item.icon = v["icon"] | "";
    item.category = v["category"] | 0;
    item.rarity = v["rarity"] | 0;
    item.secondary = secondary;
    out.push_back(std::move(item));
  }
}

// The error a trap claim gets when it isn't ready: /cooldown|wait|not.?ready/i.
static bool says_not_ready(const std::string &detail) {
  const std::string message = lowercase(detail);
  if (message.find("cooldown") != std::string::npos || message.find("wait") != std::string::npos) return true;
  for (size_t p = message.find("not"); p != std::string::npos; p = message.find("not", p + 1)) {
    size_t ready = message.find("ready", p + 3);
    if (ready != std::string::npos && ready <= p + 4) return true;
  }
  return false;
}

AuthResult fetch_beetle_state(HttpClient &http, const std::string &access_token, BeetleState &out,
                              std::string &detail) {
  JsonDocument filter, doc;
  user_filter(filter.to<JsonObject>());
  AuthResult r = site_get_json(http, access_token, "/api/beetle/user", filter, doc, detail);
  if (r == AuthResult::Ok) read_user(doc.as<JsonVariantConst>(), out);
  return r;
}

// POST an action (body is always {}). On success `state` takes the returned user, refetched if
// the response had none, and the won items go in `step`.
static AuthResult post_action(HttpClient &http, const std::string &access_token, const char *action,
                              BeetleState &state, BeetleStep &step, std::string &detail) {
  JsonDocument filter, doc;
  filter["result"]["beetleCard"] = true;
  filter["result"]["secondaryItemDrops"] = true;
  user_filter(filter["user"].to<JsonObject>());
  AuthResult r = site_post_json(http, access_token, std::string("/api/beetle/action/") + action, "{}", filter,
                                doc, detail);
  if (r != AuthResult::Ok) return r;

  read_items(doc["result"]["beetleCard"], false, step.items);
  // The junk faucet's whole win comes as secondaryItemDrops: shown as the main item.
  read_items(doc["result"]["secondaryItemDrops"], step.kind != BeetleStep::Kind::Junk, step.items);
  if (doc["user"].is<JsonObjectConst>()) {
    read_user(doc["user"], state);
  } else {
    std::string ignored;
    fetch_beetle_state(http, access_token, state, ignored);  // stale state is caught by the next run
  }
  for (const BeetleItem &item : step.items) hal_log("[beetle] %s: %s\n", action, item.name.c_str());
  return AuthResult::Ok;
}

AuthResult beetle_action(HttpClient &http, const std::string &access_token, BeetleStep::Kind kind,
                         BeetleState &state, std::vector<BeetleStep> &steps, std::string &detail) {
  const char *action = kind == BeetleStep::Kind::Cheese ? "claimUBC"
                       : kind == BeetleStep::Kind::Trap ? "catchBeetle"
                       : kind == BeetleStep::Kind::Junk ? "junkFaucet"
                                                        : "beetleHunt";
  BeetleStep step{kind};
  AuthResult r = post_action(http, access_token, action, state, step, detail);
  if (r == AuthResult::Ok) {
    steps.push_back(std::move(step));
    return r;
  }
  std::string ignored;
  if (r == AuthResult::NetworkError) fetch_beetle_state(http, access_token, state, ignored);  // not when limited
  if (kind == BeetleStep::Kind::Trap && r == AuthResult::NetworkError && says_not_ready(detail)) {
    detail = "trap not ready (" + detail + ")";
    return AuthResult::Ok;
  }
  return r;
}

AuthResult beetle_craft(HttpClient &http, const std::string &access_token, const CraftRequest &request,
                        BeetleState &state, CraftReply &reply, std::string &detail) {
  // Every field, as the website sends them: empty ones as "", filled from slot1 up. Type 1 assembles,
  // 2 smashes.
  JsonDocument body;
  body["type"] = request.smash ? 2 : 1;
  int filled = 0;
  for (const std::string &slot : request.slots)
    if (!slot.empty()) body["slot" + std::to_string(++filled)] = slot;
  while (filled < CraftRequest::SLOTS) body["slot" + std::to_string(++filled)] = "";
  body["slot4"] = "";
  body["sacrifice"] = request.smash ? request.sacrifice : "";
  body["hammer"] = request.smash ? request.hammer : "";
  std::string json;
  serializeJson(body, json);

  JsonDocument filter, doc;
  filter["success"] = true;
  filter["message"] = true;
  filter["hammer_broke"] = true;
  filter["result"] = true;
  user_filter(filter["user"].to<JsonObject>());
  AuthResult r = site_post_json(http, access_token, "/api/beetle/action/craft", json, filter, doc, detail);
  if (r != AuthResult::Ok) return r;

  reply.success = doc["success"] | false;
  reply.message = doc["message"] | "";
  reply.hammerBroke = doc["hammer_broke"] | false;
  if (reply.success) {
    std::vector<BeetleItem> items;
    read_items(doc["result"], false, items);
    if (!items.empty()) reply.item = items[0];
  }
  if (doc["user"].is<JsonObjectConst>()) {
    read_user(doc["user"], state);
  } else {
    std::string ignored;
    fetch_beetle_state(http, access_token, state, ignored);
  }
  detail = reply.message + (reply.hammerBroke ? ", hammer broke" : "");
  hal_log("[beetle] craft: %s %s\n", reply.message.c_str(), reply.item.key.c_str());
  return AuthResult::Ok;
}

bool bundled_item_icon(const std::string &icon_path, int max_size, OwnedImage &out) {
  const BundledIcon *end = bundled_item_icons + bundled_item_icon_count;
  const BundledIcon *it = std::lower_bound(bundled_item_icons, end, icon_path.c_str(),
                                           [](const BundledIcon &i, const char *p) { return strcmp(i.path, p) < 0; });
  if (it == end || icon_path != it->path) return false;
  return OwnedImage::fromPng(std::string((const char *)it->png, it->size), max_size, out);
}

bool local_item_icon(AssetCache &cache, const std::string &icon_path, int max_size, OwnedImage &out) {
  std::string bytes;
  return bundled_item_icon(icon_path, max_size, out) ||
         (cache.load(icon_path, bytes) && OwnedImage::deserialize(bytes, out));
}

bool load_item_icon(HttpClient &http, const std::string &access_token, AssetCache &cache, const std::string &icon_path,
                    int max_size, OwnedImage &out) {
  if (local_item_icon(cache, icon_path, max_size, out)) return true;
  if (access_token.empty() || !fetch_item_icon(http, access_token, icon_path, max_size, out)) return false;
  cache.store(icon_path, out.serialize());
  return true;
}

bool fetch_item_icon(HttpClient &http, const std::string &access_token, const std::string &icon_path, int max_size,
                     OwnedImage &out, int *status) {
  HttpResult r = http.get(SITE + icon_path, site_auth(access_token));
  if (status) *status = r.status;
  // Item icons are PNGs; PFP thumbnails (the site's image proxy) are WebPs, cropped square.
  const bool webp = r.body.size() > 12 && r.body.compare(0, 4, "RIFF") == 0 && r.body.compare(8, 4, "WEBP") == 0;
  const bool ok = r.status == 200 && (webp ? OwnedImage::fromWebp(r.body, max_size, out) : OwnedImage::fromPng(r.body, max_size, out));
  if (!ok) {
    std::string why = r.status == 200 ? "not a PNG or WebP" : r.statusText();
    hal_log("[beetle] icon %s: %s\n", icon_path.c_str(), why.c_str());
    return false;
  }
  return true;
}
