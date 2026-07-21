# Packed Camera Background (`hires/NNNN.raw`)

Per-camera pre-rendered background with paired 16-bit heightmap for z-buffered actor compositing. `NNNN` is a 4-digit zero-padded `selected_camera` index.

Reader: `load_raw()` in `src/topo.c:606`. Unpackers: `unpack_bitmap()` / `unpack_mask()` in `src/asm_f.c:110` / `asm_f.c:167`.

## Top-level dispatch

```
le16 signature
if signature == 0x686D ('mh')      # legacy mhwanh-style: uncompressed
    u8[0x1E]        rest_of_hdr    # remaining bitmap_hdr_t bytes
    u8[0x300]       palette        # 768-byte VGA palette (0..63 DAC)
    u8[W*H]         bitmap         # 8-bpp indexed pixels → bitmap[3]
    u8[2*W*H]       mask           # 16-bit heightmap → mask_map[2]
else                               # packed variant
    le32 size_of_background
    le32 size_of_heightmap
    u8[size_of_background + size_of_heightmap] packed_blob
    # blob = [packed bitmap][packed heightmap]
    unpack_bitmap(bitmap[3],   blob)
    unpack_mask  (mask_map[2], blob + size_of_background)
```

`W = screen_width`, `H = screen_height` (typically 640 × 480 for E2 hi-res).

Sanity check: `size_of_background + size_of_heightmap` must be `< 2 * W * H`, else `quit("packed info too big!")`.

## `unpack_bitmap` — 8-bit indexed decompressor

Header-byte-driven RLE with 4-bit signed delta option. Header byte layout:

```
bit 7..2   length       # output-byte count of this span
bit 1..0   type
```

`length == 0` → end-of-stream.

| Type | Meaning |
|-----:|---|
| 0 | Packed deltas. Each src byte = two signed 4-bit deltas. **Low nibble emitted first**, then high. Deltas add to a running u8 accumulator (persists across spans). `length` counts *outputs*; consumes `ceil(length/2)` src bytes. If `length` is odd, the final high nibble is unused. |
| 1 | Fill. One src byte → new accumulator value → written `length` times. |
| 2 | Copy. `length` raw src bytes emitted verbatim; each updates the accumulator. |
| 3 | Same as type 1 (fill). |

## `unpack_mask` — 16-bit heightmap decompressor

Same header semantics, but outputs are int16 and all deltas / raw values are **`<< 2` scaled**. Accumulator is int16 and persists across spans.

| Type | Meaning |
|-----:|---|
| 0 | Two signed 4-bit deltas per src byte, each `<< 2`. Low-nibble-first. |
| 1 | Signed 8-bit delta per src byte, `<< 2`. |
| 2 | Raw le16 per output, `<< 2`. Accumulator = last value. |
| 3 | Fill: one le16 (`<< 2`) repeated `length` times. |

## Post-load

`load_raw()` then calls:
- `load_visibility_map()` — visibility/PVS.
- `load_pallette(NULL)` — see [palette-files.md](palette-files.md).
- `clip_mask(2, 1, ...)` and `clip_mask(2, 0, ...)` — clamp heightmap to viewport.

## Related bugs from PLAN.md

- Endianness / nibble order in `unpack_bitmap` and `unpack_mask` was originally wrong on port; both fixed 2026-06-23. If reimplementing, mirror the low-nibble-first order exactly — the accumulator makes any deviation silently corrupt.
