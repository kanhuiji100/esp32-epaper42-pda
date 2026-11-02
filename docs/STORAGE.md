# Storage

The firmware has two storage areas:

| Path | Backing | Size | Purpose |
|---|---|---:|---|
| `/int` | internal flash partition `spiffs` | **384 KB** | small config/recovery files |
| `/ext` | external **W25Q128** (128 Mbit / 16 MB) on HSPI | 16 MB | images, text, fonts, logs, packages |

`/int` is always present. `/ext` is mounted only when the external flash answers
the JEDEC ID probe; if it is missing the firmware logs a warning and keeps
running.

## Wiring (verified on hardware)

| W25Qxx module | ESP32 | Board header |
|---|---|---|
| VCC | 3V3 | J3 pin 1 |
| GND | GND | J3 pin 14 / J4 pin 1 |
| CLK | **IO18** | J4 pin 9 (`P18`) |
| DO / MISO | **IO17** | J4 pin 11 (`P17`) |
| DI / MOSI | **IO23** | J4 pin 2 (`P23`) |
| CS | **IO32** | J3 pin 7 (`P32`) |

`WP#`/`HOLD#` (if broken out) to 3V3. IO19 is the "official" MISO but it exists
on the board as J4 pin 8 with **no silkscreen**, so IO17 is used instead.

The pin assignment lives in NVS (namespace `ext`) and can be changed at runtime:

```text
> flash
flash: clk=18 miso=17 mosi=23 cs=32 -> JEDEC EF 40 18
{"ok":1,"int":{...},"ext":{"ready":1,"id":"EF4018","size":16777216,"total":16777216,"used":32768,"free":16744448}}
> flash pins 18 17 23 32     # save a different assignment (reboot to apply)
> flash probe                # read the JEDEC ID with the current pins
```

`EF 40 18` = Winbond W25Q128 (16 MB). A freshly formatted `/ext` reports
`used` ≈ 8 KB (LittleFS metadata).

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
| Method | Path | Result |
|---|---|---|
| GET | `/api/storage` | `{int:{ready,total,used}, ext:{ready,id,size,total,used,free}}` |
| GET | `/api/storage/list?path=/ext` | JSON directory listing (`dir:1` for folders) |
| GET | `/api/storage/file?path=/ext/foo.txt` | raw file bytes |
| POST | `/api/storage/file?path=/ext/foo.txt` | body is written to the file (≤ 4 MB); missing parent directories are created |
| POST | `/api/storage/mkdir?path=/ext/books` | create a directory |
| DELETE | `/api/storage/file?path=/ext/foo.txt` | remove the file |

Paths must start with `/int` or `/ext`; `..` is rejected. Sub-directories are
supported and are created automatically on upload (`/ext/media/pics/a.bin` makes
both levels).

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
> mkdir /ext/books
mkdir: ok
```

## Measured performance

On this board with the W25Q128 at 20 MHz and Wi-Fi at ~-40 dBm:

| Operation | Throughput | 1 MB |
|---|---:|---:|
| Upload over HTTP (`POST /api/storage/file`) | ~57 KB/s | ~18 s |
| Download over HTTP (`GET /api/storage/file`) | ~380 KB/s | ~2.8 s |

The upload path is limited by the Wi-Fi receive path, not by the flash. A 15 KB
image uploads in well under a second. Verified with `md5sum` round-trips and
across a reboot (files, directories and contents all persist).

## External flash wiring

W25Q128 on the 19-pin header, using HSPI (separate from the panel VSPI bus):

| Flash pin | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | IO32 |
| CLK | IO18 |
| DI/MOSI | IO23 |
| DO/MISO | IO17 |
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
