#!/usr/bin/env python3
"""Generate the openFPGA platform banner (Platforms/_images/<platform>.bin).

521x165, 2 bytes per pixel, little-endian, high byte zero and the low byte
carrying intensity — the same encoding as the core icon, read off the banners
shipped with working cores. Background is 0xff, ink 0x00.

Writing our own removes the last piece of third-party artwork from the
deployment, and replaces the generic openfpgaOS banner with the game's name.

    ./mkbanner.py ../core/platform_image.bin
"""

import sys

from mkicon import FONT, GLYPH_W, GLYPH_H

W, H = 521, 165

BG, INK = 0xFF, 0x00


def draw_text(px, text, scale, cx, top):
    """Blit `text` at `scale`, horizontally centred on `cx`, top edge `top`."""
    advance = (GLYPH_W + 1) * scale
    width = len(text) * advance - scale
    left = cx - width // 2
    for i, ch in enumerate(text):
        glyph = FONT[ch]
        x0 = left + i * advance
        for gy in range(GLYPH_H):
            for gx in range(GLYPH_W):
                if glyph[gy][gx] != "#":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        x, y = x0 + gx * scale + sx, top + gy * scale + sy
                        if 0 <= x < W and 0 <= y < H:
                            px[y][x] = INK


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "platform_image.bin"

    px = [[BG] * W for _ in range(H)]

    # "ECSTATICA" at scale 8: 9 glyphs * 6 * 8 = 432 wide, 56 tall.
    draw_text(px, "ECSTATICA", 8, W // 2, (H - GLYPH_H * 8) // 2)

    # Double rule top and bottom, matching the framed look of the stock art.
    for x in range(W):
        for y in (0, 1, H - 2, H - 1):
            px[y][x] = INK
    for y in range(H):
        for x in (0, 1, W - 2, W - 1):
            px[y][x] = INK

    data = bytearray()
    for y in range(H):
        for x in range(W):
            data += bytes((px[y][x], 0x00))

    assert len(data) == W * H * 2, len(data)
    with open(out, "wb") as f:
        f.write(data)
    print(f"wrote {out} ({len(data)} bytes, {W}x{H})")


if __name__ == "__main__":
    main()
