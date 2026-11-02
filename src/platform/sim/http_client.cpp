#include "services/http_client.h"

#include <curl/curl.h>
#include <strings.h>

static constexpr long TIMEOUT_S = 15;

// libcurl ships with macOS. Global init isn't thread-safe, so do it at startup on the main thread.
static const bool curl_ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;

static size_t append(char *data, size_t size, size_t n, void *out) {
  static_cast<std::string *>(out)->append(data, size * n);
  return size * n;
}

// Collects "Set-Cookie: name=value; attributes..." lines into HttpResult::cookies.
static size_t header_line(char *data, size_t size, size_t n, void *out) {
  std::string line(data, size * n);
  static const char PREFIX[] = "set-cookie:";
  if (line.size() > sizeof(PREFIX) && strncasecmp(line.c_str(), PREFIX, sizeof(PREFIX) - 1) == 0) {
    std::string pair = line.substr(sizeof(PREFIX) - 1);
    pair = pair.substr(0, pair.find_first_of(";\r\n"));
    size_t start = pair.find_first_not_of(' '), eq = pair.find('=');
    if (start != std::string::npos && eq != std::string::npos && eq > start)
      static_cast<HttpHeaders *>(out)->emplace_back(pair.substr(start, eq - start), pair.substr(eq + 1));
  }
  return size * n;
}

class CurlHttpClient : public HttpClient {
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
    HttpResult res;
    CURL *curl = curl_ready ? curl_easy_init() : nullptr;
    if (!curl) {
      res.error = "curl unavailable";
      return res;
    }
    curl_slist *list = nullptr;
    for (auto &h : headers) list = curl_slist_append(list, (h.first + ": " + h.second).c_str());
    if (content_type) list = curl_slist_append(list, (std::string("Content-Type: ") + content_type).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_S);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_line);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res.cookies);
    if (body) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->data());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body->size());
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
      long status = 0;
      char *location = nullptr;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);
      res.status = (int)status;
      if (location) res.location = location;
    } else {
      res.error = curl_easy_strerror(rc);
    }
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return res;
  }
};

std::unique_ptr<HttpClient> create_http_client() { return std::make_unique<CurlHttpClient>(); }
