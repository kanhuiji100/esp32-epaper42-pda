#include "ui.h"
#include "ui_font.h"
#include "qrcodegen.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

namespace ui {

uint8_t fb[panel42::IMGSZ];

namespace {

constexpr int W = panel42::W;
constexpr int H = panel42::H;
constexpr int STRIDE = panel42::ROWB;
constexpr int IMGSZ = panel42::IMGSZ;
constexpr uint8_t INK = 0;    // black pixel
constexpr uint8_t PAPER = 1;  // white pixel

enum class App { HOME, CLOCK, NOTES, IMAGE, SETTINGS, QR };
App app = App::HOME;
int menuSel = 0;
int notePage = 0;
String note;
bool hasImage = false;
uint8_t imageFb[IMGSZ];
String qrPayload, qrType;
bool qrValid = false;
int qrSize = 0;
uint8_t qrBuf[qrcodegen_BUFFER_LEN_MAX];
uint8_t qrTemp[qrcodegen_BUFFER_LEN_MAX];
uint8_t qrCand[qrcodegen_BUFFER_LEN_MAX];
panel42::Timings lastT;
int lastMin = -1;
Preferences prefs;

// ---------------- framebuffer ----------------
void putPx(int x, int y, uint8_t c) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  uint8_t m = 0x80 >> (x & 7);
  if (c == INK) fb[(size_t)y * STRIDE + (x >> 3)] &= (uint8_t)~m;
  else          fb[(size_t)y * STRIDE + (x >> 3)] |= m;
}
void fillRect(int x, int y, int w, int h, uint8_t c) {
  if (w <= 0 || h <= 0) return;
  if (x == 0 && w == W) {
    memset(fb + (size_t)y * STRIDE, c == PAPER ? 0xFF : 0x00, (size_t)h * STRIDE);
    return;
  }
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++) putPx(x + xx, y + yy, c);
}
void hline(int x, int y, int w, uint8_t c) { fillRect(x, y, w, 1, c); }
void vline(int x, int y, int h, uint8_t c) { fillRect(x, y, 1, h, c); }
void drawRect(int x, int y, int w, int h, uint8_t c) {
  hline(x, y, w, c); hline(x, y + h - 1, w, c);
  vline(x, y, h, c); vline(x + w - 1, y, h, c);
}
void drawLine(int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    putPx(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
void drawCircle(int x0, int y0, int r, uint8_t c) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    putPx(x0 + x, y0 + y, c); putPx(x0 + y, y0 + x, c);
    putPx(x0 - y, y0 + x, c); putPx(x0 - x, y0 + y, c);
    putPx(x0 - x, y0 - y, c); putPx(x0 - y, y0 - x, c);
    putPx(x0 + y, y0 - x, c); putPx(x0 + x, y0 - y, c);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}
void fillCircle(int x0, int y0, int r, uint8_t c) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) putPx(x0 + x, y0 + y, c);
}

// ---------------- 1bpp Unifont text ----------------
const UiGlyph* findGlyph(uint32_t cp) {
  int lo = 0, hi = UI_CJK_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint32_t v = UI_CJK[mid].cp;
    if (v == cp) return &UI_CJK[mid];
    if (v < cp) lo = mid + 1; else hi = mid - 1;
  }
  return nullptr;
}
int glyphWidth(uint32_t cp) { return cp < 0x80 ? 8 : 16; }

