# Ecstatica — Decompilation & Reimplementation

Reverse engineering Ecstatica 1 & 2 (DOS/Win95) into portable C99.
Primary target: **Ecstatica 2** (`E2WIN95.EXE`, unpatched). E1 secondary.

## Build

```bash
make          # cmake build
make e2       # build + run E2 (copies binary to data/e2/)
make e1       # build + run E1 (copies binary to data/e1/W/)
make clean    # remove build/
```

CMake project in `src/CMakeLists.txt`. C99 + ObjC (macOS platform layer).
Binary output: `build/bin/ecstatica`.

## Code Style

- C99 strict, snake_case, 4-space indent
- No comments unless explaining a non-obvious "why"
- `#pragma pack(push, 1)` for structs matching original binary layout
- Fixed-point math: 14-bit fraction (`FIXED_POINT_SHIFT`)
- All structs/types in `types.h`, forward-declared with typedefs
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
  win.c         — window/platform stubs
  platforms/macos.m  — Cocoa NSView framebuffer, input, timing
  platform.h    — platform abstraction interface
  types.h       — all structs, enums, constants, forward decls
```

Framebuffer: 8-bit indexed palette. `platform_blit` does palette expansion + scale.
Game version detected at runtime via `game_version` (E1=1, E2=2).

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

## Data Files

Game data lives in `data/` (not committed):
- `data/e1/W/` — Ecstatica 1 Win95 (640x480)
- `data/e1/` — Ecstatica 1 bundled DOS version (320x200)
- `data/e1-dos/` — Ecstatica 1 original DOS release (320x200)
- `data/e2/` — Ecstatica 2 Win95 (640x480) + DOS (320x200)

`.FAN` archives contain game resources (actors, graphics, scripts).

## Key Types

See `types.h` for complete definitions. Important ones:
- `actor_t` — game entity (pool: 200)
- `part_t` — body part of actor (pool: 4000)
- `ellipse_t` — rendered ellipsoid shape
- `scene_t`, `script_t`, `action_t` — game logic
- `event_t` — keyframe events (pool: 40000)
- `matrix3x3_t` — 3x3 fixed-point rotation matrix (18 bytes, packed)
- `vector_t` — 3D vector, 16-bit signed components (6 bytes, packed)

## Debug

- `DBG_LOG(level, ...)` macro — level 1 = important, 2 = verbose
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
