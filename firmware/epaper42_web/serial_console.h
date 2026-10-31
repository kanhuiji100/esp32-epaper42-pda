#pragma once
// Line-based serial console.  Included once from epaper42_web.ino (single TU).
// Credentials are persisted in NVS namespace "wifi"; secrets.h is the fallback.
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

namespace console {

enum Mode { CM_NORMAL, CM_SSID, CM_PASS, CM_QR_PASS };
static Mode mode = CM_NORMAL;
static String pendingSsid, pendingPass;
static String buf;
static bool lastCR = false;

static void prompt() { Serial.print("> "); }
static void newline() { Serial.print("\r\n"); }

static void printWifiStatus() {
  Serial.printf("wifi: status=%d netReady=%d ssid=%s ip=%s rssi=%d mac=%s\n",
                (int)WiFi.status(), netReady ? 1 : 0,
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                WiFi.RSSI(), WiFi.macAddress().c_str());
}

static void printStatus() {
  Serial.println(ui::stateJson());
  Serial.printf("ip=%s rssi=%d heap=%lu minheap=%lu uptime=%lus\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                (unsigned long)(millis() / 1000));
  Serial.printf("panel powered=%d mode=%s netReady=%d\n",
                panel42::isPowered() ? 1 : 0, panel42::modeName(), netReady ? 1 : 0);
}

// 50x30 ASCII preview; each character samples an 8x10 pixel block.
static void printScreen() {
  const uint8_t* fb = ui::screen();
  char line[64];
  line[0] = '+';
  for (int i = 0; i < 50; i++) line[1 + i] = '-';
  line[51] = '+'; line[52] = 0;
  Serial.printf("screen: app=%s\n", ui::appName());
  Serial.println(line);
  for (int by = 0; by < 30; by++) {
    Serial.print('|');
    for (int bx = 0; bx < 50; bx++) {
      int black = 0;
      for (int y = by * 10; y < by * 10 + 10; y += 2)
        for (int x = bx * 8; x < bx * 8 + 8; x += 2)
          if (!((fb[(size_t)y * 50 + (x >> 3)] >> (7 - (x & 7))) & 1)) black++;
      Serial.print(black >= 3 ? '#' : ' ');
    }
    Serial.println('|');
  }
  Serial.println(line);
}

static void printTime() {
  time_t now = time(nullptr);
  if (now < 1700000000) {
    Serial.println("time: NTP not synced");
    return;
  }
  struct tm t;
  localtime_r(&now, &t);
  char b[48];
  strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S %Z", &t);
  Serial.printf("time: %s\n", b);
}

static bool connectAndRestore(const String& ssid, const String& pass) {
  String oldS, oldP;
  bool hadOld = wifiLoadCredentials(oldS, oldP);
  Serial.printf("wifi: trying %s\n", ssid.c_str());
  if (wifiStart(ssid, pass, true)) return true;
  if (hadOld && (oldS != ssid || oldP != pass)) {
    Serial.println("wifi: restoring previous credentials");
    wifiStart(oldS, oldP, false);
  }
  return false;
}

static void wifiScan() {
  Serial.println("wifi: scanning...");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  bool reset = false, wasConnected = false;
  String oldS, oldP;

  // After a failed connection attempt the radio can be left in a state where
  // the first scan returns 0. Power-cycle the station and retry.
  if (n <= 0) {
    wasConnected = (WiFi.status() == WL_CONNECTED);
    if (wasConnected) wifiLoadCredentials(oldS, oldP);
    if (wasConnected) WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); delay(150);
    WiFi.mode(WIFI_STA); delay(250);
    n = WiFi.scanNetworks();
    reset = true;
  }

  if (n <= 0) {
    Serial.println("wifi: no networks found");
  } else {
    Serial.printf("wifi: %d networks\n", n);
    for (int i = 0; i < n; i++) {
      Serial.printf("%2d  %-32s %4d dBm  %s\n", i + 1, WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "encrypted");
    }
  }
  WiFi.scanDelete();
  if (reset && wasConnected && oldS.length() > 0) {
    Serial.println("wifi: restoring previous connection");
    wifiStart(oldS, oldP, false);
  }
}

static void printHelp() {
  Serial.println("commands:");
  Serial.println("  help | ?               this help");
  Serial.println("  status                 app/UI + network + panel state");
  Serial.println("  screen                 ASCII preview of the 400x300 framebuffer");
  Serial.println("  key up|down|left|right|ok|back|home");
  Serial.println("  app home|clock|notes|image|settings");
  Serial.println("  note                   show current note");
  Serial.println("  note <text>            set note (UTF-8)");
  Serial.println("  note clear             clear note");
  Serial.println("  qr <text>              generate a text QR (UTF-8) and show it");
  Serial.println("  qr wifi <ssid> [pass]  Wi-Fi QR; omit pass for hidden prompt");
  Serial.println("  qr clear               clear QR / show placeholder");
  Serial.println("  host                   show hostname / mDNS URL");
  Serial.println("  host <name>            set hostname (a-z 0-9 -, 1-32 chars)");
  Serial.println("  wifi                   current Wi-Fi state");
  Serial.println("  wifi scan              scan visible networks");
  Serial.println("  wifi set               interactive provisioning (password hidden)");
  Serial.println("  wifi set <ssid>        prompt for password");
  Serial.println("  wifi set <ssid> <pass> non-interactive (password visible)");
  Serial.println("  wifi forget            remove NVS credentials (reboot to fall back)");
  Serial.println("  wifi reconnect         reconnect with NVS/secrets credentials");
  Serial.println("  time                     current local time / NTP state");
  Serial.println("  ntp                      restart NTP sync");
  Serial.println("  refresh                  HQ 50Hz full refresh (4.1s, clean ghosting)");
  Serial.println("  power off|on             panel power");
  Serial.println("  clear                    clear white + home");
  Serial.println("  heap                     free heap");
  Serial.println("  reboot                   restart ESP32");
}

static int tokenize(const String& s, String out[], int max) {
  int n = 0, i = 0, len = s.length();
  while (i < len && n < max) {
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i >= len) break;
    String t;
    if (s[i] == '"' || s[i] == '\'') {
      char q = s[i++];
      while (i < len && s[i] != q) {
        if (s[i] == '\\' && i + 1 < len) i++;
        t += s[i++];
      }
      if (i < len) i++;
    } else {
      while (i < len && s[i] != ' ' && s[i] != '\t') t += s[i++];
    }
    out[n++] = t;
  }
  return n;
}

