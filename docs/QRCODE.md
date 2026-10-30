# QR codes

The device has a dedicated **QR** screen (the fourth home-menu item). It can encode
arbitrary text or a Wi-Fi join code, from the web page, the USB serial console, or
the HTTP API.

## Modes

| Mode | Input | Encoded payload |
|---|---|---|
| Text | any UTF-8 text (≤ QR capacity) | the text itself |
| Wi-Fi | SSID + password (or empty password) | `WIFI:T:WPA;S:<ssid>;P:<pass>;;` |

For an open network the payload is `WIFI:T:nopass;S:<ssid>;;`.

SSID and password are escaped per the ZXing Wi-Fi convention: `\`, `;`, `,`, `:`
and `"` are prefixed with a backslash. This makes the code parse correctly for
SSIDs such as `我的WiFi` or passwords containing `;,.` characters.

## Input

### Web

The page has a **二维码** card:

- textarea → **生成文字二维码**
- SSID + password → **生成 WiFi 二维码**
- **清空二维码**

The page POSTs raw UTF-8 for text, or URL-encoded SSID/password for Wi-Fi. The
device switches to the QR screen automatically. The card notes that Wi-Fi
credentials will be visible on the e-paper screen.

### Serial console

```text
> qr 你好，这是一段文字
> qr text https://example.com
> qr wifi "MyRouter" "my passphrase"   # password visible
> qr wifi MyRouter                     # prompts with hidden password
> qr                                   # valid=1 type=... modules=...
> qr clear
```

`qr wifi <ssid>` prompts `WiFi password (hidden):` and masks the password with
`*`, the same mechanism as `wifi set`. See [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md).

### HTTP

```bash
# text
curl -X POST --data-binary 'hello QR' 'http://<ip>/api/qr?type=text'

# Wi-Fi (URL-encoded values)
curl -X POST \
  'http://<ip>/api/qr?type=wifi&ssid=MyRouter&pass=my%20passphrase'

# open network
curl -X POST 'http://<ip>/api/qr?type=wifi&ssid=Guest&pass='

# clear
curl -X POST 'http://<ip>/api/qr?type=clear'
```

See [API.md](API.md#qr) for response fields.

## Rendering

- A QR is encoded on the ESP32 with the Nayuki C library (MIT), **ECC M**,
  version 1–13 (21–69 modules).
- The matrix is drawn with an integer module scale and a **4-module quiet zone**,
  centered on a pure white 400×300 canvas. The QR screen has **no status bar**, so
  nothing intrudes into the quiet zone.
- The image is written with `panel42::fullHq()` — the original **50 Hz, 4.14 s**
  waveform. This is the slowest/highest-contrast mode and is used every time a
  valid QR is (re)drawn, including entering the QR app from the menu.
- A valid QR screen is static: `ui::tick()` skips it, so no partial refreshes or
  minute-bar updates touch the code. It stays byte-identical until another QR is
  generated or the user leaves the screen.
- The placeholder (no QR yet) uses the normal standard waveform; it is just text.

Measured on the device: text QR `ms=4152–4153`, `refresh=4127`; the QR app entry
via the menu also reports ~4.15 s.

## Limits

| Item | Limit |
|---|---|
| QR version | 1–13 (69 modules max) |
| Error correction | M |
| Text body | API accepts 1–1024 bytes; encoder rejects payloads that do not fit version 13 |
| SSID | 1–32 bytes (UTF-8 allowed) |
| Wi-Fi password | 8–63 bytes, or empty for open network |
| Persistence | **none** — the QR lives in RAM only; reboot returns to Home |

If a payload is too large, the API returns HTTP 400 and the previous QR stays on
screen (the encoder writes into a candidate buffer first).

## Security

- A Wi-Fi QR necessarily contains the network password; it is displayed on the
  e-paper screen. The web page warns about this.
- The password is not logged by the firmware, not included in `/api/ui` or
  `qrSummary()`, and not stored in NVS. `qr clear` / reboot drops it from RAM.
- The serial `qr wifi <ssid>` prompt masks the password; the non-interactive form
  is documented as visible on the terminal.
- Text QR content is likewise RAM-only; the HTTP API does not echo it back.

## Verification

The generated 1bpp framebuffer was decoded with `zxing-cpp` for both modes:

| Input | Decoded payload |
|---|---|
| `Hello QR 123` | `Hello QR 123` |
| `你好，墨水屏 QR` | `你好，墨水屏 QR` |
| `TestNet` / `Passw0rd123` | `WIFI:T:WPA;S:TestNet;P:Passw0rd123;;` |
| `我的WiFi` / `a;b,c:d"e\f` | `WIFI:T:WPA;S:我的WiFi;P:a\;b\,c\:d\"e\\f;;` |
| `OpenNet` / empty | `WIFI:T:nopass;S:OpenNet;;` |
