#!/usr/bin/env python3
"""Generate the storefront/packaging art for Vita, PSP, Pocket and Steam
Deck from the two screenshots in assets/screenshots/ — real captures, not
mockups (see assets/README.md for where they came from).

Run from anywhere; paths below are all relative to the repo root.

    python3 assets/generate_packaging_art.py
"""
import os
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCREENSHOTS = f"{ROOT}/assets/screenshots"
FONT_BOLD = "/System/Library/Fonts/Supplemental/Georgia Bold.ttf"

E1_ACCENT = (232, 168, 64)     # warm amber, matches the outdoor riding shot
E2_ACCENT = (196, 32, 32)      # the red already baked into E2's own logo card

SRC = {
    # E1 has no in-engine logo card, so all its key art is the riding shot
    # (frame_dump_004.ppm) plus a drawn wordmark.
    "e1_hero": Image.open(f"{SCREENSHOTS}/e1_riding.png").convert("RGB"),
    # E2's frame_dump_001.ppm already renders "ECSTATICA II" in-engine — used
    # verbatim for wide formats, no drawn text on top of it.
    "e2_hero": Image.open(f"{SCREENSHOTS}/e2_logo.png").convert("RGB"),
}


def font(size):
    return ImageFont.truetype(FONT_BOLD, size)


def subcrop(img, box_frac):
    """Crop a sub-rectangle first, in fractional (l, t, r, b) coords, so a
    later cover() zooms into just that region instead of the whole frame."""
    w, h = img.size
    l, t, r, b = box_frac
    return img.crop((round(l * w), round(t * h), round(r * w), round(b * h)))


def cover(img, w, h, focus=(0.5, 0.42)):
    """Resize+crop to exactly (w,h), preserving aspect (cover, not fit).
    `focus` picks which part of the source survives the crop (x, y) in 0..1."""
    sw, sh = img.size
    scale = max(w / sw, h / sh)
    nw, nh = round(sw * scale), round(sh * scale)
    img = img.resize((nw, nh), Image.LANCZOS)
    fx, fy = focus
    x = min(max(0, round(nw * fx - w / 2)), nw - w)
    y = min(max(0, round(nh * fy - h / 2)), nh - h)
    return img.crop((x, y, x + w, y + h))


def darken_bottom(img, strength=0.65, from_frac=0.30):
    w, h = img.size
    out = img.convert("RGBA")
    grad = Image.new("L", (w, h), 0)
    px = grad.load()
    for y in range(h):
        t = max(0.0, (y - h * from_frac) / (h * (1 - from_frac)))
        v = int(255 * strength * t)
        for x in range(w):
            px[x, y] = v
    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    overlay.putalpha(grad)
    return Image.alpha_composite(out, overlay).convert("RGB")


def draw_title(img, text, accent, size, pos, anchor="lm", shadow=3):
    draw = ImageDraw.Draw(img)
    f = font(size)
    x, y = pos
    draw.text((x + shadow, y + shadow), text, font=f, fill=(0, 0, 0), anchor=anchor)
    draw.text((x, y), text, font=f, fill=accent, anchor=anchor)
    return img


def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print("wrote", os.path.relpath(path, ROOT), img.size)


# ---- wide/landscape key art: full-bleed screenshot, legible at a glance ---
def key_art(game, w, h, focus):
    src = SRC[f"{game}_hero"]
    img = cover(src, w, h, focus=focus)
    if game == "e1":
        # No in-engine logo to lean on — darken for contrast and draw one.
        img = darken_bottom(img, strength=0.7)
        size = max(10, round(h * 0.09))
        pad = round(h * 0.06)
        img = draw_title(img, "ECSTATICA", E1_ACCENT, size, (pad, h - pad), anchor="ls")
    # E2's frame already carries "ECSTATICA II" in dramatic red gothic type;
    # adding another wordmark on top would just clutter it.
    return img


