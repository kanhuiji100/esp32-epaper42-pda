// ESP32 + Waveshare 4.2" e-Paper (WFT0420CZ15 / UC8176)
// Fast full refresh (~0.5-0.8s) and partial window refresh (~0.3s).
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "panel42.h"
#include "storage_impl.h"
#include "ui_impl.h"
#include "webpage.h"

// ---- WiFi ----
// Copy secrets.example.h to secrets.h and fill in your network.
// secrets.h is git-ignored; the fallback keeps CI builds working.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif
static const char* DEFAULT_SSID = WIFI_SSID;
static const char* DEFAULT_PASS = WIFI_PASSWORD;

WiFiServer server(80);
static uint8_t rxbuf[panel42::IMGSZ];
static uint32_t lastActivity = 0;
Preferences wifiPrefs;
Preferences hostPrefs;
static bool netReady = false;
static String gHostname = "epaper42";
static bool hostnameSet(const String& name);   // defined with the Wi-Fi helpers


// ---------------- HTTP helpers ----------------
static String readLine(WiFiClient& c, uint32_t timeoutMs = 2000) {
  String s;
  s.reserve(96);
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (c.available()) {
      char ch = (char)c.read();
      if (ch == '\r\n') {
        if (s.endsWith("\r")) s.remove(s.length() - 1);
        return s;
      }
      if (ch != '\r') s += ch;
      if (s.length() > 1024) return s;
    }
    if (!c.connected()) break;
    delay(1);
  }
  return s;
}

static const char* reason(int code) {
  switch (code) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 411: return "Length Required";
    case 500: return "Internal Server Error";
    default:  return "Error";
  }
}

static void sendJson(WiFiClient& c, int code, const String& body) {
  c.print("HTTP/1.1 "); c.print(code); c.print(' '); c.print(reason(code)); c.print("\r\n");
  c.print("Content-Type: application/json; charset=utf-8\r\n");
  c.print("Cache-Control: no-store\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n");
  c.print("Content-Length: "); c.print(body.length()); c.print("\r\n\r\n");
  c.print(body);
  c.stop();
}

static void sendPage(WiFiClient& c) {
  c.print("HTTP/1.1 200 OK\r\n");
  c.print("Content-Type: text/html; charset=utf-8\r\n");
  c.print("Cache-Control: no-store\r\n");
  c.print("Connection: close\r\n");
  c.print("Content-Length: "); c.print(strlen(PAGE_HTML)); c.print("\r\n\r\n");
  c.print(PAGE_HTML);
  c.stop();
}

static void sendBytes(WiFiClient& c, const uint8_t* data, size_t len) {
  c.print("HTTP/1.1 200 OK\r\n");
  c.print("Content-Type: application/octet-stream\r\n");
  c.print("Cache-Control: no-store\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n");
  c.print("Content-Length: "); c.print((unsigned long)len); c.print("\r\n\r\n");
  c.write(data, len);
  c.stop();
}

static void sendFile(WiFiClient& c, const String& path) {
  if (!storage::pathOk(path)) { sendJson(c, 400, "{\"ok\":0,\"err\":\"bad path\"}"); return; }
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) { sendJson(c, 404, "{\"ok\":0,\"err\":\"not found\"}"); return; }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  if (n < 0) n = 0;
  fseek(f, 0, SEEK_SET);
  c.print("HTTP/1.1 200 OK\r\n");
  c.print("Content-Type: application/octet-stream\r\n");
  c.print("Cache-Control: no-store\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n");
  c.print("Content-Length: "); c.print(n); c.print("\r\n\r\n");
  uint8_t buf[1024];
  size_t r;
  while ((r = fread(buf, 1, sizeof(buf), f)) > 0) c.write(buf, r);
  fclose(f);
  c.stop();
}

static void sendText(WiFiClient& c, const String& s) {
  c.print("HTTP/1.1 200 OK\r\n");
  c.print("Content-Type: text/plain; charset=utf-8\r\n");
  c.print("Cache-Control: no-store\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n");
  c.print("Content-Length: "); c.print(s.length()); c.print("\r\n\r\n");
  c.print(s);
  c.stop();
}

