# Game Versions & Format Differences

Five known game distributions exist across two titles, each with distinct file format versions.

## Distributions

### E1 DOS Standalone (`data/e1-dos/`)
The original retail DOS release. Ships individual `.FAN` files organized by type in subdirectories. No `OFFSETS` file — resources loaded by scanning directories. This is the oldest format with the widest version spread across files (v7–v27).

### E1 DOS Bundled (`data/e1/`)
A DOS version bundled on the same CD as the Windows release. Uses the offset-based merged archive (`FILES/ECSTATIC`). Format version v30 — same as the Windows version's code archive. Includes both 4MB and 8MB executables (`ECST4MEG.EXE`, `ECST8MEG.EXE`).

### E1 Windows (`data/e1/W/`)
The Windows 95 release. Also offset-based with `FILES/ECSTATIC`. Code archive v48. Adds hi-res backgrounds (`HIRES/`), visibility data (`VISIB/`), shadow tables (`SHADOW.DAT`), and anti-aliasing data.

### E1 Windows DOS-compat (`data/e1/W/D/`)
A DOS-compatible build shipped inside the Windows release folder. Code archive v51 with an extended OFFSETS file (33320 bytes — larger than standard E1 but smaller than E2). Uses only `FILES/ECSTATIC` (no `ECST2`). Also has hi-res support.

### E2 (`data/e2/`)
Ecstatica II (Windows 95). Code archive v55. Largest table sizes, most features. Full offset-based loading with extended OFFSETS (56240 bytes).

## Version Matrix

| Property              | E1 DOS Standalone | E1 DOS Bundled | E1 Windows | E1 Win DOS-compat | E2          |
|-----------------------|-------------------|----------------|------------|---------------------|-------------|
| Data path             | `data/e1-dos/`    | `data/e1/`     | `data/e1/W/` | `data/e1/W/D/`   | `data/e2/`  |
| CODE FAN version      | 27                | 30             | 48         | 51                  | 55          |
| Actor FAN versions    | 7–25              | (merged)       | (merged)   | (merged)            | (merged)    |
| Scene FAN versions    | 7–27              | (merged)       | (merged)   | (merged)            | (merged)    |
| Total .FAN files      | 2770              | 3              | 3          | 2                   | 2           |
| OFFSETS               | none              | 14100 bytes    | 14100      | 33320               | 56240       |
| OFF2                  | none              | yes            | yes        | no                  | yes         |
| FILES/ECSTATIC        | none              | yes            | yes        | yes                 | yes         |
| FILES/ECST2           | none              | yes            | yes        | no                  | yes         |
| new_name_sys          | 0                 | 0              | 0          | 0                   | 0           |
| Internal name         | `code\E2.FAN`     | (empty)        | `\fast\code\ecstatic.fan` | `\fast\code\ecstatic.fan` | `z:\fast\code\ecstatic.fa` |
| HIRES/                | no                | no             | yes        | yes                 | yes         |
| VISIB/                | no                | no             | yes        | yes                 | yes         |
| SHADOW.DAT            | no                | no             | yes        | yes                 | yes         |
| Screen mode           | VGA 320x200       | VGA 320x200    | SVGA 640x480 | SVGA 640x480     | SVGA 640x480 |
| Detection method      | No OFFSETS + `ACTORS/` dir | OFFSETS <= 14100 | OFFSETS <= 14100 | OFFSETS > 14100 (!) | OFFSETS > 14100 |

**Detection caveat**: E1 Win DOS-compat has OFFSETS > 14100, so the current detection logic classifies it as E2. Its extended offset table uses different table sizes than standard E1 or E2.

## E1 DOS Standalone Version Spread

The standalone release contains 2770 individual `.FAN` files spanning format versions 7–27. Files were created at different stages of development:

