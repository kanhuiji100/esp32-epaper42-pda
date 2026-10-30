# Third-party notices

This project is based on, or derived from, the following open-source work.
The original licenses apply to the corresponding portions.

## Waveshare e-Paper

- **Waveshare e-Paper** examples/drivers — MIT License.
- <https://github.com/waveshare/e-Paper>
- Used for the UC8176 initialization sequence, full-refresh LUT concept and
  partial-refresh handling. The optimized driver in `panel42.h` is a rewrite for
  ESP32 hardware SPI.

## GxEPD2

- **GxEPD2** by Jean-Marc Zingg — MIT License.
- <https://github.com/ZinggJM/GxEPD2>
- The partial waveform values `T1=25, T2=1, T3=4, T4=25` and the
  “partial waveform over the full area without a 0x91/0x90 window” approach come
  from `GxEPD2_420.cpp`.

## QR Code generator library (C)

- Files: `firmware/epaper42_web/qrcodegen.h`, `firmware/epaper42_web/qrcodegen.c`
- Author: Project Nayuki
- License: MIT — see [docs/NAYUKI_QR_LICENSE.txt](docs/NAYUKI_QR_LICENSE.txt)
- Source: https://www.nayuki.io/page/qr-code-generator-library

## GNU Unifont

- **GNU Unifont** 16 px — GPLv2+ with the GNU font embedding exception, and
  also available under the SIL Open Font License 1.1.
- <https://unifoundry.com/unifont/>
- `firmware/epaper42_web/ui_font.h` contains a generated 1bpp bitmap subset
  (7447 GB2312 characters/punctuation + 95 ASCII glyphs) produced by
  `tools/gen_font.py`.
- The full license text is in [`docs/UNIFONT_LICENSE.txt`](docs/UNIFONT_LICENSE.txt),
  including the font embedding exception. The font data remains under its own
  license; the embedding exception is what allows it to be included in this
  firmware.

## Build/runtime dependencies (not bundled)

- **Arduino ESP32 core 2.0.17** — LGPL-2.1; installed separately by the user.
- **Arduino CLI** — GPL-3.0; tool only.
- **Pillow**, **requests**, **pyserial**, **Playwright** — tool-only Python
  dependencies; see `tools/requirements.txt` and their respective licenses.

## Font generation

`ui_font.h` is generated; `tools/gen_font.py` is part of this project (MIT) and
uses the system's Unifont OTF. Re-running it produces the same bitmap data from
the same font version; the generated file retains the Unifont notice at its top.
