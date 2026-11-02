#include "ui.h"
#include "ui_font.h"
#include "qrcodegen.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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

enum class App { HOME, CLOCK, CALENDAR, NOTES, IMAGE, SETTINGS, QR, NEWS, PET, GAMES, GOMOKU, DDZ };
App app = App::HOME;
int menuSel = 0;
int notePage = 0;
String note;
String hostLabel = "epaper42";
Preferences prefs;
bool hasImage = false;
uint8_t imageFb[IMGSZ];

// ---- image gallery on /ext/images -------------------------------------------
// Files are immutable once written (name i0000.bin ...), so a photo is written
// exactly once and every later view is a pure read. This keeps flash wear down:
// LittleFS already spreads writes (log-structured, dynamic wear levelling) and
// the only recurring write is the 4-byte "current index" in NVS, and only when
// the user actually switches picture.
constexpr int IMG_MAX = 200;
constexpr const char* IMG_DIR = "/ext/images";
// Name table lives on the heap: it is only needed while browsing the gallery.
char (*imgNames)[13] = nullptr;
int imgCount = 0;
int imgCur = 0;

bool imgAlloc() {
  if (!imgNames) imgNames = (char(*)[13])malloc(IMG_MAX * 13);
  return imgNames != nullptr;
}

int imgCompareNames(const void* a, const void* b) {
  return strcmp((const char*)a, (const char*)b);
}

void imgScan() {
  imgCount = 0;
  if (!storage::extReady() || !imgAlloc()) return;
  DIR* d = opendir(IMG_DIR);
  if (!d) return;
  struct dirent* e;
  while ((e = readdir(d)) != nullptr && imgCount < IMG_MAX) {
    const char* n = e->d_name;
    size_t L = strlen(n);
    if (L != 9 || n[0] != 'i' || strcmp(n + 5, ".bin") != 0) continue;
    bool digits = true;
    for (int k = 1; k <= 4; k++) if (n[k] < '0' || n[k] > '9') digits = false;
    if (!digits) continue;
    strncpy(imgNames[imgCount], n, 12);
    imgNames[imgCount][12] = 0;
    imgCount++;
  }
  closedir(d);
  if (imgCount > 1) qsort(imgNames, imgCount, sizeof(imgNames[0]), imgCompareNames);
}

String imgPath(int idx) {
  if (idx < 0 || idx >= imgCount) return "";
  return String(IMG_DIR) + "/" + imgNames[idx];
}

void imgLoadPrefs() {
  prefs.begin("pda", false);
  imgCur = prefs.getInt("img_cur", 0);
  prefs.end();
}

bool imageLoadIndex(int idx) {
  if (idx < 0 || idx >= imgCount) return false;
  FILE* f = fopen(imgPath(idx).c_str(), "rb");
  if (!f) return false;
  size_t n = fread(imageFb, 1, IMGSZ, f);
  fclose(f);
  if (n != IMGSZ) return false;
  hasImage = true;
  return true;
}

// Persist a new picture as the next slot and make it current.
bool imageSaveNew(const uint8_t* data, int& idxOut) {
  if (!storage::extReady()) return false;
  if (imgCount >= IMG_MAX) return false;
  // Nothing to do if it is byte-identical to what is already shown.
  if (hasImage && memcmp(data, imageFb, IMGSZ) == 0) { idxOut = imgCur; return true; }
  char dir[32]; strcpy(dir, IMG_DIR);
  mkdir(dir, 0777);
  int next = 0;
  if (imgCount > 0) next = atoi(imgNames[imgCount - 1] + 1) + 1;
  char name[16];
  snprintf(name, sizeof(name), "i%04d.bin", next % 10000);
  String path = String(IMG_DIR) + "/" + name;
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  size_t w = fwrite(data, 1, IMGSZ, f);
  fclose(f);
  if (w != IMGSZ) { remove(path.c_str()); return false; }
  imgScan();
  idxOut = imgCount - 1;
  imgCur = idxOut;
  prefs.begin("pda", false); prefs.putInt("img_cur", imgCur); prefs.end();
  return true;
}

