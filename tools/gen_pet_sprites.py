#!/usr/bin/env python3
"""Convert catbw.jpg / foxbw.jpg into 96x96 1bpp pet sprites.

The user supplies the two black-and-white chibi images (not committed); this
script cleans the background, downsizes to 192x192 with Floyd-Steinberg
dithering, and emits two frames per species (frame 1 is shifted up 3 px to give
a subtle breathing animation) into firmware/epaper42_web/pet_sprites.h.
"""
import os
from PIL import Image, ImageOps

S = 192
SRC = {'PET_CAT': ('references/catbw.jpg', 'catbw.jpg'),
       'PET_FOX': ('references/foxbw.jpg', 'foxbw.jpg')}

def load(paths):
    if isinstance(paths, str):
        paths = (paths,)
    path = next((p for p in paths if os.path.exists(p)), paths[0])
    im = Image.open(path).convert('L')
    im = im.point(lambda p: 255 if p >= 232 else p)   # clean JPEG background noise
    w, h = im.size
    m = int(w * 0.02)
    im = im.crop((m, m, w - m, h - m))
    im = ImageOps.autocontrast(im, cutoff=0)
    return im.resize((S, S), Image.LANCZOS).convert('1')

def shifted(im, dy):
    out = Image.new('1', (S, S), 1)
    out.paste(im, (0, -dy))
    return out

def rows(im):
    px = im.load(); data = []
    for y in range(S):
        v = 0
        for x in range(S):
            if px[x, y] == 0:
                v |= 1 << (S - 1 - x)
        data.append(v.to_bytes(S // 8, 'big'))
    return data

def c_array(name, data):
    lines = [f"static const uint8_t {name}[PET_H][{S // 8}] = {{"]
    for r in data:
        lines.append("  {" + ",".join(f"0x{b:02X}" for b in r) + "},")
    lines.append("};")
    return "\n".join(lines)

def main():
    header = ["#pragma once", f"#define PET_W {S}", f"#define PET_H {S}", ""]
    for name, src in SRC.items():
        base = load(src)
        header.append(c_array(f"{name}_0", rows(base)))
        header.append("")
        header.append(c_array(f"{name}_1", rows(shifted(base, 3))))
        header.append("")
        base.convert('L').resize((S, S), Image.NEAREST).save(f'/tmp/{name.lower()}_preview.png')
    open('firmware/epaper42_web/pet_sprites.h', 'w').write("\n".join(header))
    print('wrote firmware/epaper42_web/pet_sprites.h')

if __name__ == '__main__':
    main()