| Version | Count | Notes |
|---------|-------|-------|
| 7       | 25    | Earliest actors/scenes |
| 8       | 90    | |
| 9       | 137   | Common for actors |
| 10      | 134   | |
| 11      | 30    | |
| 13      | 76    | |
| 14      | 54    | |
| 15      | 22    | INTERACT code remapping begins |
| 16      | 15    | Sound names/data added |
| 18      | 3     | |
| 19      | 51    | |
| 20      | 56    | Map area names added |
| 21      | 15    | |
| 22      | 30    | |
| 23      | 584   | Internal name field added |
| 24      | 983   | Most common (scenes, sounds, actions) |
| 25      | 250   | |
| 26      | 109   | new_name_sys flag added |
| 27      | 106   | Highest in standalone |

Distribution by subdirectory:

| Subdir   | Dominant versions | Total |
|----------|-------------------|-------|
| ACTIONS/ | v23–v27           | 804   |
| ACTORS/  | v7–v25 (wide spread) | 503 |
| CODE/    | v27               | 1     |
| REP/     | v23–v27           | 147   |
| SCENES/  | v7–v27 (wide spread) | 908 |
| SOUNDS/  | v24               | 407   |

## Directory Layouts

### E1 DOS Standalone
```
data/e1-dos/
  ACTORS/           individual .FAN files per actor (v7-v25)
    A.FAN           hero character
    FDINHE.FAN      intro dragon
    ...
  SCENES/           individual .FAN files per scene (v7-v27)
  SOUNDS/           individual .FAN files per sound (v24)
  ACTIONS/          individual .FAN files per action (v23-v27)
  REP/              repertoire .FAN files (v23-v27)
  CODE/
    ECSTATIC.FAN    main code archive (v27)
  VIEWS/            camera backgrounds + palettes
  MUSIC/            music data files
```

### E1 DOS Bundled / E1 Windows
```
data/e1/           (DOS bundled)
data/e1/W/         (Windows)
  FILES/
    ECSTATIC       merged archive blob
    ECST2          secondary archive
  OFFSETS          offset index (14100 bytes)
  OFF2             secondary offset index
  CODE/
    ECSTATIC.FAN   main code archive (v30 / v48)
  VIEWS/           camera backgrounds + palettes
  MUSIC/           music data files
  HIRES/           hi-res backgrounds (Windows only)
  VISIB/           visibility data (Windows only)
  SHADOW.DAT       shadow palette (Windows only)
```

### E2
```
data/e2/
  FILES/
    ECSTATIC       merged archive blob
    ECST2          secondary archive
  OFFSETS          offset index (56240 bytes)
  OFF2             secondary offset index
  CODE/
    ECSTATIC.FAN   main code archive (v55)
  VIEWS/           camera backgrounds + palettes
  HIRES/           hi-res backgrounds
  VISIB/           visibility data
  SHADOW.DAT       shadow palette
  MUSIC/           music data files
```

## FAN File Header

See [fan-header.md](fan-header.md) for detailed byte-level format.

```
u32      magic         'FANT' (0x46414E54, big-endian)
be16     version       file format version
be16     new_name_sys  (version >= 26 only; 0 = name tables in file, 1 = identity mapping)
u8[26]   internal_name (version >= 23 only; original build path, zero-padded)
```

## Name Tables by Version

Name tables map file-local indices to global runtime indices. Present when `new_name_sys == 0`. Names are null-terminated C strings; a zero byte signals end of table.

| Table          | Minimum version |
|----------------|-----------------|
| Part names     | always          |
| Thing names    | always          |
| Action names   | always          |
| Scene names    | always          |
| Point names    | >= 2            |
| Triangle names | >= 2            |
| Code names     | >= 6            |
| Repertoire names | >= 12         |
| Sound names    | >= 16           |
| Map area names | >= 20           |
| Texture names  | >= 37           |

## Data Sections by Version

After name tables, data is read as event streams (not tagged blocks). Section availability depends on version:

