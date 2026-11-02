// The beetle game: one task at a time, its result, the readiness of each action, and the catalog.
#include <iterator>
#include <set>

#include "app/app.h"
#include "hal/hal.h"
#include "media/sounds.h"

// A refresh asked for this soon after the last read sends nothing: entering and leaving screens in a
// loop would otherwise send a request each time.
static constexpr uint32_t REFRESH_MIN_AGE_MS = 3000;
// Home has nothing to show until the first beetle read works: tried again this long after a failure
// (after RATE_LIMITED_PAUSE_MS if rate-limited or refused).
static constexpr uint32_t BEETLE_RETRY_MS = 3000;
static constexpr uint32_t RESULT_SPARKLE_MS = 2000;  // the lights after a hunt, claim or craft
static constexpr const char *CATALOG_CACHE_KEY = "/api/beetle/cards";

const BeetleState &App::beetle() const {
  static const BeetleState NONE;
  return profile_ ? profile_->beetle : NONE;
}

void App::refreshBeetle() {
  if (!profile_ || hal_millis() - profile_->beetle.fetchedAt < REFRESH_MIN_AGE_MS) return;
  startBeetleTask(BeetleTask::Refresh);
}

void App::catchBeetle() { startBeetleTask(BeetleTask::Trap); }
void App::huntBeetle() { startBeetleTask(BeetleTask::Hunt); }
void App::claimCheese() { startBeetleTask(BeetleTask::Cheese); }
void App::claimJunk() { startBeetleTask(BeetleTask::Junk); }

void App::craft(const CraftRequest &request) {
  if (beetleTask() != BeetleTask::None) return;  // one at a time: the screen waits for the result
  craftRequest_ = request;
  startBeetleTask(BeetleTask::Craft);
}

bool App::beetleTaskAllowed(BeetleTask task) const {
  const BeetleState &b = profile_->beetle;
  const uint32_t now = hal_millis();
  switch (task) {
    case BeetleTask::Trap: return b.trapMsLeft(now) == 0;
    case BeetleTask::Hunt: return b.huntsRemaining(hal_epoch_ms()) > 0;
    case BeetleTask::Cheese: return b.cheeseMsLeft(now) == 0;
    case BeetleTask::Junk: return b.junkMsLeft(now) == 0;
    default: return true;  // a craft: the server says if the items are there
  }
}

void App::startBeetleTask(BeetleTask task) {
  if (!profile_) return;
  if (beetleTask_ == BeetleTask::Refresh && task != BeetleTask::Refresh && pendingTask_ == BeetleTask::None) {
    pendingTask_ = task;  // started when the refresh ends
    return;
  }
  if (beetleTask_ != BeetleTask::None) return;
  beetleTask_ = task;
  if (task != BeetleTask::Refresh) beetleFailed_ = BeetleTask::None;  // a background read keeps "RETRY"
  // Shared, not copied with the closure: the closure is copied several times on its way to the job,
  // and small allocations come from internal RAM, which the job's thread needs for its stack.
  auto known_icons = std::make_shared<std::set<std::string>>();
  for (const auto &icon : itemIcons_) known_icons->insert(icon.first);
  AssetCache *cache = cache_.get();
  startAuthed(beetleJob_, [task, state = profile_->beetle, known_icons, cache, request = craftRequest_](
                              HttpClient &http, const std::string &token) {
    BeetleOutcome o;
    o.state = state;
    using Kind = BeetleStep::Kind;
    switch (task) {
      case BeetleTask::Refresh: o.result = fetch_beetle_state(http, token, o.state, o.detail); break;
      case BeetleTask::Trap: o.result = beetle_action(http, token, Kind::Trap, o.state, o.steps, o.detail); break;
      case BeetleTask::Hunt: o.result = beetle_action(http, token, Kind::Hunt, o.state, o.steps, o.detail); break;
      case BeetleTask::Cheese: o.result = beetle_action(http, token, Kind::Cheese, o.state, o.steps, o.detail); break;
      case BeetleTask::Junk: o.result = beetle_action(http, token, Kind::Junk, o.state, o.steps, o.detail); break;
      case BeetleTask::Craft:
        o.result = beetle_craft(http, token, request, o.state, o.craft, o.detail);
        if (o.craft.success && !o.craft.item.key.empty()) {  // its icon: from the catalog, on the main thread
          BeetleStep step{Kind::Craft};
          step.items.push_back(o.craft.item);
          o.steps.push_back(std::move(step));
        }
        break;
      case BeetleTask::None: break;
    }
    // Icons for the result animation.
    std::set<std::string> fetched = *known_icons;
    for (const BeetleStep &step : o.steps) {
      for (const BeetleItem &item : step.items) {
        if (item.icon.empty() || !fetched.insert(item.icon).second) continue;
        OwnedImage image;
        if (load_item_icon(http, token, *cache, item.icon, ITEM_ICON_SIZE, image))
          o.icons.emplace_back(item.icon, std::move(image));
      }
    }
    return o;
  }, [this, task] {
    if (task != BeetleTask::Refresh) beetleFailed_ = task;
    beetleTask_ = BeetleTask::None;
    beetleRetryAt_ = hal_millis() + BEETLE_RETRY_MS;
    if (task == BeetleTask::Craft) {  // the Craft screen waits for a result: an error it is
      beetleResult_ = {task, false, false, {}, {}};
      beetleResultSeq_++;
    }
  });
}

