#pragma once
// Games for the 400x300 e-paper UI.  Included inside ui_impl.h's anonymous
// namespace, after the drawing primitives, so it can use drawText/fillRect/etc.

// ============================ game menu ============================
static int gameMenuSel = 0;
static const char* GAME_MENU[] = {"五子棋 · 人机", "五子棋 · 双人",
                                  "斗地主 · 人机", "斗地主 · 三人", "返回"};

static void drawGamesApp() {
  fillRect(0, 0, W, H, PAPER);
  drawStatus("游戏");
  drawText(20, 34, "选择游戏", 2, INK);
  const int top = 78, ih = 34;
  for (int i = 0; i < 5; i++) {
    int y = top + i * ih;
    bool sel = (i == gameMenuSel);
    if (sel) fillRect(12, y, 376, 30, INK);
    uint8_t ink = sel ? PAPER : INK;
    drawText(30, y + 7, GAME_MENU[i], 1, ink);
    if (sel) drawText(366, y + 7, ">", 1, ink);
  }
  hline(12, 270, 376, INK);
  drawText(14, 276, "↑↓ 选择   OK 开始   Back 返回", 1, INK);
}

// ============================ Gomoku ============================
#define GN 15
static uint8_t gb[GN][GN];              // 0 empty, 1 black, 2 white
static int gcx = 7, gcy = 7;
static bool gai = true;
static uint8_t gturn = 1;
static bool gover = false;
static uint8_t gwinner = 0;

static void gReset(bool ai) {
  memset(gb, 0, sizeof(gb));
  gai = ai;
  gturn = 1;
  gcx = gcy = 7;
  gover = false;
  gwinner = 0;
}

static bool gInside(int x, int y) { return x >= 0 && x < GN && y >= 0 && y < GN; }

static bool gWinAt(int x, int y, uint8_t c) {
  const int dx[4] = {1, 0, 1, 1};
  const int dy[4] = {0, 1, 1, -1};
  for (int d = 0; d < 4; d++) {
    int n = 1;
    for (int s = 1; s <= 4; s++) { int nx = x + dx[d] * s, ny = y + dy[d] * s; if (!gInside(nx, ny) || gb[ny][nx] != c) break; n++; }
    for (int s = 1; s <= 4; s++) { int nx = x - dx[d] * s, ny = y - dy[d] * s; if (!gInside(nx, ny) || gb[ny][nx] != c) break; n++; }
    if (n >= 5) return true;
  }
  return false;
}

static int gPattern(int cnt, int open) {
  if (cnt >= 5) return 10000000;
  if (cnt == 4) return open >= 2 ? 1000000 : (open == 1 ? 100000 : 0);
  if (cnt == 3) return open >= 2 ? 50000 : (open == 1 ? 5000 : 0);
  if (cnt == 2) return open >= 2 ? 2000 : (open == 1 ? 200 : 0);
  if (cnt == 1) return open >= 2 ? 50 : 0;
  return 0;
}

static int gLineScore(int x, int y, int dx, int dy, uint8_t c) {
  int cnt = 1, open = 0;
  int nx = x + dx, ny = y + dy;
  while (gInside(nx, ny) && gb[ny][nx] == c) { cnt++; nx += dx; ny += dy; }
  if (gInside(nx, ny) && gb[ny][nx] == 0) open++;
  nx = x - dx; ny = y - dy;
  while (gInside(nx, ny) && gb[ny][nx] == c) { cnt++; nx -= dx; ny -= dy; }
  if (gInside(nx, ny) && gb[ny][nx] == 0) open++;
  return gPattern(cnt, open);
}

static int gCellScore(int x, int y, uint8_t c) {
  if (gb[y][x] != 0) return -1;
  return gLineScore(x, y, 1, 0, c) + gLineScore(x, y, 0, 1, c) +
         gLineScore(x, y, 1, 1, c) + gLineScore(x, y, 1, -1, c);
}

static bool gNearStone(int x, int y) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int nx = x + dx, ny = y + dy;
      if (gInside(nx, ny) && gb[ny][nx]) return true;
    }
  return false;
}

