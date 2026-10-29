import serial, threading, time, sys, os, http.client
from PIL import Image, ImageDraw

IP = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("EPAPER_IP", "epaper42.local")
IMGF = sys.argv[2] if len(sys.argv) > 2 else "build/legacy_pattern.pbm"

if not os.path.exists(IMGF):
    os.makedirs(os.path.dirname(IMGF) or ".", exist_ok=True)
    _im = Image.new('1', (400, 300), 1)
    _d = ImageDraw.Draw(_im)
    for y in range(0, 150, 10):
        for x in range(0, 400, 10):
            if (x // 10 + y // 10) % 2 == 0:
                _d.rectangle([x, y, x + 9, y + 9], fill=0)
    _d.rectangle([0, 150, 399, 159], fill=0)
    _d.text((20, 200), "LEGACY PROTOCOL TEST", fill=0)
    _im.save(IMGF)

im = Image.open(IMGF).convert('1')
W, H = im.size
assert (W, H) == (400, 300), (W, H)
px = im.load()

# bit=1 means white (non-black), same as browser
rows = []
for y in range(H):
    row = bytearray(W // 8)
    for x in range(W):
        if px[x, y] != 0:  # white
            row[x >> 3] |= 0x80 >> (x & 7)
    rows.append(row)
raw = b''.join(rows)
print("raw bytes", len(raw))

def nib(v):
    return chr((v & 0xF) + 97) + chr(((v >> 4) & 0xF) + 97)

def enc(data):
    return ''.join(nib(b) for b in data)

def word(v):
    return nib(v & 0xFF) + nib((v >> 8) & 0xFF)

def post(path, timeout=30):
    c = http.client.HTTPConnection(IP, 80, timeout=timeout)
    c.request('POST', path, body='')
    r = c.getresponse()
    b = r.read()
    c.close()
    return r.status, b

def send_chunks(data, label):
    t0 = time.time()
    n = 0
    for off in range(0, len(data), 500):
        chunk = data[off:off+500]
        s = enc(chunk)
        # split into <=500 byte chunks (1000 chars) as the browser does
        post('/' + s + word(len(s)) + 'LOAD_')
        n += 1
    print(f"{label}: {len(data)} bytes in {n} requests, {time.time()-t0:.3f}s")
    return time.time() - t0

# serial monitor
lines = []
stop = False
def mon():
    try:
        p = serial.Serial('/dev/ttyACM0', 115200, timeout=0.05)
        while not stop:
            d = p.read(4096)
            if d:
                for ln in d.decode('utf-8', 'replace').splitlines():
                    if ln.strip():
                        lines.append((time.time(), ln.strip()))
        p.close()
    except Exception as e:
        print("serial error", e)
th = threading.Thread(target=mon, daemon=True)
th.start()

t0 = time.time()
post('/EPDn_')          # 4.2 inch
t_init = time.time() - t0
print(f"EPD init: {t_init:.3f}s")

# browser general flow: 50%, NEXT, 50%, SHOW
# reconstruct exactly: pixels array, not packed bytes directly
# first phase is first 60000 pixels (top 150 rows)
half = 150 * 50
t1 = send_chunks(raw[:half], "phase1")
post('/NEXT_')
t2 = send_chunks(raw[half:], "phase2")
t_show0 = time.time()
post('/SHOW_', timeout=60)
t_show = time.time() - t_show0
total = time.time() - t0
stop = True
time.sleep(0.2)
print(f"SHOW: {t_show:.3f}s")
print(f"TOTAL: {total:.3f}s  (init {t_init:.2f} + xfer {t1+t2:.2f} + show {t_show:.2f})")
print("--- serial ---")
for ts, ln in lines[-15:]:
    print(f"{ts-t0:7.3f} {ln}")
