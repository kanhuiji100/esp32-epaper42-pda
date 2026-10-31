#!/usr/bin/env python3
"""Generate 48x48 1bpp pet sprites (cat / fox) as a C header.
Original chibi pixel art inspired by the user's cat.jpg / fox.jpg references:
cat = lavender twin-tail catgirl with a bow on her tail; fox = pink foxgirl with
big ears, a flower and a fluffy tail.
"""
from PIL import Image, ImageDraw

S = 48

def canvas():
    return Image.new('L', (S, S), 255)

def dither(d, box, step=2):
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        for x in range(x0, x1):
            if ((x + y) % step) == 0:
                d.point((x, y), fill=0)

def outline(d, pts, fill=255, width=1):
    d.polygon(pts, fill=fill, outline=0)

def cat(frame):
    im = canvas(); d = ImageDraw.Draw(im)
    sway = 2 if frame else 0
    # tail (left) with a bow
    d.line([(14, 30), (7, 24 + sway), (5, 18)], fill=0, width=3)
    d.polygon([(5, 16), (0, 11), (1, 20)], fill=255, outline=0)
    d.polygon([(5, 16), (10, 11), (9, 20)], fill=255, outline=0)
    d.ellipse([3, 14, 7, 18], fill=0)
    # dress / body
    outline(d, [(17, 31), (31, 31), (35, 46), (13, 46)])
    d.line([(17, 34), (10, 40)], fill=0, width=2)
    d.line([(31, 34), (38, 40)], fill=0, width=2)
    # twin tails
    d.line([(11, 17), (5, 30 + sway), (9, 43)], fill=0, width=3)
    d.line([(37, 17), (43, 30 - sway), (39, 43)], fill=0, width=3)
    dither(d, (7, 20, 12, 40), 2)
    dither(d, (36, 20, 41, 40), 2)
    # head
    d.ellipse([10, 7, 38, 35], fill=255, outline=0)
    # ears
    outline(d, [(12, 13), (7, 1), (21, 8)])
    outline(d, [(36, 13), (41, 1), (27, 8)])
    dither(d, (12, 5, 18, 12), 2)
    dither(d, (30, 5, 36, 12), 2)
    # bangs
    d.arc([8, 2, 40, 30], 190, 350, fill=0)
    d.line([(12, 12), (16, 20)], fill=0)
    d.line([(36, 12), (32, 20)], fill=0)
    # eyes
    d.ellipse([15, 19, 21, 25], fill=0)
    d.ellipse([16, 20, 18, 22], fill=255)
    if frame:
        d.line([(26, 22), (33, 22)], fill=0, width=2)
    else:
        d.ellipse([27, 19, 33, 25], fill=0)
        d.ellipse([29, 20, 31, 22], fill=255)
    # blush + smile
    dither(d, (11, 25, 16, 29), 2)
    dither(d, (32, 25, 37, 29), 2)
    d.arc([21, 24, 29, 31], 0, 180, fill=0)
    return im

def fox(frame):
    im = canvas(); d = ImageDraw.Draw(im)
    sway = 2 if frame else 0
    # fluffy tail (right)
    d.line([(36, 30), (44, 25 + sway), (46, 17), (41, 11)], fill=0, width=3)
    d.arc([36, 8, 47, 26], 270, 90, fill=0)
    dither(d, (38, 12, 46, 28), 2)
    # jacket / body
    outline(d, [(16, 32), (32, 32), (36, 46), (12, 46)])
    d.line([(20, 34), (24, 46)], fill=0)
    d.line([(28, 34), (24, 46)], fill=0)
    d.line([(16, 35), (9, 41)], fill=0, width=2)
    d.line([(32, 35), (39, 41)], fill=0, width=2)
    # long hair
    d.line([(13, 18), (8, 32 + sway), (12, 44)], fill=0, width=3)
    d.line([(35, 18), (40, 32 - sway), (36, 44)], fill=0, width=3)
    dither(d, (10, 22, 14, 42), 2)
    dither(d, (34, 22, 38, 42), 2)
    # head
    d.ellipse([10, 8, 38, 36], fill=255, outline=0)
    # big fox ears
    outline(d, [(13, 14), (6, 0), (22, 8)])
    outline(d, [(35, 14), (42, 0), (26, 8)])
    dither(d, (11, 4, 18, 12), 2)
    dither(d, (30, 4, 37, 12), 2)
    # flower on the left ear (white petals, black outline)
    for px_, py_ in [(8, 3), (12, 3), (10, 1), (10, 6), (10, 3)]:
        d.ellipse([px_ - 1, py_ - 1, px_ + 1, py_ + 1], fill=255, outline=0)
    d.ellipse([9, 2, 11, 4], fill=0)
    # bangs
    d.arc([8, 3, 40, 31], 190, 350, fill=0)
    d.line([(13, 13), (17, 21)], fill=0)
    d.line([(35, 13), (31, 21)], fill=0)
    # eyes
    d.ellipse([15, 20, 21, 26], fill=0)
    d.ellipse([16, 21, 18, 23], fill=255)
    if frame:
        d.line([(27, 23), (33, 23)], fill=0, width=2)
    else:
        d.ellipse([27, 20, 33, 26], fill=0)
        d.ellipse([29, 21, 31, 23], fill=255)
    # smile + blush
    d.arc([21, 25, 29, 32], 0, 180, fill=0)
    dither(d, (11, 26, 16, 30), 2)
    dither(d, (32, 26, 37, 30), 2)
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
    header = ["#pragma once", "#define PET_W 48", "#define PET_H 48", ""]
    for name, fn in [("PET_CAT", cat), ("PET_FOX", fox)]:
        for f in (0, 1):
            header.append(c_array(f"{name}_{f}", rows(fn(f))))
            header.append("")
    open('firmware/epaper42_web/pet_sprites.h', 'w').write("\n".join(header))
    for name, fn in [("cat", cat), ("fox", fox)]:
        for f in (0, 1):
            fn(f).convert('1').resize((S * 6, S * 6), Image.NEAREST).save(f'/tmp/pet_{name}_{f}.png')
    print("wrote firmware/epaper42_web/pet_sprites.h")

if __name__ == '__main__':
    main()
