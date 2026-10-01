#!/usr/bin/env python3
"""Generate the storefront/packaging art for Vita, PSP, Pocket and Steam
Deck from the source art in assets/screenshots/ (see assets/README.md for
where it came from).

Run from anywhere; paths below are all relative to the repo root.

    python3 assets/generate_packaging_art.py
"""
import os
from PIL import Image, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCREENSHOTS = f"{ROOT}/assets/screenshots"

SRC = {
    # Finished landscape key art (460x215) — each game's logo is already
    # baked into the painting/render, nothing drawn on top of it here.
    "e1_hero": Image.open(f"{SCREENSHOTS}/e1-header.png").convert("RGB"),
    "e2_hero": Image.open(f"{SCREENSHOTS}/e2-header.png").convert("RGB"),
    # Finished portrait box art (600x900), same deal — title baked in.
    "e1_cover": Image.open(f"{SCREENSHOTS}/e1-cover.png").convert("RGB"),
    "e2_cover": Image.open(f"{SCREENSHOTS}/e2-cover.png").convert("RGB"),
    # Standalone transparent wordmarks, for spots that need just the logo
    # (a Steam logo.png layer, a small icon) rather than a full scene.
    "e1_logo": Image.open(f"{SCREENSHOTS}/e1-logo.png").convert("RGBA"),
    "e2_logo": Image.open(f"{SCREENSHOTS}/e2-logo.png").convert("RGBA"),
}


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


def paste_logo(canvas, logo, w_frac, center):
    """Alpha-composite the transparent wordmark onto canvas, scaled to
    w_frac of canvas width and centered at `center`."""
    canvas = canvas.convert("RGBA")
    cw, _ = canvas.size
    lw, lh = logo.size
    tw = max(1, round(cw * w_frac))
    th = max(1, round(lh * tw / lw))
    resized = logo.resize((tw, th), Image.LANCZOS)
    cx, cy = center
    canvas.alpha_composite(resized, (round(cx - tw / 2), round(cy - th / 2)))
    return canvas


def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print("wrote", os.path.relpath(path, ROOT), img.size)


# ---- wide/landscape key art: full-bleed crop of the finished header art,
# logo already baked in by the source painting/render. ----------------------
def key_art(game, w, h, focus):
    return cover(SRC[f"{game}_hero"], w, h, focus=focus)


# ---- square/small icon art: a slice of the cover art, cropped away from
# its own baked-in title so the pasted wordmark logo doesn't collide with
# it, legible from a Vita bubble down to a 36px Pocket icon. ----------------
def logo_icon(game, w, h):
    if game == "e1":
        # Title sits in the top strip of the cover; crop below it onto the
        # dragon's head, the most recognisable part of the painting.
        cropped = subcrop(SRC["e1_cover"], (0.0, 0.19, 1.0, 1.0))
        bg = cover(cropped, w, h, focus=(0.5, 0.35))
    else:
        # Title is a vertical band down the right edge of the cover; crop
        # to the stone-and-blood texture left of it.
        cropped = subcrop(SRC["e2_cover"], (0.0, 0.0, 0.78, 1.0))
        bg = cover(cropped, w, h, focus=(0.35, 0.55))

    bg = bg.convert("RGBA")
    scrim = Image.new("RGBA", (w, h), (0, 0, 0, 70))
    bg = Image.alpha_composite(bg, scrim)
    return paste_logo(bg, SRC[f"{game}_logo"], 0.82, (w / 2, h / 2)).convert("RGB")


# ---------------------------------------------------------------- Vita ----
def quantize_for_vita(img):
    """Real Vita firmware's LiveArea processor rejects truecolor sce_sys
    PNGs (VitaShell install error 0x8010113D) — it needs 8-bit indexed
    color, same as running the art through pngquant."""
    return img.convert("P", palette=Image.ADAPTIVE, colors=256)


def build_vita():
    for game, focus_hero in (("e1", (0.5, 0.55)), ("e2", (0.5, 0.40))):
        d = f"{ROOT}/platforms/vita/sce_sys/{game}"
        save(quantize_for_vita(key_art(game, 840, 500, focus_hero)), f"{d}/livearea/contents/bg.png")
        save(quantize_for_vita(key_art(game, 280, 158, focus_hero)), f"{d}/livearea/contents/startup.png")
        save(quantize_for_vita(logo_icon(game, 128, 128)), f"{d}/icon0.png")


# ----------------------------------------------------------------- PSP ----
def build_psp():
    # One shared EBOOT for both games (runtime-detected), so one generic set
    # that reads as "Ecstatica" rather than favouring either game.
    save(key_art("e1", 480, 272, (0.5, 0.5)), f"{ROOT}/platforms/psp/PIC1.PNG")
    icon0 = cover(SRC["e1_hero"], 144, 80, focus=(0.5, 0.55))
    save(ImageOps.autocontrast(icon0, cutoff=1), f"{ROOT}/platforms/psp/ICON0.PNG")


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
    # One core shared by both games — the banner uses E2's header art
    # (already reads as a title card at a glance); the icon is E2's logo
    # over a cropped slice of its cover, since 36px is too small for a
    # full scene either way.
    banner = ImageOps.autocontrast(key_art("e2", 521, 165, (0.5, 0.42)).convert("L"), cutoff=1)
    encode_of_gray(banner.convert("RGB"), f"{ROOT}/platforms/pocket/core/platform_image.bin")

    icon = ImageOps.autocontrast(logo_icon("e2", 36, 36).convert("L"), cutoff=0)
    encode_of_gray(icon.convert("RGB"), f"{ROOT}/platforms/pocket/core/icon.bin")


# ------------------------------------------------------------ Steam Deck --
def build_steam():
    for game, focus in (("e1", (0.5, 0.5)), ("e2", (0.5, 0.40))):
        d = f"{ROOT}/platforms/steamdeck/{game}"
        save(key_art(game, 460, 215, focus), f"{d}/grid_landscape.png")
        save(key_art(game, 1920, 620, focus), f"{d}/hero.png")
        save(cover(SRC[f"{game}_cover"], 600, 900), f"{d}/grid_portrait.png")
        save(logo_icon(game, 256, 256), f"{d}/icon.png")

        canvas = Image.new("RGBA", (640, 360), (0, 0, 0, 0))
        canvas = paste_logo(canvas, SRC[f"{game}_logo"], 0.72, (320, 180))
        save(canvas, f"{d}/logo.png")


if __name__ == "__main__":
    build_vita()
    build_psp()
    build_pocket()
    build_steam()
