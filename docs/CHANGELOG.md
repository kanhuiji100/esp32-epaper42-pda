# Changelog

## v0.4.3 — 2026-10-31

- Notes: body moved up to y=28 and 12 lines/page (was y=40, 10 lines).
  Last line ends ~y=264, still above the y=270 hint bar.

## v0.4.2 — 2026-10-31

- Notes: removed the second title row; page number now sits in the top status bar
  to the left of the Wi-Fi icon.
- Notes body starts at y=40 and paginates at 10 lines/page.
- Notes/Image/Settings/QR placeholder bottom hint bars unified with Home
  (`hline y=270`, text `y=276`).

## v0.4.1 — 2026-10-31

- "Refresh / 全屏清残影" now uses the **HQ 50 Hz waveform** (`refreshHq`,
  ~4.15 s) instead of the standard 0.98 s waveform, for maximum ghost removal.
- API `POST /api/refresh`, serial `refresh`, and the web "高清清残影 4.1s"
  button all use the HQ path.
- Docs updated.

## v0.4.0 — 2026-10-30

- Custom device hostname, default `epaper42`; stored in NVS namespace `net`.
- Home status bar and Settings show the hostname; web card and serial `host`
  command update it.
- `GET/POST /api/host`; mDNS restarts immediately so `http://<name>.local` works;
  DHCP hostname applies after reboot.
- Validation: 1–32 chars `a-z 0-9 -`, no leading/trailing hyphen.
- Docs and screenshots updated; firmware ~86%.

## v0.3.0 — 2026-10-30

- New **QR** screen: text QR and Wi-Fi join QR (`WIFI:T:WPA;S:...;P:...;;`).
- Input from the web QR card, `POST /api/qr`, and serial `qr` / `qr wifi`.
- QR codes always render with the 50 Hz HQ waveform (~4.15 s), 4-module quiet
  zone, integer scaling and no status bar; the QR screen is static.
- Home menu now has five items; QR is the fourth.
- Wi-Fi QR escapes `\ ; , : "` and supports open networks (`T:nopass`).
- Added Nayuki QR Code generator (C, MIT); text/Wi-Fi codes decoded with
  `zxing-cpp` in testing.
- Docs: [QRCODE.md](QRCODE.md), API and serial-console updates; firmware ~85%.

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
