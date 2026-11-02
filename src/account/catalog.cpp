#include "account/catalog.h"

#include <ArduinoJson.h>

#include "account/site_api.h"

// Each card's fields we keep (the rest is lore and pictures).
static void card_filter(JsonObject f) {
  f["beetle_name"] = true;
  f["icon"] = true;
  f["category"] = true;
  f["rarity"] = true;
}

static void read_cards(JsonObjectConst cards, Catalog &out) {
  for (JsonPairConst card : cards) {
    ItemCard &c = out[card.key().c_str()];
    c.name = card.value()["beetle_name"] | card.key().c_str();
    c.icon = card.value()["icon"] | "";
    c.category = card.value()["category"] | 0;
    c.rarity = card.value()["rarity"] | 0;
  }
}

AuthResult fetch_catalog(HttpClient &http, const std::string &access_token, Catalog &out, std::string &detail) {
  JsonDocument filter, doc;
  card_filter(filter["*"].to<JsonObject>());  // a map of item key to card
  AuthResult r = site_get_json(http, access_token, "/api/beetle/cards", filter, doc, detail);
  if (r != AuthResult::Ok) return r;
  read_cards(doc.as<JsonObjectConst>(), out);
  detail = std::to_string(out.size()) + " cards";
  return out.empty() ? AuthResult::NetworkError : AuthResult::Ok;
}

std::string serialize_catalog(const Catalog &catalog) {
  JsonDocument doc;
  for (const auto &item : catalog) {
    JsonObject c = doc[item.first].to<JsonObject>();
    c["beetle_name"] = item.second.name;
    c["icon"] = item.second.icon;
    c["category"] = item.second.category;
    c["rarity"] = item.second.rarity;
  }
  std::string bytes;
  serializeJson(doc, bytes);
  return bytes;
}

bool parse_catalog(const std::string &bytes, Catalog &out) {
  JsonDocument doc;
  if (deserializeJson(doc, bytes) || !doc.is<JsonObjectConst>()) return false;
  read_cards(doc.as<JsonObjectConst>(), out);
  return !out.empty();
}
