# Project Contents

## Source Modules (`src/`)

### Core

| Module   | Description                                                        |
|----------|--------------------------------------------------------------------|
| main.c   | Entry point, platform init, main game loop handoff                 |
| init.c   | Input handling, display primitives, math tables, palette, timing   |
| game.c   | Script execution, actor spawn/remove, fade, collision, combat      |
| edit.c   | Entity management, part/key/event add/remove, scene/action control |

### Rendering

| Module     | Description                                                    |
|------------|----------------------------------------------------------------|
| display.c  | Skeletal hierarchy, matrix/vector math, view pipeline, actors  |
| ellipse.c  | Ellipsoid column rendering, shade_map lighting, projection     |
| tri.c      | Polygon rendering, quad-to-triangle splitting, texture mapping |
| icon.c     | Resolution constants, VGA/SVGA display mode configuration      |
| chars.c    | Font data (character set bitmap glyphs for text rendering)     |
| asm_f.c    | Fixed-point matrix/vector math, bitmap unpacking, rasterizers  |

### World

| Module   | Description                                                   |
|----------|---------------------------------------------------------------|
| map.c    | Camera switching, view loading, visibility, actor display list |
| topo.c   | Terrain height queries, map elements, collision, gravity       |
| move.c   | Actor movement, walking, collision, height, behavior state     |
| anim.c   | Keyframe ellipse management, action directory loading          |

### I/O and Resources

| Module   | Description                                                   |
|----------|---------------------------------------------------------------|
| file.c   | .FAN archive I/O, offset-based resource loading, save/load    |
| music.c  | MIDI-like tune playback, ambient sounds, SFX, volume control  |

### UI

| Module   | Description                                                   |
|----------|---------------------------------------------------------------|
| menu.c   | Main menu, pause menu, settings, navigation, dialogs          |
| req.c    | Requester dialogs, file picker, input, game-over screens      |

### Platform

| Module           | Description                                          |
|------------------|------------------------------------------------------|
| win.c            | Window/platform stubs, page flip, DirectDraw layer   |
| platform_macos.m | macOS Cocoa window, NSView framebuffer, input/timing |
| platform.h       | Platform abstraction interface                       |

### Debug / Utility

| Module           | Description                                          |
|------------------|------------------------------------------------------|
| debug_overlay.c  | Runtime debug overlay rendering                      |
| compat.h         | Compiler/platform compatibility macros               |

## Other Directories

| Directory | Description                                                    |
|-----------|----------------------------------------------------------------|
| data/     | Game data files (E1, E1-DOS, E2 variants)                      |
| decomp/   | Decompilation tooling: wdump parsing, IDA scripts, symbol maps |
| docs/     | Research notes, format documentation, plans                    |
