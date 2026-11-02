#pragma once

#include <memory>
#include <string>

// Persistent key-value storage for settings and secrets. One entry per key, so a single value
// (e.g. the rotating refresh token) can be rewritten on its own. Keys are at most 15 chars (NVS limit).
class ConfigStore {
 public:
  virtual ~ConfigStore() = default;
  virtual bool get(const char *key, std::string &out) = 0;
  virtual bool set(const char *key, const std::string &value) = 0;
  virtual void clear() = 0;
};

std::unique_ptr<ConfigStore> create_config_store();  // per platform