| Section      | Min version | Read order | Notes |
|--------------|-------------|------------|-------|
| Actors       | always      | < v4: first; >= v4: second | Event stream |
| Actions      | always      | < v4: second; >= v4: first | Event stream |
| Scenes       | always      | After actors+actions | Event stream with PSEUDO_SCENE events |
| Code         | >= 6        | After scenes | Token streams with text lines |
| Repertoires  | >= 13       | After code | Sentinel-delimited |
| Sounds       | >= 16       | After repertoires | Little-endian (!), sentinel-delimited |
| Textures     | >= 38       | After sounds | Little-endian, sentinel-delimited |
| Map          | >= 9        | Last | Little-endian grid + mixed-endian elements |

See individual type docs: [fan-thing.md](fan-thing.md), [fan-action.md](fan-action.md), [fan-scene.md](fan-scene.md), [fan-code.md](fan-code.md), [fan-sound.md](fan-sound.md), [fan-repertoire.md](fan-repertoire.md), [fan-map.md](fan-map.md), [fan-texture.md](fan-texture.md).

## Event Processing Differences

### INTERACT Events (type 46)
Keyframe events dispatched by sub-type (`param1`). See [fan-action.md](fan-action.md) for full event type table.

| Sub-type | Name              | Part required | Remapping        | Min version |
|----------|-------------------|---------------|------------------|-------------|
| 0        | CheckPartHit      | Yes           | None             | -           |
| 1        | CheckPickUp       | Yes           | Code (param2)    | >= 15       |
| 2        | DropObject        | Yes           | None             | -           |
| 3        | AttachToHeld      | Yes           | None             | -           |
| 4        | ExecutePartCode   | Optional      | Code (param2)    | >= 15       |
| 5        | PlaySound         | No            | Sound (param2)   | >= 16       |
| 6        | FireBullet        | Yes           | None             | E2 only     |
| 7        | BloodSpurt        | Yes           | None             | E2 only     |
| 8        | SpawnActor        | Yes           | Thing+Action     | >= 43       |
| 9        | SpawnActor2       | Yes           | Thing+Action     | >= 43       |

Sub-type 4 (ExecutePartCode) is part-targeted in event type flags but can fire without a part in E1 DOS standalone (actors may lack parts during intro scenes). When part is NULL, code executes without part context.

### THING_CODE Events
`param1`, `param2`, `param3` use **1-based** code indices (0 = none). Remapped through `new_code_name[param - 1] + 1` during file loading.

### THING_FLAGS Event
- `param3` set to -1 for `file_version < 49`
- Bit 0x0002 cleared for `file_version < 11`
- `field_144` set from `param3` for `file_version >= 30`

### ACTOR_REP Event
- `param1` clamped to -1 for `file_version < 18` when index exceeds `no_of_rep_names`

## Map Element Differences

| Field            | E1                     | E2                            |
|------------------|------------------------|-------------------------------|
| Camera index     | 1 byte (unsigned)      | le16                          |
| Jump camera      | = camera index         | le16 (separate)               |
| Reserved words   | None                   | 4 words + 7 words (22 bytes)  |
| Wanderer spawn   | 0 (hardcoded)          | 1 byte                        |
| Camera top_clip  | Computed default       | le16 (version >= 36)          |
| Padding (v34-39) | None                   | 4 bytes skipped               |

## Runtime Behavior Differences

### Startup Sequence
- **E1**: Loads player character, checks scene loaded, starts scene directly
- **E2**: Executes `StartUp` code, then game-specific start codes (`StartGame`, `StartF1`..`StartF12`)

### Palette Handling
- **E1**: Direct palette set from `colour_map`
- **E2**: Fade system with flags (1=set, 2=fade-to-black, 3=fade-to-white)

### Audio
- **E1 DOS** (320x200): Downsamples 21kHz audio to 11025Hz
- **E1 Win / E2**: Native sample rates

### Repertoire Actions
- **E1**: Special handling for action types 33-35 (movement variations), mapped to `rep->field_2[50 + variation + d*3]`
- **E2**: Standard action mapping

## Offsets File Sizes