static void gPlace(int x, int y, uint8_t c) {
  gb[y][x] = c;
  if (gWinAt(x, y, c)) { gover = true; gwinner = c; }
  else gturn = (uint8_t)(3 - c);
}

static void gAiMove() {
  int best = -1, bx = 7, by = 7;
  bool any = false;
  for (int y = 0; y < GN; y++)
    for (int x = 0; x < GN; x++)
      if (gb[y][x] == 0 && gNearStone(x, y)) any = true;
  if (!any) { gPlace(7, 7, 2); return; }
  for (int y = 0; y < GN; y++)
    for (int x = 0; x < GN; x++) {
      if (gb[y][x] != 0 || !gNearStone(x, y)) continue;
      int a = gCellScore(x, y, 2);
      int d = gCellScore(x, y, 1);
      int s = a + a / 10 + d + (7 - abs(x - 7)) + (7 - abs(y - 7));
      if (s > best) { best = s; bx = x; by = y; }
    }
  gPlace(bx, by, 2);
}

static bool gKey(const String& k) {
  if (gover) {
    if (k == "ok") gReset(gai);
    else if (k == "back" || k == "home") app = App::GAMES;
    return true;
  }
  if (k == "left") gcx = (gcx + GN - 1) % GN;
  else if (k == "right") gcx = (gcx + 1) % GN;
  else if (k == "up") gcy = (gcy + GN - 1) % GN;
  else if (k == "down") gcy = (gcy + 1) % GN;
  else if (k == "ok") {
    if (gb[gcy][gcx] == 0) {
      gPlace(gcx, gcy, gturn);
      if (!gover && gai && gturn == 2) gAiMove();
    }
  } else if (k == "back" || k == "home") {
    app = App::GAMES;
  }
  return true;
}

static void drawGomokuApp() {
  fillRect(0, 0, W, H, PAPER);
  String sub = gover ? (gwinner == 1 ? String("黑胜") : String("白胜"))
                     : (gturn == 1 ? String("黑方") : String("白方"));
  drawStatus("五子棋", sub.c_str());
  const int cell = 16, ox = 88, oy = 30;
  for (int i = 0; i < GN; i++) {
    hline(ox, oy + i * cell, (GN - 1) * cell, INK);
    vline(ox + i * cell, oy, (GN - 1) * cell, INK);
  }
  for (int y = 0; y < GN; y++)
    for (int x = 0; x < GN; x++) {
      int px = ox + x * cell, py = oy + y * cell;
      if (gb[y][x] == 1) fillCircle(px, py, 6, INK);
      else if (gb[y][x] == 2) { fillCircle(px, py, 6, INK); fillCircle(px, py, 4, PAPER); }
    }
  int cx = ox + gcx * cell, cy = oy + gcy * cell;
  drawRect(cx - 7, cy - 7, 14, 14, INK);
  hline(12, 270, 376, INK);
  drawText(14, 276, gover ? "OK 重开   Back 返回" : "←↑↓→ 移动   OK 落子   Back 返回", 1, INK);
}

// ============================ Dou Dizhu (simplified) ============================
struct DCard { uint8_t r; uint8_t s; };   // r: 3..15 normal, 16 small joker, 17 big joker
struct DCombo { uint8_t type; uint8_t len; uint8_t rank; };
// type: 0 invalid, 1 single, 2 pair, 3 triple, 4 triple+single, 5 bomb, 6 rocket, 7 straight

static DCard dHand[3][20];
static uint8_t dN[3];
static DCard dBottom[3];
static bool dHuman[3] = {true, false, false};
static uint8_t dPhase = 0;      // 0 bid, 1 play, 2 over
static uint8_t dTurn = 0;
static uint8_t dLandlord = 0;
static uint8_t dWinner = 0;
static uint8_t dBidPass = 0;
static DCard dLast[20];
static uint8_t dLastN = 0;
static uint8_t dLastType = 0;
static uint8_t dLastRank = 0;
static uint8_t dLastLen = 0;
static int dLastPlayer = -1;
static uint8_t dPasses = 0;
static uint8_t dSel[20];
static int dCur = 0;
static String dMsg;
static DCard dCand[64][20];
static uint8_t dCandN[64];

