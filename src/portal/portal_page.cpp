#include "portal/portal_page.h"

// The blue VHS screen, like the device's: white text with red/cyan ghosts, scanlines on top,
// text-only buttons with a rounded outline that light up and squish when pressed.
static const char STYLE[] =
    ":root{--w:rgba(255,255,255,.92);--r:rgba(255,59,107,.6);--c:rgba(59,240,255,.6);--lit:rgba(255,255,255,.2)}"
    "body{margin:0;min-height:100vh;color:var(--w);font:15px/1.45 Menlo,ui-monospace,'DejaVu Sans Mono',monospace;"
    "text-shadow:-1px 0 var(--r),1px 0 var(--c);-webkit-text-size-adjust:100%}"
    "body:before{content:'';position:fixed;inset:0;z-index:-1;background:#2328c8 url(/bg.jpg) center/cover}"
    "body:after{content:'';position:fixed;inset:0;pointer-events:none;"
    "background:repeating-linear-gradient(transparent 0 2px,rgba(0,0,20,.16) 2px 3px)}"
    "main{max-width:440px;margin:auto;padding:18px 18px 40px}"
    "header{display:flex;justify-content:space-between;letter-spacing:.12em;margin-bottom:22px}"
    "h1,h2,.lbl{font-size:15px;font-weight:400;letter-spacing:.1em;text-transform:uppercase;margin:26px 0 10px}"
    "h1{font-size:20px;margin-top:0}"
    ".nets{display:flex;flex-direction:column;gap:2px}"
    ".net input{position:absolute;opacity:0}"
    ".row{display:flex;align-items:center;gap:10px;padding:11px 12px;border-radius:6px;cursor:pointer;"
    "transition:transform .08s}"
    ".row:active{transform:scale(.97)}"
    ".row:before{content:'\\25B6';width:1em;visibility:hidden}"
    ".net input:checked+.row{background:var(--lit)}"
    ".net input:checked+.row:before{visibility:visible}"
    ".name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".tag{font-size:12px;opacity:.75}"
    ".bars{display:flex;align-items:flex-end;gap:2px;height:14px}"
    ".bars i{width:4px;background:var(--w);box-shadow:-1px 0 var(--r),1px 0 var(--c)}"
    ".bars i.off{opacity:.25}"
    ".pw{padding:8px 12px 14px 34px}"
    "input:not([type=radio]){box-sizing:border-box;width:100%;margin-top:8px;padding:11px 12px;border-radius:6px;"
    "border:2px solid var(--w);background:rgba(0,0,40,.35);color:#fff;font:inherit;text-shadow:none;outline:0}"
    "input:not([type=radio]):focus{box-shadow:-2px 0 var(--r),2px 0 var(--c)}"
    "input::placeholder{color:rgba(255,255,255,.5)}"
    "small{display:block;margin-top:10px;font-size:12px;opacity:.75}"
    "button{display:block;width:100%;margin-top:30px;padding:12px;border:2px solid var(--w);border-radius:6px;"
    "background:transparent;color:inherit;font:inherit;letter-spacing:.12em;text-transform:uppercase;"
    "text-shadow:inherit;box-shadow:-1px 0 var(--r),1px 0 var(--c);transition:transform .08s}"
    "button:active{background:var(--lit);transform:scale(.97)}"
    ".err{border:2px solid #ff3b6b;border-radius:6px;padding:10px 12px;text-transform:uppercase}"
    "[hidden]{display:none!important}";

// Moves the password box under the picked network and hides it for open ones. Without JS it just
// stays under the list.
static const char SCRIPT[] =
    "var pw=document.getElementById('pwbox');"
    "function pick(focus){var c=document.querySelector('input[name=ssid]:checked');"
    "if(!c){pw.hidden=true;return}"
    "c.parentNode.after(pw);pw.hidden=c.dataset.open==='1';if(pw.hidden)pw.firstChild.value='';"
    "if(focus&&!pw.hidden)pw.firstChild.focus()}"
    "document.querySelectorAll('input[name=ssid]').forEach(function(r){r.onchange=function(){pick(true)}});"
    "pick(false);";

