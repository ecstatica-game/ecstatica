# SHADOW.DAT

Raw dump of the precomputed shadow palette lookup table.

Reader: `load_shadow_tab()` in `src/init.c:500`.

## Layout

```
u8 shadow_tab[3][16][256]
```

Total: **12288 bytes**. Read as one contiguous `fread`.

## Semantics

- Dimension 0 (`3`): shadow variant — used by `ellipse.c:110-115`:
  - `[0][color][*]` → primary shadow-body table (`wSbMod_tab1`).
  - `[1][color][*]` → shadow-edge table (`wSeMod_tab`).
  - `[2][color][*]` → secondary shadow-body table (`wSbMod_tab2`).
- Dimension 1 (`16`): source part color (`part->color`, 0..15).
- Dimension 2 (`256`): source pixel index → shadowed pixel index remap.

Generated at editor time via `fill_in_shadow_tab()` (`init.c:496`, no-op at runtime) from the base color map. Only `SHADOW.DAT` is loaded at runtime.
