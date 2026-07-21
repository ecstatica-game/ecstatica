# .RAW Image

Uncompressed 8-bpp indexed image with an embedded VGA palette. Used for logos, title screens, static graphics, and per-camera reference stills.

Files: `PALLETTE.RAW`, `TSCREEN.RAW`, `TITLE_S.RAW`, `TITLE1.RAW..TITLE7.RAW`, `PSYGLOGO.RAW`, `AASGLOGO.RAW`, `ASLOGO.RAW`, `ASSP.RAW`, `END*.RAW`, `graphics/*.RAW`.

Readers: `load_logo()` / `load_def_pallette()` / `load_background()` in `src/init.c:330`, `src/init.c:360`, `src/init.c:384`; generic `load_raw_graphic()` in `src/topo.c:657`.

## Layout

```
u8[32]   header               # bitmap_hdr_t
u8[768]  palette              # 256 × { R, G, B } — VGA DAC (0..63), each channel u8
u8[W*H]  pixels               # row-major, 8-bpp indexed
```

Total on-disk size = 32 + 768 + W × H.

## Header (`bitmap_hdr_t`, `src/game.h:747`)

Signature is `mhwanh` at offset 0 (Amiga DPaint IFF-like magic reused). All int16 fields are **big-endian** on disk; the loader byteswaps via `reverse_char_word_val()`.

```
offset  type   name       notes
  0x00  u8[6]  signature  "mhwanh"
  0x06  be16   field_6    typically 0x0004
  0x08  be16   size_x     image width, pixels
  0x0A  be16   size_y     image height, pixels
  0x0C  be16   field_C    0x0100 in shipped files
  0x0E  be16   field_E
  0x10  be16   field_10
  0x12  be16[7] padding   0x00
```

Example (`TITLE_S.RAW`): `6d 68 77 61 6e 68 00 04 01 40 00 c8` → 320 × 200.

## Palette

256 entries of R/G/B bytes, DAC range **0..63** on disk. Loaders shift right by 2 to normalize to `palette_entry_t`:

```c
spare_cmap[i].R = pal[i*3+0] >> 2;
spare_cmap[i].G = pal[i*3+1] >> 2;
spare_cmap[i].B = pal[i*3+2] >> 2;
```

## Pixels

Row-major, top-to-bottom, left-to-right. Values are palette indices. Some loaders remap indices `0` and `1` to transparent (-1) at load time — e.g. `game.c:2402` for interface graphics.

Screen-sized RAWs (`TSCREEN.RAW`, `logos`) are blitted into `bitmap[3]` at load, then `clip_blit`'d to planes 0 and 1.