static String dRankStr(uint8_t r) {
  if (r <= 10) return String((int)r);
  if (r == 11) return "J";
  if (r == 12) return "Q";
  if (r == 13) return "K";
  if (r == 14) return "A";
  if (r == 15) return "2";
  if (r == 16) return "w";
  return "W";
}

static uint8_t dSuit(uint8_t s) { return s < 4 ? s : 4; }

static void dSort(DCard* h, uint8_t n) {
  for (int i = 1; i < n; i++) {
    DCard key = h[i];
    int j = i - 1;
    while (j >= 0 && h[j].r < key.r) { h[j + 1] = h[j]; j--; }
    h[j + 1] = key;
  }
}

static void dDeal() {
  DCard deck[54];
  int k = 0;
  for (uint8_t r = 3; r <= 15; r++)
    for (uint8_t s = 0; s < 4; s++) deck[k++] = {r, s};
  deck[k++] = {16, 4};
  deck[k++] = {17, 4};
  for (int i = 53; i > 0; i--) { int j = (int)(esp_random() % (i + 1)); DCard t = deck[i]; deck[i] = deck[j]; deck[j] = t; }
  for (int p = 0; p < 3; p++) {
    dN[p] = 17;
    for (int i = 0; i < 17; i++) dHand[p][i] = deck[p * 17 + i];
    dSort(dHand[p], dN[p]);
  }
  for (int i = 0; i < 3; i++) dBottom[i] = deck[51 + i];
  dPhase = 0;
  dTurn = 0;
  dBidPass = 0;
  dPasses = 0;
  dLastN = 0;
  dLastPlayer = -1;
  dWinner = 0;
  memset(dSel, 0, sizeof(dSel));
  dCur = 0;
  dMsg = "叫地主：OK 叫，↓ 不叫";
}

static int dStrength(int p) {
  int s = 0;
  for (int i = 0; i < dN[p]; i++) if (dHand[p][i].r >= 14) s++;
  for (int i = 0; i + 3 < dN[p]; i++)
    if (dHand[p][i].r == dHand[p][i + 3].r) { s += 2; i += 3; }
  return s;
}

static void dStartPlay() {
  for (int i = 0; i < 3; i++) dHand[dLandlord][dN[dLandlord] + i] = dBottom[i];
  dN[dLandlord] += 3;
  dSort(dHand[dLandlord], dN[dLandlord]);
  dPhase = 1;
  dTurn = dLandlord;
  dLastN = 0;
  dPasses = 0;
  dMsg = "开始出牌";
}

static int dCountRank(const DCard* h, uint8_t n, uint8_t r) {
  int c = 0;
  for (int i = 0; i < n; i++) if (h[i].r == r) c++;
  return c;
}

static bool dClassify(const DCard* c, uint8_t n, DCombo& out) {
  out = {0, 0, 0};
  if (n == 0) return false;
  DCard tmp[20];
  for (int i = 0; i < n; i++) tmp[i] = c[i];
  dSort(tmp, n);
  uint8_t cnt[18] = {0};
  for (int i = 0; i < n; i++) cnt[tmp[i].r]++;
  if (n == 2 && cnt[16] == 1 && cnt[17] == 1) { out = {6, 1, 17}; return true; }
  if (n == 1) { out = {1, 1, tmp[0].r}; return true; }
  if (n == 2 && tmp[0].r == tmp[1].r) { out = {2, 1, tmp[0].r}; return true; }
  if (n == 3 && tmp[0].r == tmp[2].r) { out = {3, 1, tmp[0].r}; return true; }
  if (n == 4 && tmp[0].r == tmp[3].r) { out = {5, 1, tmp[0].r}; return true; }
  if (n == 4) { // triple + single
    uint8_t a = tmp[0].r, b = tmp[3].r;
    if (cnt[a] == 3 && cnt[b] == 1) { out = {4, 1, a}; return true; }
    if (cnt[a] == 1 && cnt[b] == 3) { out = {4, 1, b}; return true; }
  }
  if (n >= 5) { // straight, all singles, consecutive, <=A (14)
    bool ok = true;
    for (int i = 0; i < n; i++) if (cnt[tmp[i].r] != 1 || tmp[i].r > 14) ok = false;
    for (int i = 1; i < n && ok; i++) if (tmp[i].r != tmp[i - 1].r + 1) ok = false;
    if (ok) { out = {7, (uint8_t)n, tmp[n - 1].r}; return true; }
  }
  return false;
}