void drawGlyph(int x, int y, uint32_t cp, int scale, uint8_t c) {
  if (cp < 0x80) {
    if (cp < 0x20 || cp > 0x7E) { drawRect(x, y, 7 * scale, 14 * scale, c); return; }
    const uint8_t* rows = UI_ASCII[cp - 0x20];
    for (int r = 0; r < 16; r++) {
      uint8_t b = rows[r];
      if (!b) continue;
      for (int col = 0; col < 8; col++)
        if (b & (0x80 >> col))
          fillRect(x + col * scale, y + r * scale, scale, scale, c);
    }
  } else {
    const UiGlyph* g = findGlyph(cp);
    if (!g) { drawRect(x, y, 14 * scale, 14 * scale, c); return; }
    for (int r = 0; r < 16; r++) {
      uint8_t hi = g->bmp[r * 2], lo = g->bmp[r * 2 + 1];
      if (!hi && !lo) continue;
      for (int col = 0; col < 16; col++) {
        uint8_t b = (col < 8) ? (hi & (0x80 >> col)) : (lo & (0x80 >> (col - 8)));
        if (b) fillRect(x + col * scale, y + r * scale, scale, scale, c);
      }
    }
  }
}

// decode one UTF-8 character; returns next index
int utf8Next(const String& s, int i, uint32_t& cp) {
  int n = s.length();
  if (i >= n) { cp = 0; return i; }
  uint8_t c = (uint8_t)s[i];
  if (c < 0x80) { cp = c; return i + 1; }
  int len = 0;
  if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
  else { cp = '?'; return i + 1; }
  if (i + len > n) { cp = '?'; return i + 1; }
  for (int k = 1; k < len; k++) {
    uint8_t cc = (uint8_t)s[i + k];
    if ((cc & 0xC0) != 0x80) { cp = '?'; return i + 1; }
    cp = (cp << 6) | (cc & 0x3F);
  }
  return i + len;
}

int textWidth(const String& s, int scale) {
  int maxW = 0, w = 0, i = 0;
  while (i < (int)s.length()) {
    uint32_t cp;
    i = utf8Next(s, i, cp);
    if (cp == '\n') { if (w > maxW) maxW = w; w = 0; }
    else if (cp == '\t') w += 4 * 8 * scale;
    else w += glyphWidth(cp) * scale;
  }
  return w > maxW ? w : maxW;
}

int drawText(int x, int y, const String& s, int scale, uint8_t c) {
  int cx = x, cy = y, maxW = 0, i = 0;
  while (i < (int)s.length()) {
    uint32_t cp;
    i = utf8Next(s, i, cp);
    if (cp == '\n') { if (cx - x > maxW) maxW = cx - x; cx = x; cy += 18 * scale; continue; }
    if (cp == '\t') { cx += 4 * 8 * scale; continue; }
    drawGlyph(cx, cy, cp, scale, c);
    cx += glyphWidth(cp) * scale;
  }
  if (cx - x > maxW) maxW = cx - x;
  return maxW;
}

// draw wrapped text; returns lines actually used, sets nextOffset
int layoutText(int x, int y, int maxW, int lineH, int maxLines,
               const String& s, int scale, uint8_t c, bool draw,
               int start, int* nextOffset) {
  int i = start, cx = x, cy = y, lines = 1;
  while (i < (int)s.length()) {
    int save = i;
    uint32_t cp;
    i = utf8Next(s, i, cp);
    if (cp == '\n') {
      cx = x; cy += lineH; lines++;
      if (lines > maxLines) { if (nextOffset) *nextOffset = save; return lines - 1; }
      continue;
    }
    if (cp == '\t') cp = ' ';
    int adv = glyphWidth(cp) * scale;
    if (cx + adv > x + maxW && cx > x) {
      cx = x; cy += lineH; lines++;
      if (lines > maxLines) { if (nextOffset) *nextOffset = save; return lines - 1; }
    }
    if (draw) drawGlyph(cx, cy, cp, scale, c);
    cx += adv;
  }
  if (nextOffset) *nextOffset = i;
  return lines;
}

// ---------------- time ----------------
bool nowTm(struct tm& t) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  localtime_r(&now, &t);
  return true;
}
int currentMinute() {
  struct tm t;
  return nowTm(t) ? (t.tm_hour * 60 + t.tm_min) : -1;
}

