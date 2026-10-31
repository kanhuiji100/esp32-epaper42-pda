#pragma once
#include "storage.h"
#include <LittleFS.h>
#include <SPI.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_littlefs.h"
#include "driver/spi_common.h"
#include "hal/spi_flash_types.h"

namespace storage {

namespace {
bool s_int = false;
bool s_ext = false;
String s_extId = "";
uint32_t s_extSize = 0;
esp_flash_t* s_extChip = nullptr;

String jsonEscape(const String& in) {
  String o;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

bool probeExt(uint8_t id[3]) {
  SPIClass hspi(HSPI);
  hspi.begin(18, 19, 23, 22);
  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH);
  hspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(22, LOW);
  hspi.transfer(0x9F);                 // JEDEC ID
  id[0] = hspi.transfer(0);
  id[1] = hspi.transfer(0);
  id[2] = hspi.transfer(0);
  digitalWrite(22, HIGH);
  hspi.endTransaction();
  hspi.end();
  return id[0] == 0xEF;                // Winbond W25Qxx
}

bool mountExt() {
  uint8_t id[3] = {0, 0, 0};
  if (!probeExt(id)) {
    Serial.println("storage: external SPI flash not detected");
    return false;
  }
  char idbuf[8];
  snprintf(idbuf, sizeof(idbuf), "%02X%02X%02X", id[0], id[1], id[2]);
  s_extId = idbuf;
  Serial.printf("storage: external flash id=%s\n", idbuf);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = 23;
  bus.miso_io_num = 19;
  bus.sclk_io_num = 18;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 4096;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
    Serial.println("storage: HSPI init failed");
    return false;
  }
  esp_flash_spi_device_config_t dev = {};
  dev.host_id = SPI2_HOST;
  dev.cs_io_num = 22;
  dev.io_mode = SPI_FLASH_FASTRD;
  dev.speed = ESP_FLASH_20MHZ;
  dev.input_delay_ns = 0;
  dev.cs_id = 0;
  if (spi_bus_add_flash_device(&s_extChip, &dev) != ESP_OK || !s_extChip) {
    Serial.println("storage: add flash device failed");
    return false;
  }
  if (esp_flash_init(s_extChip) != ESP_OK) {
    Serial.println("storage: esp_flash_init failed");
    s_extChip = nullptr;
    return false;
  }
  s_extSize = s_extChip->size;
  const esp_partition_t* part = nullptr;
  if (esp_partition_register_external(s_extChip, 0, s_extSize, "ext",
                                      ESP_PARTITION_TYPE_DATA,
                                      ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                      &part) != ESP_OK) {
    Serial.println("storage: register external partition failed");
    return false;
  }
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = "/ext";
  conf.partition_label = "ext";
  conf.format_if_mount_failed = 1;
  conf.grow_on_mount = 1;
  if (esp_vfs_littlefs_register(&conf) != ESP_OK) {
    Serial.println("storage: LittleFS mount on /ext failed");
    return false;
  }
  Serial.printf("storage: /ext mounted, %u bytes\n", (unsigned)s_extSize);
  return true;
}
}  // namespace

void begin() {
  s_int = LittleFS.begin(true, "/int", 10, "spiffs");
  if (!s_int) {
    Serial.println("storage: internal LittleFS mount failed, formatting...");
    LittleFS.format();
    s_int = LittleFS.begin(false, "/int", 10, "spiffs");
  }
  if (s_int) {
    Serial.printf("storage: /int mounted, %u/%u bytes used\n",
                  (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  } else {
    Serial.println("storage: /int unavailable");
  }
  s_ext = mountExt();
}

bool intReady() { return s_int; }
bool extReady() { return s_ext; }

bool pathOk(const String& path) {
  if (path.length() == 0 || path.length() > 160) return false;
  if (!path.startsWith("/int") && !path.startsWith("/ext")) return false;
  if (path.indexOf("..") >= 0) return false;
  return true;
}

String infoJson() {
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"ok\":1,\"int\":{\"ready\":%d,\"total\":%u,\"used\":%u},"
           "\"ext\":{\"ready\":%d,\"id\":\"%s\",\"size\":%u}}",
           s_int ? 1 : 0,
           s_int ? (unsigned)LittleFS.totalBytes() : 0u,
           s_int ? (unsigned)LittleFS.usedBytes() : 0u,
           s_ext ? 1 : 0, s_extId.c_str(), (unsigned)s_extSize);
  return String(buf);
}

String summary() {
  char buf[96];
  snprintf(buf, sizeof(buf), "int %u/%uK ext %s",
           s_int ? (unsigned)(LittleFS.usedBytes() / 1024) : 0u,
           s_int ? (unsigned)(LittleFS.totalBytes() / 1024) : 0u,
           s_ext ? "ready" : "off");
  return String(buf);
}

String listJson(const String& path) {
  if (!pathOk(path)) return "{\"ok\":0,\"err\":\"bad path\"}";
  DIR* d = opendir(path.c_str());
  if (!d) return "{\"ok\":0,\"err\":\"open failed\"}";
  String out = "{\"ok\":1,\"path\":\"" + jsonEscape(path) + "\",\"files\":[";
  bool first = true;
  struct dirent* e;
  while ((e = readdir(d)) != nullptr) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
    String full = path;
    if (!full.endsWith("/")) full += "/";
    full += e->d_name;
    struct stat st;
    size_t sz = 0;
    bool dir = false;
    if (stat(full.c_str(), &st) == 0) {
      sz = (size_t)st.st_size;
      dir = S_ISDIR(st.st_mode);
    }
    if (!first) out += ",";
    first = false;
    out += "{\"name\":\"" + jsonEscape(String(e->d_name)) + "\",\"dir\":";
    out += dir ? "1" : "0";
    out += ",\"size\":" + String((unsigned)sz) + "}";
  }
  closedir(d);
  out += "]}";
  return out;
}

size_t fileSize(const String& path) {
  if (!pathOk(path)) return 0;
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return 0;
  return (size_t)st.st_size;
}

bool readText(const String& path, String& out, size_t maxLen) {
  if (!pathOk(path)) return false;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out = "";
  out.reserve(maxLen < 2048 ? maxLen : 2048);
  uint8_t buf[256];
  size_t total = 0;
  while (total < maxLen) {
    size_t want = maxLen - total;
    if (want > sizeof(buf)) want = sizeof(buf);
    size_t n = fread(buf, 1, want, f);
    if (n == 0) break;
    out.concat((const char*)buf, n);
    total += n;
  }
  fclose(f);
  return true;
}

bool writeFile(const String& path, const uint8_t* data, size_t len) {
  if (!pathOk(path)) return false;
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return false;
  size_t n = fwrite(data, 1, len, f);
  fclose(f);
  return n == len;
}

bool removeFile(const String& path) {
  if (!pathOk(path)) return false;
  return remove(path.c_str()) == 0;
}

}  // namespace storage
