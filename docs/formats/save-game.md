# save%d.dat

Player save file. `%d` = slot index.

Writer: `save_game()` in `src/file.c:769`. Reader: `load_game()` in `src/file.c:804`.

## Layout

```
le32   game_time                     # via putl / getlLoHi — LE
le32   game_timer

# Hero (actor 0)
be16   hero_pos.X
be16   hero_pos.Y
be16   hero_pos.Z
be16   hero_orientation.Y

# Active actors — fixed loop of ACTOR_HEAP_SIZE iterations
for i in 0..ACTOR_HEAP_SIZE-1:
    be16 actor_flags[i]
    if actor_flags[i] & 0x8:         # active bit
        be16 rep_name                # actor_rep_name[i]
        be16 pos.X, pos.Y, pos.Z     # actor_position[i]
        be16 orient.X, .Y, .Z        # actor_orientation[i]
        be16 hit_points              # actor_hit_points[i]
```

## Endianness quirk

- Timers use **little-endian** helpers (`putl` writes LE, `getlLoHi` reads LE).
- Every other field uses **big-endian** (`putw_be` / `getw_be`).

Mixed endianness is intentional — the timer path came from a different (later?) code path than the actor serializer. Do not "normalize"; the shipped saves depend on it.

## Load behavior

- `new_game()` is called first to reset state.
- For each actor with the active bit set, `load_a_thing(rep_name)` reloads the underlying thing template, then `actor_heap_arr[i] = *thing_tab[rep_name]` clones defaults before the saved fields overlay position / orientation / HP.
- Missing hero fields (X-orient, Z-orient) are not saved for the player — hero only gets Y-orient. Any rotation about X/Z at save time is lost.

## Limitations vs reference

Reference (Win95) save format includes: scene flags, inventory, code execution state, current camera. The current C port only persists the minimum listed above. E1..E5 in `PLAN.md` roadmap tracks full save/load parity as a Phase E task.