String timeHM() {
  struct tm t;
  if (!nowTm(t)) return "--:--";
  char b[8]; snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min); return String(b);
}
String timeHMS() {
  struct tm t;
  if (!nowTm(t)) return "--:--:--";
  char b[12]; snprintf(b, sizeof(b), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec); return String(b);
}
String dateCN() {
  struct tm t;
  if (!nowTm(t)) return "等待网络时间";
  return String(t.tm_year + 1900) + "年" + String(t.tm_mon + 1) + "月" + String(t.tm_mday) + "日";
}
String weekCN() {
  struct tm t;
  if (!nowTm(t)) return "";
  const char* wk[] = {"日", "一", "二", "三", "四", "五", "六"};
  return String("星期") + wk[t.tm_wday];
}

// ---------------- icons ----------------
void iconClock(int x, int y, uint8_t c) {
  drawCircle(x + 12, y + 12, 9, c);
  drawLine(x + 12, y + 12, x + 12, y + 6, c);
  drawLine(x + 12, y + 12, x + 17, y + 14, c);
}
void iconNote(int x, int y, uint8_t c) {
  drawRect(x + 3, y + 2, 18, 20, c);
  hline(x + 6, y + 7, 12, c); hline(x + 6, y + 11, 12, c); hline(x + 6, y + 15, 9, c);
}
void iconImage(int x, int y, uint8_t c) {
  drawRect(x + 2, y + 4, 20, 16, c);
  drawLine(x + 4, y + 18, x + 10, y + 11, c);
  drawLine(x + 10, y + 11, x + 14, y + 15, c);
  drawLine(x + 14, y + 15, x + 17, y + 11, c);
  drawLine(x + 17, y + 11, x + 20, y + 15, c);
  fillRect(x + 15, y + 7, 2, 2, c);
}
void iconGear(int x, int y, uint8_t c) {
  drawCircle(x + 12, y + 12, 7, c);
  fillCircle(x + 12, y + 12, 2, c == PAPER ? INK : PAPER);
  for (int k = 0; k < 8; k++) {
    int dx = (k == 0 || k == 4) ? 1 : (k == 2 || k == 6 ? 0 : 0);
    (void)dx;
  }
  drawLine(x + 12, y + 2, x + 12, y + 5, c);
  drawLine(x + 12, y + 19, x + 12, y + 22, c);
  drawLine(x + 2, y + 12, x + 5, y + 12, c);
  drawLine(x + 19, y + 12, x + 22, y + 12, c);
  drawLine(x + 5, y + 5, x + 7, y + 7, c);
  drawLine(x + 17, y + 17, x + 19, y + 19, c);
  drawLine(x + 19, y + 5, x + 17, y + 7, c);
  drawLine(x + 7, y + 17, x + 5, y + 19, c);
}
void iconQr(int x, int y, uint8_t c) {
  drawRect(x + 2, y + 2, 7, 7, c);   drawRect(x + 4, y + 4, 3, 3, c);
  drawRect(x + 15, y + 2, 7, 7, c);  drawRect(x + 17, y + 4, 3, 3, c);
  drawRect(x + 2, y + 15, 7, 7, c);  drawRect(x + 4, y + 17, 3, 3, c);
  fillRect(x + 12, y + 12, 2, 2, c);
  fillRect(x + 15, y + 15, 2, 2, c);
  fillRect(x + 18, y + 12, 2, 2, c);
}
void iconFor(int i, int x, int y, uint8_t c) {
  switch (i) {
    case 0: iconClock(x, y, c); break;
    case 1: iconNote(x, y, c); break;
    case 2: iconImage(x, y, c); break;
    case 3: iconQr(x, y, c); break;
    default: iconGear(x, y, c); break;
  }
}

