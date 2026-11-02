#pragma once
// Calendar with Chinese holidays / 调休, the 24 solar terms (节气) and user
// events.  Included inside ui_impl.h's anonymous namespace.
#include "calendar_data.h"

static int calY = 2026, calM = 11, calD = 2;
static int calView = 0;          // 0 = month grid, 1 = day detail
static int calSelEv = 0;
static String calEvents;         // "YYYY-MM-DD|text\n" ...

// Truncate to at most `maxChars` UTF-8 characters, appending … when cut.
static String calClip(const String& s, int maxChars) {
  int n = 0, i = 0;
  while (i < (int)s.length() && n < maxChars) {
    uint8_t c = (uint8_t)s[i];
    int step = (c < 0x80) ? 1 : ((c >> 5) == 0x6 ? 2 : ((c >> 4) == 0xE ? 3 : 4));
    i += step; n++;
  }
  if (i >= (int)s.length()) return s;
  return s.substring(0, i) + "…";
}

static int calDim(int y, int m) {
  static const uint8_t dm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return dm[m - 1];
}

static int calDow(int y, int m, int d) {     // 0 = Sunday
  static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) y -= 1;
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static const char* CAL_WEEK[7] = {"日","一","二","三","四","五","六"};

static const CalHol* calHolAt(int y, int m, int d, uint8_t* kind = nullptr) {
  int cur = m * 100 + d;
  for (int i = 0; i < CAL_HOL_N; i++) {
    const CalHol& h = CAL_HOL[i];
    if (h.y != y) continue;
    int from = h.m0 * 100 + h.d0, to = h.m1 * 100 + h.d1;
    if (cur >= from && cur <= to) { if (kind) *kind = h.kind; return &h; }
  }
  return nullptr;
}

static int calTermAt(int y, int m, int d) {   // term index 0..23 or -1
  int row = y - CAL_TERM_Y0;
  if (row < 0 || row >= CAL_TERM_YEARS) return -1;
  for (int k = 0; k < 24; k++) {
    if (k / 2 + 1 != m) continue;
    if (CAL_TERM_DAY[row][k] == d) return k;
  }
  return -1;
}

static String calKey(int y, int m, int d) {
  char b[12];
  snprintf(b, sizeof(b), "%04d-%02d-%02d", y, m, d);
  return String(b);
}

static bool calLoaded = false;

static void calLoadEvents() {
  prefs.begin("cal", true);
  calEvents = prefs.getString("ev", "");
  prefs.end();
  calLoaded = true;
}

// Load once, lazily: the web/console may touch events before the app is opened.
static void calEnsure() { if (!calLoaded) calLoadEvents(); }

static void calSaveEvents() {
  prefs.begin("cal", false);
  prefs.putString("ev", calEvents);
  prefs.end();
}

static int calEventsOn(int y, int m, int d, String* out, int max) {
  String key = calKey(y, m, d);
  int n = 0, p = 0;
  while (p < (int)calEvents.length() && n < max) {
    int e = calEvents.indexOf('\n', p);
    if (e < 0) e = calEvents.length();
    String line = calEvents.substring(p, e);
    int bar = line.indexOf('|');
    if (bar > 0 && line.substring(0, bar) == key) out[n++] = line.substring(bar + 1);
    p = e + 1;
  }
  return n;
}

static int calCountEvents(int y, int m, int d) {
  String tmp[1];
  String key = calKey(y, m, d);
  int n = 0, p = 0;
  while (p < (int)calEvents.length()) {
    int e = calEvents.indexOf('\n', p);
    if (e < 0) e = calEvents.length();
    String line = calEvents.substring(p, e);
    int bar = line.indexOf('|');
    if (bar > 0 && line.substring(0, bar) == key) n++;
    p = e + 1;
  }
  return n;
}

