#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "account/keycloak.h"
#include "media/image.h"
#include "services/asset_cache.h"
#include "services/http_client.h"

// The beetle game: counters, cooldowns, and the claim/hunt actions.

// A hammer the user has (user.hammers): its live chance to shatter on a smash, percent.
struct HammerState {
  std::string key;  // hammer_t1..hammer_t5
  int breakRate = 0;
};

// What GET /api/beetle/user says, with the moment it was read (cooldowns are durations from then).
struct BeetleState {
  static constexpr int MAX_HUNTS = 3;  // per 90-minute cycle

  long cheese = 0;               // inventory.cheese
  long beetles = 0;              // beetle_count
  int level = 0;                 // level
  int huntsUsed = 0;             // beetleHuntsUsed (can be stale: see huntsRemaining)
  int64_t lastHuntDate = 0;      // lastBeetleHuntDate, epoch ms
  uint32_t cheeseCooldown = 0;   // cooldowns.claimUBC, ms left at fetchedAt (0 = ready)
  uint32_t trapCooldown = 0;     // cooldowns.catchBeetle, ms left at fetchedAt (0 = ready)
  uint32_t junkCooldown = 0;     // cooldowns.junkFaucet, ms left at fetchedAt (0 = ready)
  uint32_t fetchedAt = 0;        // hal_millis()
  bool loaded = false;           // read at least once (Home shows it after the profile)
  // Item key -> count (missing: 0). Shared, never changed: a new read makes a new map, so copying
  // the state (to every background task) doesn't copy ~150 entries.
  std::shared_ptr<const std::map<std::string, long>> inventory;
  std::vector<HammerState> hammers;
  bool slot3Unlocked = false;    // crafting_slot3_unlocked: the third craft slot can be used

  long count(const std::string &key) const;
  const std::map<std::string, long> &items() const;  // the inventory (empty before the first read)

  // `now` is hal_millis(); `epoch_ms` is hal_epoch_ms() (0 if the clock isn't set yet).
  uint32_t cheeseMsLeft(uint32_t now) const;
  uint32_t trapMsLeft(uint32_t now) const;
  uint32_t junkMsLeft(uint32_t now) const;
  int huntsRemaining(int64_t epoch_ms) const;
  bool beetlesReady(uint32_t now, int64_t epoch_ms) const;  // the trap, or a hunt, can be done
  // Until the trap or a hunt is ready, whichever comes first (the trap's alone without a clock).
  uint32_t beetlesMsLeft(uint32_t now, int64_t epoch_ms) const;
};

// One thing won: the beetle card or a secondary drop (junk, flower...).
struct BeetleItem {
  std::string key;   // "green", "juicebox"... (inventory key)
  std::string name;  // beetle_name
  std::string icon;  // site path of its PNG icon, as given (never built from the key)
  int category = 0;  // 2 = beetles, 3 = junk (cubes included), 5 = flowers...
  int rarity = 0;
  bool secondary = false;  // a secondaryItemDrops entry, not the beetle card
};

// One action's outcome: what it won.
struct BeetleStep {
  enum class Kind { Cheese, Trap, Hunt, Junk, Craft } kind;
  std::vector<BeetleItem> items;
};

// All blocking. Rejected = the access token was refused (401/403).
AuthResult fetch_beetle_state(HttpClient &http, const std::string &access_token, BeetleState &out,
                              std::string &detail);

// One action: one POST, gated by the caller on `state` (the last /user read or action response, its
// cooldowns counted down since; the server needs no fresher read). Success appends the step and
// takes the returned user.
// A trap "not ready" answer isn't an error: nothing is appended. On any failure `state` is re-read
// so the gates are right for the next press.
AuthResult beetle_action(HttpClient &http, const std::string &access_token, BeetleStep::Kind kind,
                         BeetleState &state, std::vector<BeetleStep> &steps, std::string &detail);

// Crafting: assemble, or smash (a hammer and a beetle sacrificed). Slots hold
// item keys, "" when empty. No recipes are known here: the server says what (if anything) it makes.
struct CraftRequest {
  static constexpr int SLOTS = 3;  // the website's 4th slot is never used
  bool smash = false;
  std::string slots[SLOTS];
  std::string hammer, sacrifice;  // smash only
};
struct CraftReply {
  bool success = false;
  std::string message;   // SUCCESS, UNLUCKY_ROLL, ALREADY_OWNED, INVALID_RECIPE, NOT_ENOUGH_RESOURCES...
  bool hammerBroke = false;
  BeetleItem item;       // what it made (success only); `icon` from the catalog, if the reply has none
};
// One POST. The game's own refusals are HTTP 200 (Ok, with reply.success false). `state` takes the
// returned user (inventory, hammers).
AuthResult beetle_craft(HttpClient &http, const std::string &access_token, const CraftRequest &request,
                        BeetleState &state, CraftReply &reply, std::string &detail);

// An item's icon shipped in the firmware (scripts/item_icons.py), fitted in max_size x max_size.
bool bundled_item_icon(const std::string &icon_path, int max_size, OwnedImage &out);

// Shipped in the firmware, or in `cache` from an earlier download; fitted in max_size x max_size.
// Safe on any thread.
bool local_item_icon(AssetCache &cache, const std::string &icon_path, int max_size, OwnedImage &out);
// local_item_icon(), else downloaded (needs a token) and kept in `cache`. A failed download is tried
// again on the next call.
bool load_item_icon(HttpClient &http, const std::string &access_token, AssetCache &cache, const std::string &icon_path,
                    int max_size, OwnedImage &out);

// Downloads an item's icon (BeetleItem::icon, a PNG) or a PFP thumbnail (a WebP, cropped square) and
// fits it in max_size x max_size.
// `status`: the HTTP status, if wanted (429: rate-limited, worth trying again later).
bool fetch_item_icon(HttpClient &http, const std::string &access_token, const std::string &icon_path, int max_size,
                     OwnedImage &out, int *status = nullptr);
