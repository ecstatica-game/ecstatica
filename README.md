# Ecstatica

Decompilation and portable C99 reimplementation of **Ecstatica 1 & 2**, the 1994/1997 ellipsoid-rendered adventure games by Andrew Spencer Studios.

Primary reverse-engineering target is `E2WIN95.EXE` (Ecstatica 2 Win95 build, which includes high-res mode). Ecstatica 1 is a secondary target and runs from the same binary — game version is auto-detected at runtime.

The goal is a single native executable that runs both games on modern systems (macOS first, Linux/Windows scaffolded) using the original game data.

## Goals

* Disassemble Ecstatica 1 & 2 with IDA Pro 9.1 + Open Watcom debug symbols
* Use `E2WIN95.EXE` as decomp base
* Translate Hex-Rays / Watcom pseudocode into portable C99
* Single binary for both games (E1 + E2), runtime version detect
* Native platform builds via thin platform layer (macOS, Linux, Windows)
* Optional: retarget DOS build with Open Watcom (`wcl386`)

## Status

* Build system: CMake + Makefile wrapper
* Platforms: macOS (Cocoa/NSView framebuffer) working; Linux + Windows scaffolded, untested
* Renderer: 8-bit indexed palette framebuffer, palette expansion + scale in `platform_blit`
* Modules ported: init, display, ellipse, tri, asm_f (fixed-point + rasterizers), edit, move, map, topo, anim, file (.FAN I/O), music, menu, req, icon, chars, win, game
* Fixed-point math: 14-bit fraction (`FIXED_POINT_SHIFT`)
* Structs packed to original binary layout (`#pragma pack(push, 1)`)

## Build

```bash
make          # cmake build → build/bin/ecstatica
make e2       # build + stage into data/e2 + run
make e1       # build + stage into data/e1/W + run
make e1-dos   # build + stage into data/e1 (bundled DOS)
make clean    # remove build/
```

CMake project lives in `src/CMakeLists.txt`. C99 strict, plus ObjC for the macOS platform layer.

## Disassembly Requirements

* **IDA Pro 9.1** — connected via `ida-mcp` MCP server for live pseudocode / xref queries
* **Open Watcom 2.0** (`wcl386`) — original compiler; used to regenerate debug symbols and optionally target DOS
* `wdump` — for dumping the original PE

Building Open Watcom on Apple Silicon: <https://retrocoding.net/building-for-dos-os2-and-dos-on-a-macbook-apple-silicon>

## Code Style

* C99 strict
* `snake_case`
* 4-space indent
* No comments except non-obvious "why"
* All structs / enums / typedefs in `types.h`
* Module names mirror original Watcom source files (`init`, `display`, `edit`, `game`, …)
* Original Watcom symbol pattern: `module_function_name_hexaddr` (e.g. `init_init_mouse_410100`)

## Layout

```
src/
  main.c            entry point, game loop
  init.c            input, display primitives, palette, timing
  game.c            script execution, actors, combat
  display.c         skeletal hierarchy, view pipeline
  ellipse.c         ellipsoid rendering, shade_map
  tri.c             polygon/quad rendering, textures
  asm_f.c           fixed-point math, bitmap unpack, rasterizers
  edit.c            entity management, scene/action control
  move.c            actor movement, collision, behavior state
  map.c             camera, view loading, visibility
  topo.c            terrain, height, gravity
  anim.c            keyframe / action directory loading
  file.c            .FAN archive I/O, save/load
  music.c           MIDI playback, ambient, SFX
  menu.c            main/pause menus, settings
  req.c             dialogs, file picker, game-over
  icon.c            resolution constants, VGA/SVGA config
  chars.c           font glyphs
  win.c             window/platform stubs
  platform.h        platform abstraction interface
  types.h           structs, enums, constants, forward decls
  platforms/
    macos.m         Cocoa NSView framebuffer + input + timing
    linux.c         Linux platform (WIP, untested)
    windows.c       Windows platform (WIP, untested)

decomp/
  parse.py          extract Watcom debug symbols → JSON + IDC
  modules.txt       module list with descriptions
  dump/             wdump outputs, parsed symbols, IDC scripts
  ida/              IDA Pro database files
```

## Data Folder

Game data is not committed. Place original game files under `data/`:

* `data/e1/W/`    — Ecstatica 1 Win95 (640×480)
* `data/e1/`      — Ecstatica 1 bundled DOS (320×200)
* `data/e1-dos/`  — Ecstatica 1 original DOS release (320×200)
* `data/e2/`      — Ecstatica 2 Win95 (640×480) + DOS (320×200)

`.FAN` archives hold game resources (actors, graphics, scripts).

## Debug

* `DBG_LOG(level, ...)` — `1` = important, `2` = verbose
* `debug_verbose` global toggles level
* Log file: `ecstatica_debug.log`
* Compile flags: `SKIP_START_LOGO`, `SKIP_DRAW_TRIANGLE`, `APP_ALWAYS_ACTIVE`

## License

Reverse engineering / preservation project. Original game assets and executables are © their respective owners and are **not** included in this repository.
