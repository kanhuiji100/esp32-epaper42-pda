#pragma once
// Electronic pet.  Included inside ui_impl.h's anonymous namespace.
#include "pet_sprites.h"

enum PetView { PV_ADOPT, PV_MAIN, PV_DEAD };
static PetView pvView = PV_ADOPT;
static Preferences petPrefs;
static bool petLoaded = false;
static bool petAdopted = false;
static uint8_t petSpecies = 0;        // 0 cat, 1 fox
static String petName = "咪咪";
static float petHunger = 80, petHappy = 80, petEnergy = 80, petClean = 80, petHealth = 100;
static uint32_t petAgeMin = 0;
static int petCoins = 10;
static uint8_t petPoop = 0;
static bool petAlive = true, petSleeping = false;
static uint32_t petLastEpoch = 0;
static uint32_t petPoopTimer = 0;
static int petAction = 0;             // 0 feed .. 5 work
static String petMsg;
static uint32_t petMsgUntil = 0, petPetCd = 0;
static uint32_t petAccum = 0, petLastMs = 0, petLastRefresh = 0, petLastFull = 0, petLastAnim = 0, petSaveAt = 0;

static const char* PET_ACTIONS[] = {"喂食", "玩耍", "睡觉", "清洁", "抚摸", "打工"};

static float petClamp(float v, float lo = 0, float hi = 100) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static const char* petStage() {
  if (petAgeMin < 1440) return "幼年";
  if (petAgeMin < 10080) return "成年";
  return "老年";
}

static String petAgeStr() {
  uint32_t days = petAgeMin / 1440, hours = (petAgeMin % 1440) / 60;
  if (days > 0) return String(days) + "天" + String(hours) + "小时";
  return String(hours) + "小时";
}

static void petSave() {
  if (!petAdopted) return;
  petPrefs.begin("pet", false);
  petPrefs.putUChar("sp", petSpecies);
  petPrefs.putString("name", petName);
  petPrefs.putFloat("hun", petHunger);
  petPrefs.putFloat("hap", petHappy);
  petPrefs.putFloat("ene", petEnergy);
  petPrefs.putFloat("cln", petClean);
  petPrefs.putFloat("hp", petHealth);
  petPrefs.putUInt("age", petAgeMin);
  petPrefs.putInt("coins", petCoins);
  petPrefs.putUChar("poop", petPoop);
  petPrefs.putUChar("alive", petAlive ? 1 : 0);
  petPrefs.putUChar("sleep", petSleeping ? 1 : 0);
  petPrefs.putUInt("epoch", (uint32_t)time(nullptr));
  petPrefs.end();
}

static void petDecay(float minutes, bool offline) {
  if (!petAlive || minutes <= 0) return;
  float m = minutes;
  if (petSleeping) {
    petEnergy += 2.2f * m;
    petHunger -= 0.45f * m;
    petHappy -= 0.15f * m;
  } else {
    petHunger -= 0.9f * m;
    petHappy -= 0.6f * m;
    petEnergy -= 0.6f * m;
    petClean -= 0.4f * m;
  }
  if (petPoop > 0) {
    petHappy -= 0.35f * m * petPoop;
    petClean -= 0.25f * m * petPoop;
  }
  petHunger = petClamp(petHunger);
  petHappy = petClamp(petHappy);
  petEnergy = petClamp(petEnergy);
  petClean = petClamp(petClean);
  if (!petSleeping) {
    petPoopTimer += (uint32_t)(m * 10);
    if (petPoopTimer >= 1500) { petPoopTimer = 0; if (petPoop < 4) petPoop++; }
  }
  if (petHunger < 20 || petClean < 20 || petHappy < 20) petHealth -= 1.2f * m;
  else if (petHealth < 100) petHealth += 0.3f * m;
  if (petEnergy < 10) petHealth -= 0.4f * m;
  petHealth = petClamp(petHealth);
  if (petHealth <= 0) { petHealth = 0; petAlive = false; }
  if (petAgeMin < 1000000) petAgeMin += (uint32_t)m;
}

