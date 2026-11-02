#include "services/asset_cache.h"

#include <sys/stat.h>

#include <fstream>
#include <sstream>

// Plain files in sim_data/cache/ (delete the folder to start over).
class FileAssetCache : public AssetCache {
 public:
  FileAssetCache() { mkdir(DIR, 0755); }

  bool load(const std::string &key, std::string &bytes) override {
    std::ifstream in(path(key), std::ios::binary);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    bytes = buf.str();
    return true;
  }

  void store(const std::string &key, const std::string &bytes) override {
    std::ofstream out(path(key), std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), (std::streamsize)bytes.size());
  }

 private:
  static constexpr const char *DIR = "sim_data/cache";
  static std::string path(const std::string &key) { return std::string(DIR) + "/" + asset_cache_filename(key); }
};

std::unique_ptr<AssetCache> create_asset_cache() { return std::make_unique<FileAssetCache>(); }
