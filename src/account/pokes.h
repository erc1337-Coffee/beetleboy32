#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "account/keycloak.h"
#include "media/image.h"
#include "services/http_client.h"

// Pokes from other users: they come as notifications (type "poke"); poking back has a cooldown per
// user, which their profile says. All blocking.

struct Poke {
  std::string id;                   // the notification's
  std::string handle, displayName;  // who poked
  std::string pfpUrl;               // theirs (a site path)
  int64_t at = 0;                   // the notification's updatedAt (ms): a new poke from the same user updates it
};

// What the viewer can do on someone's profile (its viewerContext).
struct PokeStatus {
  bool canPoke = false;
  long cooldownS = 0;  // until they can be poked again
};

// The pending and unread notifications' pokes.
AuthResult fetch_pokes(HttpClient &http, const std::string &access_token, std::vector<Poke> &out, std::string &detail);
// Page `page` (from 1) of all the notifications, read or not: its pokes, newest first; `more`: pages after it.
AuthResult fetch_poke_feed(HttpClient &http, const std::string &access_token, int page, std::vector<Poke> &out,
                           bool &more, std::string &detail);
// poke.handle's; fills in its name and PFP if it didn't come with them.
AuthResult fetch_poke_status(HttpClient &http, const std::string &access_token, Poke &poke, PokeStatus &out,
                             std::string &detail);
// The poker's PFP, size x size; not kept (other users' pictures never are).
bool fetch_poke_pfp(HttpClient &http, const std::string &access_token, const std::string &url, int size,
                    OwnedImage &out);
AuthResult mark_notification_read(HttpClient &http, const std::string &access_token, const std::string &id,
                                  std::string &detail);
AuthResult poke_user(HttpClient &http, const std::string &access_token, const std::string &handle, std::string &detail);

std::string poke_wait_text(long seconds);  // a cooldown, short: "2h 5m", "5m", "40s"
