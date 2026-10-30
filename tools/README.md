# Tools

Python 3.10+; install dependencies:

```bash
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements.txt
.venv/bin/playwright install firefox   # only for ui_remote_test.py
```

The device IP defaults to `epaper42.local`; override with the first argument or
the `EPAPER_IP` environment variable:

```bash
.venv/bin/python tools/api_test.py 192.168.1.50
EPAPER_IP=192.168.1.50 .venv/bin/python tools/ui_remote_test.py
```

| Script | Purpose |
|---|---|
| `gen_font.py` | Regenerate `firmware/epaper42_web/ui_font.h` from GNU Unifont 16 px. Requires the Unifont OTF (`fonts-unifont` / `unifont` package). |
| `api_test.py` | Generate a 400×300 test image, upload it through `/api/full?mode=fast`, exercise `/api/partial`, `/api/refresh`; print timings. |
| `ui_remote_test.py` | Playwright end-to-end test of the web remote: preview, D-pad, clock app, note send, image upload, back. Saves screenshots to `/tmp`. |
| `legacy_upload_test.py` | Reproduce the **stock Waveshare** browser protocol and measure the 8.6 s baseline. Use only with the original firmware, and pass the device IP / serial port as needed. |

`api_test.py` and `legacy_upload_test.py` write generated images to `build/`
(git-ignored).