bool imageDeleteIndex(int idx) {
  String p = imgPath(idx);
  if (p.length() == 0) return false;
  if (remove(p.c_str()) != 0) return false;
  imgScan();
  if (imgCur >= imgCount) imgCur = imgCount > 0 ? imgCount - 1 : 0;
  prefs.begin("pda", false); prefs.putInt("img_cur", imgCur); prefs.end();
  if (imgCount > 0) imageLoadIndex(imgCur); else hasImage = false;
  return true;
}
String qrPayload, qrType;
bool qrValid = false;
int qrSize = 0;
uint8_t qrBuf[qrcodegen_BUFFER_LEN_MAX];
uint8_t qrTemp[qrcodegen_BUFFER_LEN_MAX];
uint8_t qrCand[qrcodegen_BUFFER_LEN_MAX];
panel42::Timings lastT;
int lastMin = -1;

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
    else if (cp == '\r') { /* ignore CR */ }
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
    if (cp == '\r') { continue; }
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
    if (cp == '\r') { continue; }          // CR is ignored; LF breaks the line
    if (cp == '\n') {
      cx = x; cy += lineH; lines++;
      if (lines > maxLines) {
        // Consume the newline that ends the last line of this page.  Pointing at
        // `save` would leave it at the start of the next page, which rendered as
        // an empty first line on pages 2+.
        if (nextOffset) *nextOffset = i;
        return lines - 1;
      }
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
void iconCalendar(int x, int y, uint8_t c) {
  drawRect(x + 3, y + 4, 18, 16, c);
  fillRect(x + 3, y + 4, 18, 4, c);
  for (int i = 0; i < 3; i++) fillRect(x + 6 + i * 5, y + 11, 2, 2, c);
  for (int i = 0; i < 3; i++) fillRect(x + 6 + i * 5, y + 15, 2, 2, c);
}
void iconNews(int x, int y, uint8_t c) {
  drawRect(x + 2, y + 3, 20, 18, c);
  hline(x + 5, y + 7, 14, c);
  hline(x + 5, y + 11, 14, c);
  hline(x + 5, y + 15, 9, c);
}
void iconPet(int x, int y, uint8_t c) {
  fillCircle(x + 12, y + 15, 5, c);
  fillCircle(x + 5, y + 7, 3, c);
  fillCircle(x + 12, y + 5, 3, c);
  fillCircle(x + 19, y + 7, 3, c);
}
void iconGame(int x, int y, uint8_t c) {
  drawRect(x + 2, y + 6, 20, 12, c);
  fillCircle(x + 7, y + 12, 2, c);
  fillCircle(x + 12, y + 12, 2, c);
  fillCircle(x + 17, y + 12, 2, c);
  drawLine(x + 12, y + 2, x + 12, y + 6, c);
}
void iconFor(int i, int x, int y, uint8_t c) {
  switch (i) {
    case 0: iconClock(x, y, c); break;
    case 1: iconCalendar(x, y, c); break;
    case 2: iconNote(x, y, c); break;
    case 3: iconImage(x, y, c); break;
    case 4: iconQr(x, y, c); break;
    case 5: iconGame(x, y, c); break;
    case 6: iconNews(x, y, c); break;
    case 7: iconPet(x, y, c); break;
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
void drawStatus(const char* title, const char* extra = nullptr) {
  fillRect(0, 0, W, 26, PAPER);
  drawText(8, 5, String(title), 1, INK);
  String hm = timeHM();
  int tw = textWidth(hm, 1);
  drawText(W - 8 - tw, 5, hm, 1, INK);
  int wifiX = W - 8 - tw - 24;
  drawWifi(wifiX, 7);
  if (extra && extra[0]) {
    String e(extra);
    int ew = textWidth(e, 1);
    drawText(wifiX - 8 - ew, 5, e, 1, INK);   // page number left of the Wi-Fi icon
  }
  hline(0, 25, W, INK);
}

// ---------------- apps ----------------
const char* MENU[] = {"时钟", "日历", "记事", "图片", "二维码", "游戏", "新闻", "宠物", "设置"};
const int MENU_N = 9;

void drawHome() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus(hostLabel.c_str());
  drawText(20, 30, "主菜单", 2, INK);
  drawText(334, 38, "PDA", 1, INK);
  const int cw = 112, ch = 56, gx = 18, gy = 8;
  for (int i = 0; i < MENU_N; i++) {
    int row = i / 3, col = i % 3;
    int inRow = MENU_N - row * 3; if (inRow > 3) inRow = 3;
    int x = (inRow == 3) ? (20 + col * (cw + gx))
                         : ((W - (inRow * cw + (inRow - 1) * gx)) / 2 + col * (cw + gx));
    int y = 72 + row * (ch + gy);
    bool sel = (i == menuSel);
    if (sel) fillRect(x - 4, y - 4, cw + 8, ch + 8, INK);
    uint8_t ink = sel ? PAPER : INK;
    iconFor(i, x + (cw - 24) / 2, y + 4, ink);
    String label = MENU[i];
    int tw = textWidth(label, 1);
    drawText(x + (cw - tw) / 2, y + 34, label, 1, ink);
  }
  hline(12, 270, 376, INK);
  drawText(14, 276, "←↑↓→ 选择   OK 进入", 1, INK);
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
constexpr int NOTE_LINES = 12;

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
  int pages = notePages();
  if (notePage < 0) notePage = 0;
  if (notePage >= pages) notePage = pages - 1;
  String pg = String(notePage + 1) + "/" + String(pages);
  drawStatus("记事", pg.c_str());
  if (note.length() == 0) {
    drawText(20, 72, "在网页输入文字，点“发送到记事”。", 1, INK);
  } else {
    int start = noteStart(notePage);
    layoutText(20, 28, NOTE_MAXW, NOTE_LINEH, NOTE_LINES, note, 1, INK, true, start, nullptr);
  }
  hline(12, 270, 376, INK);
  drawText(14, 276, "↑↓ 翻页   Back 返回", 1, INK);
}

void drawSettingsApp() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("设置");
  int y = 40, step = 23;
  auto line = [&](const String& s) { drawText(20, y, s, 1, INK); y += step; };
  line(String("WiFi  ") + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("未连接")));
  line(String("IP    ") + WiFi.localIP().toString());
  line(String("RSSI  ") + String(WiFi.RSSI()) + " dBm");
  line(String("HOST  ") + hostLabel);
  line(String("FS    ") + storage::summary());
  line(String("HEAP  ") + String(ESP.getFreeHeap() / 1024) + " KB");
  line(String("MODE  ") + panel42::modeName());
  line(String("FLASH ") + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB");
  line(String("UP    ") + String(millis() / 1000) + " s");
  line(String("WEB   http://") + WiFi.localIP().toString());
  hline(12, 270, 376, INK);
  drawText(14, 276, "Back 返回", 1, INK);
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
  hline(12, 270, 376, INK);
  drawText(14, 276, "Back 返回", 1, INK);
}

void drawImageApp() {
  if (hasImage) {
    memcpy(fb, imageFb, IMGSZ);
    // overlay the gallery position in the status bar
    if (imgCount > 1) {
      String pos = String(imgCur + 1) + "/" + String(imgCount);
      fillRect(0, 0, W, 26, PAPER);
      drawStatus("图片", pos.c_str());
    }
    return;
  }
  fillRect(0, 0, W, H, PAPER);
  drawStatus("图片");
  drawText(20, 70, "还没有图片。", 1, INK);
  drawText(20, 100, "在网页“图片上传”里选择图片，", 1, INK);
  drawText(20, 124, "点“全屏刷新”后会自动进入图片页。", 1, INK);
  hline(12, 270, 376, INK);
  drawText(14, 276, "Back 返回", 1, INK);
}

#include "games_impl.h"

void renderAndCommit(bool forceFull);   // defined below, used by news loading screens
#include "news_impl.h"
#include "pet_impl.h"
#include "calendar_impl.h"

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


// ---------------- boot splash ----------------
// Drawn right after the panel is initialised, before Wi-Fi is touched, so the
// board shows something useful instead of a blank white screen while the radio
// is trying to associate.
void drawBootSplash(const String& title, const String& sub, int progress,
                    const String& status) {
  fillRect(0, 0, W, H, PAPER);
  drawRect(0, 0, W, H, INK);
  drawRect(3, 3, W - 6, H - 6, INK);

  // Title (scale 3 = 48 px tall) then subtitle well below it.
  String t = title.length() ? title : String("E-Paper PDA");
  if (textWidth(t, 3) > W - 40) t = "E-Paper PDA";
  int tw = textWidth(t, 3);
  drawText((W - tw) / 2, 34, t, 3, INK);

  String s2 = sub.length() ? sub : String("电子墨水屏 PDA");
  int sw = textWidth(s2, 1);
  drawText((W - sw) / 2, 90, s2, 1, INK);

  hline(40, 116, W - 80, INK);

  // Progress bar with the percentage outside, so the fill never covers it.
  const int bx = 52, by = 138, bw = 240, bh = 20;
  drawRect(bx, by, bw, bh, INK);
  int p = progress; if (p < 0) p = 0; if (p > 100) p = 100;
  int fillw = (bw - 6) * p / 100;
  if (fillw > 0) fillRect(bx + 3, by + 3, fillw, bh - 6, INK);
  drawText(bx + bw + 10, by + 4, String(p) + "%", 1, INK);

  if (status.length()) {
    String st = status;
    while (textWidth(st, 1) > W - 80 && st.length() > 4) st.remove(st.length() - 1);
    drawText(40, 176, st, 1, INK);
  }
  drawText(40, 200, "本地界面已就绪，可随时使用", 1, INK);
  drawText(40, 220, "网页遥控与推送需要 Wi-Fi", 1, INK);

  hline(40, 242, W - 80, INK);
  String url = String("http://") + hostLabel + ".local";
  int uw = textWidth(url, 1);
  drawText((W - uw) / 2, 250, url, 1, INK);
}

void render() {
  switch (app) {
    case App::HOME: drawHome(); break;
    case App::CLOCK: drawClockApp(); break;
    case App::CALENDAR: drawCalendarApp(); break;
    case App::NOTES: drawNotesApp(); break;
    case App::IMAGE: drawImageApp(); break;
    case App::SETTINGS: drawSettingsApp(); break;
    case App::QR: drawQrApp(); break;
    case App::NEWS: drawNewsApp(); break;
    case App::PET: drawPetApp(); break;
    case App::GAMES: drawGamesApp(); break;
    case App::GOMOKU: drawGomokuApp(); break;
    case App::DDZ: drawDdzApp(); break;
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
  else if (s == "cal" || s == "calendar" || s == "rili") { calendarOpen(); app = App::CALENDAR; }
  else if (s == "notes" || s == "note") app = App::NOTES;
  else if (s == "image" || s == "picture") app = App::IMAGE;
  else if (s == "settings" || s == "setting") app = App::SETTINGS;
  else if (s == "qr" || s == "qrcode" || s == "code") app = App::QR;
  else if (s == "news" || s == "xinwen") { newsReset(); app = App::NEWS; }
  else if (s == "pet" || s == "chongwu") { app = App::PET; petEnter(); }
  else if (s == "games" || s == "game") app = App::GAMES;
  else if (s == "gomoku") { gReset(true); app = App::GOMOKU; }
  else if (s == "ddz" || s == "doudizhu") { dReset(true); app = App::DDZ; }
  else return false;
  return true;
}

} // namespace

// ---------------- public ----------------
void petCommand(const String& line) {
  String cmd = line; cmd.trim(); cmd.toLowerCase();
  if (cmd.length() == 0 || cmd == "status" || cmd == "stat") { petPrintStatus(); return; }
  if (cmd == "feed") { petDoAction(0); petPrintStatus(); return; }
  if (cmd == "play") { petDoAction(1); petPrintStatus(); return; }
  if (cmd == "sleep") { petDoAction(2); petPrintStatus(); return; }
  if (cmd == "clean") { petDoAction(3); petPrintStatus(); return; }
  if (cmd == "pet" || cmd == "touch") { petDoAction(4); petPrintStatus(); return; }
  if (cmd == "work") { petDoAction(5); petPrintStatus(); return; }
  if (cmd == "adopt cat" || cmd == "adopt 0") { petReset(0); petPrintStatus(); return; }
  if (cmd == "adopt fox" || cmd == "adopt 1") { petReset(1); petPrintStatus(); return; }
  if (cmd == "reset") { petReset(petSpecies); petPrintStatus(); return; }
  if (cmd == "save") { petSave(); Serial.println("pet: saved"); return; }
  if (cmd == "unadopt") {
    petPrefs.begin("pet", false); petPrefs.clear(); petPrefs.end();
    petAdopted = false; pvView = PV_ADOPT;
    Serial.println("pet: unadopted (adopt again with: pet adopt cat|fox)");
    return;
  }
  if (cmd.startsWith("name ")) {
    String n = line.substring(5); n.trim();
    if (n.length() > 0 && n.length() <= 12) { petName = n; petSave(); Serial.printf("pet: name=%s\r\n", petName.c_str()); }
    else Serial.println("pet: name must be 1-12 bytes");
    return;
  }
  Serial.println("usage: pet [status|feed|play|sleep|clean|touch|work|adopt cat|fox|name <n>|reset|save]");
}

// ---------------- calendar public API ----------------
static String jsonEsc(const String& in) {
  String o;
  for (size_t i = 0; i < in.length(); i++) {
    char ch = in[i];
    if (ch == '"' || ch == '\\') o += '\\';
    if (ch == '\n') { o += "\\n"; continue; }
    o += ch;
  }
  return o;
}

String calendarMonthJson(int y, int m) {
  calEnsure();
  if (m < 1 || m > 12 || y < 1970 || y > 2100) return String("{\"ok\":0,\"err\":\"bad ym\"}");
  String o;
  o.reserve(64 + 31 * 48);
  o += "{\"ok\":1,\"y\":"; o += y; o += ",\"m\":"; o += m;
  o += ",\"dim\":"; o += calDim(y, m);
  o += ",\"first\":"; o += calDow(y, m, 1);
  o += ",\"days\":[";
  for (int d = 1; d <= calDim(y, m); d++) {
    uint8_t kind = 0;
    const CalHol* h = calHolAt(y, m, d, &kind);
    int t = calTermAt(y, m, d);
    int e = calCountEvents(y, m, d);
    if (!h && t < 0 && e == 0) continue;
    if (o.charAt(o.length() - 1) != '[') o += ',';
    o += "{\"d\":"; o += d;
    if (h) { o += ",\"h\":\""; o += h->name; o += "\",\"k\":"; o += (int)kind; }
    if (t >= 0) { o += ",\"t\":\""; o += CAL_TERM[t]; o += "\""; }
    if (e > 0) { o += ",\"e\":"; o += e; }
    o += "}";
  }
  o += "]}";
  return o;
}

String calendarEventsJson() {
  calEnsure();
  String o = "{\"ok\":1,\"raw\":\"";
  o += jsonEsc(calEvents);
  o += "\",\"list\":[";
  int p = 0, first = 1;
  while (p < (int)calEvents.length()) {
    int e = calEvents.indexOf('\n', p);
    if (e < 0) e = calEvents.length();
    String line = calEvents.substring(p, e);
    int bar = line.indexOf('|');
    if (bar > 0) {
      if (!first) o += ',';
      first = 0;
      o += "{\"date\":\""; o += line.substring(0, bar);
      o += "\",\"text\":\""; o += jsonEsc(line.substring(bar + 1)); o += "\"}";
    }
    p = e + 1;
  }
  o += "]}";
  return o;
}

bool calendarAddEvent(const String& date, const String& text) {
  calEnsure();
  if (date.length() != 10) return false;
  bool ok = calAddEvent(date, text);
  if (ok && app == App::CALENDAR && calY == date.substring(0,4).toInt() &&
      calM == date.substring(5,7).toInt()) renderAndCommit(true);
  return ok;
}

bool calendarDelEvent(const String& date, int idx) {
  calEnsure();
  bool ok = calDelEvent(date, idx);
  if (ok && app == App::CALENDAR) renderAndCommit(true);
  return ok;
}

void calendarGoto(int y, int m, int d) {
  calY = y; calM = m; calD = d; calClampDay(); calView = 0;
}

void calendarCommand(const String& line) {
  calEnsure();
  String c = line; c.trim();
  String lc = c; lc.toLowerCase();
  if (lc.length() == 0 || lc == "show") {
    uint8_t kind = 0;
    const CalHol* h = calHolAt(calY, calM, calD, &kind);
    int t = calTermAt(calY, calM, calD);
    Serial.printf("cal: %04d-%02d-%02d 周%s", calY, calM, calD, CAL_WEEK[calDow(calY, calM, calD)]);
    if (h) Serial.printf("  %s(%s)", h->name, kind == 2 ? "班" : "休");
    if (t >= 0) Serial.printf("  节气:%s", CAL_TERM[t]);
    Serial.println();
    String evs[16];
    int n = calEventsOn(calY, calM, calD, evs, 16);
    for (int i = 0; i < n; i++) Serial.printf("  %d. %s\r\n", i + 1, evs[i].c_str());
    return;
  }
  if (lc == "today") { calToday(); if (app == App::CALENDAR) renderAndCommit(true); Serial.printf("cal: %04d-%02d-%02d\r\n", calY, calM, calD); return; }
  if (lc.startsWith("goto ")) {
    String d = c.substring(5); d.trim();
    if (d.length() >= 10) {
      int y = d.substring(0, 4).toInt(), m = d.substring(5, 7).toInt(), dd = d.substring(8, 10).toInt();
      calendarGoto(y, m, dd);
      if (app == App::CALENDAR) renderAndCommit(true);
      Serial.printf("cal: %04d-%02d-%02d\r\n", calY, calM, calD);
      return;
    }
  }
  if (lc.startsWith("add ")) {
    String rest = c.substring(4); rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 10) { Serial.println("usage: cal add YYYY-MM-DD text"); return; }
    String d = rest.substring(0, sp), txt = rest.substring(sp + 1);
    bool ok = calendarAddEvent(d, txt);
    if (ok && app == App::CALENDAR) renderAndCommit(true);
    Serial.println(ok ? "cal: added" : "cal: add failed (bad date or full)");
    return;
  }
  if (lc.startsWith("del ")) {
    String rest = c.substring(4); rest.trim();
    int sp = rest.indexOf(' ');
    if (sp < 10) { Serial.println("usage: cal del YYYY-MM-DD <n>"); return; }
    String d = rest.substring(0, sp);
    int idx = rest.substring(sp + 1).toInt() - 1;
    bool okd = calendarDelEvent(d, idx);
    if (okd && app == App::CALENDAR) renderAndCommit(true);
    Serial.println(okd ? "cal: deleted" : "cal: delete failed");
    return;
  }
  if (lc == "list") { Serial.println(calendarEventsJson()); return; }
  Serial.println("usage: cal | cal add YYYY-MM-DD text | cal del YYYY-MM-DD n | cal list | cal today");
}

