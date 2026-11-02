#pragma once

#include <map>
#include <string>

#include "account/keycloak.h"
#include "services/http_client.h"

// What the game says about each item (GET /api/beetle/cards): its name, kind and rarity, and its
// icon. Crafting needs it to sort the inventory into what can go where.
struct ItemCard {
  std::string name;  // beetle_name
  std::string icon;  // site path of its PNG icon
  int category = 0;  // 2 = beetles, 3 = junk (cubes included), 5 = flowers...
  int rarity = 0;    // 1 (common) up
};
using Catalog = std::map<std::string, ItemCard>;  // by item key (inventory key)

AuthResult fetch_catalog(HttpClient &http, const std::string &access_token, Catalog &out, std::string &detail);

// Kept in flash (AssetCache) between boots.
std::string serialize_catalog(const Catalog &catalog);
bool parse_catalog(const std::string &bytes, Catalog &out);
