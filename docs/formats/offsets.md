# OFFSETS / OFF2 Index

Fixed-layout table of 32-bit **big-endian** offsets into the merged `.FAN` blob, used for lazy per-resource loading. Present under `data/e1/OFFSETS`, `data/e1/OFF2`, `data/e2/…`. Shipped size (E1): **14100 bytes** each.

Reader: `read_offsets_file()` in `src/file.c:172`.

## Layout

Read as a straight sequence of be32 values in this order:

```
scene_offset     [SCENE_TAB_SIZE]
actor_offset     [THING_TAB_SIZE]
action_offset    [ACTION_TAB_SIZE]
repertoire_offset[REPERTOIRE_TAB_SIZE]
sound_offset     [SOUND_TAB_SIZE]
tune_offset      [10][96]                  # 960 entries: [driver][slot]
palette_offset   [PALETTES_MAX = 1200]
visib_offset     [PALETTES_MAX = 1200]
```

`OFFSETS` and `OFF2` share the same layout; `OFF2` is a secondary/alternate index (details TBD — currently identical size, differing content).

## Semantics

- Offset value = absolute byte offset into the merged `.FAN` blob. Negative (typically -1 / 0xFFFFFFFF) means "no such resource".
- Load path (`load_by_offset_index`, `file.c:727`):
  1. `fseek(file_pointer, file_offsets[index], SEEK_SET)`
  2. `merge_file_contents(file_pointer)` — parses one tagged block starting at that offset (see [fan-archive.md](fan-archive.md)).
- Per-type wrappers translate a domain index to a global slot:
  ```
  thing i        → offset index  i
  sound i        → i + THING_TAB_SIZE
  scene i        → i + THING_TAB_SIZE + SOUND_TAB_SIZE
  repertoire i   → i + THING_TAB_SIZE + SOUND_TAB_SIZE + SCENE_TAB_SIZE
  ```
  (Palette / tune / visibility have their own arrays, not this shared index space.)

## Tune offset table (10 × 96)

`tune_offset[driver][slot]`. Driver index corresponds to sound-card driver family:

| Driver idx | Files under `MUSIC/` |
|-----:|---|
| 0..? | `AWE0.BIN` (SB AWE / EMU8000 wavetable) |
| | `GUS0.BIN` (Gravis UltraSound wavetable) |
| | `SBL0.BIN` (Sound Blaster / OPL) |
| | `SCC0.BIN` (SCC / general MIDI) |
| | `LAP0.BIN` (LAPC / MT-32) |

Exact mapping still to be confirmed against the E2 driver init code. See [music-bin.md](music-bin.md).