static bool dBeats(const DCombo& a, const DCombo& b) {
  if (a.type == 0) return false;
  if (b.type == 0) return true;
  if (a.type == 6) return true;
  if (b.type == 6) return false;
  if (a.type == 5 && b.type != 5) return true;
  if (b.type == 5 && a.type != 5) return false;
  if (a.type != b.type) return false;
  if (a.type == 7 && a.len != b.len) return false;
  return a.rank > b.rank;
}

static void dRemoveCards(int p, const DCard* c, uint8_t n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < dN[p]; j++) {
      if (dHand[p][j].r == c[i].r && dHand[p][j].s == c[i].s) {
        for (int k = j; k < dN[p] - 1; k++) dHand[p][k] = dHand[p][k + 1];
        dN[p]--;
        break;
      }
    }
  }
}

static void dSetLast(const DCard* c, uint8_t n, const DCombo& cb, int p) {
  for (int i = 0; i < n; i++) dLast[i] = c[i];
  dLastN = n;
  dLastType = cb.type;
  dLastRank = cb.rank;
  dLastLen = cb.len;
  dLastPlayer = p;
  dPasses = 0;
}

static void dNextTurn() { dTurn = (uint8_t)((dTurn + 1) % 3); }

static void dCheckWin(int p) {
  if (dN[p] == 0) { dPhase = 2; dWinner = p; }
}

static bool dPlayCards(int p, const DCard* c, uint8_t n) {
  DCombo cb;
  if (!dClassify(c, n, cb)) { if (p == 0) dMsg = "牌型不对"; return false; }
  if (dLastN > 0 && dLastPlayer != p) {
    DCombo last = {(uint8_t)dLastType, dLastLen, dLastRank};
    if (!dBeats(cb, last)) { if (p == 0) dMsg = "管不上"; return false; }
  }
  dRemoveCards(p, c, n);
  dSetLast(c, n, cb, p);
  dCheckWin(p);
  if (dPhase != 2) dNextTurn();
  return true;
}

static void dPassTurn(int p) {
  if (dLastN == 0 || dLastPlayer == p) { if (p == 0) dMsg = "你先出牌"; return; }
  dPasses++;
  if (dPasses >= 2) {
    dLastN = 0;
    dPasses = 0;
    dTurn = (uint8_t)dLastPlayer;
    dMsg = "两家不要，重新出牌";
  } else {
    dNextTurn();
  }
}