static void sendNoContent(WiFiClient& c) {
  c.print("HTTP/1.1 204 No Content\r\n");
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Access-Control-Allow-Methods: GET,POST,DELETE,OPTIONS\r\n");
  c.print("Access-Control-Allow-Headers: Content-Type\r\n");
  c.print("Connection: close\r\n\r\n");
  c.stop();
}

static bool readBody(WiFiClient& c, uint8_t* buf, int len) {
  int got = 0;
  uint32_t t0 = millis();
  while (got < len && millis() - t0 < 5000) {
    int n = c.read(buf + got, (size_t)((len - got) < 1460 ? (len - got) : 1460));
    if (n > 0) got += n;
    else {
      if (!c.connected()) break;
      delay(1);
    }
  }
  return got == len;
}

static bool getParamStr(const String& q, const char* key, String& out) {
  String k = String(key) + "=";
  int p = q.indexOf(k);
  if (p < 0) return false;
  int e = q.indexOf('&', p);
  out = q.substring(p + k.length(), e < 0 ? q.length() : e);
  return out.length() > 0;
}

static String urlDecode(const String& in) {
  String out;
  for (int i = 0; i < (int)in.length(); i++) {
    char c = in[i];
    if (c == '+') { out += ' '; continue; }
    if (c == '%' && i + 2 < (int)in.length()) {
      int hi = -1, lo = -1;
      char a = in[i + 1], b = in[i + 2];
      if (a >= '0' && a <= '9') hi = a - '0';
      else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
      else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
      if (b >= '0' && b <= '9') lo = b - '0';
      else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
      else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
      if (hi >= 0 && lo >= 0) { out += (char)((hi << 4) | lo); i += 2; continue; }
    }
    out += c;
  }
  return out;
}

static bool getParam(const String& q, const char* key, int& out) {
  String k = String(key) + "=";
  int p = q.indexOf(k);
  if (p < 0) return false;
  int e = q.indexOf('&', p);
  String v = q.substring(p + k.length(), e < 0 ? q.length() : e);
  if (v.length() == 0) return false;
  out = v.toInt();
  return true;
}

static void sendTiming(WiFiClient& c, const char* mode, const panel42::Timings& t) {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"ok\":1,\"mode\":\"%s\",\"init\":%lu,\"data\":%lu,\"refresh\":%lu,\"total\":%lu,\"heap\":%lu}",
           mode, (unsigned long)t.init, (unsigned long)t.data,
           (unsigned long)t.refresh, (unsigned long)t.total,
           (unsigned long)ESP.getFreeHeap());
  sendJson(c, 200, buf);
}

// ---------------- request dispatcher ----------------
static bool webLogEnabled = false;
static void setWebLogEnabled(bool on) { webLogEnabled = on; }
static bool webLogEnabledState() { return webLogEnabled; }

