import sys, time, io, json, os
import requests
from PIL import Image, ImageDraw, ImageFont

IP = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("EPAPER_IP", "epaper42.local")
BASE = f"http://{IP}"

def pack(im):
    im = im.convert('1')
    w,h = im.size
    px = im.load()
    out = bytearray((w//8)*h)
    for y in range(h):
        for x in range(w):
            if px[x,y]:  # white
                out[y*(w//8)+(x>>3)] |= 0x80>>(x&7)
    return bytes(out), w, h

def post(path, data=None, timeout=30):
    t0=time.time()
    r=requests.post(BASE+path, data=data, timeout=timeout,
                    headers={'Content-Type':'application/octet-stream'} if data is not None else {})
    dt=time.time()-t0
    try: j=r.json()
    except Exception: j={'raw':r.text[:200]}
    return r.status_code, j, dt

# full test image
im=Image.new('1',(400,300),1)
d=ImageDraw.Draw(im)
d.rectangle([0,0,399,299],outline=0,width=3)
try:
    f1=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',48)
    f2=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',24)
except Exception:
    f1=f2=None
d.text((30,40),"E-PAPER 4.2",font=f1,fill=0)
d.text((30,110),"200Hz full refresh",font=f2,fill=0)
d.text((30,150),"fast partial update",font=f2,fill=0)
for i in range(0,400,20):
    d.line([i,220,i,250],fill=0)
for i in range(0,320,20):
    d.line([20,220+i,380,220+i],fill=0)
os.makedirs('build', exist_ok=True)
im.save('build/api_full.png')
data,_,_=pack(im)
print("GET /", requests.get(BASE+'/', timeout=10).status_code)
print("status", requests.get(BASE+'/api/status', timeout=10).json())
code,j,dt=post('/api/full', data)
print(f"FULL http={code} client={dt*1000:.0f}ms server={j}")
time.sleep(0.5)

# partial status bar
bar=Image.new('1',(400,40),1)
bd=ImageDraw.Draw(bar)
bd.rectangle([0,39,399,39],fill=0)
try:
    fb=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf',30)
except Exception:
    fb=None
bd.text((8,2),"12:34:56",font=fb,fill=0)
bd.text((300,8),"10-29",font=fb,fill=0)
bar.save('build/api_bar.png')
bdata,_,_=pack(bar)
for i in range(3):
    code,j,dt=post('/api/partial?x=0&y=0&w=400&h=40', bdata)
    print(f"PART http={code} client={dt*1000:.0f}ms server={j}")
    time.sleep(0.2)

code,j,dt=post('/api/refresh')
print(f"REFRESH http={code} client={dt*1000:.0f}ms server={j}")
