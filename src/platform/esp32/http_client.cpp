#include "services/http_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <mutex>
#include <vector>

#include "root_ca.h"

// A TLS handshake costs about a second here, so connections stay open between requests and are
// shared by every client (jobs each make their own, on their own threads).
static constexpr size_t MAX_IDLE = 2;              // ~40 KB of RAM each
static constexpr uint32_t IDLE_MAX_MS = 30 * 1000;  // under the servers' keep-alive timeouts
static constexpr uint32_t TIMEOUT_MS = 15 * 1000;

// Collects a response body into a std::string; binary-safe, handles chunked responses.
class StringSink : public Stream {
 public:
  explicit StringSink(std::string &out) : out_(out) {}
  size_t write(uint8_t c) override {
    out_ += (char)c;
    return 1;
  }
  size_t write(const uint8_t *data, size_t len) override {
    out_.append((const char *)data, len);
    return len;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }

 private:
  std::string &out_;
};

// One connection to one host. HTTPClient lives with it: its destructor closes the socket.
struct Connection {
  std::string host;
  WiFiClientSecure tls;
  HTTPClient http;
  uint32_t idleSince = 0;

  explicit Connection(std::string h) : host(std::move(h)) {
    tls.setCACert(ROOT_CA_PEM);
    http.setReuse(true);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setTimeout(TIMEOUT_MS);
  }
};

static std::mutex pool_mutex;
static std::vector<std::unique_ptr<Connection>> pool;  // idle, most recent last

static std::string host_of(const std::string &url) {
  size_t start = url.find("://");
  start = start == std::string::npos ? 0 : start + 3;
  return url.substr(start, url.find_first_of(":/?#", start) - start);
}

// An open connection to `host` if one is idle, else a new (unconnected) one.
static std::unique_ptr<Connection> take(const std::string &host) {
  std::lock_guard<std::mutex> lock(pool_mutex);
  std::unique_ptr<Connection> found;
  for (auto it = pool.begin(); it != pool.end();) {
    bool stale = millis() - (*it)->idleSince > IDLE_MAX_MS || !(*it)->tls.connected();
    if (stale || (!found && (*it)->host == host)) {
      if (!stale) found = std::move(*it);
      it = pool.erase(it);  // a stale one closes here
    } else {
      ++it;
    }
  }
  return found ? std::move(found) : std::make_unique<Connection>(host);
}

static void give_back(std::unique_ptr<Connection> conn) {
  if (!conn->tls.connected()) return;  // the server closed it (Connection: close)
  conn->idleSince = millis();
  std::lock_guard<std::mutex> lock(pool_mutex);
  pool.push_back(std::move(conn));
  if (pool.size() > MAX_IDLE) pool.erase(pool.begin());
}

class EspHttpClient : public HttpClient {
 public:
  HttpResult get(const std::string &url, const HttpHeaders &headers) override {
    return request(url, nullptr, nullptr, headers);
  }
  HttpResult post(const std::string &url, const char *content_type, const std::string &body,
                  const HttpHeaders &headers) override {
    return request(url, content_type, &body, headers);
  }

 private:
  HttpResult request(const std::string &url, const char *content_type, const std::string *body,
                     const HttpHeaders &headers) {
    std::string host = host_of(url);
    for (int attempt = 0;; attempt++) {
      std::unique_ptr<Connection> conn = take(host);
      bool reused = conn->tls.connected();
      HttpResult res;
      int code = send(*conn, url, content_type, body, headers, res);
      // A kept connection the server dropped meanwhile fails before any answer: once more, fresh.
      if (code < 0 && reused && code != HTTPC_ERROR_READ_TIMEOUT && attempt == 0) continue;
      give_back(std::move(conn));
      return res;
    }
  }

  static int send(Connection &conn, const std::string &url, const char *content_type, const std::string *body,
                  const HttpHeaders &headers, HttpResult &res) {
    HTTPClient &http = conn.http;
    // Set-Cookie is read through HTTPClient's jar (its header list keeps only the last of a
    // repeated header). Empty per request, so it never adds cookies of its own to what we send.
    CookieJar jar;
    http.setCookieJar(&jar);
    if (!http.begin(conn.tls, url.c_str())) {
      res.error = "bad URL";
      return HTTPC_ERROR_CONNECTION_REFUSED;
    }
    static const char *collect[] = {"Location"};
    http.collectHeaders(collect, 1);
    for (auto &h : headers) http.addHeader(h.first.c_str(), h.second.c_str());

    int code;
    if (body) {
      if (content_type) http.addHeader("Content-Type", content_type);
      code = http.POST((uint8_t *)body->data(), body->size());
    } else {
      code = http.GET();
    }
    if (code > 0) {
      res.status = code;
      res.location = http.header("Location").c_str();
      StringSink sink(res.body);
      if (http.writeToStream(&sink) < 0) conn.tls.stop();  // body cut short: not reusable
      for (const Cookie &c : jar) res.cookies.emplace_back(c.name.c_str(), c.value.c_str());
    } else {
      res.error = HTTPClient::errorToString(code).c_str();
    }
    http.end();  // keeps the socket open unless the server asked to close it
    http.setCookieJar(nullptr);  // the jar dies with this call
    return code;
  }
};

std::unique_ptr<HttpClient> create_http_client() { return std::make_unique<EspHttpClient>(); }
