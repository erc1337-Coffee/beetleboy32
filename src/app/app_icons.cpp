// Item icons. The ones on the device (shipped, or downloaded before) load on the main loop, a few
// each frame: no thread to start (internal RAM may have no room for one), no token, no network, never
// stuck behind downloads. Downloads go a few at a time, so they show as they come, and are paced:
// the site rate-limits past ~30 requests in 30 s.
#include <algorithm>

#include "app/app.h"
#include "hal/hal.h"

static constexpr uint32_t LOCAL_ICONS_MS = 6;  // per frame
static constexpr size_t DOWNLOADS_PER_JOB = 2;
// 10 downloads every 12 s is ~50 a minute, under the site's limit of ~30 per 30 s.
static constexpr int DOWNLOADS_PER_BURST = 10;
static constexpr uint32_t BURST_PAUSE_MS = 12 * 1000;

const lv_image_dsc_t *App::itemIcon(const std::string &icon_path) {
  auto it = itemIcons_.find(icon_path);
  return it == itemIcons_.end() ? nullptr : it->second.dsc();
}

const lv_image_dsc_t *App::itemIcon(const std::string &icon_path, int size) {
  auto key = std::make_pair(icon_path, size);
  auto small = smallIcons_.find(key);
  if (small != smallIcons_.end()) return small->second.dsc();
  auto it = itemIcons_.find(icon_path);
  if (it == itemIcons_.end()) return nullptr;
  OwnedImage shrunk;
  if (!it->second.shrunk(size, shrunk)) return it->second.dsc();  // already that small
  return smallIcons_.emplace(key, std::move(shrunk)).first->second.dsc();
}

// Moves the ones of `paths` in `queue` to its front (in that order); returns the others.
static std::vector<std::string> move_up_front(std::vector<std::string> &queue, const std::vector<std::string> &paths) {
  std::vector<std::string> found, others;
  for (const std::string &path : paths) {
    auto at = std::find(queue.begin(), queue.end(), path);
    if (at == queue.end()) {
      others.push_back(path);
      continue;
    }
    queue.erase(at);
    found.push_back(path);
  }
  queue.insert(queue.begin(), found.begin(), found.end());
  return others;
}

void App::loadItemIcons(const std::vector<std::string> &icon_paths, bool first) {
  std::vector<std::string> wanted;
  for (const std::string &path : icon_paths) {
    if (path.empty() || itemIcons_.count(path) || iconsFailed_.count(path)) continue;
    if (std::find(iconBatch_.begin(), iconBatch_.end(), path) != iconBatch_.end()) {
      iconsUnwanted_.erase(path);  // loading already: kept after all
      continue;
    }
    if (std::find(wanted.begin(), wanted.end(), path) == wanted.end()) wanted.push_back(path);
  }
  if (first) {
    wanted = move_up_front(iconDownloads_, move_up_front(iconQueue_, wanted));
  } else {
    auto queued = [this](const std::string &path) {
      return std::find(iconQueue_.begin(), iconQueue_.end(), path) != iconQueue_.end() ||
             std::find(iconDownloads_.begin(), iconDownloads_.end(), path) != iconDownloads_.end();
    };
    wanted.erase(std::remove_if(wanted.begin(), wanted.end(), queued), wanted.end());
  }
  iconQueue_.insert(first ? iconQueue_.begin() : iconQueue_.end(), wanted.begin(), wanted.end());
}

void App::dropQueuedIcons(const std::vector<std::string> &icon_paths) {
  for (const std::string &path : icon_paths) {
    for (std::vector<std::string> *queue : {&iconQueue_, &iconDownloads_})
      queue->erase(std::remove(queue->begin(), queue->end(), path), queue->end());
    if (std::find(iconBatch_.begin(), iconBatch_.end(), path) != iconBatch_.end()) iconsUnwanted_.insert(path);
  }
}

void App::releaseItemIcons(const std::vector<std::string> &icon_paths, int size) {
  if (size) {
    for (const std::string &path : icon_paths) smallIcons_.erase({path, size});
    return;
  }
  dropQueuedIcons(icon_paths);
  for (const std::string &path : icon_paths) {
    itemIcons_.erase(path);
    for (auto it = smallIcons_.begin(); it != smallIcons_.end();)
      it = it->first.first == path ? smallIcons_.erase(it) : std::next(it);
  }
}

