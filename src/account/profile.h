#pragma once

#include <memory>
#include <string>
#include <vector>

#include "account/beetle.h"
#include "account/keycloak.h"
#include "media/image.h"
#include "services/asset_cache.h"
#include "services/config_store.h"
#include "services/http_client.h"

// The website's theme setting: "default" (white) or "color" with a hue.
struct ThemeSetting {
  bool colored = false;  // theme == "color"
  int hue = 0;           // color, 0-360; kept (and sent back) even when not colored, like the website

  bool operator==(const ThemeSetting &o) const { return colored == o.colored && hue == o.hue; }
  bool operator!=(const ThemeSetting &o) const { return !(*this == o); }
};

// A picture the user owns and may use as their PFP (profile.user.property): a token of a project.
struct PfpChoice {
  std::string project, id;
  std::string url;  // its thumbnail (a signed site path, fetched as is)
};

// What the home card shows. fetch_profile fills in what the card can't do without; App adds the
// beetle state and a new avatar once Home is up.
struct Profile {
  std::string handle;       // whoami.userHandle
  std::string displayName;  // profile.user.displayName
  std::string bio;          // profile.user.bio (the card's bottom plate)
  ThemeSetting theme;       // profile.user.theme + color
  std::string cover;        // profile.user.cover; not shown, but the theme update must send it back
  BeetleState beetle;       // beetle/user: level, cheese, beetles, cooldowns (kept current by App)
  std::string pfpUrl;       // profile.user.pfpUrl
  OwnedImage avatar;        // pfpUrl, decoded; empty until downloaded, or if that failed (not fatal)

  // For changing the PFP (update_pfp sends the whole profile back): from a fresh load only, not the
  // saved card (`editable`).
  bool editable = false;
  std::string pfpProject, pfpId;     // profile.user.pfp: the one in use
  std::string location;              // profile.user.location
  std::vector<int> achievements;     // profile.user.achievementsDisplayed[].id
  std::vector<std::string> friends;  // profile.user.friendsDisplayed[].userHandle
  std::vector<PfpChoice> pfps;       // property, by project then id (no Banners)
};

// Blocking. Rejected = the access token was refused (401/403). Leaves out the beetle state, and the
// avatar unless `cache` has it for the current URL (fetch_avatar gets it).
AuthResult fetch_profile(HttpClient &http, AssetCache &cache, const std::string &access_token, Profile &out,
                         std::string &detail);

// The card from the last load (text in `store`, avatar from `cache`), so Home shows at boot while
// the account check and a fresh load run behind it. No beetle state: that's always read anew.
bool load_saved_profile(ConfigStore &store, AssetCache &cache, Profile &out);
void save_profile(ConfigStore &store, const Profile &profile);  // writes only if it changed

// Blocking. Downloads the avatar at `url` and caches it.
bool fetch_avatar(HttpClient &http, AssetCache &cache, const std::string &access_token, const std::string &url,
                  OwnedImage &out, std::string &detail);

// Blocking. Makes project/id the PFP, the way the website's profile editor does: the whole profile is
// sent, everything else as `profile` has it (needs `editable`).
AuthResult update_pfp(HttpClient &http, const std::string &access_token, const Profile &profile,
                      const std::string &project, const std::string &id, std::string &detail);

// Blocking. Saves the theme the way the website does; the cover is sent unchanged.
AuthResult update_theme(HttpClient &http, const std::string &access_token, const ThemeSetting &theme,
                        const std::string &cover, std::string &detail);
