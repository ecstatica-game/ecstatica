# Packaging Art

Finished art — not mockups — that the storefront and packaging art for
every platform is built from, three pieces per game:

- `screenshots/{e1,e2}-header.png` (460x215) — landscape key art, logo
  already baked into the painting/render. Used as-is for Steam's
  `grid_landscape.png`, and cropped/upscaled for every other platform's
  wide art (Vita `bg`/`startup`, PSP `PIC1`, Pocket banner, Steam `hero`).
- `screenshots/{e1,e2}-cover.png` (600x900) — portrait box art, title also
  baked in. Used as-is for Steam's `grid_portrait.png`, and cropped (away
  from its own title) as the background behind the small icon crops.
- `screenshots/{e1,e2}-logo.png` — standalone transparent wordmark, for
  spots that need just the logo rather than a full scene: Steam's
  `logo.png` overlay layer, and composited over the icon background crops
  above.

`generate_packaging_art.py` (needs Pillow: `pip install pillow`) turns those
six images into every platform's required sizes and formats in one pass:

| Platform | Output |
|---|---|
| Vita | `platforms/vita/sce_sys/{e1,e2}/icon0.png`, `.../livearea/contents/{bg,startup}.png` |
| PSP | `platforms/psp/ICON0.PNG`, `platforms/psp/PIC1.PNG` (one shared EBOOT for both games) |
| Pocket | `platforms/pocket/core/icon.bin`, `platforms/pocket/core/platform_image.bin` (openfpgaOS's 2-bytes-per-pixel grayscale format — see the script) |
| Steam Deck | `platforms/steamdeck/{e1,e2}/{hero,grid_landscape,grid_portrait,icon,logo}.png` |

Re-run it after swapping in different source art, or after tuning a crop
`focus` in the script:

```
python3 assets/generate_packaging_art.py
```

It always writes every target — there's no per-platform flag — so a run
touches all four platforms' art at once.
