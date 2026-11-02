#pragma once
// VK36N16I 4x4 touch keypad over I2C (emakefun Matrix keypad Module v3.x).
// Protocol: address 0x65, read 2 bytes -> uint16 bitmask of pressed keys.
//
//   bit:  0   1   2   3  | 4   5   6   7  | 8   9  10  11  | 12  13  14  15
//   key:  1   4   7   *  | 2   5   8   0  | 3   6   9   #  |  A   B   C   D
//
// No documentation ships with the module, so the pin pair is auto-detected and
// can be overridden from the serial console (`kbd pins <sda> <scl>`).
#include <Wire.h>
#include <Preferences.h>
#include <Arduino.h>

namespace keypad {

static const uint8_t ADDR = 0x65;
static int sdaPin = -1, sclPin = -1;
static bool ready = false;
static uint16_t lastMask = 0;
static uint32_t lastRepeatMs = 0;
static uint8_t heldKey = 0;          // bit index currently repeating
static Preferences kp;

// Candidate pins: skip the e-paper pins (2,13,14,15,25,26,27,33), the reserved
// external-flash pins (18,19,22,23), the input-only pins (34,35) and the
// strapping pin 12.
static const int CAND[] = {21, 22, 4, 5, 16, 17};
static const int CAND_N = (int)(sizeof(CAND) / sizeof(CAND[0]));

static inline void enablePulls(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
}

static bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Returns the address found (or 0). Tries 0x65 first, then scans 0x08..0x77.
static uint8_t probeBus() {
  if (probeAddress(ADDR)) return ADDR;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (a == ADDR) continue;
    if (probeAddress(a)) return a;
  }
  return 0;
}

static bool readMask(uint16_t& out) {
  if (!ready) return false;
  if (Wire.requestFrom((int)ADDR, 2) != 2) return false;
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  out = (uint16_t)lo | ((uint16_t)hi << 8);
  return true;
}

static bool usePins(int sda, int scl) {
  Wire.end();
  enablePulls(sda, scl);
  if (!Wire.begin(sda, scl, 100000)) { ready = false; return false; }
  delay(5);
  if (!probeBus()) { ready = false; return false; }
  sdaPin = sda; sclPin = scl; ready = true;
  Serial.printf("kbd: VK36N16I at 0x%02X on SDA=%d SCL=%d\r\n", ADDR, sda, scl);
  return true;
}

// Auto-detect the wiring across every candidate SDA/SCL pair.
static bool scan() {
  Serial.println("kbd: scanning candidate pin pairs...");
  for (int i = 0; i < CAND_N; i++) {
    for (int j = 0; j < CAND_N; j++) {
      if (i == j) continue;
      int sda = CAND[i], scl = CAND[j];
      Wire.end();
      enablePulls(sda, scl);
      if (!Wire.begin(sda, scl, 100000)) continue;
      delay(3);
      uint8_t found = probeBus();
      if (found) {
        if (found != ADDR) {
          Serial.printf("kbd: device 0x%02X on SDA=%d SCL=%d (not 0x65)\r\n", found, sda, scl);
          continue;
        }
        sdaPin = sda; sclPin = scl; ready = true;
        uint16_t m = 0; readMask(m);
        Serial.printf("kbd: FOUND 0x%02X on SDA=%d SCL=%d mask=0x%04X\r\n", ADDR, sda, scl, m);
        return true;
      }
    }
  }
  ready = false;
  Serial.println("kbd: no 0x65 device found on candidates (检查接线/供电)");
  return false;
}

static void savePins() {
  if (sdaPin < 0 || sclPin < 0) return;
  kp.begin("kbd", false);
  kp.putChar("sda", (int8_t)sdaPin);
  kp.putChar("scl", (int8_t)sclPin);
  kp.end();
}

static bool loadPins() {
  kp.begin("kbd", true);
  int sda = kp.getChar("sda", -1);
  int scl = kp.getChar("scl", -1);
  kp.end();
  if (sda < 0 || scl < 0) return false;
  return usePins(sda, scl);
}

static bool begin() {
  if (loadPins()) return true;
  return scan();
}

static const char* keyName(uint8_t bit) {
  static const char* names[16] = {"1","4","7","*","2","5","8","0",
                                  "3","6","9","#","A","B","C","D"};
  return bit < 16 ? names[bit] : "?";
}

// Map the 16 keys to UI actions. Layout (physical):
//   * 7 4 1      numbers form a numeric keypad on the right 2 columns,
//   0 8 5 2      so 2/8/4/6 work as up/down/left/right.
//   # 9 6 3      A=OK  B=Back  C=Home  D=Refresh
//   D C B A
static String mapKey(uint8_t bit) {
  switch (bit) {
    case 4:  return "up";      // '2'
    case 6:  return "down";    // '8'
    case 5:  return "ok";      // '5'
    case 1:  return "left";    // '4'
    case 9:  return "right";   // '6'
    case 12: return "ok";      // 'A'
    case 13: return "back";    // 'B'
    case 14: return "home";    // 'C'
    case 15: return "refresh"; // 'D' -> full refresh (clear ghosting)
    default: return "";
  }
}

static bool isRepeatable(uint8_t bit) {
  return bit == 4 || bit == 6 || bit == 1 || bit == 9;   // 2/8/4/6 directions
}

// Returns the next UI key event ("" if none). Call frequently from loop().
static String poll() {
  if (!ready) return "";
  uint16_t m = 0;
  if (!readMask(m)) { return ""; }
  uint32_t now = millis();

  if (m != lastMask) {
    uint16_t pressed = (uint16_t)(m & ~lastMask);
    lastMask = m;
    for (uint8_t b = 0; b < 16; b++) {
      if (pressed & (1u << b)) {
        lastRepeatMs = now;
        heldKey = b;
        String k = mapKey(b);
        if (k.length()) return k;
      }
    }
    return "";
  }

  // Auto-repeat for a held direction key.
  if (heldKey < 16 && (m & (1u << heldKey)) && isRepeatable(heldKey)) {
    if (now - lastRepeatMs > 450) {
      lastRepeatMs = now;
      return mapKey(heldKey);
    }
  }
  return "";
}

static void status() {
  if (!ready) { Serial.println("kbd: not ready (run: kbd scan)"); return; }
  uint16_t m = 0; readMask(m);
  Serial.printf("kbd: ok 0x%02X SDA=%d SCL=%d mask=0x%04X\r\n", ADDR, sdaPin, sclPin, m);
}

} // namespace keypad