void newsCommand(const String& line) {
  String c = line; c.trim(); c.toLowerCase();
  if (c == "clear" || c == "clean") { newsCacheClear(); Serial.println("news: cache cleared"); return; }
  if (c.startsWith("offline")) {
    int cat = c.length() > 7 ? c.substring(8).toInt() : 0;
    if (cat < 0 || cat >= NEWS_CAT_N) cat = 0;
    if (!newsCacheLoadList(cat)) { Serial.println("news: no cache for that category"); return; }
    nvCatName = NEWS_CATS[cat].name;
    nvCached = true; nvListSel = 0; nvListTop = 0; nvView = NV_LIST; app = App::NEWS;
    renderAndCommit(true);
    Serial.printf("news: showing cached %s (%d items)\r\n", NEWS_CATS[cat].name, nvN);
    return;
  }
  if (c.startsWith("art ")) {
    int idx = c.substring(4).toInt();
    if (idx < 0 || idx >= nvN) { Serial.println("news: use 'news show <cat>' first"); return; }
    String t = newsReadFile(newsArticlePath(nvLink[idx]));
    Serial.printf("news cache article[%d]: %u bytes  link=%s\r\n",
                  idx, (unsigned)t.length(), nvLink[idx].c_str());
    if (t.length()) { Serial.println(t.substring(0, 120)); }
    return;
  }
  if (c.startsWith("show ")) {
    int cat = c.substring(5).toInt();
    if (cat < 0 || cat >= NEWS_CAT_N) { Serial.println("news: bad category index"); return; }
    if (!newsCacheLoadList(cat)) { Serial.println("news: no cache for that category"); return; }
    Serial.printf("news cache[%s]: %d item(s)\r\n", NEWS_CATS[cat].name, nvN);
    for (int i = 0; i < nvN && i < 3; i++) {
      Serial.printf("  %d. %s\r\n", i + 1, nvTitle[i].c_str());
      Serial.printf("     %s\r\n", nvLink[i].c_str());
    }
    return;
  }
  int files = 0, lists = 0;
  size_t bytes = 0;
  DIR* d = opendir(NEWS_CACHE_DIR);
  if (d) {
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
      if (e->d_name[0] == '.') continue;
      String p = String(NEWS_CACHE_DIR) + "/" + e->d_name;
      struct stat st;
      if (stat(p.c_str(), &st) == 0) bytes += (size_t)st.st_size;
      files++;
      if (e->d_name[0] == 'L') lists++;
    }
    closedir(d);
  }
  Serial.printf("news cache: %d files (%d list files), %u bytes in %s\r\n",
                files, lists, (unsigned)bytes, NEWS_CACHE_DIR);
  Serial.println("  news show <cat> | news art <n> | news offline <cat> | news clear");
}

