# Hardware

## Board

Waveshare **e-Paper ESP32 Driver Board**.

- ESP32-D0WDQ6, dual core, 240 MHz.
- 4 MB SPI flash, **no PSRAM**.
- USB-serial: CH343P (`/dev/ttyACM0` on Linux).
- 3.3 V logic and panel supply; boost converter for panel rails.

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