| Distribution       | OFFSETS size | Formula |
|--------------------|-------------|---------|
| E1 DOS Standalone  | none        | Individual files, no offsets |
| E1 DOS Bundled     | 14100       | (1000+500+1000+150+500+375) * 4 |
| E1 Windows         | 14100       | Same as bundled |
| E1 Win DOS-compat  | 33320       | Extended tables (between E1 and E2) |
| E2                 | 56240       | (2500+5000+2000+500+700+960+1200+1200) * 4 |

## Runtime Table Sizes

Fixed arrays declared in `src/game.h` — the game uses two size regimes selected at load time by `game_version`. Loaders (`file.c`) read the smaller regime when E1 is detected; heap arrays are still allocated at the E2 size but indexed up to the E1 cap so E1 saves/loads never overflow.

| Constant                | E1 value | E2 value | Notes |
|-------------------------|----------|----------|-------|
| `E1_THING_TAB_SIZE`     | 500      | 5000     | Actor/thing name table size |
| `E1_ACTION_TAB_SIZE`    | 1000     | 2000     | Action name table size |
| `E1_SCENE_TAB_SIZE`     | 1000     | 2500     | Scene name table size |
| `E1_REPERTOIRE_TAB_SIZE`| 150      | 500      | Repertoire (behavior set) count |
| `E1_SOUND_TAB_SIZE`     | 500      | 700      | Sound name table size |
| `E1_SOUND_DRIVER_COUNT` | 5        | 10       | Music driver rows in offsets file |
| `E1_TUNE_COUNT`         | 75       | 96       | Music tune columns in offsets file |

Loader (`file.c:281-287`) picks the count via `(game_version == GAME_VERSION_E1) ? E1_* : *`.

Screen-mode constants are picked in `init.c:162-167`:
- E1 DOS (FAN version ≤ 30) → VGA 320×200
- E1 Windows (FAN version > 30) → SVGA 640×480
- E2 → SVGA 640×480

## Version-Gated Loader Fields

### FAN header
```
u32   magic         'FANT'
be16  version       file format version (see distribution matrix)
be16  new_name_sys  present iff version ≥ 26
u8[26] internal_name present iff version ≥ 23
```

### Name tables (present iff `new_name_sys == 0`)
| Table              | Minimum FAN version |
|--------------------|---------------------|
| Part, Thing, Action, Scene names | always |
| Point, Triangle names | ≥ 2 |
| Code names         | ≥ 6 |
| Repertoire names   | ≥ 12 |
| Sound names        | ≥ 16 |
| Map area names     | ≥ 20 |
| Texture names      | ≥ 37 |

### Actor / event stream (see `fan-thing.md`, `fan-action.md`)
- **THING_CODE**: `param1`/`param2`/`param3` remap via `new_code_name` (1-based, 0 = none).
- **THING_FLAGS**: version < 49 forces `param3 = -1`; version < 11 clears bit 0x0002; version ≥ 30 stores `param3` in `extra_action_index` (`file.c:1498-1509`).
- **ACTOR_REP**: version < 18 clamps `param1` to -1 when index exceeds `no_of_rep_names` (`file.c:1463`).
- **INTERACT type 6/7** (FireBullet, BloodSpurt): E2 only.
- **INTERACT type 8/9** (SpawnActor): version ≥ 43 (E2 only in practice).

### Camera / view data (`file.c:1882-1904`)
| Field         | E1                          | E2                                  |
|---------------|-----------------------------|-------------------------------------|
| Camera index  | 1 byte (unsigned)           | le16                                |
| Jump camera   | = camera index              | le16 (separate field)               |
| Reserved      | none                        | 4 words + 7 words padding           |
| Wanderer spawn| 0 (hardcoded)               | 1 byte                              |
| `top_clip`    | default `(-127 << shift)`   | le16 read from file if version ≥ 36 |
| Padding v34–39| none                        | 4 bytes skipped                     |

`cam_has_top_clip = (game_version == GAME_VERSION_E2 && file_version >= 36)`.

### Repertoire action slots (`file.c:765-772`)
E1 remaps repertoire action slots 33–35 into an expanded 9-variation table at slots 50+, laid out as `50 + variation + d*3` for `d = 0..8`. E2 uses the straight slot mapping. This encodes E1's per-direction attack/turn variants that E2 replaces with a repertoire-native scheme.

