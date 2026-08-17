#!/usr/bin/env python3
"""Generate the openFPGA core icon (Cores/<id>/icon.bin).

Format, read off the icons shipped with working openfpgaOS cores: 36x36
pixels, 2 bytes per pixel, little-endian, high byte always zero and the low
byte carrying intensity. Background is 0xff and the glyphs are 0x00, so the
art is dark ink on a light field.

    ./mkicon.py ../core/icon.bin
"""

import sys

W = H = 36

# 5x7 glyphs, one string per row, '#' = ink.
FONT = {
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
}

GLYPH_W, GLYPH_H = 5, 7
ADVANCE = GLYPH_W + 1


def draw_text(px, text, top):
    """Blit `text` centred horizontally with its top edge at row `top`."""
    width = len(text) * ADVANCE - 1
    left = (W - width) // 2
    for i, ch in enumerate(text):
        glyph = FONT[ch]
        x0 = left + i * ADVANCE
        for gy in range(GLYPH_H):
            for gx in range(GLYPH_W):
                if glyph[gy][gx] == "#":
                    x, y = x0 + gx, top + gy
                    if 0 <= x < W and 0 <= y < H:
                        px[y][x] = 0x00


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "icon.bin"

    px = [[0xFF] * W for _ in range(H)]

    # "ECSTA" / "TICA" stacked — 36px is too narrow for the name on one line.
    draw_text(px, "ECSTA", 9)
    draw_text(px, "TICA", 20)

    # A one-pixel border frames the tile the way the stock icons do.
    for x in range(W):
        px[0][x] = px[H - 1][x] = 0x00
    for y in range(H):
        px[y][0] = px[y][W - 1] = 0x00

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
