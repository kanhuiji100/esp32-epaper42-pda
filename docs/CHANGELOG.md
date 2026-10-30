# Changelog

## v0.2.0 — 2026-10-30

- USB serial console at 115200: `help`, `status`, `screen`, keys/apps/notes,
  panel control, reboot.
- Wi-Fi provisioning over serial (`wifi set` interactive, hidden password;
  `wifi scan`, `wifi forget`, `wifi reconnect`).
- Credentials now live in NVS namespace `wifi` and override `secrets.h`;
  flash without secrets and provision on first boot.
- `wifi scan` recovers from a failed-connect radio state by power-cycling the
  station and retrying.
- Docs: `docs/SERIAL_CONSOLE.md`; README quick start now uses serial provisioning.

## v0.1.0 — 2026-10-29

- Initial public release.
- Hardware: Waveshare ESP32 e-Paper Driver Board + WFT0420CZ15 (4.2″, UC8176).
- Fast driver: hardware SPI 10 MHz, 100 Hz PLL, three LUT classes, partial window
  refresh; full image 0.61 s device / 0.69–0.80 s round trip.
- Local UI: home, clock, notes, image, settings; 1bpp framebuffer; GB2312 +
  ASCII Unifont bitmaps; diff-based refresh.
- Web remote: 15 KB screen mirror, D-pad, app shortcuts, note editor.
- Minute-level clock with full refresh; 1 Hz partial clock removed after finding
  cumulative panel fading.
- Documentation of the stock-loader bottlenecks and waveform/PLL experiments.
