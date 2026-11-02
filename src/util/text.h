#pragma once

#include <ctype.h>

#include <string>

// `s` without leading and trailing whitespace.
inline std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && isspace((unsigned char)s[b])) b++;
  while (e > b && isspace((unsigned char)s[e - 1])) e--;
  return s.substr(b, e - b);
}

// ASCII lower case, for matching words in server messages.
inline std::string lowercase(std::string s) {
  for (char &c : s) c = (char)tolower((unsigned char)c);
  return s;
}
