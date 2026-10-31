# HTTP API

The ESP32 runs a single-threaded HTTP server on port 80. It serves the remote
console at `/` and a small JSON/binary API. All responses include
`Connection: close` and CORS headers; the browser opens a new TCP connection per
request.

Base URL: `http://<device-ip>/` (the serial log prints the DHCP IP; mDNS is
`http://<hostname>.local`, default `epaper42.local`).

For USB serial provisioning and console commands, see
[SERIAL_CONSOLE.md](SERIAL_CONSOLE.md).

Storage paths start with `/int` (384 KB internal LittleFS) or `/ext` (external
W25Q128, 16 MB, when fitted). See [STORAGE.md](STORAGE.md).

## Image / screen format

- Size: **15,000 bytes** = `400 / 8 × 300`.
- Row-major, 50 bytes per row, MSB = leftmost pixel.
- **1 = white, 0 = black.**
- No header, no compression.

Partial images use `(w / 8) × h` bytes; `x` and `w` must be multiples of 8.

## Endpoints

| Method | Path | Body | Result |
|---|---|---|---|
| GET | `/` | — | remote console HTML |
| GET | `/api/status` | — | device status JSON (includes hostname) |
| GET | `/api/host` | — | current hostname + mDNS URL |
| GET | `/api/storage` | — | internal/external storage info |
| GET | `/api/storage/list?path=/int` | — | directory listing |
| GET | `/api/storage/file?path=...` | — | raw file bytes |
| POST | `/api/storage/file?path=...` | binary ≤ 4 MB | write file |
| DELETE | `/api/storage/file?path=...` | — | remove file |
| POST | `/api/host?name=...` | — | set hostname; restarts mDNS, returns host/URL |
| GET | `/api/screen` | — | current framebuffer, 15,000 bytes |
| GET | `/api/ui` | — | local-UI state JSON |
| GET | `/api/qr` | — | QR state (same fields as `/api/ui`) |
| GET | `/api/note` | — | current note, UTF-8 text |
| POST | `/api/key?k=...` | — | send a key, returns UI state |
| POST | `/api/app?name=...` | — | switch app, returns UI state |
| POST | `/api/note` | UTF-8 text ≤ 4095 B | save note + show notes app |
| POST | `/api/qr?type=text` | UTF-8 text, 1–1024 B | generate text QR, show QR app |
| POST | `/api/qr?type=wifi&ssid=&pass=` | — | generate Wi-Fi QR (URL-encoded) |
| POST | `/api/qr?type=clear` | — | clear QR / show placeholder |
| POST | `/api/full?mode=...` | 15,000 B frame | upload full image |
| POST | `/api/refresh` | — | HQ 50 Hz full refresh of current frame (clean ghosting, ~4.1 s) |
| POST | `/api/clear` | — | clear white + return home |
| POST | `/api/poweroff` | — | panel power off (image stays) |
| POST | `/api/partial?x=&y=&w=&h=` | `(w/8)*h` B | **debug only**; desyncs local UI |

### `GET /api/status`

```json
{"ok":1,"ip":"192.168.1.50","host":"epaper42","heap":180000,"powered":1,"mode":"part"}
```

`mode` is `none|std|hq|part`.

### Hostname

```bash
# read
curl http://<ip>/api/host

# set (lowercased; 1-32 chars a-z/0-9/-, no leading/trailing -)
curl -X POST 'http://<ip>/api/host?name=pda42'
```

Response:

```json
{"ok":1,"host":"pda42","mdns":"http://pda42.local"}
```

The mDNS responder is restarted immediately, so `http://pda42.local` works as
soon as the request returns. `WiFi.setHostname()` is also updated; the DHCP
hostname is fully applied after the next reboot/reconnect. The Home status bar
and Settings screen are redrawn when they are visible. Invalid names return HTTP
400 and leave the current hostname unchanged.

### `GET /api/ui`

```json
{
  "ok": 1,
  "app": "clock",
  "sel": 0,
  "page": 0,
  "pages": 2,
  "hasImage": 0,
  "qr": 0,
  "qrType": "",
  "qrModules": 0,
  "host": "epaper42",
  "ms": 984,
  "refresh": 959,
  "ip": "192.168.1.50",
  "heap": 178000,
  "time": "22:29"
}
```

- `sel`: 0-based home menu selection.
- `page`/`pages`: note page.
- `ms`: duration of the last UI refresh (0 if no pixels changed).
- `refresh`: BUSY wait of the last refresh.

### `GET /api/screen`

Returns `application/octet-stream`, 15,000 bytes. Render it in the browser with a
canvas, or convert to PNG:

```python
from PIL import Image
d = requests.get("http://192.168.1.50/api/screen").content
im = Image.new("1", (400, 300))
px = im.load()
for y in range(300):
    for x in range(400):
        px[x, y] = 255 if (d[y * 50 + (x >> 3)] >> (7 - (x & 7))) & 1 else 0
im.save("screen.png")
```

### Keys