## Startup Sequence

Both games share `start_game_medium()` (`game.c:2310`) but the two branches inside are almost disjoint.

### E1 branch (game.c:2316-2365)
1. Look up `fdinhe` in the thing table; if present, unlink from display+thing lists and free.
2. Load hero actor from offset (`actor_offset[female ? 1 : 0]`) via `merge_seeked_file`.
3. Save `selected_thing`, set its Wanderer flag (`thing_name_flags[i] |= 0x0002`).
4. `initialise_game()`.
5. `check_scene_loaded(notUsed1)` — asks the offset loader to page in the start scene.
6. If `scene_tab[notUsed1]`: restore `selected_thing`, clear `active_camera`, `check_actors_in_scene_loaded`, `start_scene`.
7. `selected_thing->actor_behavior = BH_JOYSTICK`.

Then the shared tail: `no_wanderers = true; intro_flag = true; memset(cameras_viewed); armour_factor = 100; hero_material = 0;`.

**E1 never executes named codes at startup.** All initial gameplay logic is driven by scene 0 firing its `code_idx` and its actors' scripts, with `intro_flag` cleared by `CT_END_OF_INTRO`.

### E2 branch (game.c:2361-2427)
1. Walk `code_list`, find every code whose name ends in `"StartUp"` and execute it.
2. `active_camera = NULL`.
3. Walk `code_list` again, find the first code whose name ends in `"StartGame"` and execute it (`break` on first hit).
4. If not found, `do_info2_req("Can't find start code", "StartGame")`.
5. `set_pallette(all_black_cmap)` twice (kept for binary parity).
6. Shared tail with additional `kill_count = 0; treasure_count = 0;`.

**Implication for the port**: E2 needs its start codes present in data; E1 needs scene 0 code_idx / code_2 to actually drive gameplay. Because E1 v48's `endintro` code (527) ships with 0 tokens, the port injects `CT_END_OF_INTRO + CT_REPEAT_SCENE 0` on load (`file.c:1698-1706`) to give the intro a way to end. See "Data completeness quirks" below.

## Palette / Fade Handling

- **E1** (`display.c:653`): every frame sets palette directly from `colour_map`. No fade queue.
- **E2**: uses `view_cmap` + `set_pallette_flag` state machine (1 = set, 2 = fade-to-black, 3 = fade-to-white). Fades driven by the `CT_FADE_*` tokens and by scene transitions.
- Intro palette file (`topo.c:729-732`):
  - E2 with `intro_flag` set → `VIEWS/%04d.PA2` (palette animation).
  - Otherwise → `VIEWS/%04d.PAL` (static palette).

## Audio Subsystem

- Sample-rate downsample (`game.c:2834`): E1 with `screen_width <= 320` and source rate 21 kHz downsamples to 11.025 kHz to match DOS SB output. E1 Windows / E2 keep native rate.
- Music offset table (`init.c:303,317,320-343`):
  - E1: 5 drivers × 75 tunes; per-file fallback under `MUSIC/*.{scc,sbl,gus,awe,lap}` if archive lookup fails.
  - E2: 10 drivers × 96 tunes; archive-only, no per-file fallback.
- E1 sound tables use `E1_SOUND_TAB_SIZE = 500`; E2 uses 700.

## Language / Menu

- `NUM_LANGUAGES_E1 = 3` (English, German, French).
- `NUM_LANGUAGES_E2 = 7` (adds Italian, Spanish, Polish, and a reserved slot).
- Difficulty picker (`menu.c:412`): E2 only. E1 skips straight into the game after language selection.

## Data Completeness Quirks

These are places where shipped game data lacks tokens the runtime expects, requiring loader-level compensation. Documented here because they are *not* file-format differences but *content* differences that behave like format differences in practice.

### E1 v48 `endintro` (code 527) is empty
The code block name-mapped to global index 527 in E1 Windows v48 loads with 0 tokens and 1 empty text line. In the original E1 executable the CT_END_OF_INTRO opcode has a real handler at `0x45093F` that clears `intro_flag` and refreshes HUD icons, but nothing calls it because the token stream is empty. E2 v55 ships this code block populated.