static void dAiPlay(int p) {
  // collect candidates (static to keep the loop stack small)
  DCard (*cand)[20] = dCand;
  uint8_t* cn = dCandN;
  int nc = 0;
  #define ADDC(n) do { if (nc < 64) { cn[nc] = (n); nc++; } } while (0)
  uint8_t cnt[18] = {0};
  for (int i = 0; i < dN[p]; i++) cnt[dHand[p][i].r]++;
  // singles
  for (uint8_t r = 3; r <= 17; r++) {
    if (!cnt[r]) continue;
    for (int i = 0; i < dN[p]; i++) if (dHand[p][i].r == r) { cand[nc][0] = dHand[p][i]; ADDC(1); break; }
  }
  // pairs
  for (uint8_t r = 3; r <= 17; r++) {
    if (cnt[r] < 2) continue;
    uint8_t k = 0;
    for (int i = 0; i < dN[p] && k < 2; i++) if (dHand[p][i].r == r) cand[nc][k++] = dHand[p][i];
    ADDC(2);
  }
  // triples and triple+single
  for (uint8_t r = 3; r <= 17; r++) {
    if (cnt[r] < 3) continue;
    uint8_t k = 0;
    for (int i = 0; i < dN[p] && k < 3; i++) if (dHand[p][i].r == r) cand[nc][k++] = dHand[p][i];
    ADDC(3);
    for (uint8_t r2 = 3; r2 <= 17; r2++) {
      if (r2 == r || cnt[r2] < 1) continue;
      for (int i = 0; i < dN[p]; i++) if (dHand[p][i].r == r2) {
        for (int j = 0; j < 3; j++) cand[nc][j] = cand[nc - 1][j];
        cand[nc][3] = dHand[p][i]; ADDC(4); break;
      }
      break;
    }
  }
  // bombs
  for (uint8_t r = 3; r <= 17; r++) {
    if (cnt[r] < 4) continue;
    uint8_t k = 0;
    for (int i = 0; i < dN[p] && k < 4; i++) if (dHand[p][i].r == r) cand[nc][k++] = dHand[p][i];
    ADDC(4);
  }
  // rocket
  if (cnt[16] && cnt[17]) {
    uint8_t k = 0;
    for (int i = 0; i < dN[p]; i++) if (dHand[p][i].r == 16 || dHand[p][i].r == 17) cand[nc][k++] = dHand[p][i];
    ADDC(2);
  }
  // straights
  for (int len = 5; len <= 12; len++) {
    for (uint8_t st = 3; st + len - 1 <= 14; st++) {
      bool ok = true;
      for (uint8_t r = st; r < st + len; r++) if (!cnt[r]) { ok = false; break; }
      if (!ok) continue;
      uint8_t k = 0;
      for (uint8_t r = st; r < st + len; r++)
        for (int i = 0; i < dN[p]; i++) if (dHand[p][i].r == r) { cand[nc][k++] = dHand[p][i]; break; }
      ADDC((uint8_t)len);
    }
  }
  int best = -1, bestScore = 0x7fffffff;
  for (int i = 0; i < nc; i++) {
    DCombo cb;
    if (!dClassify(cand[i], cn[i], cb)) continue;
    if (dLastN > 0 && dLastPlayer != p) {
      DCombo last = {(uint8_t)dLastType, dLastLen, dLastRank};
      if (!dBeats(cb, last)) continue;
    }
    int score = cb.rank * 10 + cb.type * 2 + cn[i];
    if (score < bestScore) { bestScore = score; best = i; }
  }
  if (best >= 0) dPlayCards(p, cand[best], cn[best]);
  else dPassTurn(p);
}

static void dAiBid(int p) {
  if (dStrength(p) >= 4) {
    dLandlord = (uint8_t)p;
    dStartPlay();
  } else {
    dBidPass++;
    if (dBidPass >= 3) { dDeal(); }
    else dNextTurn();
  }
}

static void dRunAi() {
  int guard = 0;
  while (dPhase != 2 && !dHuman[dTurn] && guard++ < 12) {
    if (dPhase == 0) dAiBid(dTurn);
    else dAiPlay(dTurn);
  }
}

static void dReset(bool ai) {
  dHuman[0] = true;
  dHuman[1] = !ai;
  dHuman[2] = !ai;
  dDeal();
  if (!dHuman[dTurn]) dRunAi();
}

static bool dKey(const String& k) {
  if (dPhase == 2) {
    if (k == "ok") dReset(!dHuman[1]);
    else if (k == "back" || k == "home") app = App::GAMES;
    return true;
  }
  if (k == "back" || k == "home") { app = App::GAMES; return true; }
  if (!dHuman[dTurn]) { dRunAi(); return true; }

  if (dPhase == 0) {
    if (k == "ok") { dLandlord = dTurn; dStartPlay(); dRunAi(); }
    else if (k == "down") {
      dBidPass++;
      if (dBidPass >= 3) dDeal();
      else dNextTurn();
      dRunAi();
    }
    return true;
  }

  // play phase
  int n = dN[dTurn];
  if (n <= 0) return true;
  if (k == "left") dCur = (dCur + n - 1) % n;
  else if (k == "right") dCur = (dCur + 1) % n;
  else if (k == "up") dSel[dCur] = dSel[dCur] ? 0 : 1;
  else if (k == "down") { dPassTurn(dTurn); dRunAi(); }
  else if (k == "ok") {
    DCard sel[20];
    uint8_t sn = 0;
    for (int i = 0; i < n; i++) if (dSel[i]) sel[sn++] = dHand[dTurn][i];
    if (sn == 0) { dMsg = "先选牌"; return true; }
    if (dPlayCards(dTurn, sel, sn)) {
      memset(dSel, 0, sizeof(dSel));
      dCur = 0;
      dRunAi();
    }
  }
  return true;
}