static void handleLine(const String& raw) {
  if (mode == CM_SSID) {
    String ssid = raw;
    ssid.trim();
    if (ssid.length() == 0) {
      mode = CM_NORMAL;
      Serial.println("cancelled");
      return;
    }
    pendingSsid = ssid;
    pendingPass = "";
    mode = CM_PASS;
    Serial.print("Password (hidden): ");
    return;
  }
  if (mode == CM_PASS) {
    pendingPass = raw;          // preserve spaces in the password
    mode = CM_NORMAL;
    connectAndRestore(pendingSsid, pendingPass);
    pendingPass = "";
    return;
  }
  if (mode == CM_QR_PASS) {
    pendingPass = raw;
    mode = CM_NORMAL;
    if (ui::setWifiQr(pendingSsid, pendingPass)) {
      lastActivity = millis();
      Serial.print("qr: "); Serial.println(ui::qrSummary());
    } else {
      Serial.println("qr: failed (SSID 1-32 bytes, password 8-63 or empty)");
    }
    pendingPass = "";
    return;
  }

  String line = raw;
  line.trim();
  if (line.length() == 0) return;

  String args[8];
  int argc = tokenize(line, args, 8);
  if (argc == 0) return;
  String cmd = args[0];
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") { printHelp(); return; }
  if (cmd == "status") { printStatus(); return; }
  if (cmd == "screen") { printScreen(); return; }
  if (cmd == "time") { printTime(); return; }
  if (cmd == "heap") {
    Serial.printf("heap: free=%lu min=%lu\n", (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap());
    return;
  }
  if (cmd == "reboot") {
    Serial.println("rebooting...");
    Serial.flush();
    delay(200);
    ESP.restart();
    return;
  }
  if (cmd == "ntp") {
    configTzTime("CST-8", "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
    Serial.println("ntp: sync restarted");
    return;
  }

  if (cmd == "key") {
    if (argc < 2) { Serial.println("usage: key up|down|left|right|ok|back|home"); return; }
    String k = args[1]; k.toLowerCase();
    if (k != "up" && k != "down" && k != "left" && k != "right" &&
        k != "ok" && k != "back" && k != "home") {
      Serial.println("bad key");
      return;
    }
    panel42::Timings t = ui::key(k);
    lastActivity = millis();
    Serial.printf("key=%s ms=%lu refresh=%lu\n", k.c_str(),
                  (unsigned long)t.total, (unsigned long)t.refresh);
    printStatus();
    return;
  }

  if (cmd == "app") {
    if (argc < 2) { Serial.println("usage: app home|clock|notes|image|settings"); return; }
    String name = args[1]; name.toLowerCase();
    ui::setApp(name);
    lastActivity = millis();
    Serial.printf("app=%s\n", ui::appName());
    printStatus();
    return;
  }

  if (cmd == "note") {
    String rest;
    int sp = line.indexOf(' ');
    if (sp >= 0) { rest = line.substring(sp + 1); rest.trim(); }
    if (rest.length() == 0) {
      Serial.println("note:");
      Serial.println(ui::noteText());
      Serial.printf("bytes=%d\n", ui::noteText().length());
      return;
    }
    if (rest == "clear") {
      ui::setNote("");
      lastActivity = millis();
      Serial.println("note cleared");
      return;
    }
    ui::setNote(rest);
    lastActivity = millis();
    Serial.println("note saved");
    printStatus();
    return;
  }

  if (cmd == "host" || cmd == "hostname") {
    if (argc == 1) {
      Serial.printf("host: %s  http://%s.local\n", gHostname.c_str(), gHostname.c_str());
      return;
    }
    String name = args[1];
    if (name == "set" && argc >= 3) name = args[2];
    if (hostnameSet(name)) {
      lastActivity = millis();
      Serial.printf("host: %s  http://%s.local\n", gHostname.c_str(), gHostname.c_str());
      Serial.println("DHCP hostname fully applies after reboot; mDNS is immediate.");
    } else {
      Serial.println("host: invalid; use 1-32 chars a-z 0-9 -, no leading/trailing -");
    }
    return;
  }

  if (cmd == "qr") {
    if (argc == 1) {
      Serial.print("qr: "); Serial.println(ui::qrSummary());
      return;
    }
    String sub = args[1];
    sub.toLowerCase();
    if (sub == "clear") {
      ui::clearQr();
      lastActivity = millis();
      Serial.println("qr: cleared");
      return;
    }
    if (sub == "wifi") {
      if (argc >= 4) {
        if (ui::setWifiQr(args[2], args[3])) {
          lastActivity = millis();
          Serial.print("qr: "); Serial.println(ui::qrSummary());
        } else {
          Serial.println("qr: failed (SSID 1-32 bytes, password 8-63 or empty)");
        }
        return;
      }
      if (argc == 3) {
        pendingSsid = args[2];
        pendingPass = "";
        mode = CM_QR_PASS;
        Serial.print("WiFi password (hidden): ");
        return;
      }
      Serial.println("usage: qr wifi <ssid> [pass]");
      return;
    }
    String rest;
    if (sub == "text") {
      int p = line.indexOf(' ', 3);
      rest = p < 0 ? String("") : line.substring(p + 1);
    } else {
      int p = line.indexOf(' ');
      rest = p < 0 ? String("") : line.substring(p + 1);
    }
    rest.trim();
    if (rest.length() == 0) {
      Serial.println("usage: qr <text>");
      return;
    }
    if (ui::setQr(rest, "text")) {
      lastActivity = millis();
      Serial.print("qr: "); Serial.println(ui::qrSummary());
    } else {
      Serial.println("qr: text too long for QR version 13");
    }
    return;
  }

  if (cmd == "wifi") {
    String sub = argc > 1 ? args[1] : "";
    sub.toLowerCase();
    if (sub == "status" || sub.length() == 0) { printWifiStatus(); return; }
    if (sub == "scan") { wifiScan(); return; }
    if (sub == "forget") {
      wifiPrefs.begin("wifi", false);
      wifiPrefs.remove("ssid");
      wifiPrefs.remove("pass");
      Serial.println("wifi: NVS credentials removed; reboot or wifi reconnect to use secrets.h");
      return;
    }
    if (sub == "reconnect") {
      String s, p;
      wifiLoadCredentials(s, p);
      connectAndRestore(s, p);
      printWifiStatus();
      return;
    }
    if (sub == "set") {
      if (argc >= 4) {
        connectAndRestore(args[2], args[3]);
        printWifiStatus();
        return;
      }
      if (argc == 3) {
        pendingSsid = args[2];
        pendingPass = "";
        mode = CM_PASS;
        Serial.print("Password (hidden): ");
        return;
      }
      pendingSsid = "";
      pendingPass = "";
      mode = CM_SSID;
      Serial.print("SSID: ");
      return;
    }
    Serial.println("usage: wifi [status|scan|set|forget|reconnect]");
    return;
  }

  if (cmd == "refresh") {
    panel42::Timings t;
    panel42::refreshHq(t);
    lastActivity = millis();
    Serial.printf("refresh: total=%lu refresh=%lu\n",
                  (unsigned long)t.total, (unsigned long)t.refresh);
    return;
  }

  if (cmd == "power") {
    String sub = argc > 1 ? args[1] : "";
    sub.toLowerCase();
    if (sub == "off") {
      panel42::powerOff();
      Serial.println("panel: off");
      return;
    }
    if (sub == "on") {
      ui::setApp(String(ui::appName()));
      lastActivity = millis();
      Serial.println("panel: on");
      return;
    }
    Serial.println("usage: power off|on");
    return;
  }

  if (cmd == "clear") {
    panel42::Timings t;
    panel42::clear(t);
    ui::onClear();
    lastActivity = millis();
    Serial.println("clear: done");
    return;
  }

  Serial.printf("unknown command: %s (type help)\n", cmd.c_str());
}

static void handleEnter() {
  String line = buf;
  buf = "";
  handleLine(line);
}

static void begin() {
  Serial.println();
  Serial.println("Serial console ready. Type 'help'.");
  prompt();
}

static void loop() {
  while (Serial.available()) {
    int ci = Serial.read();
    if (ci < 0) break;
    char c = (char)ci;

    if (c == 0x03) { // Ctrl-C
      mode = CM_NORMAL;
      buf = "";
      Serial.println("^C");
      prompt();
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (c == '\n' && lastCR) { lastCR = false; continue; }
      lastCR = (c == '\r');
      Serial.println();
      handleEnter();
      if (mode == CM_NORMAL) prompt();
      continue;
    }
    lastCR = false;

    if (c == 0x08 || c == 0x7F) {
      if (buf.length() > 0) {
        buf.remove(buf.length() - 1);
        Serial.print("\b \b");
      }
      continue;
    }
    if (c < 0x20) continue;
    if (buf.length() >= 200) continue;
    buf += c;
    if (mode == CM_PASS || mode == CM_QR_PASS) Serial.print('*');
    else Serial.print(c);
  }
}

} // namespace console
