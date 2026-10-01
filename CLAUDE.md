# Ecstatica — Decompilation & Reimplementation

Reverse engineering Ecstatica 1 & 2 (DOS/Win95) into portable C99.
Primary target: **Ecstatica 2** (`E2WIN95.EXE`, unpatched). E1 secondary.

## Build

```bash
make          # cmake build
make e2       # build + run E2 (copies binary to data/e2/)
make e1       # build + run E1 (copies binary to data/e1/W/)
make e2-viewer / make e1-viewer   # model & animation browser (--viewer)
make e2-scenes / make e1-scenes   # scripted-scene browser (--scenes)
make e2-pointcloud / make e1-pointcloud   # views + depth → pointcloud.ply (--pointcloud)
make clean    # remove build/
```

CMake project in `src/CMakeLists.txt`. C99 + ObjC (macOS platform layer).
DOS and Win9x: Open Watcom `wmake` builds in `platforms/dos/` and
`platforms/win9x/` (`make dos`, `make win9x`).
Binary output: `build/bin/ecstatica`.

All non-desktop platform build trees (DOS, Win9x, Analogue Pocket, PSP, Vita,
Steam Deck packaging art) live under `platforms/`.

**Analogue Pocket / MiSTer:** a separate build under `platforms/pocket/`
targets openfpgaOS (rv32 soft CPU inside an openFPGA core) via the
openfpgaSDK; see `platforms/pocket/README.md`. It stages a flat copy of
`src/` into the SDK tree, so edit `src/`, never the staged copy.

**PlayStation Portable:** a separate build under `platforms/psp/` targets the
pspdev toolchain; see `platforms/psp/README.md`. GNU make on top of the
SDK's `build.mak`, and it compiles `src/` in place — objects go to
`platforms/psp/obj/`. Video goes through sceGu as a T8 texture + CLUT; music
is silent (no OS synth).

**PlayStation Vita:** a separate build under `platforms/vita/` targets
VitaSDK; see `platforms/vita/README.md`. CMake on VitaSDK's toolchain file
(`make vita`, needs `$VITASDK`) → `platforms/vita/build/ecstatica-e1.vpk` and
`-e2.vpk`, one LiveArea bubble per game (`VITA_GAME_DIR`). SceDisplay
framebuffer with a CPU palette-expand + 4:3 scale; game data lives in
`ux0:data/ecstatica/e1|e2`, not the VPK. Tested in Vita3K. Both handheld
builds take `PROFILE=1` / `-DPROFILE=ON` for `prof.log` (frame phases, see
`src/prof.h`).

**Linux music (optional):** macOS and Windows get a General MIDI synth from the
OS (`AVMIDIPlayer` / MCI `sequencer`); Linux has no equivalent, so tunes are
rendered by FluidSynth. `libfluidsynth` is `dlopen`'d at runtime, not linked —
it is not a build dependency, and without it the game runs with music silent
(SFX and speech are unaffected, they go through the ALSA mixer). Needs a GM
soundfont; searched in order: `$ECSTATICA_SOUNDFONT`, game data dir,
`~/.local/share/soundfonts`, `/usr/share/soundfonts`, `/usr/share/sounds/sf2`.
On Arch/SteamOS: `pacman -S fluidsynth soundfont-fluid`.

**Steam Deck:** just the Linux build above, no packaging step.
`platforms/steamdeck/` holds Steam library art (grid/hero/logo/icon) for
adding it as a non-Steam game; see `platforms/steamdeck/README.md`.

**Packaging art:** `assets/` holds the two real screenshots the Vita, PSP,
Pocket and Steam Deck packaging art is generated from, plus the script that
builds it; see `assets/README.md`.

## Code Style

- C99 strict, snake_case, 4-space indent
- No comments unless explaining a non-obvious "why"
- `#pragma pack(push, 1)` for structs matching original binary layout
- Fixed-point math: 14-bit fraction (`FIXED_POINT_SHIFT`)
- Shared types, pool sizes and forward typedefs in `types.h`; full struct definitions live in the owning module header (`actor_s` in `game.h`, `part_s` in `display.h`, etc.)
- Module naming mirrors original Watcom source files (init, display, edit, game, etc.)

## Architecture

```
src/
  main.c        — entry point, game loop
  init.c        — input, display primitives, palette, timing
  game.c        — script execution, actors, combat
  display.c     — skeletal hierarchy, view pipeline
  ellipse.c     — ellipsoid rendering, shade_map
  tri.c         — polygon/quad rendering, textures
  asm_f.c       — fixed-point math, bitmap unpacking, rasterizers
  edit.c        — entity management, scene/action control
  move.c        — actor movement, collision, behavior state
  map.c         — camera, view loading, visibility
  topo.c        — terrain, height, gravity
  anim.c        — keyframe, action directory loading
  file.c        — .FAN archive I/O, save/load
  music.c       — MIDI playback, ambient, SFX
  menu.c        — main/pause menus, settings
  req.c         — dialogs, file picker, game-over
  icon.c        — resolution constants, VGA/SVGA config
  chars.c       — font bitmap glyphs
  win.c         — window/platform glue, page flip, window_proc input mapping
  render.c      — backend-independent hardware-renderer draw lists (render.h seam)
  render_gl.c   — optional OpenGL 3.3 backend (ECS_ENABLE_GL, desktop only)
  gl_loader.c   — GL entry-point resolver
  debug_overlay.c — runtime debug overlay
  compat.h      — compiler/platform compatibility macros
  tools/viewer.c     — model/animation/scene browser (--viewer, --scenes); not in the original
  tools/pointcloud.c — unprojects every view's depth into a PLY point cloud (--pointcloud); not in the original
  platforms/desktop_common.c — data dir / save paths shared by macOS, Linux, Windows
  platforms/macos.m  — Cocoa NSView framebuffer, input, timing
  platforms/linux.c  — X11/GLX, ALSA, FluidSynth music
  platforms/windows.c — Win32 (also Win9x via the root platforms/win9x/ Open Watcom build)
  platforms/dos.c    — DOS/4GW (the root platforms/dos/ Open Watcom build)
  platforms/openfpga.c — openfpgaOS (Analogue Pocket / MiSTer) backend
  platforms/psp.c    — PlayStation Portable backend (sceGu / sceCtrl / sceAudio)
  platforms/vita.c   — PlayStation Vita backend (SceDisplay / SceCtrl / SceTouch / SceAudio)
  prof.c        — frame-phase profiler, compiled only with ECS_PROFILE
  platform.h    — platform abstraction interface
  types.h       — shared enums, constants, pool sizes, forward typedefs
```