```bash
curl -X POST 'http://<ip>/api/key?k=up'
curl -X POST 'http://<ip>/api/key?k=down'
curl -X POST 'http://<ip>/api/key?k=ok'
curl -X POST 'http://<ip>/api/key?k=back'
curl -X POST 'http://<ip>/api/key?k=home'
```

`left` / `right` also page the notes app. `app` accepts `news`, `games`, `gomoku` and `ddz` in addition to the base apps. The response is the `/api/ui` JSON
(including the refresh time).

### Apps

```bash
curl -X POST 'http://<ip>/api/app?name=home'
curl -X POST 'http://<ip>/api/app?name=clock'
curl -X POST 'http://<ip>/api/app?name=notes'
curl -X POST 'http://<ip>/api/app?name=image'
curl -X POST 'http://<ip>/api/app?name=settings'
```

### Notes

```bash
curl -X POST --data-binary '第一行
第二行中文' \
  -H 'Content-Type: text/plain; charset=utf-8' \
  'http://<ip>/api/note'
```

- Body limit: 4095 bytes (the page limits the textarea to 1200 characters).
- Saved in NVS namespace `pda`, key `note`; survives reboot.
- The device wraps at 360 px and paginates at 12 lines/page.

### QR

Generate a QR code and switch the local UI to the QR screen. The device uses the
**50 Hz HQ waveform** (`fullHq`, ~4.15 s) for maximum contrast and a 4-module
quiet zone; the QR screen is static afterwards. See [QRCODE.md](QRCODE.md).

**Text:**

```bash
curl -X POST --data-binary 'Hello QR 你好' \
  'http://<ip>/api/qr?type=text'
```

Body is raw UTF-8, 1–1024 bytes; the encoder rejects payloads that do not fit QR
version 13 (ECC M) with HTTP 400. The previous QR remains on screen on failure.

**Wi-Fi:**

```bash
curl -X POST \
  'http://<ip>/api/qr?type=wifi&ssid=MyRouter&pass=my%20passphrase'

# open network
curl -X POST 'http://<ip>/api/qr?type=wifi&ssid=Guest&pass='
```

`ssid` and `pass` are URL-encoded. SSID 1–32 bytes; password 8–63 bytes or empty.
The payload is `WIFI:T:WPA;S:<ssid>;P:<pass>;;` (or `T:nopass` for empty);
`\`, `;`, `,`, `:` and `"` are escaped.

**Clear:**

```bash
curl -X POST 'http://<ip>/api/qr?type=clear'
```

Response (same fields as `/api/ui`):

```json
{"ok":1,"app":"qr","sel":3,"page":0,"pages":1,"hasImage":0,
 "qr":1,"qrType":"text","qrModules":33,"ms":4153,"refresh":4127,
 "ip":"192.168.1.50","heap":160000,"time":"10:18"}
```

`qr` = valid code; `qrType` = `text`/`wifi`; `qrModules` = matrix side; `ms` is
the HQ refresh time. The payload/password is not returned.

### Full image

```bash
curl -X POST --data-binary @frame.bin \
  'http://<ip>/api/full?mode=fast'
```

`mode`:

| value | waveform | device time | note |
|---|---|---:|---|
| `fast` (default) | partial LUT, inverted | 0.58 s + data | slightly gray |
| `std` | full LUT, normal | 0.95 s + data | cleaner |
| `hq` | original 50 Hz LUT | 4.11 s + data | darkest |

After a successful upload the local UI switches to the image viewer. The image
is kept in RAM only.

Response:

```json
{"ok":1,"mode":"fast","init":0,"data":27,"refresh":580,"total":607,"heap":214260}
```

### Refresh / clear / power

```bash
# clean ghosting: HQ 50 Hz full refresh of the current framebuffer (~4.1 s)
curl -X POST 'http://<ip>/api/refresh'

# clear white and go back to the home screen
curl -X POST 'http://<ip>/api/clear'

# turn off panel driving voltages; the image remains; next key/upload powers on
curl -X POST 'http://<ip>/api/poweroff'
```

### Debug partial update

`POST /api/partial?x=0&y=0&w=400&h=40` with `(w/8)*h` bytes writes a window
directly to the panel. This bypasses the local UI framebuffer; after using it the
screen and `/api/screen` disagree until the next full image or app switch. It is
kept for driver testing only.

## Timing fields

| field | meaning |
|---|---|
| `init` | power-on / LUT-mode switch |
| `data` | SPI transfer into panel RAM |
| `refresh` | BUSY wait for the panel waveform |
| `total` | device-side total |
| `client_ms` | measured by the web page around `fetch` (in `/api/ui` responses), not returned by the firmware |

## Notes / constraints

- No chunked request bodies; send `Content-Length`.
- 15 KB bodies are read with a 5 s timeout; bodies are not kept after the request.
- The server blocks during a panel refresh (~0.58–4.1 s); concurrent requests wait.
- Panel idle power-off after 120 s (unless the clock screen is active); a key/upload
  after that costs ~148 ms extra.
