#!/usr/bin/env python3
"""Generate 80x80 1bpp pet sprites (cat / fox) as a C header.

Clean chibi line-art inspired by the user's cat.jpg / fox.jpg references:
cat = cat ears + twin tails + tail bow; fox = big ears + ear flower + fluffy tail.
"""
from PIL import Image, ImageDraw

S = 80

def canvas():
    return Image.new('L', (S, S), 255)

def dot(d, x, y):
    if 0 <= x < S and 0 <= y < S:
        d.point((x, y), fill=0)

def dither(d, box, step=2):
    x0, y0, x1, y1 = box
    for y in range(max(0, y0), min(S, y1)):
        for x in range(max(0, x0), min(S, x1)):
            if ((x + y) % step) == 0:
                dot(d, x, y)

def shape(d, pts, w=1):
    d.polygon(pts, fill=255)
    d.line(list(pts) + [pts[0]], fill=0, width=w, joint='curve')

def ellipse(d, box, w=1):
    d.ellipse(box, fill=255, outline=0, width=w)

def cat(frame):
    im = canvas(); d = ImageDraw.Draw(im)
    sway = 2 if frame else 0
    # tail + bow
    d.line([(30, 62), (14, 54 + sway), (10, 40), (18, 30)], fill=0, width=3)
    shape(d, [(18, 28), (6, 19), (8, 36)])
    shape(d, [(18, 28), (30, 19), (28, 36)])
    d.ellipse([14, 25, 22, 33], fill=0)
    # dress
    shape(d, [(34, 58), (50, 58), (58, 78), (26, 78)])
    d.line([(35, 62), (28, 72)], fill=0)
    d.line([(49, 62), (56, 72)], fill=0)
    # twin tails
    d.line([(26, 32), (12, 50 + sway), (20, 76)], fill=0, width=3)
    d.line([(54, 32), (68, 50 - sway), (60, 76)], fill=0, width=3)
    # head
    ellipse(d, [22, 14, 58, 54])
    # ears
    shape(d, [(29, 24), (14, 2), (44, 14)])
    shape(d, [(51, 24), (66, 2), (36, 14)])
    dither(d, (22, 8, 30, 18))
    dither(d, (50, 8, 58, 18))
    # fringe + side locks
    d.arc([20, 6, 60, 40], 195, 345, fill=0)
    d.line([(26, 22), (30, 34)], fill=0)
    d.line([(54, 22), (50, 34)], fill=0)
    d.line([(40, 18), (40, 30)], fill=0)
    # eyes
    d.ellipse([32, 32, 42, 46], fill=0)
    d.ellipse([34, 34, 37, 38], fill=255)
    if frame:
        d.arc([48, 34, 58, 44], 200, 340, fill=0, width=2)
    else:
        d.ellipse([48, 32, 58, 46], fill=0)
        d.ellipse([51, 34, 54, 38], fill=255)
    # blush, mouth, whiskers
    dither(d, (24, 44, 32, 50))
    dither(d, (48, 44, 56, 50))
    d.arc([36, 48, 42, 54], 0, 180, fill=0)
    d.arc([42, 48, 48, 54], 0, 180, fill=0)
    for y in (44, 49):
        d.line([(30, y), (14, y - 3)], fill=0)
        d.line([(50, y), (66, y - 3)], fill=0)
    # collar bell
    d.arc([30, 54, 50, 64], 0, 180, fill=0)
    d.ellipse([38, 60, 44, 66], fill=0)
    return im

def fox(frame):
    im = canvas(); d = ImageDraw.Draw(im)
    sway = 2 if frame else 0
    # fluffy tail (right)
    shape(d, [(56, 64), (74, 54 + sway), (78, 34), (66, 18), (56, 30)])
    d.arc([58, 20, 79, 50], 250, 110, fill=0)
    dither(d, (62, 28, 76, 52))
    # jacket
    shape(d, [(32, 58), (50, 58), (58, 78), (24, 78)])
    d.line([(34, 62), (40, 78)], fill=0)
    d.line([(48, 62), (42, 78)], fill=0)
    d.line([(33, 62), (24, 72)], fill=0)
    d.line([(49, 62), (58, 72)], fill=0)
    # long hair
    d.line([(26, 34), (14, 54 + sway), (22, 78)], fill=0, width=3)
    d.line([(54, 34), (66, 54 - sway), (58, 78)], fill=0, width=3)
    # head
    ellipse(d, [22, 16, 58, 56])
    # big ears
    shape(d, [(30, 26), (10, 0), (44, 16)])
    shape(d, [(50, 26), (70, 0), (36, 16)])
    dither(d, (18, 8, 30, 20))
    dither(d, (50, 8, 62, 20))
    # flower on left ear
    for px_, py_ in [(13, 11), (21, 11), (17, 5), (17, 17), (17, 11)]:
        d.ellipse([px_ - 3, py_ - 3, px_ + 3, py_ + 3], fill=255, outline=0)
        d.arc([px_ - 3, py_ - 3, px_ + 3, py_ + 3], 0, 360, fill=0)
    d.ellipse([15, 9, 19, 13], fill=0)
    # fringe + side locks
    d.arc([20, 8, 60, 42], 195, 345, fill=0)
    d.line([(27, 24), (31, 36)], fill=0)
    d.line([(53, 24), (49, 36)], fill=0)
    # muzzle + eyes
    ellipse(d, [35, 44, 47, 56])
    shape(d, [(38, 47), (44, 47), (41, 51)])
    d.arc([36, 50, 46, 58], 0, 180, fill=0)
    d.ellipse([31, 34, 41, 48], fill=0)
    d.ellipse([33, 36, 36, 40], fill=255)
    if frame:
        d.arc([47, 36, 57, 46], 200, 340, fill=0, width=2)
    else:
        d.ellipse([47, 34, 57, 48], fill=0)
        d.ellipse([50, 36, 53, 40], fill=255)
    dither(d, (24, 46, 32, 52))
    dither(d, (48, 46, 56, 52))
    return im

def rows(im):
    px = im.load(); out = []
    for y in range(S):
        v = 0
        for x in range(S):
            if px[x, y] == 0:
                v |= 1 << (S - 1 - x)
        out.append(v.to_bytes(S // 8, 'big'))
    return out

def c_array(name, data):
    lines = [f"static const uint8_t {name}[PET_H][{S // 8}] = {{"]
    for r in data:
        lines.append("  {" + ",".join(f"0x{b:02X}" for b in r) + "},")
    lines.append("};")
    return "\n".join(lines)

def main():
    header = ["#pragma once", f"#define PET_W {S}", f"#define PET_H {S}", ""]
    for name, fn in [("PET_CAT", cat), ("PET_FOX", fox)]:
        for f in (0, 1):
            header.append(c_array(f"{name}_{f}", rows(fn(f))))
            header.append("")
    open('firmware/epaper42_web/pet_sprites.h', 'w').write("\n".join(header))
    for name, fn in [("cat", cat), ("fox", fox)]:
        for f in (0, 1):
            fn(f).convert('1').resize((S * 4, S * 4), Image.NEAREST).save(f'/tmp/pet_{name}_{f}.png')
    print("wrote firmware/epaper42_web/pet_sprites.h")

if __name__ == '__main__':
    main()