// ---------------- chrome ----------------
void drawWifi(int x, int y) {
  bool conn = WiFi.status() == WL_CONNECTED;
  int rssi = WiFi.RSSI();
  int bars = conn ? (rssi > -60 ? 3 : (rssi > -75 ? 2 : 1)) : 0;
  for (int i = 0; i < 3; i++) {
    int bh = 4 + i * 3;
    int bx = x + i * 5, by = y + 12 - bh;
    drawRect(bx, by, 3, bh, INK);
    if (i < bars) fillRect(bx, by, 3, bh, INK);
  }
  if (!conn) {
    drawLine(x, y, x + 14, y + 12, INK);
    drawLine(x + 14, y, x, y + 12, INK);
  }
}
void drawStatus(const char* title) {
  fillRect(0, 0, W, 26, PAPER);
  drawText(8, 5, String(title), 1, INK);
  String hm = timeHM();
  int tw = textWidth(hm, 1);
  drawText(W - 8 - tw, 5, hm, 1, INK);
  drawWifi(W - 8 - tw - 24, 7);
  hline(0, 25, W, INK);
}

// ---------------- apps ----------------
const char* MENU[] = {"时钟", "记事", "图片", "二维码", "设置"};

void drawHome() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("PDA");
  drawText(20, 34, "主菜单", 2, INK);
  drawText(300, 42, "PDA", 1, INK);
  const int top = 78, ih = 36;
  for (int i = 0; i < 5; i++) {
    int y = top + i * ih;
    bool sel = (i == menuSel);
    if (sel) fillRect(12, y, 376, 34, INK);
    uint8_t ink = sel ? PAPER : INK;
    iconFor(i, 27, y + 5, ink);
    drawText(66, y + 8, MENU[i], 1, ink);
    if (sel) drawText(368, y + 8, ">", 1, ink);
  }
  hline(12, 270, 376, INK);
  drawText(14, 276, "↑↓ 选择   OK 进入", 1, INK);
}

void drawClockApp() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("时钟");
  struct tm t;
  bool ok = nowTm(t);
  String hm = ok ? timeHM() : "--:--";
  int tw = textWidth(hm, 7);
  drawText((W - tw) / 2, 72, hm, 7, INK);
  hline(20, 198, 360, INK);
  drawText(20, 212, dateCN(), 1, INK);
  drawText(20, 236, weekCN(), 1, INK);
  drawText(20, 260, ok ? "NTP 已同步 · 每分钟全刷" : "正在同步 NTP", 1, INK);
  drawText(20, 282, "Back 返回主页", 1, INK);
}

constexpr int NOTE_MAXW = 360;
constexpr int NOTE_LINEH = 20;
constexpr int NOTE_LINES = 9;

int notePages() {
  if (note.length() == 0) return 1;
  int start = 0, pages = 1;
  while (start < (int)note.length() && pages < 200) {
    int next = start;
    layoutText(20, 0, NOTE_MAXW, NOTE_LINEH, NOTE_LINES, note, 1, INK, false, start, &next);
    if (next <= start) break;
    start = next;
    if (start < (int)note.length()) pages++;
  }
  return pages;
}
int noteStart(int page) {
  int start = 0;
  for (int p = 0; p < page; p++) {
    int next = start;
    layoutText(20, 0, NOTE_MAXW, NOTE_LINEH, NOTE_LINES, note, 1, INK, false, start, &next);
    if (next <= start) break;
    start = next;
  }
  return start;
}

void drawNotesApp() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("记事");
  int pages = notePages();
  if (notePage < 0) notePage = 0;
  if (notePage >= pages) notePage = pages - 1;
  drawText(20, 34, "记事本", 1, INK);
  String pg = String(notePage + 1) + "/" + String(pages);
  int pw = textWidth(pg, 1);
  drawText(W - 20 - pw, 34, pg, 1, INK);
  hline(20, 56, 360, INK);
  if (note.length() == 0) {
    drawText(20, 72, "在网页输入文字，点“发送到记事”。", 1, INK);
  } else {
    int start = noteStart(notePage);
    layoutText(20, 64, NOTE_MAXW, NOTE_LINEH, NOTE_LINES, note, 1, INK, true, start, nullptr);
  }
  hline(20, 258, 360, INK);
  drawText(20, 266, "↑↓ 翻页   Back 返回", 1, INK);
}

