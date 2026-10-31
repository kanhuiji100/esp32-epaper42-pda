# Serial console

The ESP32 exposes a line-based console on the USB serial port at **115200 8N1**.
It is useful for first-time Wi-Fi provisioning, for simulating the (not yet
installed) physical buttons, and for checking the device when HTTP/network is
unavailable.

Open it with any terminal:

```bash
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
# or: screen /dev/ttyACM0 115200
# or PuTTY / Arduino Serial Monitor at 115200
```

You should see:

```text
Serial console ready. Type 'help'.
> 
```

Commands are case-insensitive. Arguments with spaces can be quoted with `"` or
`'`. `Ctrl-C` cancels a password/SSID prompt.

## Wi-Fi provisioning

### Interactive (password hidden, recommended)

```text
> wifi set
SSID: MyRouter
Password (hidden): ********
WiFi: connecting to MyRouter
....
WiFi: saved to NVS
mDNS: http://epaper42.local
IP: 192.168.1.50
HTTP server on :80
```

- The password is echoed as `*` and is never printed.
- Credentials are saved in NVS namespace **`wifi`** and override `secrets.h` on
  every boot.
- Connection timeout is 20 s. On failure the previous credentials are restored
  automatically.
- ESP32 is 2.4 GHz only. Use a 2.4 GHz SSID, not a 5 GHz one.

### Scripted

```text
> wifi set "My SSID" "my passphrase"
```

This prints the password in the terminal echo; only use it on a trusted local
console.

### Other Wi-Fi commands

| Command | Result |
|---|---|
| `wifi` / `wifi status` | status, SSID, IP, RSSI, MAC, `netReady`, power-save mode, channel, TX power |
| `wifi scan` | scan visible 2.4 GHz networks; if the first scan returns 0 after a failed connection, the station is power-cycled and retried |
| `wifi reconnect` | reconnect with NVS (or `secrets.h`) credentials |
| `wifi forget` | remove NVS credentials; current connection stays until reboot |

`wifi forget` does not erase the current session; `reboot` (or `wifi reconnect`)
then falls back to `secrets.h`.

## Simulate input / UI

| Command | Result |
|---|---|
| `key up\|down\|left\|right\|ok\|back\|home` | call the same `ui::key()` used by the web remote |
| `app home\|clock\|notes\|image\|qr\|news\|pet\|games\|gomoku\|ddz\|settings` | switch app / start game / open news / pet |
| `pet [status\|feed\|play\|sleep\|clean\|touch\|work\|adopt cat\|fox\|name <n>\|reset\|save]` | electronic pet status/actions |
| `note` | print current note |
| `note <text>` | set note (UTF-8, one line) and save to NVS |
| `note clear` | clear note |
| `screen` | 50×30 ASCII preview of the 400×300 framebuffer |
| `status` | app/selection/page + IP/RSSI/heap/panel state |

Example:

```text
> key down
key=down ms=982 refresh=957
{"ok":1,"app":"home","sel":1,...}
> app clock
app=clock
...
> screen
screen: app=clock
+--------------------------------------------------+
|                                                  |
...
```

`screen` samples each 8×10 pixel block and prints `#` for dark areas; it is a
quick sanity check, not a pixel-perfect mirror.

## QR codes

| Command | Result |
|---|---|
| `qr <text>` | generate a text QR (UTF-8, one line) and show the QR app |
| `qr text <text>` | same, explicit text form |
| `qr wifi <ssid> <pass>` | Wi-Fi QR; password visible in the command echo |
| `qr wifi <ssid>` | prompt for the password with `*` masking |
| `qr` | show `valid/type/modules/bytes` (never the payload or password) |
| `qr clear` | clear the code / show the placeholder |

Examples:

```text
> qr 你好，扫码测试
qr: valid=1 type=text modules=25 bytes=21

> qr 你好控制台
qr: valid=1 type=text modules=25 bytes=15

> qr wifi MyRouter
WiFi password (hidden): **************
qr: valid=1 type=wifi modules=29 bytes=42
```

The QR is displayed with the **50 Hz HQ waveform (4.14 s)** and a 4-module quiet
zone. It stays static (no minute partial updates) until you generate another code
or leave the screen. QR content is RAM-only; `qr clear` or reboot drops it. See
[QRCODE.md](QRCODE.md) for payload details and limits.

## Storage

| Command | Result |
|---|---|
| `storage` / `df` | internal/external storage JSON |
| `ls [path]` | list `/int` or `/ext` |
| `cat <path>` | print a text resource (≤ 4 KB) |
| `write <path> <text>` | write a small text resource |
| `rm <path>` | remove a file |

See [STORAGE.md](STORAGE.md) for the partition layout and external wiring.

## Hostname

| Command | Result |
|---|---|
| `host` / `hostname` | show current hostname and `http://<name>.local` |
| `host <name>` | set hostname (lowercased; 1–32 chars `a-z 0-9 -`) |
| `host set <name>` | same, explicit form |

```text
> host pda42
mDNS: http://pda42.local
host: pda42  http://pda42.local
DHCP hostname fully applies after reboot; mDNS is immediate.
```

The Home status bar and Settings screen update immediately. mDNS is restarted, so
`http://pda42.local` works right away on mDNS-capable clients; the DHCP hostname
used by router local DNS is applied after the next boot/reconnect. The name is
stored in NVS namespace `net`, key `host`.

## Panel / system

| Command | Result |
|---|---|
| `refresh` | HQ 50 Hz full refresh of the current frame (4.1 s, cleans ghosting) |
| `power off` / `power on` | panel power off / redraw current app (powers on) |
| `clear` | clear white + return to home |
| `time` | local time / NTP status |
| `ntp` | restart NTP sync (`CST-8`) |
| `heap` | free / minimum free heap |
| `reboot` | restart ESP32 |
| `help` / `?` | command list |

## Design notes

- Line editor: 200-byte max, backspace, CR/LF tolerant, password echo masked.
- Wi-Fi credentials are stored as plaintext NVS; this is normal for ESP32 but
  means physical/console access implies credential access.
- Serial commands block the main loop while the panel refreshes (~0.6–4.1 s), so
  HTTP requests wait during a command.
- HTTP request logs (`REQ ...`) can interleave with the prompt; the console still
  accepts input.
- The console only uses `Serial`; it does not change the HTTP API.

## Troubleshooting

| Symptom | Check |
|---|---|
| `wifi scan` returns 0 | 2.4 GHz AP in range? The console retries with a radio reset; try again. |
| `wifi set` fails | 2.4 GHz SSID, correct password, WPA2 (not WPA3-only). Old credentials are restored. |
| IP is `0.0.0.0` | Not connected; run `wifi set` or `wifi reconnect`. |
| Prompt not visible | 115200 baud, correct port (`/dev/ttyACM0`), DTR/RTS not held in reset. |
| `screen` is noisy | It is a coarse 50×30 preview; use the web `/api/screen` for exact pixels. |