static void petDecayOffline(float minutes) {
  if (!petAlive || minutes <= 0) return;
  float m = minutes > 1440 ? 1440 : minutes;   // cap at one day
  petHunger -= 0.25f * m;
  petHappy -= 0.15f * m;
  petEnergy += 0.8f * m;
  petClean -= 0.12f * m;
  petHunger = petClamp(petHunger); petHappy = petClamp(petHappy);
  petEnergy = petClamp(petEnergy); petClean = petClamp(petClean);
  if (petHunger < 15) petHealth -= 0.2f * m;
  petHealth = petClamp(petHealth);
  if (petHealth < 10) petHealth = 10;          // no offline death
  if (petAgeMin < 1000000) petAgeMin += (uint32_t)minutes;
}

static void petLoad() {
  if (petLoaded) return;
  petLoaded = true;
  petPrefs.begin("pet", true);
  petAdopted = petPrefs.getBool("adopted", false);
  if (!petAdopted) { petPrefs.end(); return; }
  petSpecies = petPrefs.getUChar("sp", 0);
  petName = petPrefs.getString("name", petSpecies ? "小狐" : "咪咪");
  petHunger = petPrefs.getFloat("hun", 80);
  petHappy = petPrefs.getFloat("hap", 80);
  petEnergy = petPrefs.getFloat("ene", 80);
  petClean = petPrefs.getFloat("cln", 80);
  petHealth = petPrefs.getFloat("hp", 100);
  petAgeMin = petPrefs.getUInt("age", 0);
  petCoins = petPrefs.getInt("coins", 10);
  petPoop = petPrefs.getUChar("poop", 0);
  petAlive = petPrefs.getUChar("alive", 1) != 0;
  petSleeping = petPrefs.getUChar("sleep", 0) != 0;
  petLastEpoch = petPrefs.getUInt("epoch", 0);
  petPrefs.end();
  time_t now = time(nullptr);
  if (now > 1700000000 && petLastEpoch > 0 && (uint32_t)now > petLastEpoch) {
    uint32_t mins = ((uint32_t)now - petLastEpoch) / 60;
    if (mins > 10080) mins = 10080;
    petDecayOffline((float)mins);
  }
  pvView = petAlive ? PV_MAIN : PV_DEAD;
}

static void petReset(uint8_t species) {
  petSpecies = species;
  petName = species ? "小狐" : "咪咪";
  petHunger = petHappy = petEnergy = petClean = 80;
  petHealth = 100;
  petAgeMin = 0;
  petCoins = 10;
  petPoop = 0;
  petAlive = true;
  petSleeping = false;
  petPoopTimer = 0;
  petAction = 0;
  petMsg = "";
  petAdopted = true;
  petLoaded = true;
  pvView = PV_MAIN;
  petPrefs.begin("pet", false);
  petPrefs.putBool("adopted", true);
  petPrefs.end();
  petSave();
}

static void petSay(const String& m) { petMsg = m; petMsgUntil = millis() + 3500; }

static void petDoAction(int a) {
  if (!petAdopted || !petAlive) return;
  if (petSleeping && a != 2) { petSay("睡着呢…"); return; }
  switch (a) {
    case 0:
      if (petHunger > 92) { petSay("吃不下了"); break; }
      if (petCoins < 2) { petSay("金币不够"); break; }
      petCoins -= 2; petHunger += 28; petHappy += 4; petClean -= 4;
      petSay("好吃！");
      break;
    case 1:
      if (petEnergy < 15) { petSay("太累了…"); break; }
      petHappy += 20; petEnergy -= 14; petHunger -= 6;
      petSay("好开心！");
      break;
    case 2:
      petSleeping = !petSleeping;
      petSay(petSleeping ? "晚安…" : "起床啦");
      break;
    case 3:
      if (petClean > 95 && petPoop == 0) { petSay("很干净"); break; }
      petClean += 30; if (petPoop > 0) petPoop--;
      petSay("干净多了");
      break;
    case 4:
      if (millis() < petPetCd) { petSay("再摸摸…"); break; }
      petHappy += 8; petPetCd = millis() + 20000;
      petSay("呼噜呼噜");
      break;
    case 5:
      if (petEnergy < 25) { petSay("太累了…"); break; }
      petCoins += 5; petEnergy -= 18; petHappy -= 10;
      petSay("赚了 5 金币");
      break;
  }
  petHunger = petClamp(petHunger); petHappy = petClamp(petHappy);
  petEnergy = petClamp(petEnergy); petClean = petClamp(petClean);
  petSave();
}

