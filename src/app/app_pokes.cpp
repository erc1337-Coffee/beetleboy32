// Pokes: the check for new ones, poking back, and the bell's notifications list.
#include <stdlib.h>

#include <algorithm>

#include <lvgl.h>

#include "app/app.h"
#include "hal/hal.h"
#include "media/sounds.h"
#include "ui/screens/notifications_screen.h"
#include "ui/toast.h"

// How often the notifications are read, and how long a poke's notification stays.
static constexpr uint32_t POKE_CHECK_MS = 15 * 1000;
static constexpr uint32_t POKE_CHECK_DARK_MS = 45 * 1000;  // with the screen off
static constexpr uint32_t POKE_SHOWN_MS = 3000;  // also the lights' breathe
static constexpr int POKE_PFP_SIZE = 64;  // px, left of the text and buttons: the notification's height
// The notifications list: pages loaded ahead of the one shown (PFPs and cooldowns), and only after
// this long without a touch (each is a download, and a PFP a decode too: they slow the screen down).
static constexpr size_t POKE_PAGES_AHEAD = 1;
static constexpr uint32_t POKE_AHEAD_IDLE_MS = 1500;
static constexpr const char *POKE_SEEN_KEY = "POKE_SEEN";  // ConfigStore

// The notifications' new pokes, with what Poke back can do.
void App::startPokeCheck() {
  if (!profile_) return;
  if (!pokeSeenLoaded_) {
    std::string seen;
    if (store_->get(POKE_SEEN_KEY, seen)) pokeSeenAt_ = atoll(seen.c_str());
    pokeSeenLoaded_ = true;
  }
  pokeChecking_ = true;
  startAuthed(pokeJob_, [seen = pokeSeenAt_](HttpClient &http, const std::string &token) {
    PokeOutcome o;
    std::vector<Poke> pokes;
    o.result = fetch_pokes(http, token, pokes, o.detail);
    for (const Poke &p : pokes) {
      o.newest = std::max(o.newest, p.at);
      if (seen >= 0 && p.at > seen) o.fresh.push_back(p);  // never read before: only takes note
    }
    std::sort(o.fresh.begin(), o.fresh.end(), [](const Poke &a, const Poke &b) { return a.at > b.at; });
    if (!o.fresh.empty()) {
      std::string detail;
      o.statusKnown = fetch_poke_status(http, token, o.fresh[0], o.status, detail) == AuthResult::Ok;
      fetch_poke_pfp(http, token, o.fresh[0].pfpUrl, POKE_PFP_SIZE, o.pfp);
      // Shown next: read on the site from now on.
      std::string read;
      if (mark_notification_read(http, token, o.fresh[0].id, read) != AuthResult::Ok) o.readFailure = read;
    }
    return o;
  }, [this] {
    pokeChecking_ = false;
    pokeCheckAt_ = hal_millis() + POKE_CHECK_MS;
  });
}

void App::pollPokes() {
  PokeOutcome pokes;
  if (pokeJob_.take(pokes)) {
    pokeChecking_ = false;
    bool slow = pokes.result == AuthResult::RateLimited;
    pokeCheckAt_ = hal_millis() + (slow ? RATE_LIMITED_PAUSE_MS : POKE_CHECK_MS);
    if (pokes.result != AuthResult::Ok) hal_log("[app] pokes: %s\n", pokes.detail.c_str());
    if (!pokes.readFailure.empty()) hal_log("[app] pokes: %s\n", pokes.readFailure.c_str());
    int64_t newest = std::max(pokes.newest, (int64_t)0);  // read once, even with none: new ones show
    if (pokes.result == AuthResult::Ok && newest > pokeSeenAt_) {
      pokeSeenAt_ = newest;
      store_->set(POKE_SEEN_KEY, std::to_string(pokeSeenAt_));
    }
    if (!pokes.fresh.empty()) showPoke(pokes.fresh[0], pokes.status, pokes.statusKnown, std::move(pokes.pfp));
  }
  // pokeCheckAt_ is the awake schedule: turning the screen on brings the next check forward.
  const uint32_t later = power_ == Power::Awake ? 0 : POKE_CHECK_DARK_MS - POKE_CHECK_MS;
  if (!pokeChecking_ && link_ == Link::Up && (int32_t)(hal_millis() - pokeCheckAt_ - later) >= 0) startPokeCheck();
}

