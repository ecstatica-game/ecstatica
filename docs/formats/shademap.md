# shademap.dat

Precomputed 128×128 shade + z-profile table consumed by the ellipsoid rasterizer.

Reader: `load_shade_map()` in `src/init.c:1194`.

## Layout

Interleaved cell-by-cell, row-major:

```
for y in 0..127:
    for x in 0..127:
        u8   shade_map[y][x]   # shade index (bit 7 = "off-surface" cull flag)
        be16 profile[y][x]     # z-profile displacement, int16
```

Total: 128 × 128 × 3 = **49152 bytes**.

## Usage

Consumed by `ellipse_line_win95()` and friends in `src/asm_f.c:289`. Each column of an ellipsoid indexes into the table via `shadeMapPointer` (16.16 fixed):

```c
ebp = shadeMapPointer >> 16;
if (!(shade_map[0][ebp] & 0x80)) {                     // 0x80 = past-limb cull
    ax = (profile[0][ebp] * wElMod_zsize + midpoint) >> 16;
    if (mask[maskPtr] > ax) { ... write pixel ... }
}
```

- `shade_map` byte high bit (`0x80`) → skip pixel (outside ellipsoid silhouette).
- `shade_map` low 7 bits → row index into `shade_tab` (material shading LUT).
- `profile` → per-column depth offset, scaled by ellipsoid Z half-axis.

`shade_map`/`profile` are accessed as flat arrays in tight loops (`shade_map[0][index]`) — do not add per-row padding.
