#include "services/asset_cache.h"

#include <LittleFS.h>

#include "hal/hal.h"

// LittleFS on the flash partition labelled "spiffs" (3.4MB in default_16MB.csv), formatted on
// first use. A full or broken filesystem only means re-downloading.
class FlashAssetCache : public AssetCache {
 public:
  FlashAssetCache() {
    ready_ = LittleFS.begin(true, "/littlefs", 4, "spiffs");
    hal_log("[cache] %s\n", ready_ ? "mounted" : "unavailable");
  }

  bool load(const std::string &key, std::string &bytes) override {
    if (!ready_) return false;
    File f = LittleFS.open(path(key).c_str(), "r");
    if (!f) return false;
    bytes.resize(f.size());
    bool ok = f.read((uint8_t *)&bytes[0], bytes.size()) == bytes.size();
    f.close();
    return ok;
  }

  void store(const std::string &key, const std::string &bytes) override {
    if (!ready_) return;
    File f = LittleFS.open(path(key).c_str(), "w");
    if (!f) return;
    if (f.write((const uint8_t *)bytes.data(), bytes.size()) != bytes.size()) {
      f.close();
      LittleFS.remove(path(key).c_str());  // never leave a truncated file behind
      return;
    }
    f.close();
  }

 private:
  static std::string path(const std::string &key) { return "/" + asset_cache_filename(key); }
  bool ready_ = false;
};

std::unique_ptr<AssetCache> create_asset_cache() { return std::make_unique<FlashAssetCache>(); }
