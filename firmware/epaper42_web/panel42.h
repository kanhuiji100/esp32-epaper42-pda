// Waveshare 4.2inch e-Paper (WFT0420CZ15, UC8176) fast driver for ESP32.
// Based on Waveshare EPD_4in2 (MIT) and GxEPD2 (MIT).
// 100Hz PLL (0x3A). Three update classes:
//   fast : GxEPD2 partial waveform, full screen, inverted data, ~0.58s
//   std  : full waveform, normal data, ~0.95s
//   hq   : original 50Hz waveform, normal data, ~4.1s
//   part : GxEPD2 partial waveform with 0x91/0x90 window, ~0.58s
#pragma once
#include <Arduino.h>
#include <SPI.h>

namespace panel42 {

constexpr int W = 400;
constexpr int H = 300;
constexpr int ROWB = W / 8;
constexpr int IMGSZ = ROWB * H;

constexpr uint8_t PLL_100 = 0x3A; // 100Hz
constexpr uint8_t PLL_50  = 0x3C; // 50Hz (original loader default)

extern uint8_t shadow[IMGSZ];
uint8_t shadow[IMGSZ];

struct Timings {
  uint32_t init = 0;
  uint32_t data = 0;
  uint32_t refresh = 0;
  uint32_t total = 0;
};

enum Mode { MODE_NONE, MODE_STD, MODE_HQ, MODE_PART };

static bool s_powered = false;
static Mode s_mode = MODE_NONE;
static uint8_t s_pll = 0;

static const int PIN_SCK = 13;
static const int PIN_MOSI = 14;
static const int PIN_CS = 15;
static const int PIN_BUSY = 25;
static const int PIN_RST = 26;
static const int PIN_DC = 27;
static const int PIN_CS_S = 2;
static const int PIN_PWR = 33;

// ---------- LUTs ----------
// Waveshare "fast" full LUT, 36 used bytes each
static const uint8_t FAST_V[] = {
  0x00,0x08,0x08,0x00,0x00,0x02, 0x00,0x0F,0x0F,0x00,0x00,0x01,
  0x00,0x08,0x08,0x00,0x00,0x02, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t FAST_WW[] = {
  0x50,0x08,0x08,0x00,0x00,0x02, 0x90,0x0F,0x0F,0x00,0x00,0x01,
  0xA0,0x08,0x08,0x00,0x00,0x02, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t FAST_BW[] = {
  0x50,0x08,0x08,0x00,0x00,0x02, 0x90,0x0F,0x0F,0x00,0x00,0x01,
  0xA0,0x08,0x08,0x00,0x00,0x02, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t FAST_WB[] = {
  0xA0,0x08,0x08,0x00,0x00,0x02, 0x90,0x0F,0x0F,0x00,0x00,0x01,
  0x50,0x08,0x08,0x00,0x00,0x02, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t FAST_BB[] = {
  0x20,0x08,0x08,0x00,0x00,0x02, 0x90,0x0F,0x0F,0x00,0x00,0x01,
  0x10,0x08,0x08,0x00,0x00,0x02, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};

// Original loader full LUT (50Hz, 44/42 bytes)
static const uint8_t OR_V[] = {
  0x00,0x17,0x00,0x00,0x00,0x02, 0x00,0x17,0x17,0x00,0x00,0x02,
  0x00,0x0A,0x01,0x00,0x00,0x01, 0x00,0x0E,0x0E,0x00,0x00,0x02,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00
};
static const uint8_t OR_WW[] = {
  0x40,0x17,0x00,0x00,0x00,0x02, 0x90,0x17,0x17,0x00,0x00,0x02,
  0x40,0x0A,0x01,0x00,0x00,0x01, 0xA0,0x0E,0x0E,0x00,0x00,0x02,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t OR_BW[] = {
  0x40,0x17,0x00,0x00,0x00,0x02, 0x90,0x17,0x17,0x00,0x00,0x02,
  0x40,0x0A,0x01,0x00,0x00,0x01, 0xA0,0x0E,0x0E,0x00,0x00,0x02,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t OR_WB[] = {
  0x80,0x17,0x00,0x00,0x00,0x02, 0x90,0x17,0x17,0x00,0x00,0x02,
  0x80,0x0A,0x01,0x00,0x00,0x01, 0x50,0x0E,0x0E,0x00,0x00,0x02,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t OR_BB[] = {
  0x80,0x17,0x00,0x00,0x00,0x02, 0x90,0x17,0x17,0x00,0x00,0x02,
  0x80,0x0A,0x01,0x00,0x00,0x01, 0x50,0x0E,0x0E,0x00,0x00,0x02,
  0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00
};

// GxEPD2 partial waveform: T1=25,T2=1,T3=4,T4=25 (from GxEPD2_420.cpp, MIT, J.-M. Zingg)
static const uint8_t PART_V[]  = { 0x00, 25, 1, 4, 25, 1, 0x00, 1, 0, 0, 0, 1 };
static const uint8_t PART_WW[] = { 0x18, 25, 1, 4, 25, 1, 0x00, 1, 0, 0, 0, 1 };
static const uint8_t PART_BW[] = { 0x5A, 25, 1, 4, 25, 1, 0x00, 1, 0, 0, 0, 1 };
static const uint8_t PART_WB[] = { 0xA5, 25, 1, 4, 25, 1, 0x00, 1, 0, 0, 0, 1 };
static const uint8_t PART_BB[] = { 0x24, 25, 1, 4, 25, 1, 0x00, 1, 0, 0, 0, 1 };

// ---------- low level ----------
static inline void csL() { digitalWrite(PIN_CS, LOW); }
static inline void csH() { digitalWrite(PIN_CS, HIGH); }
static inline void cmd(uint8_t c) { digitalWrite(PIN_DC, LOW);  csL(); SPI.transfer(c); csH(); }
static inline void dat(uint8_t d) { digitalWrite(PIN_DC, HIGH); csL(); SPI.transfer(d); csH(); }
static inline void datBuf(const uint8_t* p, size_t n) { digitalWrite(PIN_DC, HIGH); csL(); SPI.writeBytes(p, n); csH(); }

static uint32_t waitBusy(uint32_t timeoutMs = 8000) {
  uint32_t t0 = millis();
  while (digitalRead(PIN_BUSY) == 0) {   // BUSY is LOW while busy on this panel
    if (millis() - t0 > timeoutMs) break;
    delay(1);
  }
  return millis() - t0;
}

static void resetPanel() {
  digitalWrite(PIN_RST, HIGH); delay(20);
  digitalWrite(PIN_RST, LOW);  delay(20);
  digitalWrite(PIN_RST, HIGH); delay(20);
}

static void sendLut(uint8_t c, const uint8_t* p, size_t used, size_t total) {
  cmd(c); datBuf(p, used);
  for (size_t i = used; i < total; i++) dat(0);
}
static void loadFastLut() {
  sendLut(0x20, FAST_V,  sizeof(FAST_V),  44);
  sendLut(0x21, FAST_WW, sizeof(FAST_WW), 42);
  sendLut(0x22, FAST_BW, sizeof(FAST_BW), 42);
  sendLut(0x23, FAST_WB, sizeof(FAST_WB), 42);
  sendLut(0x24, FAST_BB, sizeof(FAST_BB), 42);
}
static void loadOrigLut() {
  sendLut(0x20, OR_V, 44, 44); sendLut(0x21, OR_WW, 42, 42);
  sendLut(0x22, OR_BW, 42, 42); sendLut(0x23, OR_WB, 42, 42);
  sendLut(0x24, OR_BB, 42, 42);
}
static void loadPartLut() {
  sendLut(0x20, PART_V,  sizeof(PART_V),  44);
  sendLut(0x21, PART_WW, sizeof(PART_WW), 42);
  sendLut(0x22, PART_BW, sizeof(PART_BW), 42);
  sendLut(0x23, PART_WB, sizeof(PART_WB), 42);
  sendLut(0x24, PART_BB, sizeof(PART_BB), 42);
}
static void setPll(uint8_t pll) {
  if (s_pll == pll) return;
  cmd(0x30); dat(pll);
  s_pll = pll;
}

static void setMode(Mode m) {
  if (m == s_mode) return;
  if (s_mode == MODE_PART && m != MODE_PART) cmd(0x92); // partial out
  if (m == MODE_PART) {
    cmd(0x50); dat(0x07);
    setPll(PLL_100);
    loadPartLut();
  } else if (m == MODE_STD) {
    cmd(0x50); dat(0x97);
    setPll(PLL_100);
    loadFastLut();
  } else if (m == MODE_HQ) {
    cmd(0x50); dat(0x97);
    setPll(PLL_50);
    loadOrigLut();
  }
  s_mode = m;
}

static void ensureMode(Mode m) {
  if (!s_powered) {
    cmd(0x04); waitBusy();
    s_powered = true;
    s_mode = MODE_NONE;
    s_pll = 0;
  }
  setMode(m);
}

static void doRefresh(uint32_t* ms = nullptr) {
  uint32_t t0 = millis();
  cmd(0x12);
  delay(10);
  waitBusy();
  if (ms) *ms = millis() - t0;
}

static void setWindow(int x0, int y0, int x1, int y1) {
  cmd(0x91); // partial in
  cmd(0x90); // window
  dat(x0 / 256); dat(x0 % 256);
  dat((x1 - 1) / 256); dat((x1 - 1) % 256);
  dat(y0 / 256); dat(y0 % 256);
  dat((y1 - 1) / 256); dat((y1 - 1) % 256);
  dat(0x28);
}

static const uint8_t WHITE_ROW[ROWB] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static const uint8_t BLACK_ROW[ROWB] = { 0 };

// row-wise inverted send, for the partial waveform which expects ~data
static void sendRowsInv(const uint8_t* src, int x, int y, int wb, int h) {
  uint8_t row[ROWB];
  cmd(0x10);
  for (int yy = 0; yy < h; yy++) {
    const uint8_t* p = src + (size_t)(y + yy) * ROWB + x / 8;
    for (int i = 0; i < wb; i++) row[i] = (uint8_t)~p[i];
    datBuf(row, wb);
  }
}
static void sendRowsInvNew(const uint8_t* src, int x, int y, int wb, int h) {
  uint8_t row[ROWB];
  cmd(0x13);
  for (int yy = 0; yy < h; yy++) {
    const uint8_t* p = src + (size_t)(y + yy) * ROWB + x / 8;
    for (int i = 0; i < wb; i++) row[i] = (uint8_t)~p[i];
    datBuf(row, wb);
  }
}

// ---------- public API ----------
inline void begin() {
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_BUSY, INPUT);
  pinMode(PIN_CS_S, OUTPUT);
  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_CS_S, HIGH);
  digitalWrite(PIN_PWR, HIGH);
  delay(50);
  SPI.begin(PIN_SCK, 36, PIN_MOSI, PIN_CS);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  memset(shadow, 0xFF, IMGSZ);
}

inline bool isPowered() { return s_powered; }
inline const char* modeName() {
  switch (s_mode) {
    case MODE_STD:  return "std";
    case MODE_HQ:   return "hq";
    case MODE_PART: return "part";
    default:        return "none";
  }
}

inline void initRegisters() {
  resetPanel();
  cmd(0x01); dat(0x03); dat(0x00); dat(0x2F); dat(0x2F); dat(0xFF);
  cmd(0x06); dat(0x17); dat(0x17); dat(0x17);
  cmd(0x04); waitBusy();          // power on
  cmd(0x00); dat(0xBF); dat(0x0B);
  cmd(0x30); dat(PLL_100); s_pll = PLL_100;
  cmd(0x61); dat(0x01); dat(0x90); dat(0x01); dat(0x2C);
  cmd(0x82); dat(0x12);
}

// Boot: init, clear to white with a full waveform (old=black -> new=white).
inline void bootClear(Timings* t = nullptr) {
  uint32_t t0 = millis();
  initRegisters();
  s_powered = true;
  s_mode = MODE_NONE;
  setMode(MODE_STD);
  uint32_t t1 = millis();

  cmd(0x10);
  for (int y = 0; y < H; y++) datBuf(BLACK_ROW, ROWB);
  cmd(0x13);
  for (int y = 0; y < H; y++) datBuf(WHITE_ROW, ROWB);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  memset(shadow, 0xFF, IMGSZ);
  if (t) { t->init = t1 - t0; t->data = t2 - t1; t->refresh = r; t->total = t3 - t0; }
}

// Standard full refresh: normal data, full waveform, ~0.95s.
inline void fullStd(const uint8_t* img, Timings& t) {
  uint32_t t0 = millis();
  ensureMode(MODE_STD);
  uint32_t t1 = millis();
  cmd(0x10); datBuf(shadow, IMGSZ);
  cmd(0x13); datBuf(img, IMGSZ);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  memcpy(shadow, img, IMGSZ);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// Fast full-screen refresh using the partial waveform (inverted data), ~0.58s.
inline void fullFast(const uint8_t* img, Timings& t) {
  uint32_t t0 = millis();
  ensureMode(MODE_PART);
  uint32_t t1 = millis();
  // no 0x91/0x90: GxEPD2 uses the full area with partial waveform for better image
  uint8_t row[ROWB];
  cmd(0x10);
  for (int y = 0; y < H; y++) {
    const uint8_t* p = shadow + (size_t)y * ROWB;
    for (int i = 0; i < ROWB; i++) row[i] = (uint8_t)~p[i];
    datBuf(row, ROWB);
  }
  cmd(0x13);
  for (int y = 0; y < H; y++) {
    const uint8_t* p = img + (size_t)y * ROWB;
    for (int i = 0; i < ROWB; i++) row[i] = (uint8_t)~p[i];
    datBuf(row, ROWB);
  }
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  memcpy(shadow, img, IMGSZ);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// HQ full refresh: original 50Hz waveform, ~4.1s.
inline void fullHq(const uint8_t* img, Timings& t) {
  uint32_t t0 = millis();
  ensureMode(MODE_HQ);
  uint32_t t1 = millis();
  cmd(0x10); datBuf(shadow, IMGSZ);
  cmd(0x13); datBuf(img, IMGSZ);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  memcpy(shadow, img, IMGSZ);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// Window partial update. x,w multiples of 8. img holds (w/8)*h bytes.
inline void partialWindow(int x, int y, int w, int h, const uint8_t* img, Timings& t) {
  const int wb = w / 8;
  uint32_t t0 = millis();
  ensureMode(MODE_PART);
  uint32_t t1 = millis();
  setWindow(x, y, x + w, y + h);
  sendRowsInv(shadow, x, y, wb, h);
  sendRowsInvNew(img, x, y, wb, h);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  cmd(0x92); // partial out
  uint32_t t3 = millis();
  for (int yy = 0; yy < h; yy++)
    memcpy(shadow + (size_t)(y + yy) * ROWB + x / 8, img + (size_t)yy * wb, wb);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// Full-refresh the current shadow with the full waveform (removes ghosting).
inline void refreshStd(Timings& t) {
  uint32_t t0 = millis();
  ensureMode(MODE_STD);
  uint32_t t1 = millis();
  cmd(0x10); datBuf(shadow, IMGSZ);
  cmd(0x13); datBuf(shadow, IMGSZ);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// Full-refresh the current shadow with the HQ (50 Hz) waveform.
inline void refreshHq(Timings& t) {
  fullHq(shadow, t);
}

// Clear to white with the full waveform.
inline void clear(Timings& t) {
  uint32_t t0 = millis();
  ensureMode(MODE_STD);
  uint32_t t1 = millis();
  cmd(0x10); datBuf(shadow, IMGSZ);
  cmd(0x13);
  for (int y = 0; y < H; y++) datBuf(WHITE_ROW, ROWB);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  uint32_t t3 = millis();
  memset(shadow, 0xFF, IMGSZ);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

// Partial window update taken directly from the full framebuffer (stride ROWB).
inline void partialFb(int x, int y, int w, int h, const uint8_t* fb, Timings& t) {
  const int wb = w / 8;
  uint32_t t0 = millis();
  ensureMode(MODE_PART);
  uint32_t t1 = millis();
  setWindow(x, y, x + w, y + h);
  sendRowsInv(shadow, x, y, wb, h);
  sendRowsInvNew(fb, x, y, wb, h);
  uint32_t t2 = millis();
  uint32_t r = 0; doRefresh(&r);
  cmd(0x92);
  uint32_t t3 = millis();
  for (int yy = 0; yy < h; yy++)
    memcpy(shadow + (size_t)(y + yy) * ROWB + x / 8,
           fb + (size_t)(y + yy) * ROWB + x / 8, wb);
  t.init = t1 - t0; t.data = t2 - t1; t.refresh = r; t.total = t3 - t0;
}

inline void powerOff() {
  if (!s_powered) return;
  if (s_mode == MODE_PART) cmd(0x92);
  cmd(0x02);
  waitBusy();
  s_powered = false;
  s_mode = MODE_NONE;
  s_pll = 0;
}

} // namespace panel42
