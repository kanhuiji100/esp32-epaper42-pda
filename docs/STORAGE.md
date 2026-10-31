# Storage

The firmware has two storage areas:

| Path | Backing | Size | Purpose |
|---|---|---:|---|
| `/int` | internal flash partition `spiffs` | **384 KB** | small config/recovery files |
| `/ext` | external **W25Q128** (128 Mbit / 16 MB) on HSPI | 16 MB | images, text, fonts, logs, packages |

`/int` is always present. `/ext` is mounted only when the external flash answers
the JEDEC ID probe; if it is missing the firmware logs a warning and keeps
running.

## Partition table

The project uses `firmware/epaper42_web/partitions.csv` (no OTA):

```text
nvs       20 KB
otadata    8 KB   (unused, keeps the standard upload recipe happy)
app0     3.5 MB
spiffs   384 KB
coredump  64 KB
```

Build with the larger app size:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-property upload.maximum_size=3670016 firmware/epaper42_web
```

`flash.sh` already passes this property.

## HTTP API

| Method | Path | Result |
|---|---|---|
| GET | `/api/storage` | `{int:{ready,total,used}, ext:{ready,id,size}}` |
| GET | `/api/storage/list?path=/int` | JSON directory listing |
| GET | `/api/storage/file?path=/int/foo.txt` | raw file bytes |
| POST | `/api/storage/file?path=/int/foo.txt` | body is written to the file (≤ 4 MB) |
| DELETE | `/api/storage/file?path=/int/foo.txt` | remove the file |

Paths must start with `/int` or `/ext`; `..` is rejected.

## Serial console

```text
> storage
{"ok":1,"int":{"ready":1,"total":393216,"used":8192},"ext":{"ready":0,"id":"","size":0}}
> ls /int
{"ok":1,"path":"/int","files":[]}
> write /int/hello.txt hello
write: /int/hello.txt (5 bytes)
> cat /int/hello.txt
hello
> rm /int/hello.txt
```

## External flash wiring

W25Q128 on the 19-pin header, using HSPI (separate from the panel VSPI bus):

| Flash pin | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | IO22 |
| CLK | IO18 |
| DI/MOSI | IO23 |
| DO/MISO | IO19 |
| WP# / HOLD# | 3V3 |

Add 100 nF + 10 µF at the chip, a 10k pull-up on CS, and 22–33 Ω series resistors
on CLK/MOSI/CS. Start at 20 MHz.

The firmware probes JEDEC ID `EF 40 18` before initialising the IDF SPI flash
driver, registers the chip as an external partition and mounts LittleFS on
`/ext`. If the chip is absent, `/ext` is reported as `ready:0`.

## RAM notes

Resource access is streamed: file reads use small buffers and the caller's
buffer (for example the existing 15 KB image framebuffer). Do not load whole
large files into RAM. LittleFS/VFS and SPI driver buffers add only a few KB.