void handleClient(WiFiClient& c) {
  c.setTimeout(2000);
  String req = readLine(c);
  if (req.length() == 0) { c.stop(); return; }

  int sp1 = req.indexOf(' ');
  int sp2 = (sp1 < 0) ? -1 : req.indexOf(' ', sp1 + 1);
  if (sp1 < 0) { sendJson(c, 400, "{\"ok\":0,\"err\":\"bad request line\"}"); return; }
  String method = req.substring(0, sp1);
  String target = req.substring(sp1 + 1, sp2 < 0 ? req.length() : sp2);
  String path = target;
  String query = "";
  int q = target.indexOf('?');
  if (q >= 0) { path = target.substring(0, q); query = target.substring(q + 1); }

  int contentLength = -1;
  bool expect100 = false;
  bool chunked = false;
  for (int i = 0; i < 64; i++) {
    String h = readLine(c);
    if (h.length() == 0) break;
    int colon = h.indexOf(':');
    if (colon < 0) continue;
    String k = h.substring(0, colon); k.toLowerCase();
    String v = h.substring(colon + 1); v.trim();
    if (k == "content-length") contentLength = v.toInt();
    else if (k == "expect" && v.indexOf("100-continue") >= 0) expect100 = true;
    else if (k == "transfer-encoding" && v.indexOf("chunked") >= 0) chunked = true;
  }

  if (webLogEnabled) Serial.printf("REQ %s %s clen=%d\r\n", method.c_str(), target.c_str(), contentLength);

  if (method == "OPTIONS") { sendNoContent(c); return; }

  if (method == "GET") {
    if (path == "/" || path == "/index.html") { sendPage(c); return; }
    if (path == "/api/status") {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "{\"ok\":1,\"ip\":\"%s\",\"host\":\"%s\",\"heap\":%lu,\"powered\":%d,\"mode\":\"%s\"}",
               WiFi.localIP().toString().c_str(), gHostname.c_str(),
               (unsigned long)ESP.getFreeHeap(),
               panel42::isPowered() ? 1 : 0,
               panel42::modeName());
      sendJson(c, 200, buf);
      return;
    }
    if (path == "/api/note") {
      sendText(c, ui::noteText());
      return;
    }
    if (path == "/api/screen") {
      sendBytes(c, ui::screen(), panel42::IMGSZ);
      return;
    }
    if (path == "/api/ui") {
      sendJson(c, 200, ui::stateJson());
      return;
    }
    if (path == "/api/qr") {
      sendJson(c, 200, ui::stateJson());
      return;
    }
    if (path == "/api/host") {
      char buf[192];
      snprintf(buf, sizeof(buf),
               "{\"ok\":1,\"host\":\"%s\",\"mdns\":\"http://%s.local\"}",
               gHostname.c_str(), gHostname.c_str());
      sendJson(c, 200, buf);
      return;
    }
    if (path == "/api/storage") {
      sendJson(c, 200, storage::infoJson());
      return;
    }
    if (path == "/api/storage/list") {
      String p = "/int";
      getParamStr(query, "path", p);
      p = urlDecode(p);
      sendJson(c, 200, storage::listJson(p));
      return;
    }
    if (path == "/api/storage/file") {
      String p;
      if (!getParamStr(query, "path", p)) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"need path\"}");
        return;
      }
      sendFile(c, urlDecode(p));
      return;
    }
    sendJson(c, 404, "{\"ok\":0,\"err\":\"not found\"}");
    return;
  }

  if (method == "DELETE") {
    if (path == "/api/storage/file") {
      String p;
      if (!getParamStr(query, "path", p)) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"need path\"}");
        return;
      }
      p = urlDecode(p);
      if (!storage::removeFile(p)) {
        sendJson(c, 404, "{\"ok\":0,\"err\":\"remove failed\"}");
        return;
      }
      sendJson(c, 200, "{\"ok\":1}");
      return;
    }
    sendJson(c, 404, "{\"ok\":0,\"err\":\"not found\"}");
    return;
  }

  if (method != "POST") { sendJson(c, 405, "{\"ok\":0,\"err\":\"method not allowed\"}"); return; }
  if (chunked) { sendJson(c, 411, "{\"ok\":0,\"err\":\"chunked body not supported\"}"); return; }

  if (path == "/api/key") {
    String k = "";
    if (!getParamStr(query, "k", k)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"need k\"}");
      return;
    }
    ui::key(k);
    lastActivity = millis();
    sendJson(c, 200, ui::stateJson());
    return;
  }

  if (path == "/api/app") {
    String name = "";
    if (!getParamStr(query, "name", name)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"need name\"}");
      return;
    }
    ui::setApp(name);
    lastActivity = millis();
    sendJson(c, 200, ui::stateJson());
    return;
  }

  if (path == "/api/note") {
    if (contentLength < 0) {
      sendJson(c, 411, "{\"ok\":0,\"err\":\"need content-length\"}");
      return;
    }
    if (contentLength > 4095) {
      sendJson(c, 413, "{\"ok\":0,\"err\":\"note too long (max 4095 bytes)\"}");
      return;
    }
    if (expect100) { c.print("HTTP/1.1 100 Continue\r\n\r\n"); }
    if (contentLength > 0 && !readBody(c, rxbuf, contentLength)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"body timeout\"}");
      return;
    }
    rxbuf[contentLength] = 0;
    String text((char*)rxbuf, contentLength);
    ui::setNote(text);
    lastActivity = millis();
    sendJson(c, 200, ui::stateJson());
    return;
  }

  if (path == "/api/storage/file") {
    String p;
    if (!getParamStr(query, "path", p)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"need path\"}");
      return;
    }
    p = urlDecode(p);
    if (!storage::pathOk(p)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"bad path\"}");
      return;
    }
    if (contentLength <= 0 || contentLength > 4 * 1024 * 1024) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"body 1..4MB\"}");
      return;
    }
    if (expect100) { c.print("HTTP/1.1 100 Continue\r\n\r\n"); }
    FILE* f = fopen(p.c_str(), "wb");
    if (!f) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"open failed\"}");
      return;
    }
    size_t remaining = contentLength;
    bool ok = true;
    while (remaining > 0) {
      size_t chunk = remaining > sizeof(rxbuf) ? sizeof(rxbuf) : remaining;
      if (!readBody(c, rxbuf, chunk)) { ok = false; break; }
      if (fwrite(rxbuf, 1, chunk, f) != chunk) { ok = false; break; }
      remaining -= chunk;
    }
    fclose(f);
    if (!ok) {
      remove(p.c_str());
      sendJson(c, 400, "{\"ok\":0,\"err\":\"write failed\"}");
      return;
    }
    lastActivity = millis();
    sendJson(c, 200, String("{\"ok\":1,\"size\":") + String((unsigned)contentLength) + "}");
    return;
  }

  if (path == "/api/host") {
    String name;
    if (!getParamStr(query, "name", name)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"need name\"}");
      return;
    }
    name = urlDecode(name);
    if (!hostnameSet(name)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"hostname must be 1-32 chars: a-z 0-9 -, no leading/trailing -\"}");
      return;
    }
    lastActivity = millis();
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"ok\":1,\"host\":\"%s\",\"mdns\":\"http://%s.local\"}",
             gHostname.c_str(), gHostname.c_str());
    sendJson(c, 200, buf);
    return;
  }

  if (path == "/api/qr") {
    String type = "text";
    getParamStr(query, "type", type);
    type.toLowerCase();

    if (type == "clear") {
      ui::clearQr();
      lastActivity = millis();
      sendJson(c, 200, ui::stateJson());
      return;
    }

    if (type == "wifi") {
      String ssid, pass;
      if (!getParamStr(query, "ssid", ssid)) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"need ssid\"}");
        return;
      }
      getParamStr(query, "pass", pass);   // empty password allowed
      ssid = urlDecode(ssid);
      pass = urlDecode(pass);
      if (!ui::setWifiQr(ssid, pass)) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"bad WiFi QR: SSID 1-32 bytes, password 8-63 or empty\"}");
        return;
      }
      lastActivity = millis();
      sendJson(c, 200, ui::stateJson());
      return;
    }

    if (type == "text") {
      if (contentLength <= 0 || contentLength > 1024) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"text body 1-1024 bytes\"}");
        return;
      }
      if (expect100) { c.print("HTTP/1.1 100 Continue\r\n\r\n"); }
      if (!readBody(c, rxbuf, contentLength)) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"body timeout\"}");
        return;
      }
      rxbuf[contentLength] = 0;
      String text((char*)rxbuf, contentLength);
      if (!ui::setQr(text, "text")) {
        sendJson(c, 400, "{\"ok\":0,\"err\":\"text too long for QR version 13\"}");
        return;
      }
      lastActivity = millis();
      sendJson(c, 200, ui::stateJson());
      return;
    }

    sendJson(c, 400, "{\"ok\":0,\"err\":\"type must be text|wifi|clear\"}");
    return;
  }

  if (path == "/api/full") {
    if (contentLength != panel42::IMGSZ) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"full image must be 15000 bytes\"}");
      return;
    }
    if (expect100) { c.print("HTTP/1.1 100 Continue\r\n\r\n"); }
    if (!readBody(c, rxbuf, panel42::IMGSZ)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"body timeout\"}");
      return;
    }
    String fm = "fast";
    getParamStr(query, "mode", fm);
    panel42::Timings t;
    if (fm == "std")      panel42::fullStd(rxbuf, t);
    else if (fm == "hq")  panel42::fullHq(rxbuf, t);
    else                  panel42::fullFast(rxbuf, t);
    ui::setImage(rxbuf, true);
    lastActivity = millis();
    Serial.printf("FULL %s total=%lu init=%lu data=%lu refresh=%lu\r\n", fm.c_str(),
                  (unsigned long)t.total, (unsigned long)t.init,
                  (unsigned long)t.data, (unsigned long)t.refresh);
    sendTiming(c, fm.c_str(), t);
    return;
  }

  if (path == "/api/partial") {
    int x = 0, y = 0, w = 0, h = 0;
    if (!getParam(query, "x", x) || !getParam(query, "y", y) ||
        !getParam(query, "w", w) || !getParam(query, "h", h)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"need x,y,w,h\"}");
      return;
    }
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x % 8 != 0 || w % 8 != 0 ||
        x + w > panel42::W || y + h > panel42::H) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"bad window: x,w multiple of 8, inside 400x300\"}");
      return;
    }
    int need = (w / 8) * h;
    if (contentLength != need) {
      char buf[128];
      snprintf(buf, sizeof(buf), "{\"ok\":0,\"err\":\"body must be %d bytes\"}", need);
      sendJson(c, 400, buf);
      return;
    }
    if (expect100) { c.print("HTTP/1.1 100 Continue\r\n\r\n"); }
    if (!readBody(c, rxbuf, need)) {
      sendJson(c, 400, "{\"ok\":0,\"err\":\"body timeout\"}");
      return;
    }
    panel42::Timings t;
    panel42::partialWindow(x, y, w, h, rxbuf, t);
    lastActivity = millis();
    Serial.printf("PART %dx%d@%d,%d total=%lu init=%lu data=%lu refresh=%lu\r\n",
                  w, h, x, y, (unsigned long)t.total, (unsigned long)t.init,
                  (unsigned long)t.data, (unsigned long)t.refresh);
    sendTiming(c, "partial", t);
    return;
  }

  if (path == "/api/clear") {
    panel42::Timings t;
    panel42::clear(t);
    ui::onClear();
    lastActivity = millis();
    Serial.printf("CLEAR total=%lu refresh=%lu\r\n", (unsigned long)t.total, (unsigned long)t.refresh);
    sendTiming(c, "clear", t);
    return;
  }

  if (path == "/api/refresh") {
    panel42::Timings t;
    panel42::refreshHq(t);
    lastActivity = millis();
    Serial.printf("REFRESH total=%lu refresh=%lu\r\n", (unsigned long)t.total, (unsigned long)t.refresh);
    sendTiming(c, "refresh", t);
    return;
  }

  if (path == "/api/poweroff") {
    panel42::powerOff();
    sendJson(c, 200, "{\"ok\":1,\"mode\":\"off\"}");
    return;
  }

  sendJson(c, 404, "{\"ok\":0,\"err\":\"not found\"}");
}

