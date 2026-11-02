#include "account/keycloak.h"

#include <ArduinoJson.h>
#include <ctype.h>
#include <string.h>

#include <map>

#include "account/site_api.h"
#include "hal/hal.h"
#include "util/secret.h"

static const char *const BASE = "https://www.remilia.net/oidc/realms/remilia/protocol/openid-connect";
static const char *const CLIENT_ID = "profile";
static const char *const REDIRECT_URI = "https://www.remilia.net/";
static const char *const SCOPE = "openid profile email offline_access";
static const char *const BROWSER_UA =  // the login pages expect a browser
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/130.0.0.0 Safari/537.36";
static constexpr int MAX_HOPS = 8;  // redirects and pages between the authorize URL and the code
// Random bytes in the PKCE code verifiers (the silent login's and the password sign-in's), and in
// the state and nonce.
static constexpr size_t SILENT_VERIFIER_BYTES = 48, PASSWORD_VERIFIER_BYTES = 32, NONCE_BYTES = 12;

static std::string url_encode(const std::string &s) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

static std::string base64url(const uint8_t *data, size_t len) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = data[i] << 16 | (i + 1 < len ? data[i + 1] << 8 : 0) | (i + 2 < len ? data[i + 2] : 0);
    out += alphabet[n >> 18 & 63];
    out += alphabet[n >> 12 & 63];
    if (i + 1 < len) out += alphabet[n >> 6 & 63];
    if (i + 2 < len) out += alphabet[n & 63];
  }
  return out;  // unpadded, as PKCE requires
}

static std::string random_token(size_t bytes) {
  uint8_t buf[64];
  for (size_t i = 0; i < bytes; i++) buf[i] = (uint8_t)hal_random();
  return base64url(buf, bytes);
}

// Value of `name` in a URL's query or fragment.
static std::string url_param(const std::string &url, const char *name) {
  std::string key = std::string(name) + "=";
  for (size_t pos = url.find(key); pos != std::string::npos; pos = url.find(key, pos + 1)) {
    if (pos == 0 || (url[pos - 1] != '?' && url[pos - 1] != '&' && url[pos - 1] != '#')) continue;
    size_t start = pos + key.size();
    return url.substr(start, url.find_first_of("&#", start) - start);
  }
  return "";
}

static AuthResult http_failure(const HttpResult &r, const char *step, std::string &detail) {
  detail = std::string(step) + ": " + r.statusText();
  if (r.status == 0) return AuthResult::NetworkError;
  JsonDocument doc;
  if (!deserializeJson(doc, r.body) && doc["error"].is<const char *>()) detail += std::string(" ") + doc["error"].as<const char *>();
  if (r.status == 429) return AuthResult::RateLimited;  // not a bad password: try again later
  return r.status >= 500 ? AuthResult::NetworkError : AuthResult::Rejected;
}

// POST to the token endpoint (code exchange or refresh) and read the token pair.
static AuthResult request_tokens(HttpClient &http, const std::string &form, TokenSet &out, std::string &detail) {
  HttpResult token = http.postForm(std::string(BASE) + "/token", form, {});
  if (token.status != 200) return http_failure(token, "token", detail);

  JsonDocument doc;
  if (deserializeJson(doc, token.body)) {
    detail = "token: bad JSON";
    return AuthResult::NetworkError;
  }
  out.accessToken = doc["access_token"] | "";
  out.refreshToken = doc["refresh_token"] | "";
  out.expiresIn = doc["expires_in"] | 0;
  if (out.accessToken.empty() || out.refreshToken.empty()) {
    detail = "token: incomplete response";
    return AuthResult::Rejected;
  }
  detail = "ok";
  return AuthResult::Ok;
}

// PKCE: the token request must prove it comes from whoever started the login.
struct Pkce {
  std::string verifier;
  std::string challenge;  // base64url(SHA-256(verifier))
  std::string state;      // echoed back in the redirect
};

static Pkce make_pkce(size_t verifier_bytes) {
  Pkce p;
  p.verifier = random_token(verifier_bytes);
  uint8_t digest[32];
  hal_sha256(p.verifier.data(), p.verifier.size(), digest);
  p.challenge = base64url(digest, sizeof(digest));
  p.state = random_token(NONCE_BYTES);
  return p;
}