void drawSettingsApp() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("设置");
  int y = 40, step = 23;
  auto line = [&](const String& s) { drawText(20, y, s, 1, INK); y += step; };
  line(String("WiFi  ") + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("未连接")));
  line(String("IP    ") + WiFi.localIP().toString());
  line(String("RSSI  ") + String(WiFi.RSSI()) + " dBm");
  line(String("HEAP  ") + String(ESP.getFreeHeap() / 1024) + " KB");
  line(String("MODE  ") + panel42::modeName());
  line(String("FLASH ") + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  line(String("UP    ") + String(millis() / 1000) + " s");
  line(String("WEB   http://") + WiFi.localIP().toString());
  hline(20, 258, 360, INK);
  drawText(20, 266, "Back 返回", 1, INK);
}

void drawQrApp() {
  if (qrValid) {
    fillRect(0, 0, W, H, PAPER);
    int n = qrSize;
    int scale = (W / (n + 8) < H / (n + 8)) ? W / (n + 8) : H / (n + 8);
    if (scale < 1) scale = 1;
    int x0 = (W - n * scale) / 2;
    int y0 = (H - n * scale) / 2;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++)
        if (qrcodegen_getModule(qrBuf, x, y))
          fillRect(x0 + x * scale, y0 + y * scale, scale, scale, INK);
    return;
  }
  fillRect(0, 0, W, H, PAPER);
  drawStatus("二维码");
  drawText(20, 70, "还没有二维码。", 1, INK);
  drawText(20, 100, "网页“二维码”或串口：", 1, INK);
  drawText(20, 124, "qr 你好", 1, INK);
  drawText(20, 148, "qr wifi \"SSID\" \"密码\"", 1, INK);
  hline(20, 258, 360, INK);
  drawText(20, 266, "Back 返回", 1, INK);
}

void drawImageApp() {
  if (hasImage) {
    memcpy(fb, imageFb, IMGSZ);
    return;
  }
  fillRect(0, 0, W, H, PAPER);
  drawStatus("图片");
  drawText(20, 70, "还没有图片。", 1, INK);
  drawText(20, 100, "在网页“图片上传”里选择图片，", 1, INK);
  drawText(20, 124, "点“全屏刷新”后会自动进入图片页。", 1, INK);
  hline(20, 258, 360, INK);
  drawText(20, 266, "Back 返回", 1, INK);
}

String qrEscape(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') o += '\\';
    o += c;
  }
  return o;
}

int encodeQr(const String& payload) {
  if (payload.length() == 0) return 0;
  bool ok = qrcodegen_encodeText(payload.c_str(), qrTemp, qrCand,
                                 qrcodegen_Ecc_MEDIUM, 1, 13,
                                 qrcodegen_Mask_AUTO, true);
  if (!ok) return 0;
  return qrcodegen_getSize(qrCand);
}

void render() {
  switch (app) {
    case App::HOME: drawHome(); break;
    case App::CLOCK: drawClockApp(); break;
    case App::NOTES: drawNotesApp(); break;
    case App::IMAGE: drawImageApp(); break;
    case App::SETTINGS: drawSettingsApp(); break;
    case App::QR: drawQrApp(); break;
  }
}