// ---------------- WiFi ----------------
// Credentials come from NVS namespace "wifi" if provisioned over serial,
// otherwise from secrets.h (or the placeholder fallback for CI builds).
static bool wifiLoadCredentials(String& ssid, String& pass) {
  wifiPrefs.begin("wifi", false);
  ssid = wifiPrefs.getString("ssid", "");
  pass = wifiPrefs.getString("pass", "");
  if (ssid.length() == 0) {
    ssid = DEFAULT_SSID;
    pass = DEFAULT_PASS;
    return false;
  }
  return true;
}

static String normalizeHost(String h) {
  h.trim();
  h.toLowerCase();
  if (h.length() == 0 || h.length() > 32) return "";
  for (size_t i = 0; i < h.length(); i++) {
    char c = h[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return "";
  }
  if (h[0] == '-' || h[h.length() - 1] == '-') return "";
  return h;
}

static void startMdns() {
  MDNS.end();
  if (MDNS.begin(gHostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\r\n", gHostname.c_str());
  } else {
    Serial.println("mDNS: begin failed");
  }
}

static void hostnameLoad() {
  hostPrefs.begin("net", false);
  String h = normalizeHost(hostPrefs.getString("host", ""));
  if (h.length() > 0) gHostname = h;
}

static bool hostnameSet(const String& name) {
  String h = normalizeHost(name);
  if (h.length() == 0) return false;
  gHostname = h;
  hostPrefs.begin("net", false);
  hostPrefs.putString("host", h);
  WiFi.setHostname(gHostname.c_str());
  if (netReady) startMdns();
  ui::setHostnameLabel(gHostname, true);
  return true;
}

static void startNetworkServices() {
  WiFi.setSleep(false);          // keep the radio awake; lower latency
  server.begin();
  server.setNoDelay(true);
  startMdns();
  configTzTime("CST-8", "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
  netReady = true;
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.println("HTTP server on :80");
}

static bool wifiStart(const String& ssid, const String& pass, bool save) {
  if (ssid.length() == 0) {
    Serial.println("WiFi: empty SSID");
    return false;
  }
  Serial.printf("WiFi: connecting to %s\r\n", ssid.c_str());
  if (netReady) {
    server.end();
    MDNS.end();
  }
  netReady = false;
  WiFi.disconnect(true);
  delay(120);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(gHostname.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    if (save) {
      wifiPrefs.putString("ssid", ssid);
      wifiPrefs.putString("pass", pass);
      Serial.println("WiFi: saved to NVS");
    }
    startNetworkServices();
    return true;
  }
  Serial.println("WiFi: connect failed");
  return false;
}

static bool wifiConnectPreferred() {
  String s, p;
  bool fromNvs = wifiLoadCredentials(s, p);
  if (wifiStart(s, p, false)) return true;
  if (fromNvs && (s != String(DEFAULT_SSID) || p != String(DEFAULT_PASS))) {
    Serial.println("WiFi: NVS credentials failed, trying secrets.h");
    return wifiStart(String(DEFAULT_SSID), String(DEFAULT_PASS), false);
  }
  return false;
}

#include "serial_console.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\r\n=== E-Paper 4.2 fast web console ===");
  panel42::begin();
  panel42::Timings bt;
  panel42::bootClear(&bt);
  Serial.printf("boot clear: total=%lu init=%lu data=%lu refresh=%lu\r\n",
                (unsigned long)bt.total, (unsigned long)bt.init,
                (unsigned long)bt.data, (unsigned long)bt.refresh);
  hostnameLoad();
  ui::setHostnameLabel(gHostname, false);
  Serial.printf("hostname: %s (http://%s.local)\r\n", gHostname.c_str(), gHostname.c_str());
  WiFi.persistent(false);
  wifiConnectPreferred();
  lastActivity = millis();
  ui::begin();
  storage::begin();
  Serial.println("local UI ready");
  console::begin();
}

void loop() {
  ui::tick();
  console::loop();
  if (WiFi.status() != WL_CONNECTED) {
    netReady = false;
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 10000) {
      WiFi.reconnect();
      lastTry = millis();
    }
  } else if (!netReady) {
    startNetworkServices();
  }
  if (netReady) {
    WiFiClient client = server.available();
    if (client) handleClient(client);
  }
  if (strcmp(ui::appName(), "clock") != 0 && panel42::isPowered() &&
      millis() - lastActivity > 120000UL) {
    panel42::powerOff();
    Serial.println("panel idle power off");
  }
  delay(1);
}
