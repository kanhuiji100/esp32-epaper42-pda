// ESP32 + Waveshare 4.2" e-Paper (WFT0420CZ15 / UC8176)
// Fast full refresh (~0.5-0.8s) and partial window refresh (~0.3s).
#include <WiFi.h>
#include <ESPmDNS.h>
#include "panel42.h"
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
static const char* SSID = WIFI_SSID;
static const char* PASS = WIFI_PASSWORD;

WiFiServer server(80);
static uint8_t rxbuf[panel42::IMGSZ];
static uint32_t lastActivity = 0;

// ---------------- HTTP helpers ----------------
static String readLine(WiFiClient& c, uint32_t timeoutMs = 2000) {
  String s;
  s.reserve(96);
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (c.available()) {
      char ch = (char)c.read();
      if (ch == '\n') {
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
  c.print("Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n");
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
static void handleClient(WiFiClient& c) {
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

  Serial.printf("REQ %s %s clen=%d\n", method.c_str(), target.c_str(), contentLength);

  if (method == "OPTIONS") { sendNoContent(c); return; }

  if (method == "GET") {
    if (path == "/" || path == "/index.html") { sendPage(c); return; }
    if (path == "/api/status") {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "{\"ok\":1,\"ip\":\"%s\",\"heap\":%lu,\"powered\":%d,\"mode\":\"%s\"}",
               WiFi.localIP().toString().c_str(),
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
    Serial.printf("FULL %s total=%lu init=%lu data=%lu refresh=%lu\n", fm.c_str(),
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
    Serial.printf("PART %dx%d@%d,%d total=%lu init=%lu data=%lu refresh=%lu\n",
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
    Serial.printf("CLEAR total=%lu refresh=%lu\n", (unsigned long)t.total, (unsigned long)t.refresh);
    sendTiming(c, "clear", t);
    return;
  }

  if (path == "/api/refresh") {
    panel42::Timings t;
    panel42::refreshStd(t);
    lastActivity = millis();
    Serial.printf("REFRESH total=%lu refresh=%lu\n", (unsigned long)t.total, (unsigned long)t.refresh);
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
static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);            // lower HTTP latency
  WiFi.setHostname("epaper42");
  WiFi.begin(SSID, PASS);
  Serial.print("WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    if (MDNS.begin("epaper42")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS: http://epaper42.local");
    }
  } else {
    Serial.println("WiFi connect failed, will retry in loop");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== E-Paper 4.2 fast web console ===");
  panel42::begin();
  panel42::Timings bt;
  panel42::bootClear(&bt);
  Serial.printf("boot clear: total=%lu init=%lu data=%lu refresh=%lu\n",
                (unsigned long)bt.total, (unsigned long)bt.init,
                (unsigned long)bt.data, (unsigned long)bt.refresh);
  connectWiFi();
  configTzTime("CST-8", "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
  server.begin();
  server.setNoDelay(true);
  lastActivity = millis();
  Serial.println("HTTP server on :80");
  ui::begin();
  Serial.println("local UI ready");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 10000) {
      WiFi.reconnect();
      lastTry = millis();
    }
  }
  ui::tick();
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
  if (strcmp(ui::appName(), "clock") != 0 && panel42::isPowered() &&
      millis() - lastActivity > 120000UL) {
    panel42::powerOff();
    Serial.println("panel idle power off");
  }
  delay(1);
}
