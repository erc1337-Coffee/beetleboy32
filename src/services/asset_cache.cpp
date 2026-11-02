#include "services/asset_cache.h"

#include "hal/hal.h"

std::string asset_cache_filename(const std::string &key) {
  uint8_t digest[32];
  hal_sha256(key.data(), key.size(), digest);
  static const char hex[] = "0123456789abcdef";
  std::string name;
  for (int i = 0; i < 8; i++) {  // 64 bits: no collisions at this scale
    name += hex[digest[i] >> 4];
    name += hex[digest[i] & 15];
  }
  return name + ".bin";
}