void commit(bool forceFull) {
  int minB = STRIDE, maxB = -1, minY = H, maxY = -1;
  for (int y = 0; y < H; y++) {
    const uint8_t* a = fb + (size_t)y * STRIDE;
    const uint8_t* b = panel42::shadow + (size_t)y * STRIDE;
    for (int x = 0; x < STRIDE; x++) {
      if (a[x] != b[x]) {
        if (x < minB) minB = x;
        if (x > maxB) maxB = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
  }
  if (maxB < 0) {
    if (forceFull) panel42::fullStd(fb, lastT);   // e.g. power on with identical frame
    else lastT = panel42::Timings{};
    return;
  }
  int x0 = minB * 8;
  int w = (maxB - minB + 1) * 8;
  int y0 = minY;
  int h = maxY - minY + 1;
  if (forceFull || (long)w * h > 90000L) {
    panel42::fullStd(fb, lastT);     // full waveform: darker, clears ghosting
  } else {
    panel42::partialFb(x0, y0, w, h, fb, lastT);
  }
}

void renderAndCommit(bool forceFull) {
  int m = currentMinute();   // minute used by this render
  if (app == App::QR && qrValid && forceFull) {
    render();
    panel42::fullHq(fb, lastT);   // QR: slowest 50 Hz waveform for maximum contrast
    lastMin = m;
    return;
  }
  render();
  commit(forceFull);
  if (forceFull) lastMin = m;
}

bool setAppByName(const String& n) {
  String s = n; s.toLowerCase();
  if (s == "home" || s == "menu") app = App::HOME;
  else if (s == "clock") app = App::CLOCK;
  else if (s == "notes" || s == "note") app = App::NOTES;
  else if (s == "image" || s == "picture") app = App::IMAGE;
  else if (s == "settings" || s == "setting") app = App::SETTINGS;
  else if (s == "qr" || s == "qrcode" || s == "code") app = App::QR;
  else return false;
  return true;
}

} // namespace

// ---------------- public ----------------
const uint8_t* screen() { return fb; }

const char* appName() {
  switch (app) {
    case App::HOME: return "home";
    case App::CLOCK: return "clock";
    case App::NOTES: return "notes";
    case App::IMAGE: return "image";
    case App::SETTINGS: return "settings";
    case App::QR: return "qr";
  }
  return "home";
}

String noteText() { return note; }

String stateJson() {
  String s;
  s.reserve(240);
  s += "{\"ok\":1,\"app\":\""; s += appName();
  s += "\",\"sel\":"; s += menuSel;
  s += ",\"page\":"; s += notePage;
  s += ",\"pages\":"; s += notePages();
  s += ",\"hasImage\":"; s += (hasImage ? 1 : 0);
  s += ",\"qr\":"; s += (qrValid ? 1 : 0);
  s += ",\"qrType\":\""; s += qrType; s += "\"";
  s += ",\"qrModules\":"; s += qrSize;
  s += ",\"ms\":"; s += (unsigned long)lastT.total;
  s += ",\"refresh\":"; s += (unsigned long)lastT.refresh;
  s += ",\"ip\":\""; s += WiFi.localIP().toString(); s += "\"";
  s += ",\"heap\":"; s += (unsigned long)ESP.getFreeHeap();
  s += ",\"time\":\""; s += timeHM(); s += "\"}";
  return s;
}

void begin() {
  prefs.begin("pda", false);
  note = prefs.getString("note", "");
  note.replace("\r\n", "\n");
  note.replace("\r", "\n");
  app = App::HOME;
  menuSel = 0;
  notePage = 0;
  hasImage = false;
  memset(fb, 0xFF, IMGSZ);
  renderAndCommit(true);
}

panel42::Timings key(const String& k) {
  String s = k; s.toLowerCase();
  bool wasOff = !panel42::isPowered();
  App before = app;
  bool pageChanged = false;
  bool homeMoved = false;
  if (s == "home") app = App::HOME;
  else if (s == "back") app = App::HOME;
  else if (s == "up") {
    if (app == App::HOME) { menuSel = (menuSel + 4) % 5; homeMoved = true; }
    else if (app == App::NOTES && notePage > 0) { notePage--; pageChanged = true; }
  } else if (s == "down") {
    if (app == App::HOME) { menuSel = (menuSel + 1) % 5; homeMoved = true; }
    else if (app == App::NOTES && notePage + 1 < notePages()) { notePage++; pageChanged = true; }
  } else if (s == "left") {
    if (app == App::NOTES && notePage > 0) { notePage--; pageChanged = true; }
  } else if (s == "right") {
    if (app == App::NOTES && notePage + 1 < notePages()) { notePage++; pageChanged = true; }
  } else if (s == "ok") {
    if (app == App::HOME) {
      switch (menuSel) {
        case 0: app = App::CLOCK; break;
        case 1: app = App::NOTES; notePage = 0; break;
        case 2: app = App::IMAGE; break;
        case 3: app = App::QR; break;
        case 4: app = App::SETTINGS; break;
      }
    } else if (app == App::IMAGE || app == App::QR) {
      app = App::HOME;
    }
  }
  renderAndCommit(app != before || pageChanged || homeMoved || wasOff);
  return lastT;
}

panel42::Timings setApp(const String& name) {
  bool wasOff = !panel42::isPowered();
  App before = app;
  if (setAppByName(name)) {
    if (app == App::NOTES) notePage = 0;
  }
  renderAndCommit(app != before || wasOff);
  return lastT;
}

panel42::Timings setNote(const String& text) {
  String t = text;
  t.replace("\r\n", "\n");
  t.replace("\r", "\n");
  if (t.length() > 4000) t = t.substring(0, 4000);
  note = t;
  prefs.putString("note", note);
  app = App::NOTES;
  notePage = 0;
  renderAndCommit(true);
  return lastT;
}

void setImage(const uint8_t* img, bool alreadyOnScreen) {
  memcpy(imageFb, img, IMGSZ);
  memcpy(fb, img, IMGSZ);
  hasImage = true;
  app = App::IMAGE;
  if (alreadyOnScreen) lastT = panel42::Timings{};
  else renderAndCommit(true);
}

bool setQr(const String& payload, const String& type) {
  int n = encodeQr(payload);
  if (n == 0) return false;
  memcpy(qrBuf, qrCand, sizeof(qrBuf));
  qrPayload = payload;
  qrType = type;
  qrSize = n;
  qrValid = true;
  app = App::QR;
  renderAndCommit(true);
  return true;
}

bool setWifiQr(const String& ssid, const String& pass) {
  if (ssid.length() == 0 || ssid.length() > 32) return false;
  if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) return false;
  String p = "WIFI:T:";
  p += pass.length() ? "WPA" : "nopass";
  p += ";S:"; p += qrEscape(ssid); p += ";";
  if (pass.length() > 0) {
    p += "P:"; p += qrEscape(pass); p += ";";
  }
  p += ";";
  return setQr(p, "wifi");
}

void clearQr() {
  qrValid = false;
  qrPayload = "";
  qrType = "";
  qrSize = 0;
  app = App::QR;
  renderAndCommit(true);
}

String qrSummary() {
  String s = "valid=";
  s += (qrValid ? 1 : 0);
  s += " type="; s += qrType;
  s += " modules="; s += qrSize;
  s += " bytes="; s += qrPayload.length();
  return s;
}

void onClear() {
  memset(fb, 0xFF, IMGSZ);
  app = App::HOME;
  renderAndCommit(true);
}

void tick() {
  if (!panel42::isPowered()) return;
  struct tm t;
  bool ok = nowTm(t);
  int minute = ok ? (t.tm_hour * 60 + t.tm_min) : -1;
  if (app == App::CLOCK) {
    if (minute != lastMin) {
      lastMin = minute;
      render();
      panel42::fullStd(fb, lastT);   // one clean full refresh per minute
    }
  } else if (app != App::IMAGE && app != App::QR) {
    if (minute != lastMin) {
      lastMin = minute;
      render();
      if (ok && (t.tm_min % 15) == 0) panel42::fullStd(fb, lastT);
      else commit(false);            // status bar only, low stress
    }
  }
}

} // namespace ui
