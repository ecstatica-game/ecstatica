# Plan — Ecstatica 1: runtime toggle between DOS (original) and Win95 (enhanced)

Status: implemented
Scope: E1 only. E2 untouched.

## Implementation notes

Shipped as planned, with two deviations:

- **Change 3 returns failure instead of falling through.** The plan had `load_raw`
  fall back to `VIEWS/` when a camera has no `HIRES` entry. That silently
  succeeds and loads a 320×200 image into a 640-wide buffer — the tiling bug
  again. It now returns `-1` in SVGA mode when `HIRES` is missing, which is what
  actually triggers the `map.c` VGA reload plus `copy_vga_to_svga()` upscale.
  The `do_info2_req` error dialog is skipped on that path.
- **`handle_lo_hi_res` (req.c:1034) is not the entry point.** It ends in
  `restore_active_req()` and the original gadget-text calls, which belong to the
  unported requester UI. The mid-session switch is `set_enhanced_graphics()` in
  game.c, called from the settings menu; it does the same teardown/rebuild
  without the requester tail.

Change 6 (music) needed no code, as expected: the DOS root keeps `load_by_offset`
so tunes continue to come from its archive. Same tune set either way.

`set_enhanced_graphics` reloads the view after the mode switch. `go_vga`/
`go_svga` clear `active_camera`, but nothing reloads the background until the
player crosses into a different camera — without it the frame keeps whatever the
old mode left in the plane. It must be `check_view(prev_camera)` and **not**
`check_camera()`: `check_camera` re-runs the map lookup, and a lookup that
misses returns camera 0, which calls `play_dead_scene(7)` and drops the player
into the dragon scene. `selected_camera` is captured before the switch because
`go_vga`/`go_svga` reset it to -1.

The in-scene branch is chosen on `selected_camera > 0`, not on
`game_up_and_running` — the latter is already true while the title screen is up,
so it cannot distinguish a loaded view from a menu.

### Hotkey input

`platform_key_hit()` was added to `platform.h` and all three backends: a sticky
per-key latch set on a genuine false→true transition and cleared when read.
`platform_key_pressed()` compares this frame's level against last frame's, which
drops any tap whose press and release land in the same event pump — during
`present_delay` pumps are 50ms apart, so most presses on the title and intro
screens vanished. The rising-transition condition also means auto-repeat cannot
re-latch (X11 additionally has `XkbSetDetectableAutoRepeat`).

`TITLE_S.RAW` and `TSCREEN.RAW` joined the swappable set: the DOS root ships
only the 320×200 title and the Win95 root only the 640×480 one, so
`load_background_title` needs to see across both roots.

## Goal

Boot the E1 DOS bundle (`data/e1`) and toggle to the enhanced Win95 presentation
(`data/e1/W`) from the in-game menu, without interrupting the session — no
restart, no save/reload, no loss of position or progress. Toggling back must be
equally free. Each data root must still run standalone exactly as it does today.

## Findings that shape the design

### The two versions are the same game

Full string-stream diff of the two databases (`CODE/ECSTATIC.FAN`, v30 DOS vs
v48 Win95) is **28 lines**:

| Difference | Count | Impact |
|---|---|---|
| Build path string `\fast\code\ecstatic.fan` | 1 | none |
| `Start_sc` / `Start_scene` reordered (both present in both) | 2 | scene index shift |
| Sound names dropped in W (`piggyvx2`, `KNIGHT1`, `KNIGHT4`, `TRIA59`, `G_H_WIT`, `FOOL3`, `FOOL2`, `TRIA10`, `RO12`) | 9 | sound index shift |
| `PutGraphic` coordinates retuned for hi-res titles | 7 | title/end screens only |

Scripts, dialogue, text, scene logic, actor definitions: identical.
`FILES/ECSTATIC` is 36,368,706 bytes (DOS) vs 36,369,328 (W) — 622 bytes apart,
i.e. the same resource blob.

**Consequence:** swapping the database buys almost nothing and is precisely what
would break a live session. Save games are pure index streams (`save_game`
writes `scene_index`, `rep_index`, event params; `scene_name_flags` is a dense
indexed array), so the reordered scene and the 9 dropped sounds shift indices
under a running game.

### Camera numbering is shared

- `data/e1/VIEWS` — 220 files, `data/e1/W/VIEWS` — 239, `data/e1/W/HIRES` — 238
- DOS set is a **strict subset** of W's; zero DOS-only files
- W's extras (0001, 0004, 0061, 0071, 0075, 0078, 0094, 0105, 0153–0155, …) fill
  gaps in DOS's numbering