# ---- square/portrait art: a screenshot crop can't carry a wide wordmark at
# this shape, so use a big letter monogram over a textured background
# instead — legible from a Vita bubble down to a 36px Pocket icon. ----------
def monogram_icon(game, w, h):
    if game == "e1":
        # A colourful, busy part of the riding shot reads well behind text.
        bg = cover(SRC["e1_hero"], w, h, focus=(0.40, 0.60))
        letter, accent = "E", E1_ACCENT
    else:
        # Zoom into a plain stone-texture corner of the same logo frame,
        # away from its baked-in lettering, so the two don't collide.
        cropped = subcrop(SRC["e2_hero"], (0.55, 0.55, 1.0, 1.0))
        bg = cover(cropped, w, h, focus=(0.5, 0.5))
        letter, accent = "E2", E2_ACCENT

    bg = bg.convert("RGBA")
    scrim = Image.new("RGBA", (w, h), (0, 0, 0, 90))
    bg = Image.alpha_composite(bg, scrim)

    # Fit by both height and width — two glyphs ("E2") need a smaller point
    # size than one ("E") to clear a narrow portrait canvas without clipping.
    size = round(h * 0.62)
    probe = ImageDraw.Draw(bg)
    while size > 4:
        box = probe.textbbox((0, 0), letter, font=font(size))
        if (box[2] - box[0]) <= w * 0.82:
            break
        size -= 2

    draw_title(bg, letter, accent, size, (w / 2, h / 2 + h * 0.04),
               anchor="mm", shadow=max(1, round(h * 0.02)))
    return bg.convert("RGB")


# ---------------------------------------------------------------- Vita ----
def build_vita():
    for game, focus_hero in (("e1", (0.5, 0.55)), ("e2", (0.5, 0.40))):
        d = f"{ROOT}/vita/sce_sys/{game}"
        save(key_art(game, 840, 500, focus_hero), f"{d}/livearea/contents/bg.png")
        save(key_art(game, 280, 158, focus_hero), f"{d}/livearea/contents/startup.png")
        save(monogram_icon(game, 128, 128), f"{d}/icon0.png")


# ----------------------------------------------------------------- PSP ----
def build_psp():
    # One shared EBOOT for both games (runtime-detected), so one generic set
    # that reads as "Ecstatica" rather than favouring either game.
    save(key_art("e1", 480, 272, (0.5, 0.5)), f"{ROOT}/psp/PIC1.PNG")
    icon0 = cover(SRC["e1_hero"], 144, 80, focus=(0.5, 0.55))
    save(ImageOps.autocontrast(icon0, cutoff=1), f"{ROOT}/psp/ICON0.PNG")


# --------------------------------------------------------------- Pocket ---
def encode_of_gray(img, path):
    """openfpgaOS image format: 2 bytes/pixel LE, high byte 0, low byte
    intensity (0xFF background / light, 0x00 ink / dark in the stock art)."""
    g = img.convert("L")
    w, h = g.size
    data = bytearray(w * h * 2)
    px = g.load()
    i = 0
    for y in range(h):
        for x in range(w):
            data[i] = px[x, y]
            i += 2
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote", os.path.relpath(path, ROOT), f"{w}x{h}", len(data), "bytes")


def build_pocket():
    # One core shared by both games — the banner uses E2's logo frame
    # (already reads as a title card at a glance); the icon is the "E2"
    # monogram, since 36px is too small for a full wordmark either way.
    banner = ImageOps.autocontrast(key_art("e2", 521, 165, (0.5, 0.42)).convert("L"), cutoff=1)
    encode_of_gray(banner.convert("RGB"), f"{ROOT}/pocket/core/platform_image.bin")

    icon = ImageOps.autocontrast(monogram_icon("e2", 36, 36).convert("L"), cutoff=0)
    encode_of_gray(icon.convert("RGB"), f"{ROOT}/pocket/core/icon.bin")


# ------------------------------------------------------------ Steam Deck --
def build_steam():
    for game, focus in (("e1", (0.5, 0.5)), ("e2", (0.5, 0.40))):
        d = f"{ROOT}/steamdeck/{game}"
        save(key_art(game, 460, 215, focus), f"{d}/grid_landscape.png")
        save(key_art(game, 1920, 620, focus), f"{d}/hero.png")
        save(monogram_icon(game, 600, 900), f"{d}/grid_portrait.png")
        save(monogram_icon(game, 256, 256), f"{d}/icon.png")

        title = "ECSTATICA" if game == "e1" else "ECSTATICA II"
        accent = E1_ACCENT if game == "e1" else E2_ACCENT
        canvas = Image.new("RGBA", (640, 360), (0, 0, 0, 0))
        draw_title(canvas, title, accent, 70, (320, 200), anchor="mm")
        save(canvas, f"{d}/logo.png")


if __name__ == "__main__":
    build_vita()
    build_psp()
    build_pocket()
    build_steam()