// At the top for POKE_SHOWN_MS: their PFP, then Close, and Poke back or how long until it can be.
void App::showPoke(const Poke &poke, const PokeStatus &status, bool statusKnown, OwnedImage pfp) {
  hal_log("[app] poked by %s\n", poke.handle.c_str());
  if (power_ != Power::Awake) wakeScreen();
  play_sound(Sound::Notification);
  lights_.play(LightEffect::Breathe, POKE_SHOWN_MS);
  std::string name = poke.displayName.empty() ? poke.handle : poke.displayName;
  std::string wait = "in " + poke_wait_text(status.cooldownS);  // next to Close: "Poke in..." doesn't fit
  std::vector<ToastButton> buttons{{"Close", [] {}}};
  if (!statusKnown || status.canPoke) {  // cooldown unknown: offer it and let the server refuse
    buttons.push_back({"Poke back", [this, poke] { pokeBack(poke); }});
  } else if (status.cooldownS > 0) {
    buttons.push_back({wait.c_str(), nullptr});
  }
  retiredPokePfp_ = std::move(pokePfp_);
  pokePfp_ = std::move(pfp);
  show_toast(pokePfp_.empty() ? nullptr : pokePfp_.dsc(), (name + " poked you!").c_str(), POKE_SHOWN_MS,
             std::move(buttons));
}

void App::pokeBack(const Poke &poke) {
  if (pokingBack_) return;
  pokingBack_ = true;
  pokingBackHandle_ = poke.handle;
  startAuthed(pokeBackJob_, [poke](HttpClient &http, const std::string &token) {
    PokeBackOutcome o;
    o.name = poke.displayName.empty() ? poke.handle : poke.displayName;
    o.result = poke_user(http, token, poke.handle, o.detail);
    return o;
  }, [this] {
    pokingBack_ = false;
    show_toast(nullptr, "Poke not sent");
  });
}

void App::pollPokeBack() {
  PokeBackOutcome pokedBack;
  if (!pokeBackJob_.take(pokedBack)) return;
  hal_log("[app] poke back: %s\n", pokedBack.detail.c_str());
  pokingBack_ = false;
  for (PokeEntry &e : pokeList_) {  // the list's row: its cooldown read again
    if (e.poke.handle != pokingBackHandle_) continue;
    e.statusAsked = e.statusDone = false;
  }
  if (pokedBack.result == AuthResult::Ok) {
    show_toast(nullptr, ("Poked " + pokedBack.name).c_str());
  } else if (pokedBack.result == AuthResult::RateLimited) {
    showRateLimited();
  } else {
    show_toast(nullptr, "Poke not sent");
  }
}

// --- The notifications list -------------------------------------------------------------------

void App::openNotifications() {
  closePokeList();
  pokeListGen_++;
  pokeFeedMore_ = true;  // nothing read yet
  screens_.push(std::make_unique<NotificationsScreen>(*this));
}

// A job still running keeps its flag until it's done; its result is then dropped (another gen).
void App::closePokeList() {
  pokeList_.clear();
  pokeFeedPage_ = 0;
  pokeFeedMore_ = pokeFeedFailed_ = false;
  pokeRangeCount_ = 0;
}

void App::showPokeRange(size_t first, size_t count) {
  pokeRangeFirst_ = first;
  pokeRangeCount_ = count;
  pokeFeedFailed_ = false;  // another page: worth another try
}

const lv_image_dsc_t *App::pokeListPfp(size_t i) {
  return i < pokeList_.size() && !pokeList_[i].pfp.empty() ? pokeList_[i].pfp.dsc() : nullptr;
}

