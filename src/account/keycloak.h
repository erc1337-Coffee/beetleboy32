#pragma once

#include <string>

#include "app/device_config.h"
#include "services/http_client.h"

struct TokenSet {
  std::string accessToken;   // ~15 min; keep in RAM only
  std::string refreshToken;  // single-use, rotates; persist on every change
  int expiresIn = 0;         // seconds
};

enum class AuthResult {
  Ok,
  Rejected,      // the account details don't work: the user has to set them up again
  NetworkError,  // couldn't ask (no internet, server down): worth a retry
  RateLimited,   // the site said too many requests (HTTP 429): wait a bit, then retry
};

// Silent login with the SSO cookies, as the website does it: authorize with prompt=none + PKCE,
// take the code from the redirect, exchange it for tokens.
// Blocking. `detail` is a short reason for logs/screens; it never contains secrets.
AuthResult keycloak_login(HttpClient &http, const AccountCookies &cookies, TokenSet &out, std::string &detail);

// One-time sign-in with the account's email and password, through the website's login form (the
// `profile` client has no password grant): authorize with PKCE, post the form, follow redirects to
// the code, exchange it. Fills the three SSO cookies to keep and the first tokens. The password is
// only sent, never kept or logged. Rejected: wrong email or password, or the account wants a step
// this flow can't do (2FA, password change...); `detail` then says which, for the user.
AuthResult keycloak_password_login(HttpClient &http, const std::string &email, const std::string &password,
                                   AccountCookies &cookies, TokenSet &out, std::string &detail);

// New token pair from the refresh token (it rotates: persist out.refreshToken before using the rest).
// Rejected = the refresh token is no longer valid (e.g. invalid_grant).
AuthResult keycloak_refresh(HttpClient &http, const std::string &refresh_token, TokenSet &out, std::string &detail);