Port compensation (`file.c:1698-1706`) injects on load, for E1 only:
```
token_store[tsi + 0] = CT_END_OF_INTRO
token_store[tsi + 1] = CT_REPEAT_SCENE
token_store[tsi + 2] = 0x4000            // scene name-space, index 0
token_store[tsi + 3] = 0                 // stream terminator
```

Runtime compensation (`game.c` `CT_END_OF_INTRO` handler): after clearing `intro_flag`, for E1 also resets `space_pressed`/`enter_pressed` (kills stuck demo-auto-space state) and restores `selected_thing->position_vector` from its `start_position`, zeroing velocity. This mimics what the missing tokens presumably did in complete data.

### Adjacent codes in the same area (dumped from E1 v48)
| Idx | Name             | Tokens                                    | Notes |
|-----|------------------|-------------------------------------------|-------|
| 13  | `endstart_scene` | `CT_REPEAT_SCENE 17125`                   | Fires when scene 0 completes; loops back into start scene. |
| 14  | `duringStartscen`| `CT_IF CT_ANY_KEY_PRESSED CT_REPEAT_SCENE 16882` | Per-frame while scene 0 active; advances on key press. |
| 513 | `endintro_h`     | `CT_REPEAT_SCENE 16882`                   | Fires when horse intro scene completes. |
| 514 | `duringStartscene`| Duplicate of 14                          | Both file-idx entries name-map to same global. |
| 527 | `endintro`       | **empty**                                 | See above. |
| 528 | `endaft_int`     | `CT_REPEAT_SCENE 16889`                   | Fires after the "aft_int" scene. |

Scene-name argument tokens are merged by upper nibble `0x4000`; `arg & 0x0FFF` becomes the runtime scene index after `new_scene_name[]` translation.

## Cross-Reference: `game_version` Branch Sites

Every place the port branches on `game_version` (as of this writing). Use this as a checklist when porting a new mechanic:

| File:line    | Purpose |
|--------------|---------|
| `file.c:255,266,268` | Detection + logging |
| `file.c:281-287`     | Offset-table row counts |
| `file.c:302-306`     | E2-only palette/visibility offsets |
| `file.c:765-772`     | E1 repertoire slot 33-35 → 50+ remap |
| `file.c:1698-1706`   | E1 `endintro` token injection |
| `file.c:1836,1956`   | Version tag in log |
| `file.c:1882-1904`   | Camera field layout (E2 wider) |
| `init.c:162-167`     | Startup screen mode |
| `init.c:303,317,320-343` | Music tune count + per-file fallback |
| `game.c:2316-2365`   | E1 startup: fdinhe swap, actor reload, `check_scene_loaded` |
| `game.c:2361-2427`   | E2 startup: `StartUp` / `StartGame` code execution |
| `game.c:2414-2418`   | Kill/treasure counters (E2), hero_material reset (both) |
| `game.c:2821`        | E1 21 kHz → 11.025 kHz downsample |
| `game.c:1382-1395`   | E1 CT_END_OF_INTRO: hero reset + key clear |
| `display.c:653`      | Palette write path (E1 direct, E2 fade) |
| `topo.c:729-732`     | Intro palette file extension (E2 `.PA2` vs `.PAL`) |
| `menu.c:306-307,363,382,412` | Language table size + difficulty picker |

## Metadata Dumps

Pre-generated metadata dumps for all versions are in `docs/dumps/`:
- `e1-dos-standalone-{stats,scan,code}.txt`
- `e1-dos-bundled-{stats,scan,code}.txt`
- `e1-windows-{stats,scan,code}.txt`
- `e1-windows-dos-{stats,scan,code}.txt`
- `e2-{stats,scan,code}.txt`
- `cross-version-compare.txt`

Regenerate with: `python3 tools/fan_dump.py <path> --stats|--scan|--names|--blocks`