void App::loadLocalIcons() {
  std::vector<std::string> remote;
  const uint32_t start = hal_millis();
  while (!iconQueue_.empty() && hal_millis() - start < LOCAL_ICONS_MS) {
    std::string path = std::move(iconQueue_.front());
    iconQueue_.erase(iconQueue_.begin());
    OwnedImage image;
    if (local_item_icon(*cache_, path, ITEM_ICON_SIZE, image)) {
      itemIcons_.emplace(path, std::move(image));
    } else {
      remote.push_back(path);
    }
  }
  iconDownloads_.insert(iconDownloads_.begin(), remote.begin(), remote.end());  // just asked for: first in line
}

// A few downloads, once any pause is over. Not startAuthed(): without a token the job still runs,
// to hand the batch back as unreachable.
void App::startIconsJob() {
  AssetCache *cache = cache_.get();
  if (iconDownloads_.empty() || link_ != Link::Up || (int32_t)(hal_millis() - iconsResumeAt_) < 0) return;
  iconsLoading_ = true;
  size_t n = std::min({DOWNLOADS_PER_JOB, iconDownloads_.size(), (size_t)(DOWNLOADS_PER_BURST - iconsBurst_)});
  iconBatch_.assign(iconDownloads_.begin(), iconDownloads_.begin() + n);
  iconDownloads_.erase(iconDownloads_.begin(), iconDownloads_.begin() + n);
  // A fresh token: what's downloaded fails with a stale one.
  withFreshToken([this, cache](bool ok) {
    std::string token = ok ? session_.accessToken : "";
    iconsJob_.start([paths = iconBatch_, token, cache] {
      IconsOutcome o;
      o.unreachable = token.empty();
      auto http = create_http_client();
      for (const std::string &path : paths) {
        if (o.rateLimited || o.unreachable) {
          o.remote.push_back(path);  // after the pause
          continue;
        }
        OwnedImage image;
        int status = 0;
        o.downloads++;
        if (fetch_item_icon(*http, token, path, ITEM_ICON_SIZE, image, &status)) {
          cache->store(path, image.serialize());  // on the device from now on
          o.icons.emplace_back(path, std::move(image));
        } else if (status == 429) {
          o.rateLimited = true;
          o.remote.push_back(path);
        } else if (status == 0 || status == 401 || status >= 500) {  // no connection, the site's trouble
          o.unreachable = true;
          o.remote.push_back(path);
        } else {
          o.failed.push_back(path);
        }
      }
      return o;
    });
  });
}

void App::pollIcons() {
  IconsOutcome icons;
  if (iconsJob_.take(icons)) {
    // What was dropped while the job ran (the page was left) is let go, not kept or asked again.
    // emplace keeps an existing entry: a screen may be drawing it.
    for (auto &icon : icons.icons)
      if (!iconsUnwanted_.count(icon.first)) itemIcons_.emplace(icon.first, std::move(icon.second));
    iconsFailed_.insert(icons.failed.begin(), icons.failed.end());
    std::vector<std::string> remote;
    for (const std::string &path : icons.remote)
      if (!iconsUnwanted_.count(path)) remote.push_back(path);
    iconDownloads_.insert(iconDownloads_.begin(), remote.begin(), remote.end());  // just asked for: first in line
    iconsUnwanted_.clear();
    iconsBurst_ += icons.downloads;
    if (icons.rateLimited || icons.unreachable || iconsBurst_ >= DOWNLOADS_PER_BURST) {
      uint32_t pause = icons.rateLimited || icons.unreachable ? RATE_LIMITED_PAUSE_MS : BURST_PAUSE_MS;
      if (icons.rateLimited || icons.unreachable)
        hal_log("[app] icons: %s, waiting %u s\n", icons.rateLimited ? "rate-limited" : "site unreachable",
                (unsigned)(pause / 1000));
      iconsResumeAt_ = hal_millis() + pause;
      iconsBurst_ = 0;
    }
    iconBatch_.clear();
    iconsLoading_ = false;
  }
  loadLocalIcons();
  if (!iconsLoading_) startIconsJob();  // the next downloads
}
