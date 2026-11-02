#pragma once

#include <string>

// Overwrites a secret (a password) before letting go of it, so it doesn't linger in freed memory.
inline void wipe_secret(std::string &s) {
  volatile char *p = s.empty() ? nullptr : &s[0];
  for (size_t i = 0; i < s.size(); i++) p[i] = 0;
  s.clear();
  s.shrink_to_fit();
}