static bool calAddEvent(const String& dateKey, const String& text) {
  String t = text; t.replace("\n", " "); t.replace("\r", " "); t.trim();
  if (t.length() == 0) return false;
  if (calEvents.length() + t.length() + 14 > 3000) return false;   // NVS-safe cap
  calEvents += dateKey; calEvents += '|'; calEvents += t; calEvents += '\n';
  calSaveEvents();
  return true;
}

static bool calDelEvent(const String& dateKey, int idx) {
  String out;
  int p = 0, cnt = 0;
  bool removed = false;
  while (p < (int)calEvents.length()) {
    int e = calEvents.indexOf('\n', p);
    if (e < 0) e = calEvents.length();
    String line = calEvents.substring(p, e);
    int bar = line.indexOf('|');
    bool match = (bar > 0 && line.substring(0, bar) == dateKey);
    if (match) {
      if (cnt == idx) { removed = true; cnt++; p = e + 1; continue; }
      cnt++;
    }
    out += line; out += '\n';
    p = e + 1;
  }
  if (removed) { calEvents = out; calSaveEvents(); }
  return removed;
}

static void calToday() {
  struct tm t;
  if (nowTm(t)) { calY = t.tm_year + 1900; calM = t.tm_mon + 1; calD = t.tm_mday; }
}

static void calClampDay() {
  if (calM < 1) { calM = 12; calY--; }
  if (calM > 12) { calM = 1; calY++; }
  int dim = calDim(calY, calM);
  if (calD > dim) calD = dim;
  if (calD < 1) calD = 1;
}

static void calMove(int days) {
  int dim = calDim(calY, calM);
  calD += days;
  while (calD > dim) { calD -= dim; calM++; if (calM > 12) { calM = 1; calY++; } dim = calDim(calY, calM); }
  while (calD < 1) { calM--; if (calM < 1) { calM = 12; calY--; } dim = calDim(calY, calM); calD += dim; }
}

// ---------------------------------------------------------------- drawing
static void calDrawCell(int x, int y, int w, int h, int day, bool selected, bool today,
                        int termIdx, const CalHol* hol, int evCount) {
  if (selected) fillRect(x, y, w, h, INK);
  uint8_t ink = selected ? PAPER : INK;
  if (today && !selected) drawRect(x, y, w, h, INK);

  char nb[4];
  snprintf(nb, sizeof(nb), "%d", day);
  drawText(x + 3, y + 1, nb, 1, ink);

  // second line: holiday name > 休/班 > solar term > event dot
  int ty = y + 17;
  if (hol) {
    String nm = hol->name;
    if (nm == "调休") nm = "班";
    else nm = calClip(nm, 2);        // character safe: "国庆中秋" -> "国庆"
    drawText(x + 3, ty, nm, 1, ink);
  } else if (termIdx >= 0) {
    drawText(x + 3, ty, CAL_TERM[termIdx], 1, ink);   // 2 chars = 32 px, fits 55
  } else if (evCount > 0) {
    drawText(x + 3, ty, String("•") + String(evCount), 1, ink);
  }
}

