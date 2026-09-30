# Packaging Art

Two real screenshots, captured from the engine itself, that the storefront
and packaging art for every platform is built from:

- `screenshots/e1_riding.png` — Ecstatica 1, an outdoor riding scene
  (`data/e1/frame_dump_004.ppm`). E1 has no in-engine logo card, so this is
  also the background behind its drawn "ECSTATICA" wordmark.
- `screenshots/e2_logo.png` — Ecstatica 2's own opening title card
  (`data/e2/frame_dump_001.ppm`), already rendering "ECSTATICA II" in-engine
  in red gothic type on a stone texture. Used as-is for wide art; a plain
  stone corner of the same frame (away from the lettering) backs the small
  "E2" monogram crops.

`generate_packaging_art.py` (needs Pillow: `pip install pillow`) turns those
two images into every platform's required sizes and formats in one pass:

| Platform | Output |
|---|---|
| Vita | `vita/sce_sys/{e1,e2}/icon0.png`, `.../livearea/contents/{bg,startup}.png` |
| PSP | `psp/ICON0.PNG`, `psp/PIC1.PNG` (one shared EBOOT for both games) |
| Pocket | `pocket/core/icon.bin`, `pocket/core/platform_image.bin` (openfpgaOS's 2-bytes-per-pixel grayscale format — see the script) |
| Steam Deck | `steamdeck/{e1,e2}/{hero,grid_landscape,grid_portrait,icon,logo}.png` |

Re-run it after swapping in different source screenshots, or after tuning a
crop `focus` in the script:

```
python3 assets/generate_packaging_art.py
```

It always writes every target — there's no per-platform flag — so a run
touches all four platforms' art at once.
