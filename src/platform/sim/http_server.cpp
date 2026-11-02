#include "http_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hal/hal.h"

static constexpr size_t MAX_REQUEST = 64 * 1024;

static std::string url_decode(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') {
      out += ' ';
    } else if (s[i] == '%' && i + 2 < s.size()) {
      out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

static void parse_form(const std::string &body, std::map<std::string, std::string> &form) {
  size_t pos = 0;
  while (pos < body.size()) {
    size_t end = body.find('&', pos);
    if (end == std::string::npos) end = body.size();
    std::string pair = body.substr(pos, end - pos);
    size_t eq = pair.find('=');
    if (eq != std::string::npos) form[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
    pos = end + 1;
  }
}

static std::string header_value(const std::string &head, const char *name) {
  size_t pos = 0;
  while ((pos = head.find("\r\n", pos)) != std::string::npos) {
    pos += 2;
    size_t colon = head.find(':', pos);
    size_t eol = head.find("\r\n", pos);
    if (colon == std::string::npos || colon > eol) continue;
    if (strncasecmp(head.c_str() + pos, name, colon - pos) == 0 && strlen(name) == colon - pos) {
      size_t v = head.find_first_not_of(' ', colon + 1);
      return head.substr(v, eol - v);
    }
  }
  return "";
}

static const char *reason(int status) {
  switch (status) {
    case 200: return "OK";
    case 302: return "Found";
    case 400: return "Bad Request";
    default: return "Error";
  }
}

bool HttpServer::begin(uint16_t port) {
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // this computer only: the form takes passwords
  addr.sin_port = htons(port);
  if (bind(fd_, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd_, 8) != 0) {
    hal_log("[sim] can't listen on port %u (already in use?)\n", port);
    stop();
    return false;
  }
  fcntl(fd_, F_SETFL, O_NONBLOCK);
  return true;
}

void HttpServer::stop() {
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
}

void HttpServer::poll(const Handler &handler) {
  if (fd_ < 0) return;
  for (;;) {
    int c = accept(fd_, nullptr, nullptr);
    if (c < 0) return;  // nothing pending

    // The listening socket is non-blocking; the connection itself may block briefly while reading.
    fcntl(c, F_SETFL, 0);
    timeval timeout = {1, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int yes = 1;
    setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));

    std::string data;
    char buf[4096];
    size_t head_end = std::string::npos, need = 0;
    while (data.size() < MAX_REQUEST) {
      ssize_t n = recv(c, buf, sizeof(buf), 0);
      if (n <= 0) break;
      data.append(buf, n);
      if (head_end == std::string::npos && (head_end = data.find("\r\n\r\n")) != std::string::npos)
        need = head_end + 4 + atoi(header_value(data.substr(0, head_end + 2), "Content-Length").c_str());
      if (head_end != std::string::npos && data.size() >= need) break;
    }
    if (head_end == std::string::npos) {
      close(c);
      continue;
    }

    std::string head = data.substr(0, head_end + 2);
    HttpRequest req;
    size_t sp1 = head.find(' '), sp2 = head.find(' ', sp1 + 1);
    req.method = head.substr(0, sp1);
    std::string target = head.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t q = target.find('?');
    req.path = target.substr(0, q);
    if (q != std::string::npos) parse_form(target.substr(q + 1), req.form);
    req.host = header_value(head, "Host");
    parse_form(data.substr(head_end + 4), req.form);

    HttpResponse res = handler(req);
    std::string out = "HTTP/1.1 " + std::to_string(res.status) + " " + reason(res.status) + "\r\n" +
                      "Content-Type: " + res.contentType +
                      (res.contentType.rfind("text/", 0) == 0 ? "; charset=utf-8" : "") + "\r\n" +
                      "Content-Length: " + std::to_string(res.body.size()) + "\r\n" +
                      "Cache-Control: " + res.cacheControl + "\r\nConnection: close\r\n";
    if (!res.location.empty()) out += "Location: " + res.location + "\r\n";
    out += "\r\n" + res.body;
    send(c, out.data(), out.size(), 0);
    close(c);
  }
}

std::string lan_ip() {
  std::string found = "localhost";
  ifaddrs *list = nullptr;
  if (getifaddrs(&list) != 0) return found;
  for (ifaddrs *i = list; i; i = i->ifa_next) {
    // Physical interfaces only (en0, en1...): skips loopback and VPN tunnels (utun).
    if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP)) continue;
    if (strncmp(i->ifa_name, "en", 2) != 0) continue;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((sockaddr_in *)i->ifa_addr)->sin_addr, ip, sizeof(ip));
    found = ip;
    break;
  }
  freeifaddrs(list);
  return found;
}