Framebuffer: 8-bit indexed palette. `platform_blit` does palette expansion + scale.
Game version detected at runtime via `game_version` (E1=1, E2=2).
Gamepad: `platform_gamepad_poll()` per platform, mapped to key globals in `window_proc()`.

## Decompilation Workflow (IDA MCP)

IDA Pro 9.1 connected via `ida-mcp` MCP server. Use MCP tools to query the original binary directly:

- `mcp__ida-mcp__decompile` — get pseudocode for any function
- `mcp__ida-mcp__disasm` — raw disassembly at address
- `mcp__ida-mcp__lookup_funcs` — find functions by name pattern
- `mcp__ida-mcp__list_globals` — list global variables
- `mcp__ida-mcp__get_global_value` — read global variable value
- `mcp__ida-mcp__xrefs_to` — cross-references to address
- `mcp__ida-mcp__callees` — functions called by a function
- `mcp__ida-mcp__find_regex` — regex search in disassembly

### Decomp process

1. Use `lookup_funcs` or `list_globals` to find target in IDA
2. `decompile` to get Hex-Rays pseudocode
3. Translate to C99 matching project conventions (snake_case, types from types.h)
4. Verify struct layouts match original binary offsets
5. Cross-reference with `xrefs_to` and `callees` for completeness

### Symbol naming

Original Watcom debug symbols follow pattern: `module_function_name_hexaddr`
(e.g., `init_init_mouse_410100`). Module prefix maps to source file.

### Removed functions (do not re-create)

These original functions were intentionally removed — empty stubs, dead wrappers,
or unused in the C port. Do not recreate them when decompiling nearby code.

**Empty stubs (no-op in original or irrelevant to C port):**
- `init`: `analyse_view`, `archive_all`, `archive_all_fast`, `display_beep`, `expand_palette`, `set_grey_palette`, `zeroise_bitmap_pointers`, `read_from_dsp`, `write_to_dsp`, `load_hires_path` (fills `hires_path`, never read), `setup_hi_res_long_screen` (no callers)
- `anim`: `clear_choice_box`, `draw_choice_box`, `load_action_directory`, `draw_view_cone_tri`, `draw_world_square`
- `map`: `reposition_fixed_parts`
- `req`: `handle_ok2`, `handle_test`, `handle_strings_gadg`, `handle_file_gadg`, `handle_uninstall`
- `topo`: `show_topography`, `make_mask_map`
- `tri`: `clip_tri`

**Dead wrappers (called another function with same args, no callers):**
- `init`: `clip_mask2` → `clip_mask`, `text_win95` → `small_text_win95`, `do_text_with_mask_win95_japan` → `text_with_mask`, `old_draw` → `draw`, `draw_clipped` → `draw`, `clear_rectangle` → `rect_fill`
- `display`: `transpose_matrix` → `c_transpose_matrix`, `x_view_transform` → `long_view_transform`, `c_matrix_mult_display` → `c_matrix_mult`
- `edit`: `write_event` → `fwrite`
- `game`: `save_word` → `fwrite`
- `move`: `find_direction_and_distance_zx` → `find_direction_and_distance`
- `music`: `init_sound_channels` → `platform_audio_init`, `update_sound_mix` → `platform_audio_mix`
- `win`: `finish_win95` → `platform_shutdown`

## Data Files

Game data lives in `data/` (not committed):
- `data/e1/W/` — Ecstatica 1 Win95 (640x480)
- `data/e1/` — Ecstatica 1 bundled DOS version (320x200)
- `data/e1-dos/` — Ecstatica 1 original DOS release (320x200)
- `data/e2/` — Ecstatica 2 Win95 (640x480) + DOS (320x200)

`.FAN` archives contain game resources (actors, graphics, scripts).

## Key Types

Forward typedefs in `types.h`; definitions in module headers (`game.h`, `display.h`, ...). Important ones:
- `actor_t` — game entity (pool: 200)
- `part_t` — body part of actor (pool: 4000)
- `ellipse_t` — rendered ellipsoid shape
- `scene_t`, `script_t`, `action_t` — game logic
- `event_t` — keyframe events (pool: 40000)
- `matrix3x3_t` — 3x3 fixed-point rotation matrix (18 bytes, packed)
- `vector_t` — 3D vector, 16-bit signed components (6 bytes, packed)

## Debug

- `DBG_LOG(level, ...)` macro — level 1 = errors/warnings only, 2 = verbose tracing
- `debug_verbose` global controls output level
- Log file: `ecstatica_debug.log`
- Compile flags: `SKIP_START_LOGO`, `SKIP_DRAW_TRIANGLE`, `APP_ALWAYS_ACTIVE`

## Decomp Directory

```
decomp/
  parse.py       — extract Watcom debug symbols → JSON + IDC
  modules.txt    — module list with descriptions
  dump/          — wdump outputs, parsed symbols, IDC scripts
  ida/           — IDA Pro database files
```
