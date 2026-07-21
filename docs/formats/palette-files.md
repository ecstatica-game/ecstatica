# Per-Camera Palette (`views/NNNN.pal` / `.pa2`)

Compact VGA palette + control block for each camera view. `.pal` = normal, `.pa2` = intro/cutscene variant. `NNNN` = 4-digit `selected_camera`.

Reader: `load_pallette()` in `src/topo.c:696`.

## Layout

```
u8[2]    lead              # first 2 bytes of view_cmap overwritten; effectively a small header
u8[24]   pallette_control  # runtime blend/animation control block
u8[768]  palette           # 256 × { R, G, B }, DAC range 0..63
```

Total: **794 bytes**.

Load pattern (fread order matters — 2-byte lead is written into `view_cmap` first as a placeholder, then overwritten by the 768-byte palette):

```c
fread(&view_cmap,        1,  2, stream);
fread(&pallette_control, 1, 24, stream);
fread(&view_cmap,        1, 768, stream);
```

## Fixups

After load:

1. If `selected_camera != 0`, entries **8..15** are restored from the base `colour_map` (UI colors preserved across camera changes).
2. `fade_cmap` is mirrored from `view_cmap` so `check_fade` interpolates from the new palette rather than the old one. (Missing this mirroring was bug 15 in `PLAN.md` §8.)

## Offset-indexed variant

When `load_by_offset` is set and `intro_flag == 0`, palettes are read from the merged `.FAN` blob at `palette_offset[selected_camera]` instead of individual files. Layout inside the blob is identical (794 bytes).

If `palette_offset[selected_camera] < 0`, palette falls back to `colour_map`.

## Related

- `palette_offset[PALETTES_MAX]` and `visib_offset[PALETTES_MAX]` are indexed together — visibility maps use a parallel `.vis` scheme not yet documented.
