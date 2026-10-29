#pragma once
#include <Arduino.h>
#include "panel42.h"

namespace ui {
extern uint8_t fb[panel42::IMGSZ];

void begin();
void tick();

panel42::Timings key(const String& k);
panel42::Timings setApp(const String& name);
panel42::Timings setNote(const String& text);
void setImage(const uint8_t* img, bool alreadyOnScreen);
void onClear();

const uint8_t* screen();
String stateJson();
String noteText();
const char* appName();
} // namespace ui