- `HIRES` = `VIEWS` minus 0001

So `HIRES/NNNN.RAW` is addressable by the DOS database's camera indices directly.

### Assets are keyed by name or camera number, not by table index

- backgrounds — camera number (`topo.c:652`, `topo.c:655`)
- visibility maps — camera number (`topo.c:849`)
- tunes — name, via `tune_names_e1[]` (`music.c:42`), used at `init.c:337`;
  `load_tune` already has both an offsets-archive path and a per-file
  `MUSIC/<name>.<ext>` fallback
- title/end graphics — name (`topo.c:698`)

Player position is world coordinates. Nothing in the toggle-able set needs
remapping.

## Design

Split the data into two classes.

**Boot unit — chosen at startup, never swapped.**
`CODE/ECSTATIC.FAN` + `OFFSETS` + `OFF2` + `FILES/*`. These are index-paired;
mixing a DOS FAN with a W `OFFSETS` corrupts resource lookups. Whichever root
the binary is launched from owns these for the whole session.

**Presentation set — resolved through an ordered search path the toggle
reorders.**
`HIRES/`, `VIEWS/`, `VISIB/`, `GRAPHICS/`, `LOWGRAPH/`, `MUSIC/`.

Writes (`saved/`, debug logs) always stay in the launch directory — they use
bare `fopen` (`file.c:1300`, `file.c:1441`), so the search path must be
read-only and must not apply to them.

What the user forgoes: the 7 hi-res `PutGraphic` title coordinates, which live
in the W FAN. Those only affect title and end screens, which are outside
gameplay — so they follow the boot-unit choice, not the toggle.

## Changes

### 1. Search path in `fopen_ci` — `src/file.c:75`

Add a small ordered root list (2 entries is enough). `fopen_ci` currently tries
the exact path, then resolves case component-by-component. Wrap that resolution
in a loop over roots. Default list is `{ "." }`; when `./W/CODE/ECSTATIC.FAN`
exists, the list becomes `{ ".", "./W" }` (original mode) or `{ "./W", "." }`
(enhanced mode).

Must not affect `fopen` callers that write.

### 2. Replace the `low_res_only` hard lock — `src/game.c:63`, `src/init.c:175`

`low_res_only` was added to fix the DOS-bundle blank-actor bug (SVGA constants
desyncing from the platform blit size). Replace with a capability probe:

- `hires_available` = a `HIRES/` directory resolves through the search path
- `init()` (`init.c:169`) sets VGA constants for E1 low-res data as now, but only
  forces `chosen_svga = 0` and blocks `go_svga()` (`game.c:3104`) when
  `hires_available` is false
- when true, the existing lo/hi-res gadget `handle_lo_hi_res()` (`req.c:1034`)
  becomes reachable — it already does `remove_all_graphics()` → `go_vga`/
  `go_svga` → `update_game_icons()` → `draw_magic_bar()` → `prepare_parts()` →
  `draw_stuck_parts()`, which is the full mid-session teardown/rebuild
- `chosen_svga` is already persisted in saves (`file.c:1589`, `file.c:1821`) and
  reapplied at `game.c:2420-2423`

`set_vga_constants` / `set_svga_constants` (`src/icon.c`) already push
`win_set_render_size()`, so the constants and the blit size cannot drift again.

### 3. Gate background source on `mode_svga` — `src/topo.c:640` **(bug fix)**

`load_raw` tries `HIRES/%04d.RAW` first unconditionally, then falls back to
`VIEWS/`. In VGA mode this loads a 640×480 background into a 320-wide buffer —
the same class of failure as the DOS-bundle bug. Gate it:

- `mode_svga` → `HIRES/` then `VIEWS/`
- otherwise → `VIEWS/` only

Camera 0001 has no `HIRES` entry. `map.c:66-77` and `map.c:168-179` already
handle that: drop to VGA constants, re-`load_raw`, restore SVGA, then
`copy_vga_to_svga()` (`map.c:543`) upscales 320×200 → 640×480. No new code.

This fix is required regardless of the toggle work — `data/e1/W` in lo-res mode
is broken today for the same reason.

### 4. Visibility maps — `src/topo.c:849`

`load_visibility_map` builds `VISIB/%04d.VIS` then opens it with bare `fopen`,
bypassing case resolution and the search path. Switch to `fopen_ci`. The DOS
bundle ships no `VISIB/` at all, so it currently runs with visibility culling
silently disabled; routing through the search path picks up `W/VISIB` in
enhanced mode.

