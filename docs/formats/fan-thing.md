# Thing (Actor Model)

A Thing is the core 3D actor model in the FAN format. Each Thing is a hierarchy of ellipsoidal parts with attachment points and triangular face definitions. Things appear in two reading paths: the tagged-block stream (`merge_file_contents`) and the event stream (`merge_seeked_file`).

All integers are **big-endian int16** (`be16`) unless noted otherwise. Single-byte fields are unsigned (`u8`).

## Tagged-block format (block type `0x01`)

Reader: `file_read_thing()` in `file.c:424`.

### Thing header

| Offset | Size | Field              | Description                              |
|--------|------|--------------------|------------------------------------------|
| 0      | 2    | `name_index`       | Index into `thing_tab[]` runtime array   |
| 2      | 2    | `flags`            | Actor flags                              |
| 4      | 2    | `position.X`       | World position X                         |
| 6      | 2    | `position.Y`       | World position Y                         |
| 8      | 2    | `position.Z`       | World position Z                         |
| 10     | 2    | `rotate.X`         | Euler rotation X                         |
| 12     | 2    | `rotate.Y`         | Euler rotation Y                         |
| 14     | 2    | `rotate.Z`         | Euler rotation Z                         |
| 16     | 2    | `num_parts`        | Part count (N)                           |
| 18     | var  | `part[N]`          | N Part records (see below)               |
| ...    | 2    | `num_tris`         | Triangle count (T)                       |
| ...    | var  | `triangle[T]`      | T Triangle records                       |
| ...    | 2    | `num_points`       | Point count (P)                          |
| ...    | var  | `point[P]`         | P Point records                          |

Note: the Thing name string (Pascal-style: 1 length byte + chars) is read at the archive level before `file_read_thing` is called. See [fan-archive.md](fan-archive.md) block `0x01`.

### Part (22 bytes)

Reader: `file_read_part()` in `file.c:482`. Each part is an ellipsoidal body element.

| Offset | Size | Field              | Description                              |
|--------|------|--------------------|------------------------------------------|
| 0      | 2    | `name_index`       | Part name index                          |
| 2      | 2    | `offset.X`         | Local offset X                           |
| 4      | 2    | `offset.Y`         | Local offset Y                           |
| 6      | 2    | `offset.Z`         | Local offset Z                           |
| 8      | 2    | `rotate.X`         | Local Euler rotation X                   |
| 10     | 2    | `rotate.Y`         | Local Euler rotation Y                   |
| 12     | 2    | `rotate.Z`         | Local Euler rotation Z                   |
| 14     | 2    | `squash.X`         | Semi-axis X (ellipsoid shape)            |
| 16     | 2    | `squash.Y`         | Semi-axis Y                              |
| 18     | 2    | `squash.Z`         | Semi-axis Z                              |
| 20     | 1    | `color`            | Palette color index                      |
| 21     | 1    | `type`             | Part type                                |
| 22     | 2    | `flags`            | Part flags                               |
| 24     | 2    | `parent_part_link` | Parent part index (-1 for root)          |

After reading, actual values are copied to default fields (`def_rotate`, `def_offset`, etc.).

### Triangle (10 bytes)

Reader: `file_read_triangle()` in `file.c:538`.

| Offset | Size | Field              | Description                              |
|--------|------|--------------------|------------------------------------------|
| 0      | 2    | `point1_idx`       | Point index (vertex 1)                   |
| 2      | 2    | `point2_idx`       | Point index (vertex 2)                   |
| 4      | 2    | `point3_idx`       | Point index (vertex 3)                   |
| 6      | 2    | `shade_name`       | Shade/texture name reference             |
| 8      | 1    | `color3`           | Triangle color                           |
| 9      | 1    | `use_flag`         | Usage flag                               |
| 10     | 2    | `flags`            | Triangle flags                           |

Point indices are stored as raw integers and remain as cast `(point_heap_t*)(intptr_t)idx` until post-load fixup resolves them to actual point pointers.

### Point (12 bytes)

Reader: `file_read_point()` in `file.c:560`.

| Offset | Size | Field              | Description                              |
|--------|------|--------------------|------------------------------------------|
| 0      | 2    | `point_index`      | Global point index                       |
| 2      | 2    | `offset.X`         | Point offset X                           |
| 4      | 2    | `offset.Y`         | Point offset Y                           |
| 6      | 2    | `offset.Z`         | Point offset Z                           |
| 8      | 2    | `parent_part_idx`  | Parent part index (resolved at bind time)|
| 10     | 2    | `use_flag`         | Usage flag                               |

Points join a global `point_list` (not per-actor) and are matched to their parent parts via `parent_part_idx` at bind time.

## Event-stream format (FANT file)

Reader: `read_actors()` in `file.c:1389`, dispatched through `modify_part()` in `move.c:1680`.

In the `merge_seeked_file` path, Things are reconstructed from a flat stream of events. Each event is 10 bytes:

| Offset | Size | Field          | Description                              |
|--------|------|--------------------|------------------------------------------|
| 0      | 2    | `event_type`   | Event type enum                          |
| 2      | 2    | `event_index`  | Target element index (part/point/tri)    |
| 4      | 2    | `param1`       | Parameter 1                              |
| 6      | 2    | `param2`       | Parameter 2                              |
| 8      | 2    | `param3`       | Parameter 3                              |

The stream is terminated by an event with `event_type = 0` (`NO_EVENT`).

### Thing construction events

