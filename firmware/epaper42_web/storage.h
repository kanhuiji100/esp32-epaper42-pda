#pragma once
#include <Arduino.h>

// Resource storage: internal LittleFS at /int and (when fitted) W25Q128 at /ext.
namespace storage {
void begin();
bool intReady();
bool extReady();
String infoJson();
String summary();
String listJson(const String& path);
bool readText(const String& path, String& out, size_t maxLen = 4096);
bool writeFile(const String& path, const uint8_t* data, size_t len);
bool removeFile(const String& path);
size_t fileSize(const String& path);
bool pathOk(const String& path);
}  // namespace storage