int imageCount() { return imgCount; }
int imageCurrent() { return imgCur; }
void imageRescan() { imgScan(); }

// Persist only: the caller has already pushed the frame to the panel, so this
// must not refresh again (an extra full refresh per upload is pure flash wear).
bool imageSave(const uint8_t* data, int& idxOut) {
  bool ok = imageSaveNew(data, idxOut);
  if (ok) { hasImage = true; app = App::IMAGE; memcpy(imageFb, data, IMGSZ); }
  return ok;
}

bool imageSelect(int idx) {
  if (idx < 0 || idx >= imgCount) return false;
  if (!imageLoadIndex(idx)) return false;
  if (idx != imgCur) {           // only touch NVS when the selection really changes
    imgCur = idx;
    prefs.begin("pda", false); prefs.putInt("img_cur", imgCur); prefs.end();
  }
  imgCur = idx;
  app = App::IMAGE;
  renderAndCommit(true);
  return true;
}

bool imageDelete(int idx) {
  if (!imageDeleteIndex(idx)) return false;
  if (app == App::IMAGE) renderAndCommit(true);
  return true;
}

String imagesJson() {
  String o;
  o.reserve(160 + imgCount * 48);
  o += "{\"ok\":1,\"cur\":"; o += imgCur;
  o += ",\"count\":"; o += imgCount;
  o += ",\"items\":[";
  for (int i = 0; i < imgCount; i++) {
    if (i) o += ',';
    o += "{\"idx\":"; o += i;
    o += ",\"name\":\""; o += imgNames[i]; o += "\"";
    o += ",\"size\":15000}";
  }
  o += "]}";
  return o;
}

