# Changelog

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
