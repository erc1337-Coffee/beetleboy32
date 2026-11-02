// The access token: kept fresh for the jobs that need one.
#include "app/app.h"

#include "hal/hal.h"

// Refreshed this long before it expires, so none runs out mid-request.
static constexpr uint32_t TOKEN_MARGIN_S = 60;

void App::setSession(const TokenSet &tokens) {
  session_ = tokens;
  sessionAt_ = hal_millis();
  // Saved before anything else uses it: Keycloak has already invalidated the previous one.
  DeviceConfig::saveRefreshToken(*store_, session_.refreshToken);
}

void App::withFreshToken(std::function<void(bool ok)> then) {
  uint32_t age_s = (hal_millis() - sessionAt_) / 1000;
  if (age_s + TOKEN_MARGIN_S < (uint32_t)session_.expiresIn) {
    then(true);
    return;
  }
  tokenWaiters_.push_back(std::move(then));
  if (tokenWaiters_.size() > 1) return;  // already refreshing

  hal_log("[app] refreshing access token\n");
  std::string refresh_token = session_.refreshToken;
  AccountCookies cookies = config_.account;
  refreshJob_.start([refresh_token, cookies] {
    LoginOutcome o;
    auto http = create_http_client();
    o.result = keycloak_refresh(*http, refresh_token, o.tokens, o.detail);
    // A dead refresh token (rotated elsewhere, revoked...): log in again from the SSO cookies.
    if (o.result == AuthResult::Rejected) o.result = keycloak_login(*http, cookies, o.tokens, o.detail);
    return o;
  });
}

bool App::pollTokenRefresh() {
  LoginOutcome refreshed;
  if (!refreshJob_.take(refreshed)) return true;
  hal_log("[app] token refresh: %s\n", refreshed.detail.c_str());
  bool ok = refreshed.result == AuthResult::Ok;
  if (ok) setSession(refreshed.tokens);
  std::vector<std::function<void(bool)>> waiters;
  waiters.swap(tokenWaiters_);
  for (auto &then : waiters) then(ok);
  if (refreshed.result == AuthResult::Rejected) {  // neither the refresh token nor the cookies work
    loginExpired();
    return false;
  }
  return true;
}