A Thing is built by a sequence of events. The `ADD_THING` event creates the actor, then subsequent events populate its parts, points, triangles, and properties until the next `ADD_THING` or `NO_EVENT`.

| Event type (enum)    | Value | Description                                              |
|----------------------|-------|----------------------------------------------------------|
| `ADD_THING`          | 8     | Create new actor. `param1` = thing name index.           |
| `ADD_PART_TO_THING`  | 10    | Add part to current actor. `param1` = part name index.   |
| `THING_FLAGS`        | 21    | Set actor flags. `param1` = flags, `param2` = field_146, `param3` = mask. |
| `ROTATE_THING`       | 18    | Set actor rotation. `param1/2/3` = X/Y/Z.               |
| `MOVE_THING`         | 19    | Set actor position. `param1/2/3` = X/Y/Z.               |
| `START_POSITION`     | 20    | Set start position. `param1/2/3` = X/Y/Z.               |
| `THING_CODE`         | 61    | Set code indices. `param1` = hp-change, `param2` = hit, `param3` = init (all 1-based, -1 = none). |
| `THING_CODE_2`       | 76    | Extended codes. `param1` = picked-up, `param2` = dead (1-based). |
| `ACTOR_REP`          | 50    | Set repertoire. `param1` = rep index, `param2` = flag, `param3` = default rep. |
| `BACKGROUND`         | 48    | Background flag. `param1 = 1` clears bit 0x400, else sets it. |
| `REORIENT_THING`     | 35    | Mark actor for reorientation (sets flag 0x20).           |

### Part property events

These target a specific part via `event_index` (part name index):

| Event type (enum)    | Value | Description                                              |
|----------------------|-------|----------------------------------------------------------|
| `ROTATE`             | 1     | Part rotation. `param1/2/3` = X/Y/Z.                    |
| `OFFSET`             | 2     | Part offset. `param1/2/3` = X/Y/Z.                      |
| `COLOUR`             | 3     | Part color. `param1` = palette index.                    |
| `VECTOR1`            | 4     | Squash (semi-axes). `param1/2/3` = X/Y/Z.               |
| `VECTOR2`            | 5     | Relative centre. `param1/2/3` = X/Y/Z.                  |
| `TYPE`               | 9     | Part type. `param1` = type value.                        |
| `FLAGS`              | 14    | Part flags. `param1` = flags, `param2` = mask.           |
| `ADD_PART`           | 7     | Add child part. `event_index` = parent, `param1` = child part name. |
| `POSITION`           | 30    | Absolute position. `param1/2/3` = X/Y/Z.                |
| `DISP_PNT`           | 13    | Displacement point. `param1/2/3` = X/Y/Z.               |

### Point and triangle events

| Event type (enum)    | Value | Description                                              |
|----------------------|-------|----------------------------------------------------------|
| `ADD_POINT`          | 39    | Add point to part. `event_index` = parent part, `param1` = point index. |
| `OFFSET_POINT`       | 40    | Set point offset. `event_index` = point index, `param1/2/3` = X/Y/Z. |
| `ADD_TRIANGLE`       | 41    | Add triangle. `event_index` = tri index, `param1/2/3` = point indices (vertex 1/2/3). |
| `COLOUR_TRIANGLE`    | 42    | Triangle color. `event_index` = tri index, `param1` = color, `param2` = color4, `param3` = repertoire (if bit 15 set). |
| `TRIANGLE_FLAGS`     | 43    | Triangle flags. `event_index` = tri index, `param1` = flags, `param2` = mask. |
| `TRI_SHADE_NAME`     | 62    | Triangle shade name. `event_index` = tri index.          |

### Default-value events

Used to set the rest pose for parts (applied when `copy_actual_to_defaults` runs):

| Event type (enum)    | Value | Description                                              |
|----------------------|-------|----------------------------------------------------------|
| `DEF_ROTATE`         | 51    | Default rotation. `param1/2/3` = X/Y/Z.                 |
| `DEF_OFFSET`         | 52    | Default offset. `param1/2/3` = X/Y/Z.                   |
| `DEF_VECTOR1`        | 53    | Default squash. `param1/2/3` = X/Y/Z.                   |
| `DEF_VECTOR2`        | 54    | Default relative centre. `param1/2/3` = X/Y/Z.          |
| `DEF_COLOUR`         | 55    | Default color. `param1` = palette index.                 |
| `DEF_FLAGS`          | 56    | Default flags. `param1` = flags value.                   |
| `DEF_POSITION`       | 57    | Default position. `param1/2/3` = X/Y/Z.                 |

## Name resolution

In the FANT event-stream path, all name indices are file-local. After each event is read, `merge_event_names()` translates them to global indices using the `new_*_name[]` mapping tables loaded from the FANT header. Key translations:

- `ADD_THING`: `param1` remapped via `new_thing_name[]`
- `ADD_PART_TO_THING`: `param1` remapped via `new_part_name[]`
- Part-targeted events: `event_index` remapped via `new_part_name[]`
- Point/triangle events use their respective name tables

In the tagged-block path (`merge_file_contents`), name indices are already global.

## Post-load fixup

After all Things are loaded:

1. `copy_actual_to_defaults()` snapshots current part state to default fields.
2. `start_things()` copies `start_position` to `position_vector`, sets behavior to `BH_SLEEP`, and resets hit points.
3. Triangle point indices (stored as raw integers) are resolved to `point_heap_t*` pointers.
4. Points are matched to parent parts via `parent_part_idx`.
5. `selected_thing` is added to the display list with flag `0x0008` cleared.
