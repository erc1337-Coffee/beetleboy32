#include "services/config_store.h"

#include <Preferences.h>

// NVS via Preferences, stored as plaintext. NVS encryption wouldn't change this code (see ROADMAP.md).
class NvsConfigStore : public ConfigStore {
 public:
  NvsConfigStore() { prefs_.begin("beetleboy", false); }
  ~NvsConfigStore() override { prefs_.end(); }

  bool get(const char *key, std::string &out) override {
    if (!prefs_.isKey(key)) return false;
    out = prefs_.getString(key).c_str();
    return true;
  }

  // Unchanged values aren't written: a flash write stalls both cores for a moment.
  bool set(const char *key, const std::string &value) override {
    if (prefs_.isKey(key) && value == prefs_.getString(key).c_str()) return true;
    return prefs_.putString(key, value.c_str()) == value.size();
  }

  void clear() override { prefs_.clear(); }

 private:
  Preferences prefs_;
};

std::unique_ptr<ConfigStore> create_config_store() { return std::make_unique<NvsConfigStore>(); }
