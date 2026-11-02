# Hardware

## Board

Waveshare **e-Paper ESP32 Driver Board**.

- ESP32-D0WDQ6, dual core, 240 MHz.
- 4 MB SPI flash, **no PSRAM**.
- USB-serial: CH343P (`/dev/ttyACM0` on Linux).
- 3.3 V logic and panel supply; boost converter for panel rails.
- USB serial console at **115200 8N1** for Wi-Fi provisioning and input simulation
  (see [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md)).
- Optional **W25Q128** (16 MB) on HSPI: **CLK IO18, MISO IO17, MOSI IO23,
  CS IO32**; WP#/HOLD# 3V3; 3.3 V + 100 nF/10 µF, 10k CS pull-up. Mounted at
  `/ext` (see [STORAGE.md](STORAGE.md)). MISO uses IO17 because IO19 exists on
  the board (J4 pin 8) but is **not silkscreened** — see the connector table
  below.
- **VK36N16I** 4×4 I2C keypad: SDA **IO21**, SCL **IO22**, V 3V3, G GND
  (see [KEYPAD.md](KEYPAD.md)).

## IO connectors

The board breaks the ESP32 out to two 1×19 headers, **J3** (top edge) and **J4**
(bottom edge). On the silkscreen only every other pin is labelled (the odd pins
of J3 and the even pins of J4); the rest are printed but easy to miss. Full
table from the schematic:

| J3 pin | net | | J4 pin | net |
|---:|---|---|---:|---|
| 1 | 3V3 | | 1 | GND |
| 2 | EN | | 2 | IO23 |
| 3 | SVP (IO36) | | 3 | IO22 |
| 4 | SVN (IO39) | | 4 | U0TX |
| 5 | IO34 | | 5 | U0RX |
| 6 | IO35 | | 6 | IO21 |
| 7 | **IO32** | | 7 | GND |
| 8 | IO33 | | 8 | **IO19** (unlabelled) |
| 9 | IO25 | | 9 | **IO18** |
| 10 | IO26 | | 10 | IO5 (unlabelled) |
| 11 | IO27 | | 11 | **IO17** |
| 12 | IO14 | | 12 | IO16 |
| 13 | IO12 | | 13 | IO4 |
| 14 | GND | | 14 | IO0 |
| 15 | IO13 | | 15 | IO2 |
| 16 | SD2 | | 16 | IO15 |
| 17 | SD3 | | 17 | SD1 |
| 18 | CMD | | 18 | SD0 |
| 19 | 5V | | 19 | CLK |

Bold = used by this project (18/23/32/17 for flash, 21/22 for the keypad; 13/14/
15/25/26/27/2/33 go to the panel and are wired on-board).

## Panel

- Waveshare **WFT0420CZ15** raw panel, 4.2 inch.
- 400 × 300 pixels, black/white.
- Controller: **UC8176**.
- This is the **old 4.2** module (not 4.2 V2).
- Partial-refresh waveform is usable for occasional small updates, but not for a
  sustained 1 Hz clock (see below).

## Pin map (fixed by the board)

| Signal | GPIO | Notes |
|---|---:|---|
| SCK | 13 | hardware SPI |
| MOSI / DIN | 14 | hardware SPI |
| CS | 15 | active low |
| BUSY | 25 | low while busy on this panel |
| RST | 26 | active low |
| DC | 27 | low = command, high = data |
| CS_S | 2 | secondary chip select, held high |
| EPD power enable | 33 | held high |

Other free pins used by add-ons: keypad I2C on 21/22, external flash SPI on
18/19/23/32.

SPI is run at **10 MHz**, mode 0, MSB first. MISO is not used (the 4.2 has no
readable ID); the driver passes GPIO36 as a dummy MISO to `SPI.begin()`.

## Power behaviour

- The panel is **bistable**: the image remains after `0x02` power-off.
- The firmware powers the panel off after **120 s** without a UI key/upload,
  except on the clock screen (which refreshes once per minute).
- The ESP32 stays awake and keeps Wi-Fi/HTTP alive; there is no deep sleep yet.
- The clock screen keeps the panel powered 24/7. For a battery device, add a
  wake source (button/RTC) and deep sleep.

## Flashing / backup

Always back up the full 4 MB flash before replacing the stock firmware:

```bash
pip install esptool   # or use your distro/venv package
python -m esptool --chip esp32 --port /dev/ttyACM0 --baud 921600 \
  read-flash 0x0 0x400000 stock-backup.bin
```

The stock image contains the vendor app **and your Wi-Fi credentials in NVS** —
do not publish it.

Restore if needed:

```bash
python -m esptool --chip esp32 --port /dev/ttyACM0 --baud 921600 \
  write-flash 0x0 stock-backup.bin
```

## Known hardware limits

- Old 4.2 partial waveform fades the whole panel under sustained 1 Hz updates.
- Black at 100 Hz is slightly gray compared with the 50 Hz original waveform.
- 171/200 Hz PLL settings produce blank/faint pixels; do not use them.
- No battery/charging circuit is assumed by this project.