### 5. Title/menu graphics — `src/topo.c:696`

`load_raw_graphic` hardcodes `graphics/%s`. Sizes confirm the split:

| File | DOS `GRAPHICS` | W `GRAPHICS` | W `LOWGRAPH` |
|---|---|---|---|
| `TITLE1.RAW` | 5,280 | 18,119 | 4,454 |

Select by mode: `mode_svga` → `GRAPHICS/`, else `LOWGRAPH/` when it resolves,
else `GRAPHICS/`. The last fallback keeps the DOS root (whose `GRAPHICS` is
already lo-res, and which has an extra `TITLE6.RAW`) working unchanged.

### 6. Music — no code change expected

`load_tune` (`init.c:303`) resolves by name with an archive path plus a
`MUSIC/<name>.<ext>` fallback across `scc/sbl/gus/awe/lap`. DOS ships driver
patch banks (`SBL0.BIN`, `GUS0.BIN`, …) with tunes inside `FILES/ECSTATIC`; W
ships loose `.SBL` files. Same tune set either way, so the search path alone
covers it. Verify, do not pre-emptively change.

### 7. Menu entry and hotkeys

`G` (keyboard) and right stick click / R3 (gamepad) call `set_enhanced_graphics`
directly from `window_proc`, gated on `game_up_and_running`. `PKEY_G` (0x22) was
added to `platform.h` and mapped in the macOS and Linux key tables; Windows uses
raw DOS scancodes, so it needed no entry. R3 (`btn_rstick`) was populated by all
three platform backends and unused by the game.

Both bindings latch on the key/button *level* and assign the latch **before**
calling the switch: `set_enhanced_graphics` reaches `window_proc` again through
`make_game_screen`, so an edge computed after the call re-fires on every nested
frame. `set_enhanced_graphics` also carries its own re-entrancy guard.

Surface the toggle. The lo/hi-res gadget already exists in the options
requester; when `hires_available` is true it should be enabled and labelled to
reflect that it switches the whole presentation set, not just resolution.

## Verification

| Case | Expectation | Result |
|---|---|---|
| `make e1-dos-bundle`, no toggle | 320×200, unchanged | pass |
| `make e1-dos-bundle`, toggle on | 640×480, `W/HIRES` backgrounds, same position/scene, no reload | pass — actors and camera carry across |
| toggle on → off, mid-scene | back to 320×200 with actors and HUD intact | pass |
| `make e1` (from `data/e1/W`) | hi-res as today | pass |
| `make e1` then toggle to lo-res | 320×200 with `W/VIEWS` + `W/LOWGRAPH` — **was broken** | pass — fixed by changes 3 and 5 |
| `make e1-dos` (`data/e1-dos`, no `W/`) | toggle unavailable, unchanged | pass |
| `make e2` | unaffected | pass |
| repeated in-scene toggles (8×) | no dead scene, no crash, view and actors intact | pass |
| toggle on title / menu screens | resolution switches, no crash | partial — switches, but the backdrop repaint is unreliable |
| toggle across a camera change | new camera loads from the correct root | not yet exercised |
| camera 0001 while enhanced | `copy_vga_to_svga` upscale path, no tiling | not yet exercised |
| save in one mode, load in the other | same database, `chosen_svga` restores the mode | not yet exercised |

Frame dumps (`win.c:38`, auto at frames 60/120/200/300/500/800, or F12) give a
headless check of resolution and background integrity.

## Out of scope

- Cross-database save migration. Not needed under this design and not worth
  building: it would mean writing full name tables into the save and remapping
  old→new indices by name at load, with fallbacks for the 9 dropped sounds.
- Loading the W FAN into a running DOS session. This is the thing that breaks
  gameplay; the boot-unit rule exists to prevent it.
- The 7 hi-res title coordinates when booted from the DOS root.
- E2. Its data has no equivalent split.

## Risk notes

- `data/e1/W` is nested inside `data/e1`. The `W` root is discovered relative to
  the launch directory, so booting from `data/e1/W` finds no nested `W` and
  keeps a single-root path — the independence requirement holds by construction.
- Enhanced mode from `data/e1` needs `W/` present. `data/e1-dos` has none;
  the probe fails and the toggle stays locked.
- `handle_lo_hi_res` rebuilds gadgets and parts but has not been exercised
  mid-session in this port. Expect iteration there.