// `extra`: more query parameters, each starting with '&'.
static std::string authorize_url(const Pkce &pkce, const std::string &extra) {
  return std::string(BASE) + "/auth?client_id=" + CLIENT_ID + "&redirect_uri=" + url_encode(REDIRECT_URI) +
         "&response_type=code&scope=" + url_encode(SCOPE) + "&state=" + pkce.state +
         "&code_challenge=" + pkce.challenge + "&code_challenge_method=S256" + extra;
}

// The code in the redirect back to REDIRECT_URI, once it's known to answer our request.
static AuthResult code_from_redirect(const std::string &url, const Pkce &pkce, std::string &code, std::string &detail) {
  code = url_param(url, "code");
  if (code.empty()) {
    // e.g. error=login_required: the SSO session behind the cookies has ended.
    std::string error = url_param(url, "error");
    detail = "authorize: " + (error.empty() ? std::string("no code in redirect") : error);
    return AuthResult::Rejected;
  }
  if (url_param(url, "state") != pkce.state) {
    detail = "authorize: state mismatch";
    return AuthResult::Rejected;
  }
  return AuthResult::Ok;
}

static AuthResult exchange_code(HttpClient &http, const std::string &code, const Pkce &pkce, TokenSet &out,
                                std::string &detail) {
  std::string form = std::string("grant_type=authorization_code&client_id=") + CLIENT_ID + "&code=" + url_encode(code) +
                     "&redirect_uri=" + url_encode(REDIRECT_URI) + "&code_verifier=" + pkce.verifier;
  return request_tokens(http, form, out, detail);
}

AuthResult keycloak_login(HttpClient &http, const AccountCookies &cookies, TokenSet &out, std::string &detail) {
  const Pkce pkce = make_pkce(SILENT_VERIFIER_BYTES);
  std::string cookie = "AUTH_SESSION_ID=" + cookies.authSessionId + "; KEYCLOAK_SESSION=" +
                       cookies.keycloakSession + "; KEYCLOAK_IDENTITY=" + cookies.keycloakIdentity;

  HttpResult auth = http.get(authorize_url(pkce, "&prompt=none"), {{"Cookie", cookie}});
  if (auth.status != 302 && auth.status != 303) return http_failure(auth, "authorize", detail);
  std::string code;
  AuthResult r = code_from_redirect(auth.location, pkce, code, detail);
  return r == AuthResult::Ok ? exchange_code(http, code, pkce, out, detail) : r;
}

AuthResult keycloak_refresh(HttpClient &http, const std::string &refresh_token, TokenSet &out, std::string &detail) {
  std::string form = std::string("grant_type=refresh_token&client_id=") + CLIENT_ID +
                     "&refresh_token=" + url_encode(refresh_token);
  return request_tokens(http, form, out, detail);
}

// --- Password sign-in ---------------------------------------------------------------------------

// Cookies across the login's requests, like a browser's jar for one site.
class CookieJar {
 public:
  void take(const HttpResult &r) {
    for (const auto &c : r.cookies) {
      if (c.second.empty()) jar_.erase(c.first);  // cleared
      else jar_[c.first] = c.second;
    }
  }
  std::string header() const {
    std::string h;
    for (const auto &c : jar_) h += (h.empty() ? "" : "; ") + c.first + "=" + c.second;
    return h;
  }
  std::string get(const char *name) const {
    auto it = jar_.find(name);
    return it == jar_.end() ? "" : it->second;
  }

 private:
  std::map<std::string, std::string> jar_;
};

// The quoted value given to `key` in the page: `pageId: "login"`, `"loginAction":"https:\/\/..."`,
// `action="..."`. Only a quote straight after the key (past its own closing quote, spaces, `:` or
// `=`) counts; JS escapes are undone (\/ and &).
static std::string quoted_after(const std::string &html, const char *key) {
  const size_t n = html.size();
  for (size_t k = html.find(key); k != std::string::npos; k = html.find(key, k + 1)) {
    size_t i = k + strlen(key);
    if (i < n && html[i] == '"') i++;  // the key's closing quote, in JSON
    while (i < n && (html[i] == ' ' || html[i] == ':' || html[i] == '=')) i++;
    if (i >= n || html[i] != '"') continue;
    std::string out;
    for (i++; i < n && html[i] != '"'; i++) {
      if (html[i] != '\\' || i + 1 >= n) {
        out += html[i];
      } else if (html.compare(i + 1, 5, "u0026") == 0) {
        out += '&';
        i += 5;
      } else {
        out += html[++i];
      }
    }
    return out;
  }
  return "";
}

