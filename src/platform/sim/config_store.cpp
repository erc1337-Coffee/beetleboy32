#include "services/config_store.h"

#include <fstream>
#include <map>

#include "util/text.h"

// Settings as an env-style file: KEY=value per line, # comments, optional quotes around values.
// Fill it by hand to skip setup; delete it for a factory-fresh device. The app rewrites the whole
// file when it saves, so hand-written comments don't survive that.
class EnvFileStore : public ConfigStore {
 public:
  EnvFileStore() {
    std::ifstream in(PATH);
    std::string line;
    while (std::getline(in, line)) {
      line = trim(line);
      if (line.empty() || line[0] == '#') continue;
      auto eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string value = trim(line.substr(eq + 1));
      if (value.size() >= 2 && (value[0] == '"' || value[0] == '\'') && value.back() == value[0])
        value = value.substr(1, value.size() - 2);
      values_[trim(line.substr(0, eq))] = value;
    }
  }

  bool get(const char *key, std::string &out) override {
    auto it = values_.find(key);
    if (it == values_.end()) return false;
    out = it->second;
    return true;
  }

  bool set(const char *key, const std::string &value) override {
    auto it = values_.find(key);
    if (it != values_.end() && it->second == value) return true;  // unchanged: not rewritten
    values_[key] = value;
    return flush();
  }

  void clear() override {
    values_.clear();
    flush();
  }

 private:
  static constexpr const char *PATH = "sim_data/beetleboy.env";

  bool flush() {
    std::ofstream out(PATH, std::ios::trunc);
    out << "# BeetleBoy32 settings (sim). Plaintext; see README.\n"
           "# KC_AUTH_SESSION / KC_SESSION / KC_IDENTITY = the AUTH_SESSION_ID / KEYCLOAK_SESSION /\n"
           "# KEYCLOAK_IDENTITY cookies of www.remilia.net. KC_REFRESH is written by the app.\n\n";
    for (auto &kv : values_) {
      bool quote = kv.second.find_first_of(" #\"'") != std::string::npos;
      out << kv.first << '=' << (quote ? "\"" + kv.second + "\"" : kv.second) << '\n';
    }
    return out.good();
  }

  std::map<std::string, std::string> values_;
};

std::unique_ptr<ConfigStore> create_config_store() { return std::make_unique<EnvFileStore>(); }