// Takes the results in, then starts the next request (see PokeEntry for the order).
void App::updatePokeList() {
  PokeFeedOutcome feed;
  if (pokeFeedJob_.take(feed)) {
    pokeFeedLoading_ = false;
    if (feed.gen == pokeListGen_ && feed.result == AuthResult::Ok) {
      pokeFeedPage_++;
      pokeFeedMore_ = feed.more;
      for (Poke &p : feed.pokes) {
        bool listed = false;  // one row per user: their newest poke
        for (const PokeEntry &e : pokeList_) listed = listed || e.poke.handle == p.handle;
        if (listed) continue;
        pokeList_.emplace_back();
        pokeList_.back().poke = std::move(p);
      }
    } else if (feed.gen == pokeListGen_) {
      hal_log("[app] poke list: %s\n", feed.detail.c_str());
      pokeFeedFailed_ = true;
    }
  }

  PokeDetailOutcome detail;
  if (pokeDetailJob_.take(detail)) {
    pokeDetailLoading_ = false;
    if (detail.gen == pokeListGen_ && detail.index < pokeList_.size() && pokeList_[detail.index].poke.handle == detail.handle) {
      PokeEntry &e = pokeList_[detail.index];
      if (detail.isPfp) {
        if (e.pfp.empty()) e.pfp = std::move(detail.pfp);
      } else {
        e.statusDone = true;
        e.statusKnown = detail.statusKnown;
        e.status = detail.status;
        e.statusAt = hal_millis();
        if (e.poke.displayName.empty()) e.poke.displayName = detail.displayName;
        if (e.poke.pfpUrl.empty()) e.poke.pfpUrl = detail.pfpUrl;
      }
    }
  }

  if (!pokeRangeCount_ || link_ != Link::Up || pokeFeedLoading_ || pokeDetailLoading_) return;
  const size_t first = pokeRangeFirst_, end = first + pokeRangeCount_;
  const size_t ahead = end + POKE_PAGES_AHEAD * pokeRangeCount_;
  const bool feed_left = pokeFeedMore_ && !pokeFeedFailed_;
  if (feed_left && pokeList_.size() < end) return startPokeFeed();
  if (startPokeDetail(first, end)) return;
  if (lv_display_get_inactive_time(nullptr) < POKE_AHEAD_IDLE_MS) return;
  if (feed_left && pokeList_.size() < ahead) return startPokeFeed();
  if (startPokeDetail(end, ahead)) return;
  startPokeDetail(first > pokeRangeCount_ ? first - pokeRangeCount_ : 0, first);
}

void App::startPokeFeed() {
  pokeFeedLoading_ = true;
  const uint32_t gen = pokeListGen_;
  startAuthed(pokeFeedJob_, [page = pokeFeedPage_ + 1, gen](HttpClient &http, const std::string &token) {
    PokeFeedOutcome o;
    o.gen = gen;
    o.result = fetch_poke_feed(http, token, page, o.pokes, o.more, o.detail);
    return o;
  }, [this] {
    pokeFeedLoading_ = false;
    pokeFeedFailed_ = true;
  });
}

// The first PFP, else the first cooldown, not asked for yet among entries [from, to); false if none.
bool App::startPokeDetail(size_t from, size_t to) {
  to = std::min(to, pokeList_.size());
  size_t index = SIZE_MAX;
  bool pfp = false;
  for (size_t i = from; i < to && index == SIZE_MAX; i++) {
    if (pokeList_[i].pfpAsked || pokeList_[i].poke.pfpUrl.empty()) continue;
    index = i;
    pfp = true;
  }
  for (size_t i = from; i < to && index == SIZE_MAX; i++)
    if (!pokeList_[i].statusAsked) index = i;
  if (index == SIZE_MAX) return false;

  PokeEntry &e = pokeList_[index];
  if (pfp) {
    e.pfpAsked = true;
  } else {
    e.statusAsked = true;
    e.statusDone = false;
  }
  pokeDetailLoading_ = true;
  const uint32_t gen = pokeListGen_;
  startAuthed(pokeDetailJob_, [gen, index, poke = e.poke, pfp](HttpClient &http, const std::string &token) {
    PokeDetailOutcome o;
    o.gen = gen;
    o.index = index;
    o.handle = poke.handle;
    o.isPfp = pfp;
    if (pfp) {
      fetch_poke_pfp(http, token, poke.pfpUrl, POKE_ROW_PFP, o.pfp);
      return o;
    }
    Poke filled = poke;
    std::string detail;
    o.statusKnown = fetch_poke_status(http, token, filled, o.status, detail) == AuthResult::Ok;
    o.displayName = filled.displayName;
    o.pfpUrl = filled.pfpUrl;
    return o;
  }, [this, gen, index, pfp] {
    pokeDetailLoading_ = false;
    if (gen == pokeListGen_ && index < pokeList_.size() && !pfp) pokeList_[index].statusDone = true;
  });
  return true;
}
