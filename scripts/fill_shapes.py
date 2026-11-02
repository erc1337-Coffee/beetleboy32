# Fills the holes of a generated LVGL font's glyphs, so outline drawings become solid silhouettes
# (Noto Emoji's monochrome glyphs are line art). Run by scripts/fonts.sh on the font_shapes_* files:
#
#   python3 scripts/fill_shapes.py [--bold U+1F4A4,...] [--close U+1F527,...] src/fonts/font_shapes_40.c
#
# --bold: those glyphs' strokes are also thickened by a pixel each side (line art with nothing to
# fill, like the zzz).
# --close: small gaps in those outlines are sealed before filling (the fill would leak out through
# them: the wrench's jaw at small sizes).
#
# Expects lv_font_conv's --bpp 4 --no-compress output: each glyph a continuous 4-bit stream (high
# nibble first), box_w x box_h. Everything the outside can't reach becomes fully opaque; the outer
# antialiased edge stays as it was.

import re
import sys

SOLID = 15
SEE_THROUGH = 8  # below this, a pixel lets the outside through


def unpack(data, w, h):
    px = []
    for i in range(w * h):
        b = data[i // 2]
        px.append(b >> 4 if i % 2 == 0 else b & 15)
    return px


def pack(px):
    out = bytearray((len(px) + 1) // 2)
    for i, v in enumerate(px):
        out[i // 2] |= v << 4 if i % 2 == 0 else v
    return out


def fill(px, w, h):
    outside = [False] * (w * h)
    stack = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while stack:
        x, y = stack.pop()
        if not (0 <= x < w and 0 <= y < h):
            continue
        i = y * w + x
        if outside[i] or px[i] >= SEE_THROUGH:
            continue
        outside[i] = True
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return [v if outside[i] else max(v, SOLID) if v < SEE_THROUGH else v for i, v in enumerate(px)]


def thicken(px, w, h):
    out = list(px)
    for y in range(h):
        for x in range(w):
            out[y * w + x] = max(px[yy * w + xx] for yy in range(max(0, y - 1), min(h, y + 2))
                                 for xx in range(max(0, x - 1), min(w, x + 2)))
    return out


def thin(px, w, h):
    out = list(px)
    for y in range(h):
        for x in range(w):
            out[y * w + x] = min(px[yy * w + xx] if 0 <= yy < h and 0 <= xx < w else 0
                                 for yy in range(y - 1, y + 2) for xx in range(x - 1, x + 2))
    return out


def main(path, bold, close):
    src = open(path).read()
    start = src.index("glyph_bitmap[] = {")
    body_start = src.index("\n", start) + 1
    body_end = src.index("};", body_start)
    body = src[body_start:body_end]
    data = bytearray(int(b, 16) for b in re.findall(r"0x[0-9a-fA-F]+", re.sub(r"/\*.*?\*/", "", body)))
    names = re.findall(r"/\*\s*(U\+[0-9A-F]+[^*]*)\*/", body)
    glyphs = [tuple(int(v) for v in m) for m in
              re.findall(r"\.bitmap_index = (\d+), \.adv_w = \d+, \.box_w = (\d+), \.box_h = (\d+)", src)]
    glyphs = [g for g in glyphs if g[1] and g[2]]  # id 0 is empty

    out = []
    for n, (index, w, h) in enumerate(glyphs):
        size = (w * h + 1) // 2
        px = unpack(data[index:index + size], w, h)
        code = names[n].split()[0] if n < len(names) else ""
        if code in close:  # thicker, filled, then back to size: the gaps stay shut
            px = thin(fill(thicken(px, w, h), w, h), w, h)
        px = fill(px, w, h)
        if code in bold:
            px = thicken(px, w, h)
        data[index:index + size] = pack(px)
        chunk = data[index:index + size]
        out.append("    /* %s*/\n" % (names[n] if n < len(names) else ""))
        for i in range(0, len(chunk), 16):
            out.append("    " + ", ".join("0x%x" % b for b in chunk[i:i + 16]) + ",\n")
        out.append("\n")
    open(path, "w").write(src[:body_start] + "".join(out).rstrip("\n").rstrip(",") + "\n" + src[body_end:])
    print("filled: %s (%d glyphs)" % (path, len(glyphs)))


if __name__ == "__main__":
    args, opts = sys.argv[1:], {"--bold": set(), "--close": set()}
    while args and args[0] in opts:
        opts[args[0]] = set(args[1].upper().split(","))
        args = args[2:]
    for p in args:
        main(p, opts["--bold"], opts["--close"])
