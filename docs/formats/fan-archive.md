# .FAN Archive

Ecstatica 2's primary resource container. Tagged-block stream — every block starts with a 1-byte type tag, followed by a type-specific payload. Terminated by `0xFF`.

Files: `CODE\ECSTATIC.FAN` (main archive), `things/<index>.fan` (individual thing dumps), and offset-indexed reads from the merged archive via `OFFSETS`.

Reader: `merge_file_contents()` in `src/file.c:275`.

All integers below are **big-endian** unless marked otherwise. Names are Pascal-style (1 length byte + chars, no NUL).

## Top-level stream

```
loop:
    u8  block_type
    switch block_type:
        0x01: thing        (file_read_thing)
        0x02: action       (file_read_action)
        0x03: scene        (file_read_scene)
        0x04: code         (file_read_code)
        0x05: sound        (file_read_sound)      # little-endian body — see sound-block.md
        0x06: repertoire   (file_read_repertoire)
        0x07: map_area     (file_read_map_area)
        0x08: texture      (file_read_texture)
        0xFF: end-of-stream, stop
```

Unknown tags abort the loader.

## Block 0x01 — Thing (actor template)

```
be16 name_index               # index into thing_names[], THING_TAB_SIZE
u8   name_len                 # ≤25
u8[name_len] name             # ASCII, no NUL
be16 flags
be16 pos.X, pos.Y, pos.Z      # world position (int16)
be16 rot.X, rot.Y, rot.Z      # Euler rotation (int16)
be16 num_parts
part[num_parts]
be16 num_tris
triangle[num_tris]
be16 num_points
point[num_points]
```

### Part (ellipsoid body)
```
be16 name_index
be16 offset.X, .Y, .Z         # local offset
be16 rotate.X, .Y, .Z         # local Euler
be16 squash.X, .Y, .Z         # semi-axes
u8   color
u8   type
be16 flags
be16 parent_part_link         # -1 for root
```

### Triangle
```
be16 point1_idx               # stored as index; resolved to point_heap_t* post-load
be16 point2_idx
be16 point3_idx
be16 shade_name
u8   color3
u8   use_flag
be16 flags
```

### Point
```
be16 point_index
be16 offset.X, .Y, .Z
be16 parent_part_index        # resolved post-load
be16 use_flag
```

## Block 0x02 — Action (animation)

```
be16 name_index               # ACTION_TAB_SIZE
be16 thing_name_index         # actor this action targets
be16 act_duration
be16 flags                    # field_C_flags
be16 num_keys
key[num_keys]
```

### Key (keyframe)
```
be16 key_position             # timeline position (ticks)
u8   field_E
be16 num_events
event[num_events]
```

### Event
```
u8   event_type
u8   event_index
be16 param1
be16 param2
be16 param3
```

Event types are the `CT_*` opcodes used by the runtime (see `game.c` dispatchers).

## Block 0x03 — Scene

```
be16 name_index               # SCENE_TAB_SIZE
be16 scene_use_flag
be16 field_C
be16 num_actions              # up to 18 script slots retained
be16 action_ref[num_actions]  # names of actions bound to scene
```

## Block 0x04 — Code (compiled script tokens)

```
be16 name_index               # CODE_TAB_SIZE
be16 token_count              # sanity clamp: < 10000
be16 tokens[token_count]      # appended to global token_store[]
```

The `code_t` records only the base index into `token_store`; token count is implicit via the next code's base.

## Block 0x05 — Sound

**Little-endian body** (all others are BE). See [sound-block.md](sound-block.md).

```
le16 name_index               # SOUND_TAB_SIZE
le16 use_flag                 # OR'd with 1 on load
le16 field_10
le32 stored_length            # includes 32-byte header
le16 volume                   # defaults to 100 if ≤0
u8[32]  header                # opaque; likely sample-rate / loop metadata
u8[stored_length - 32] pcm    # unsigned 8-bit PCM
```

## Block 0x06 — Repertoire (action set)

```
be16 name_index               # REPERTOIRE_TAB_SIZE
be16 thing_name_index
be16 flags
be16 num_actions              # up to 208 stored
be16 action_ref[num_actions]
```

## Block 0x07 — Map Area

```
be16 name_index               # MAP_AREA_TAB_SIZE
be16 num_elements             # clamped to 10
be16 element_num[num_elements]
```

## Block 0x08 — Texture

```
be16 name_index               # TEXTURE_TAB_SIZE
be16 size_x
be16 size_y
u8[size_x * size_y] pixels    # 8-bpp indexed; sanity clamp < 0x100000
```

## Load-time linkage

- `thing_tab[name_index]` / `scene_tab[]` / `action_tab[]` etc. hold the loaded pointer.
- `next_thing1`, `next`, `next_scene`, `next_code`, `next_rep` chain into per-type lists.
- Triangle `point1..3` fields are stored as **indices** and remain casted `(point_heap_t *)(intptr_t)idx` until fixup; do not deref before fixup completes.
- Points join a global `point_list` (not per-actor) and are matched to parts via `field_A` at bind time.