static constexpr int BAR_DBM[] = {-90, -78, -67, -55};  // signal bars lit at or above each level

static std::string escape(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
  return out;
}

static std::string page(const std::string &status, const std::string &body, const char *script = "") {
  return std::string(
             "<!doctype html><html><head><meta charset=utf-8>"
             "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
             "<title>BeetleBoy32 setup</title><style>") +
         STYLE + "</style></head><body><main><header><span>BEETLEBOY32</span><span>" + status +
         "</span></header>" + body + "</main>" + (*script ? std::string("<script>") + script + "</script>" : "") +
         "</body></html>";
}

static std::string bars(int rssi) {
  std::string out = "<span class=bars>";
  int height = 5;
  for (int dbm : BAR_DBM) {
    out += std::string("<i") + (rssi >= dbm ? "" : " class=off") + " style=\"height:" + std::to_string(height) +
           "px\"></i>";
    height += 3;
  }
  return out + "</span>";
}

static std::string network_row(const ScannedNetwork &n, bool checked, bool required) {
  return "<label class=net><input type=radio name=ssid value=\"" + escape(n.ssid) + "\"" +
         (n.open ? " data-open=1" : "") + (checked ? " checked" : "") + (required ? " required" : "") +
         "><span class=row><span class=name>" + escape(n.ssid) + "</span>" +
         (n.open ? "<span class=tag>OPEN</span>" : "") + bars(n.rssi) + "</span></label>";
}

static std::string wifi_section(const PortalFormView &view) {
  std::string out = "<h2>Wi-Fi network</h2><div class=nets>";
  for (size_t i = 0; i < view.networks.size(); i++)
    out += network_row(view.networks[i], view.networks[i].ssid == view.ssid, i == 0);
  if (view.networks.empty()) out += "<p class=err>No Wi-Fi found. Restart BeetleBoy32 to scan again.</p>";
  out += "</div><div id=pwbox class=pw>"
         "<input name=password type=password placeholder=\"Wi-Fi password\" autocomplete=off></div>";
  return out;
}

std::string portal_form_page(const PortalFormView &view) {
  const char *title = view.mode == SetupMode::WifiOnly      ? "New Wi-Fi"
                      : view.mode == SetupMode::AccountOnly ? "Sign in again"
                                                            : "Setup";
  std::string body = std::string("<h1>") + title + "</h1>";
  if (!view.error.empty()) body += "<p class=err>" + escape(view.error) + "</p>";
  body += "<form method=post action=/save>";
  if (view.mode != SetupMode::AccountOnly) {
    body += wifi_section(view);
  } else {
    body += "<small>Wi-Fi: " + escape(view.keptSsid) + " (kept)</small>";
  }
  if (view.mode != SetupMode::WifiOnly) {
    if (view.mode == SetupMode::AccountOnly) body += "<p>Your remilia.net login expired.</p>";
    body +=
        "<h2>remilia.net account</h2>"
        "<input name=email type=email placeholder=Email autocomplete=username autocapitalize=off required value=\"" +
        escape(view.email) +
        "\">"
        "<input name=account_password type=password placeholder=Password autocomplete=current-password required>"
        "<small>Used once to sign in, sent only to remilia.net, never stored on the device.</small>";
  }
  body += "<button>&#9654; Connect</button></form>";
  return page("&#9654; SETUP", body, view.mode != SetupMode::AccountOnly ? SCRIPT : "");
}

std::string portal_done_page(const std::string &ssid) {
  return page("&#9632; SAVED",
              "<h1>Saved</h1><p>BeetleBoy32 is connecting to " + escape(ssid) +
                  ". Check its screen.</p><small>You can close this page.</small>");
}