static std::string html_unescape(std::string s) {
  for (size_t p = s.find("&amp;"); p != std::string::npos; p = s.find("&amp;", p + 1)) s.replace(p, 5, "&");
  return s;
}

// Where the login form posts: the page's JS context (loginAction) or the classic form's action.
static std::string login_action(const std::string &html) {
  std::string action = quoted_after(html, "loginAction");
  if (action.empty()) {
    size_t form = html.find("kc-form-login");
    if (form != std::string::npos) action = quoted_after(html.substr(form), "action");
  }
  return html_unescape(action);
}

static std::string page_id(const std::string &html) {
  std::string id = quoted_after(html, "pageId");
  if (id.size() > 4 && id.compare(id.size() - 4, 4, ".ftl") == 0) id.resize(id.size() - 4);  // "login.ftl"
  if (id.empty() && html.find("kc-form-login") != std::string::npos) id = "login";
  return id;
}

static std::string absolute(const std::string &location) {
  return location.compare(0, 4, "http") == 0 ? location : SITE + location;
}

// The login only ever talks to the site: a redirect or form pointing anywhere else is refused (it
// would get the cookies, or the password).
static bool on_site(const std::string &url) {
  const size_t n = strlen(SITE);
  return url.compare(0, n, SITE) == 0 && (url.size() == n || url[n] == '/' || url[n] == '?');
}

AuthResult keycloak_password_login(HttpClient &http, const std::string &email, const std::string &password,
                                   AccountCookies &cookies, TokenSet &out, std::string &detail) {
  const Pkce pkce = make_pkce(PASSWORD_VERIFIER_BYTES);
  CookieJar jar;
  auto headers = [&] { return HttpHeaders{{"Cookie", jar.header()}, {"User-Agent", BROWSER_UA}}; };

  // No prompt=none: there's no session yet, the login page is the point.
  bool posted = false;
  std::string code;
  HttpResult r = http.get(authorize_url(pkce, "&response_mode=query&nonce=" + random_token(NONCE_BYTES)), headers());
  for (int hop = 0; hop < MAX_HOPS && code.empty(); hop++) {
    jar.take(r);
    if (r.status == 302 || r.status == 303) {
      std::string next = absolute(r.location);
      // Back on the site root (not just any page of the site: Keycloak lives there too): done.
      const size_t root = strlen(REDIRECT_URI);
      if (next.compare(0, root, REDIRECT_URI) == 0 && (next.size() == root || next[root] == '?' || next[root] == '#')) {
        AuthResult got = code_from_redirect(next, pkce, code, detail);
        if (got != AuthResult::Ok) return got;
        break;  // don't load the site: that would spend the code
      }
      if (!on_site(next)) {
        detail = "login: redirected off the site";
        return AuthResult::NetworkError;
      }
      r = http.get(next, headers());
      continue;
    }
    if (r.status != 200) return http_failure(r, posted ? "login" : "authorize", detail);

    std::string page = page_id(r.body);
    if (page != "login") {
      detail = "remilia.net wants another sign-in step (" + (page.empty() ? std::string("unknown page") : page) +
               "): finish signing in on the website first.";
      return AuthResult::Rejected;
    }
    if (posted) {
      detail = "Wrong email or password.";  // the form came back
      return AuthResult::Rejected;
    }
    const std::string found = login_action(r.body);
    const std::string action = absolute(found);
    if (found.empty() || !on_site(action)) {
      detail = "login: no form on the page";
      return AuthResult::NetworkError;
    }
    std::string form = "username=" + url_encode(email) + "&password=" + url_encode(password) + "&credentialId=";
    posted = true;
    r = http.postForm(action, form, headers());
    wipe_secret(form);
  }
  if (code.empty()) {
    detail = "login: too many redirects";
    return AuthResult::NetworkError;
  }

  AuthResult result = exchange_code(http, code, pkce, out, detail);
  if (result != AuthResult::Ok) return result;

  cookies.authSessionId = jar.get("AUTH_SESSION_ID");
  cookies.keycloakSession = jar.get("KEYCLOAK_SESSION");
  cookies.keycloakIdentity = jar.get("KEYCLOAK_IDENTITY");
  if (!cookies.valid()) {
    detail = "login: the session cookies are missing";
    return AuthResult::Rejected;
  }
  detail = "ok";
  return AuthResult::Ok;
}