void App::pollBeetle() {
  BeetleOutcome beetle;
  if (beetleJob_.take(beetle)) {
    static const char *const TASKS[] = {"none", "refresh", "trap", "hunt", "cheese", "junk", "craft"};
    static_assert(std::size(TASKS) == (size_t)BeetleTask::Craft + 1, "one name per BeetleTask, in order");
    hal_log("[app] beetle %s: %s (%u steps)\n", TASKS[(int)beetleTask_], beetle.detail.c_str(),
            (unsigned)beetle.steps.size());
    profile_->beetle = beetle.state;  // also after a failure: the steps before it happened
    // emplace keeps an existing entry: a screen may be drawing it.
    for (auto &icon : beetle.icons) itemIcons_.emplace(icon.first, std::move(icon.second));
    if (beetle.result != AuthResult::Ok && beetleTask_ != BeetleTask::Refresh) beetleFailed_ = beetleTask_;
    // The Craft screen's result pop-up says it itself.
    if (beetle.result == AuthResult::RateLimited && beetleTask_ != BeetleTask::Craft) showRateLimited();
    if (beetleTask_ != BeetleTask::Refresh) {
      beetleResult_ = {beetleTask_, beetle.result == AuthResult::Ok, beetle.result == AuthResult::RateLimited, {},
                       beetle.craft};
      for (BeetleStep &step : beetle.steps)
        for (BeetleItem &item : step.items) beetleResult_.items.push_back(std::move(item));
      BeetleItem &made = beetleResult_.craft.item;
      auto card = catalog_.find(made.key);
      if (made.icon.empty() && card != catalog_.end()) made.icon = card->second.icon;
      if (!made.icon.empty()) loadItemIcons({made.icon}, true);  // the Craft screen's pop-up shows it when here
      beetleResultSeq_++;
      if (beetleResult_.ok) lights_.play(LightEffect::Sparkle, RESULT_SPARKLE_MS);
    }
    BeetleTask done = beetleTask_;
    beetleTask_ = BeetleTask::None;
    screens_.rebuildHidden();  // Home's counters
    if (done == BeetleTask::Refresh && !profile_->beetle.loaded) {
      bool pause = beetle.result == AuthResult::RateLimited || beetle.result == AuthResult::Rejected;
      beetleRetryAt_ = hal_millis() + (pause ? RATE_LIMITED_PAUSE_MS : BEETLE_RETRY_MS);
    }
  }
  if (!profile_->beetle.loaded && beetleTask_ == BeetleTask::None && link_ == Link::Up &&
      (int32_t)(hal_millis() - beetleRetryAt_) >= 0) {
    startBeetleTask(BeetleTask::Refresh);
  }
}

void App::runPendingTask() {
  if (pendingTask_ == BeetleTask::None || beetleTask_ != BeetleTask::None) return;
  BeetleTask task = pendingTask_;
  pendingTask_ = BeetleTask::None;
  // The refresh may show it's not ready after all: then the screen just shows the countdown.
  if (beetleTaskAllowed(task)) startBeetleTask(task);
}

void App::checkCooldowns() {
  const BeetleState &b = profile_->beetle;
  const uint32_t now = hal_millis();
  bool cheese_ready = b.cheeseMsLeft(now) == 0, beetles_ready = b.beetlesReady(now, hal_epoch_ms());
  bool trap_ready = b.trapMsLeft(now) == 0, hunt_ready = b.huntsRemaining(hal_epoch_ms()) > 0;
  bool compare = b.loaded && beetleWasLoaded_;  // not before the first read, nor on it
  beetleWasLoaded_ = b.loaded;
  // Each one becoming available rings (the trap and a hunt each, though either makes beetles_ready).
  if (compare && ((cheese_ready && !cheeseWasReady_) || (trap_ready && !trapWasReady_) || (hunt_ready && !huntWasReady_)))
    play_sound(Sound::Notification);
  if (compare && ((cheese_ready && !cheeseWasReady_) || (beetles_ready && !beetlesWasReady_))) {
    hal_log("[app] a cooldown ran out: confirming\n");
    if (link_ == Link::Up) startBeetleTask(BeetleTask::Refresh);  // skipped if a task runs: it's fresh anyway
    if (power_ != Power::Awake) wakeScreen();  // something to claim: show it
  }
  cheeseWasReady_ = cheese_ready;
  beetlesWasReady_ = beetles_ready;
  trapWasReady_ = trap_ready;
  huntWasReady_ = hunt_ready;
}

void App::loadCatalog() {
  if (catalog_.empty()) {
    std::string bytes;
    if (cache_->load(CATALOG_CACHE_KEY, bytes)) parse_catalog(bytes, catalog_);
  }
  if (catalogAsked_) return;
  catalogAsked_ = true;
  startAuthed(catalogJob_, [](HttpClient &http, const std::string &token) {
    CatalogOutcome o;
    o.result = fetch_catalog(http, token, o.catalog, o.detail);
    return o;
  }, [this] { catalogAsked_ = false; });  // asked again next time
}

void App::pollCatalog() {
  CatalogOutcome catalog;
  if (!catalogJob_.take(catalog)) return;
  hal_log("[app] catalog: %s\n", catalog.detail.c_str());
  if (catalog.result == AuthResult::Ok) {
    catalog_ = std::move(catalog.catalog);
    cache_->store(CATALOG_CACHE_KEY, serialize_catalog(catalog_));
  } else {
    catalogAsked_ = false;  // asked again next time
  }
}