static void dDrawCard(int x, int y, const DCard& c, bool selected) {
  int yy = selected ? y - 4 : y;
  fillRect(x, yy, 18, 28, PAPER);
  drawRect(x, yy, 18, 28, INK);
  drawText(x + 2, yy + 2, dRankStr(c.r), 1, INK);
  const char* suit = c.s == 0 ? "S" : (c.s == 1 ? "H" : (c.s == 2 ? "D" : (c.s == 3 ? "C" : "-")));
  drawText(x + 2, yy + 15, suit, 1, INK);
}

static void drawDdzApp() {
  fillRect(0, 0, W, H, PAPER);
  String sub;
  if (dPhase == 0) sub = String("叫地主 ") + (dTurn == 0 ? "你" : (dTurn == 1 ? "电脑1" : "电脑2"));
  else if (dPhase == 1) sub = String("出牌 ") + (dTurn == 0 ? "你" : (dTurn == 1 ? "电脑1" : "电脑2"));
  else sub = String(dWinner == 0 ? "你胜" : (dWinner == 1 ? "电脑1胜" : "电脑2胜"));
  drawStatus("斗地主", sub.c_str());

  // players
  for (int p = 0; p < 3; p++) {
    int x = 20 + p * 130;
    drawText(x, 32, p == 0 ? "你" : (p == 1 ? "电脑1" : "电脑2"), 1, INK);
    drawText(x, 50, String(dN[p]) + "张", 1, INK);
  }
  if (dLastN > 0) {
    drawText(20, 78, "上一手：", 1, INK);
    int x = 90;
    for (int i = 0; i < dLastN && i < 16; i++) {
      drawRect(x, 74, 14, 22, INK);
      drawText(x + 2, 76, dRankStr(dLast[i].r), 1, INK);
      x += 16;
    }
  }
  if (dMsg.length()) drawText(20, 108, dMsg, 1, INK);

  // hand
  int n = dN[0];
  int startX = (W - n * 18) / 2;
  if (startX < 8) startX = 8;
  for (int i = 0; i < n; i++) {
    int x = startX + i * 18;
    dDrawCard(x, 220, dHand[0][i], dSel[i] != 0);
  }
  if (dHuman[dTurn] && dPhase == 1) {
    int x = startX + dCur * 18;
    drawRect(x - 1, 215, 20, 30, INK);
  }
  hline(12, 270, 376, INK);
  if (dPhase == 0) drawText(14, 276, "OK 叫地主   ↓ 不叫   Back 返回", 1, INK);
  else if (dPhase == 2) drawText(14, 276, "OK 再来一局   Back 返回", 1, INK);
  else drawText(14, 276, "←→选 ↑选中 OK出牌 ↓不出 Back返回", 1, INK);
}

// ============================ dispatcher ============================
static void gamesKey(const String& k) {
  if (app == App::GAMES) {
    if (k == "up") gameMenuSel = (gameMenuSel + 4) % 5;
    else if (k == "down") gameMenuSel = (gameMenuSel + 1) % 5;
    else if (k == "ok") {
      switch (gameMenuSel) {
        case 0: gReset(true); app = App::GOMOKU; break;
        case 1: gReset(false); app = App::GOMOKU; break;
        case 2: dReset(true); app = App::DDZ; break;
        case 3: dReset(false); app = App::DDZ; break;
        default: app = App::HOME; break;
      }
    } else if (k == "back" || k == "home") app = App::HOME;
  } else if (app == App::GOMOKU) {
    gKey(k);
  } else if (app == App::DDZ) {
    dKey(k);
  }
}