static void petBlit(const uint8_t data[PET_H][PET_W / 8], int x, int y, int scale, uint8_t ink) {
  for (int row = 0; row < PET_H; row++) {
    for (int col = 0; col < PET_W; col++) {
      if (data[row][col >> 3] & (1 << (7 - (col & 7)))) {
        fillRect(x + col * scale, y + row * scale, scale, scale, ink);
      }
    }
  }
}

static void drawPetSprite(int x, int y, int scale, uint8_t species, uint8_t ink) {
  if (species == 0) petBlit(PET_CAT_0, x, y, scale, ink);
  else petBlit(PET_FOX_0, x, y, scale, ink);
}

static void petBar(int x, int y, int w, int h, float v) {
  drawRect(x, y, w, h, INK);
  int fill = (int)((w - 4) * petClamp(v) / 100.0f + 0.5f);
  if (fill > 0) fillRect(x + 2, y + 2, fill, h - 4, INK);
}

static void petPoopIcon(int x, int y) {
  fillRect(x + 3, y + 4, 6, 3, INK);
  fillRect(x + 2, y + 7, 8, 3, INK);
  fillRect(x + 1, y + 10, 10, 3, INK);
}

static void drawPetAdopt() {
  drawStatus("电子宠物", "领养");
  drawText(20, 22, "选择伙伴", 2, INK);
  drawPetSprite(104, 54, 1, petSpecies, INK);
  drawRect(100, 50, 200, 200, INK);
  String nm = petSpecies ? "狐娘" : "猫娘";
  int tw = textWidth(nm, 1);
  drawText((W - tw) / 2, 252, nm, 1, INK);
  hline(12, 270, 376, INK);
  drawText(14, 276, "←→ 切换   OK 领养   Back 返回", 1, INK);
}

static void drawPetDead() {
  drawStatus("电子宠物", "休息");
  drawText(20, 30, petName + " 离开了…", 2, INK);
  // small tombstone
  fillRect(170, 100, 60, 80, PAPER);
  drawRect(170, 100, 60, 80, INK);
  drawLine(200, 100, 200, 60, INK);
  drawLine(185, 75, 215, 75, INK);
  drawText(150, 200, String("享年 ") + petAgeStr(), 1, INK);
  hline(12, 270, 376, INK);
  drawText(14, 276, "OK 重新领养   Back 返回", 1, INK);
}