static void drawCalendarApp() {
  fillRect(0, 0, W, H, PAPER);

  if (calView == 1) {
    char tb[32];
    snprintf(tb, sizeof(tb), "%d-%02d-%02d %s", calY, calM, calD, CAL_WEEK[calDow(calY, calM, calD)]);
    drawStatus("日程", tb);
    int y = 32;
    uint8_t kind = 0;
    const CalHol* hol = calHolAt(calY, calM, calD, &kind);
    int term = calTermAt(calY, calM, calD);
    if (hol) {
      drawText(16, y, (String(hol->name) + (kind == 2 ? "（调休上班）" : "（放假）")), 1, INK);
      y += 20;
    }
    if (term >= 0) { drawText(16, y, String("节气：") + CAL_TERM[term], 1, INK); y += 20; }

    String evs[16];
    int n = calEventsOn(calY, calM, calD, evs, 16);
    drawText(16, y + 4, String("日程 ") + n + " 条", 1, INK);
    y += 22;
    if (n == 0) {
      drawText(28, y, "（无）用网页或串口添加", 1, INK);
    } else {
      if (calSelEv >= n) calSelEv = n - 1;
      if (calSelEv < 0) calSelEv = 0;
      for (int i = 0; i < n && i < 8; i++) {
        int yy = y + i * 20;
        if (i == calSelEv) fillRect(12, yy - 2, 376, 19, INK);
        uint8_t ink = (i == calSelEv) ? PAPER : INK;
        drawText(16, yy, String(i + 1) + ".", 1, ink);
        drawText(34, yy, calClip(evs[i], 21), 1, ink);
      }
    }
    hline(12, 270, 376, INK);
    drawText(14, 276, "↑↓ 选择  OK 删除  Back 返回", 1, INK);
    return;
  }

  char tb[24];
  snprintf(tb, sizeof(tb), "%d年%d月", calY, calM);
  drawStatus("日历", tb);

  // weekday header
  const int gx = 8, cw = 55, gy = 62, chh = 34;
  for (int i = 0; i < 7; i++) {
    int tw = textWidth(CAL_WEEK[i], 1);
    drawText(gx + i * cw + (cw - tw) / 2, 44, CAL_WEEK[i], 1, INK);
  }

  struct tm nowT;
  bool haveToday = nowTm(nowT);
  int tY = haveToday ? nowT.tm_year + 1900 : -1;
  int tM = haveToday ? nowT.tm_mon + 1 : -1;
  int tD = haveToday ? nowT.tm_mday : -1;

  int firstDow = calDow(calY, calM, 1);
  int dim = calDim(calY, calM);
  for (int i = 0; i < firstDow; i++) {   // blank leading cells
    drawRect(gx + i * cw, gy, cw - 1, chh - 1, INK);
  }
  for (int d = 1; d <= dim; d++) {
    int idx = firstDow + d - 1;
    int cx = gx + (idx % 7) * cw;
    int cy = gy + (idx / 7) * chh;
    bool sel = (d == calD);
    bool tod = (calY == tY && calM == tM && d == tD);
    calDrawCell(cx, cy, cw - 1, chh - 1, d, sel, tod,
                calTermAt(calY, calM, d), calHolAt(calY, calM, d), calCountEvents(calY, calM, d));
  }

  // bottom info for the selected day
  String info;
  uint8_t kind = 0;
  const CalHol* hol = calHolAt(calY, calM, calD, &kind);
  int term = calTermAt(calY, calM, calD);
  int evn = calCountEvents(calY, calM, calD);
  info = String(calM) + "月" + String(calD) + "日 周" + CAL_WEEK[calDow(calY, calM, calD)];
  if (hol) { info += "  "; info += hol->name; if (kind == 2) info += "(班)"; }
  if (term >= 0) { info += "  "; info += CAL_TERM[term]; }
  if (evn > 0) { info += "  日程"; info += evn; }
  hline(6, 268, W - 12, INK);
  drawText(8, 274, calClip(info, 24), 1, INK);
}

static void calendarOpen() {
  calEnsure();
  calToday();
  calView = 0;
  calSelEv = 0;
}

static void calKey(const String& k) {
  if (calView == 1) {
    if (k == "back") { calView = 0; return; }
    if (k == "home") { app = App::HOME; return; }
    String evs[16];
    int n = calEventsOn(calY, calM, calD, evs, 16);
    if (k == "up" && n > 0) calSelEv = (calSelEv + n - 1) % n;
    else if (k == "down" && n > 0) calSelEv = (calSelEv + 1) % n;
    else if (k == "ok" && n > 0) { calDelEvent(calKey(calY, calM, calD), calSelEv); if (calSelEv > 0) calSelEv--; }
    return;
  }
  if (k == "left") calMove(-1);
  else if (k == "right") calMove(1);
  else if (k == "up") calMove(-7);
  else if (k == "down") calMove(7);
  else if (k == "ok") { calView = 1; calSelEv = 0; }
  else if (k == "back" || k == "home") app = App::HOME;
  else if (k == "refresh") { calToday(); }
}
