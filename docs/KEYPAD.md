# VK36N16I keypad

A 4×4 touch matrix keypad driven by a **VK36N16I** controller, connected over
I2C. It lets the whole UI be used without the phone/browser.

## Module

- emakefun **I2C 触摸矩阵键盘 v3.x** family ("Matrix keypad Module V3.2").
- Controller: **VK36N16I** (16-channel capacitive touch).
- Connector: 4 pins — `G` (GND), `V` (3.3–5 V), `SCL`, `SDA`.
- **I2C address: `0x65`.**

## Wiring

| Keypad | ESP32 | Note |
|---|---|---|
| V | 3V3 | module accepts 3.3–5.5 V |
| G | GND | |
| SDA | **GPIO 21** | |
| SCL | **GPIO 22** | |

These are the standard ESP32 I2C pins and are free on this board. The external
W25Qxx flash therefore moved its **CS to GPIO 32** (was 22) — see
[STORAGE.md](STORAGE.md). The module has on-board pull-ups; the firmware also
enables internal pull-ups on the two pins.

## Protocol

Reading is dead simple — there is no register addressing:

```text
I2C write: none
I2C read : request 2 bytes from 0x65 -> uint16 little-endian bitmask
           bit set = key currently touched
```

| bit | key | bit | key | bit | key | bit | key |
|---:|---|---:|---|---:|---|---:|---|
| 0 | `1` | 4 | `2` | 8 | `3` | 12 | `A` |
| 1 | `4` | 5 | `5` | 9 | `6` | 13 | `B` |
| 2 | `7` | 6 | `8` | 10 | `9` | 14 | `C` |
| 3 | `*` | 7 | `0` | 11 | `#` | 15 | `D` |

## Key mapping

| Key(s) | Action |
|---|---|
| `2` `8` `4` `6` | up / down / left / right (auto-repeat when held) |
| `5` or `A` | OK / confirm |
| `B` | Back |
| `C` | Home |
| `D` | full refresh (slowest HQ waveform, clears ghosting) |
| `1` `3` `7` `9` `0` `*` `#` | unused (free for future shortcuts) |

## Auto-detection

The module ships without documentation, so the driver scans candidate SDA/SCL
pairs for a device at `0x65` on first boot and saves the pins to NVS. You can
also drive it by hand from the serial console:

```text
> kbd                 # status (pins + current key mask)
> kbd scan            # try all candidate pin pairs, save the one that answers
> kbd pins 21 22      # force SDA=21, SCL=22 and save
> kbd forget          # clear the saved pins
```

Candidate pins exclude the e-paper pins (2, 13, 14, 15, 25, 26, 27, 33), the
flash pins (18, 19, 23, 32), the input-only pins (34, 35) and strapping pin 12.

## Notes

- Polled from the main loop every iteration; a read is ~0.3 ms at 100 kHz.
- Directions auto-repeat at 450 ms while held.
- If `kbd` reports "not ready", check `V`/`G` power and that SDA/SCL are not
  swapped; then run `kbd scan`.