static void drawPetMain() {
  drawStatus("电子宠物", (petName + "·" + petStage()).c_str());
  drawPetSprite(4, 24, 1, petSpecies, INK);
  if (petSleeping) drawText(140, 28, "Zzz", 2, INK);
  if (!petAlive) { drawPetDead(); return; }
  for (int i = 0; i < petPoop; i++) petPoopIcon(8 + i * 18, 218);

  const int x = 204;
  drawText(x, 30, petName, 2, INK);
  drawText(x, 64, String(petSpecies ? "狐娘" : "猫娘") + " · " + petStage(), 1, INK);
  drawText(x, 82, String("年龄 ") + petAgeStr(), 1, INK);
  drawText(x, 100, String("金币 ") + String(petCoins), 1, INK);

  drawText(x, 120, "健康", 1, INK); petBar(252, 120, 132, 12, petHealth);
  drawText(x, 140, "饱食", 1, INK); petBar(252, 140, 132, 10, petHunger);
  drawText(x, 158, "心情", 1, INK); petBar(252, 158, 132, 10, petHappy);
  drawText(x, 176, "精力", 1, INK); petBar(252, 176, 132, 10, petEnergy);
  drawText(x, 194, "清洁", 1, INK); petBar(252, 194, 132, 10, petClean);

  String msg = petMsg;
  if (millis() > petMsgUntil) {
    if (!petAlive) msg = "需要照顾";
    else if (petHunger < 25) msg = "肚子饿了…";
    else if (petClean < 25 || petPoop > 0) msg = "该清洁了";
    else if (petHappy < 25) msg = "有点孤单…";
    else if (petEnergy < 20) msg = "想睡觉了";
    else msg = "状态不错";
  }
  drawText(x, 214, newsTruncate(msg, 24), 1, INK);

  const int aw = 60, ah = 26, ay = 236;
  for (int i = 0; i < 6; i++) {
    int ax = 20 + i * aw;
    bool sel = (i == petAction);
    if (sel) fillRect(ax, ay, aw - 4, ah, INK);
    uint8_t ink = sel ? PAPER : INK;
    drawText(ax + 12, ay + 5, PET_ACTIONS[i], 1, ink);
  }
  hline(12, 270, 376, INK);
  drawText(14, 276, "←→ 选择   OK 执行   Back 返回", 1, INK);
}

static void drawPetApp() {
  fillRect(0, 0, W, H, PAPER);
  if (!petAdopted) { drawPetAdopt(); return; }
  if (!petAlive) { drawPetDead(); return; }
  drawPetMain();
}

static void petEnter() {
  petLoad();
  pvView = petAdopted ? (petAlive ? PV_MAIN : PV_DEAD) : PV_ADOPT;
}

static void petKey(const String& k) {
  petLoad();
  if (!petAdopted) {
    if (k == "left" || k == "right") petSpecies ^= 1;
    else if (k == "ok") petReset(petSpecies);
    else if (k == "back" || k == "home") app = App::HOME;
    return;
  }
  if (!petAlive) {
    if (k == "ok") petReset(petSpecies);
    else if (k == "back" || k == "home") app = App::HOME;
    return;
  }
  if (k == "left") petAction = (petAction + 5) % 6;
  else if (k == "right") petAction = (petAction + 1) % 6;
  else if (k == "ok") petDoAction(petAction);
  else if (k == "back" || k == "home") { petSave(); app = App::HOME; }
}

static void petPrintStatus() {
  petLoad();
  if (!petAdopted) { Serial.println("pet: not adopted (use: pet adopt cat|fox)"); return; }
  Serial.printf("pet: %s name=%s alive=%d sleep=%d stage=%s age=%umin coins=%d poop=%d\n",
                petSpecies ? "fox" : "cat", petName.c_str(), petAlive ? 1 : 0,
                petSleeping ? 1 : 0, petStage(), (unsigned)petAgeMin, petCoins, petPoop);
  Serial.printf("pet: hunger=%.0f happy=%.0f energy=%.0f clean=%.0f health=%.0f\n",
                petHunger, petHappy, petEnergy, petClean, petHealth);
}

static void petTick() {
  petLoad();
  uint32_t now = millis();
  if (petLastMs == 0) petLastMs = now;
  uint32_t dt = now - petLastMs;
  petLastMs = now;
  petAccum += dt;
  if (petAccum >= 1000) {
    float mins = petAccum / 60000.0f;
    petAccum = 0;
    if (petAdopted && petAlive) petDecay(mins, false);
  }
  if (petAdopted && app == App::PET && petAlive) {
    // Full refresh only: partial updates on this panel leave ghosting on the
    // sprite and the stat bars (see the ghosting report).  Update every 2 min.
    if (now - petLastRefresh > 300000) {
      petLastRefresh = now;
      petSave();
      renderAndCommit(true);   // refresh the stat bars; sprite stays static
    }
  }
}