const uint8_t* screen() { return fb; }

const char* appName() {
  switch (app) {
    case App::HOME: return "home";
    case App::CLOCK: return "clock";
    case App::CALENDAR: return "calendar";
    case App::NOTES: return "notes";
    case App::IMAGE: return "image";
    case App::SETTINGS: return "settings";
    case App::QR: return "qr";
    case App::NEWS: return "news";
    case App::PET: return "pet";
    case App::GAMES: return "games";
    case App::GOMOKU: return "gomoku";
    case App::DDZ: return "ddz";
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
  s += ",\"host\":\""; s += hostLabel; s += "\"";
  s += ",\"ms\":"; s += (unsigned long)lastT.total;
  s += ",\"refresh\":"; s += (unsigned long)lastT.refresh;
  s += ",\"ip\":\""; s += WiFi.localIP().toString(); s += "\"";
  s += ",\"heap\":"; s += (unsigned long)ESP.getFreeHeap();
  s += ",\"time\":\""; s += timeHM(); s += "\"}";
  return s;
}

// Re-render the current app and let commit() pick a partial refresh: used when
// the Wi-Fi state changes in the background so the status bar stays truthful.
void refreshStatus() { renderAndCommit(false); }

void bootSplash(const String& title, const String& sub, int progress,
                const String& status) {
  drawBootSplash(title, sub, progress, status);
  panel42::fullStd(fb, lastT);
}

void begin() {
  prefs.begin("pda", false);
  note = prefs.getString("note", "");
  note.replace("\r\n", "\r\n");
  note.replace("\r", "\r\n");
  app = App::HOME;
  menuSel = 0;
  notePage = 0;
  hasImage = false;
  imgScan();
  imgLoadPrefs();
  if (imgCount > 0) {
    if (imgCur >= imgCount) imgCur = 0;
    imageLoadIndex(imgCur);
  }
  memset(fb, 0xFF, IMGSZ);
  renderAndCommit(true);
}

panel42::Timings key(const String& k) {
  String s = k; s.toLowerCase();
  bool wasOff = !panel42::isPowered();
  App before = app;
  if (app == App::NEWS) {
    newsKey(s);
    renderAndCommit(true);
    return lastT;
  }
  if (app == App::CALENDAR) {
    calKey(s);
    renderAndCommit(true);
    return lastT;
  }
  if (app == App::PET) {
    petKey(s);
    renderAndCommit(true);
    return lastT;
  }
  if (app == App::GAMES || app == App::GOMOKU || app == App::DDZ) {
    gamesKey(s);
    renderAndCommit(true);
    return lastT;
  }
  bool pageChanged = false;
  bool homeMoved = false;
  if (s == "home") app = App::HOME;
  else if (s == "back") app = App::HOME;
  else if (s == "up") {
    if (app == App::HOME) {
      if (menuSel >= 3) menuSel -= 3;
      else menuSel = 6 + (menuSel % 3);
      homeMoved = true;
    }
    else if (app == App::NOTES && notePage > 0) { notePage--; pageChanged = true; }
  } else if (s == "down") {
    if (app == App::HOME) {
      if (menuSel + 3 < MENU_N) menuSel += 3;
      else menuSel = menuSel % 3;
      homeMoved = true;
    }
    else if (app == App::NOTES && notePage + 1 < notePages()) { notePage++; pageChanged = true; }
  } else if (s == "left") {
    if (app == App::IMAGE && imgCount > 1) {
      imgCur = (imgCur + imgCount - 1) % imgCount;
      imageLoadIndex(imgCur);
      prefs.begin("pda", false); prefs.putInt("img_cur", imgCur); prefs.end();
      pageChanged = true;
    }
    else if (app == App::HOME) {
      int row = menuSel / 3;
      if (menuSel % 3 > 0) menuSel--;
      else menuSel = row * 3 + (row == 2 ? 1 : 2);
      homeMoved = true;
    }
    else if (app == App::NOTES && notePage > 0) { notePage--; pageChanged = true; }
  } else if (s == "right") {
    if (app == App::IMAGE && imgCount > 1) {
      imgCur = (imgCur + 1) % imgCount;
      imageLoadIndex(imgCur);
      prefs.begin("pda", false); prefs.putInt("img_cur", imgCur); prefs.end();
      pageChanged = true;
    }
    else if (app == App::HOME) {
      int row = menuSel / 3;
      if (menuSel % 3 < 2 && menuSel + 1 < 8) menuSel++;
      else menuSel = row * 3;
      homeMoved = true;
    }
    else if (app == App::NOTES && notePage + 1 < notePages()) { notePage++; pageChanged = true; }
  } else if (s == "ok") {
    if (app == App::HOME) {
      switch (menuSel) {
        case 0: app = App::CLOCK; break;
        case 1: calendarOpen(); app = App::CALENDAR; break;
        case 2: app = App::NOTES; notePage = 0; break;
        case 3: app = App::IMAGE; break;
        case 4: app = App::QR; break;
        case 5: app = App::GAMES; break;
        case 6: newsReset(); app = App::NEWS; break;
        case 7: app = App::PET; petEnter(); break;
        case 8: app = App::SETTINGS; break;
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
  t.replace("\r\n", "\r\n");
  t.replace("\r", "\r\n");
  if (t.length() > 4000) t = t.substring(0, 4000);
  note = t;
  prefs.putString("note", note);
  app = App::NOTES;
  notePage = 0;
  renderAndCommit(true);
  return lastT;
}

void setHostnameLabel(const String& name, bool redraw) {
  if (name.length() == 0) return;
  hostLabel = name;
  if (redraw && (app == App::HOME || app == App::SETTINGS) && panel42::isPowered()) {
    renderAndCommit(true);
  }
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
  petTick();
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
  } else if (app != App::IMAGE && app != App::QR && app != App::GAMES && app != App::GOMOKU && app != App::DDZ && app != App::NEWS && app != App::PET) {
    if (minute != lastMin) {
      lastMin = minute;
      render();
      if (ok && (t.tm_min % 15) == 0) panel42::fullStd(fb, lastT);
      else commit(false);            // status bar only, low stress
    }
  }
}

} // namespace ui
