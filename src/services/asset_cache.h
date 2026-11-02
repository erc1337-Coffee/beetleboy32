#pragma once

#include <memory>
#include <string>

// Downloaded files kept across reboots (own avatar, game icons), stored ready to draw.
// One file per key; keys are any string (e.g. a URL path). Safe to call from job threads.
// Never for other users' pictures: those are shown once and not kept.
class AssetCache {
 public:
  virtual ~AssetCache() = default;
  virtual bool load(const std::string &key, std::string &bytes) = 0;
  virtual void store(const std::string &key, const std::string &bytes) = 0;
};

std::unique_ptr<AssetCache> create_asset_cache();  // per platform
std::string asset_cache_filename(const std::string &key);  // short, path-safe name for a key
