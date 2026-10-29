# Investigation: why the stock loader was slow, and which waveforms work

All numbers are measured on the actual board/panel (WFT0420CZ15, UC8176,
ESP32-D0WDQ6, 10 MHz SPI unless noted). The screen was visually checked after
each change; timing alone is not enough, because some PLL settings shorten BUSY
while producing blank/faint pixels.

## 1. Stock loader baseline

Reproduced with the stock browser protocol (`tools/legacy_upload_test.py`):

| Phase | Time |
|---|---:|
| EPD initialization | **2.68 s** |
| Image transfer (240 HTTP requests) | **1.48 s** |
| `SHOW` (refresh + sleep sequence) | **4.45 s** |
| **Total** | **8.61 s** |

The user's subjective “about 5 s” is the same order; the difference is image
content, Wi-Fi and whether initialization is counted.

## 2. Root causes

### 2.1 120,000 bytes written instead of 15,000

`EPD_Init_4in2()` in the stock loader fills the old-data RAM with:

```c
for (int i = 0; i < 400 * 300; i++) EPD_SendData(0xFF);
```

That is **120,000 bytes**, but one 400×300 1bpp frame is
`400/8 × 300 = 15,000` bytes. It writes the same frame eight times. This alone
was the largest part of the 2.68 s initialization.

### 2.2 Bit-banged SPI

The vendor code toggles SCK/MOSI with `digitalWrite()` for every bit. The new
driver uses the ESP32 hardware SPI at 10 MHz. Writing 30 KB (old + new frame)
dropped from seconds to **~25 ms**.

### 2.3 240 tiny HTTP requests with ASCII encoding

The stock web UI encodes each byte as two characters (`a`–`p`), splits the image
into 500-byte chunks, and sends one HTTP request per chunk: **240 requests** for
a full image. The new UI sends one binary `POST` with a 15,000-byte body.

### 2.4 PLL: `0x3C` is 50 Hz, not 100 Hz

The vendor comment says “3A 100HZ, 29 150Hz, 39 200HZ, 31 171HZ”; `0x3C` is the
value the stock loader actually uses and it behaves as **50 Hz**. Running at
`0x3A` (100 Hz) halves the waveform time.

### 2.5 Partial waveform requires inverted data

`EPD_4IN2_PartialDisplay()` writes `~Image` to RAM, while the full display writes
`Image` directly. If partial data is sent without inversion, the result is
black-background/white-text or black bars. The old data buffer must also match
what is actually on screen.

## 3. PLL / waveform test matrix

`0x3C` = 50 Hz, `0x3A` = 100 Hz, `0x29` = 150 Hz, `0x31` = 171 Hz,
`0x39` = 200 Hz.

| PLL | Full, fast LUT | Full, original LUT | GxEPD2 partial, full screen | Visual result |
|---|---:|---:|---:|---|
| `0x3C` 50 Hz | 1.91 s | 4.12 s | 1.16 s | clean |
| `0x3A` 100 Hz | **0.96 s** | — | **0.58 s** | clean, slightly gray |
| `0x29` 150 Hz | 0.64 s | — | — | not selected |
| `0x31` 171 Hz | 0.56 s | — | 0.34 s | faint / washed out |
| `0x39` 200 Hz | 0.48 s | 1.03 s | 0.29 s | blank / very faint |

The critical lesson: **171/200 Hz shorten BUSY but do not switch the pixels
properly.** 100 Hz is the highest verified stable PLL on this panel.

Waveform notes:

- The original 50 Hz LUT is the darkest but takes ~4.1 s.
- The Waveshare “fast” full LUT at 100 Hz is the standard mode (~0.96 s).
- The GxEPD2 partial LUT (T1=25, T2=1, T3=4, T4=25) at 100 Hz works for both
  small windows and full-screen fast updates (~0.58 s), with inverted data.
- The old panel’s partial waveform is not charge-balanced for continuous use;
  see §4.

## 4. Why the clock cannot update every second

The first local clock used a 400×40 partial window per second. After the
minute full refresh, the display was clean; it then degraded progressively and
was visibly gray after **~35 seconds**. The static large `HH:MM`, date and NTP
lines faded together with the seconds. This is a panel/waveform limitation, not
a font or data problem.

A second factor made it worse: the full-width seconds progress bar made the
dirty rectangle span the whole clock area, so the static digits were re-driven
by the partial waveform every second.

Final policy:

- **Clock: full standard refresh once per minute (0.98 s), static in between,
  no seconds.**
- **Other screens: status-bar minute update via a small partial window
  (0.58 s), plus a full standard refresh every 15 minutes.**
- Menu / app switch / notes page: full standard refresh (~0.98 s).

This was verified over 20+ minutes on the actual panel: no cumulative fading.

## 5. Final refresh modes

| Mode | LUT | PLL | Data | Time | Use |
|---|---|---|---:|---:|---|
| `fullFast` | GxEPD2 partial | 100 Hz | inverted | 0.58 s | fast image display |
| `partialFb` | GxEPD2 partial | 100 Hz | inverted | 0.58 s | small UI windows |
| `fullStd` | Waveshare fast full | 100 Hz | normal | 0.98 s | UI transitions, clean image |
| `fullHq` | original | 50 Hz | normal | 4.14 s | photos / maximum black |

The ST7789-style “more Hz is always better” assumption does not hold for this
e-paper panel: the waveform must give the pixels enough time to switch.

## 6. Reproducing

- `tools/legacy_upload_test.py` — stock protocol timing.
- `tools/api_test.py` — binary API timing.
- `tools/ui_remote_test.py` — Playwright end-to-end test.
- `tools/gen_font.py` — font regeneration.

The experiments used temporary bench sketches; they are not part of the release
firmware. The numbers above are from the actual board.
