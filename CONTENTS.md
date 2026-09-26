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

### Hardware Renderer (optional, desktop only)

| Module              | Description                                                  |
|---------------------|--------------------------------------------------------------|
| render.h            | Seam between display traversal and a hardware backend        |
| render.c            | Backend-independent draw lists, shading, view/projection     |
| render_priv.h       | Draw-list types shared by render.c and backends              |
| render_gl.c         | OpenGL 3.3 backend (`ECS_ENABLE_GL`)                         |
| render_gl_shaders.h | GLSL sources for render_gl.c                                 |
| gl_loader.c/.h      | Runtime resolve of GL entry points                           |

### Platform

| Module                     | Description                                             |
|----------------------------|---------------------------------------------------------|
| platform.h                 | Platform abstraction interface                          |
| win.c                      | Window/platform glue, page flip, `window_proc` input map |
| platforms/desktop_common.c | Data dir, save paths, display caps shared by desktops   |
| platforms/macos.m          | macOS: Cocoa NSView, CoreAudio, AVMIDIPlayer, GameController |
| platforms/linux.c          | Linux: X11/GLX, ALSA, FluidSynth (dlopen'd) music       |
| platforms/windows.c        | Windows / Win9x: GDI, winmm, MCI MIDI, XInput           |
| platforms/dos.c            | DOS/4GW: VGA/VESA, SB16, PIT timer, keyboard ISR        |
| platforms/openfpga.c       | openfpgaOS (Analogue Pocket / MiSTer)                   |
| platforms/psp.c            | PlayStation Portable: sceGu, sceAudio, sceCtrl          |

### Debug / Utility

| Module           | Description                                                |
|------------------|------------------------------------------------------------|
| debug_overlay.c  | Runtime debug overlay (actor labels, flags, trigger zones) |
| compat.h         | Compiler/platform compatibility macros                     |
| tools/viewer.c   | Model/animation (`--viewer`) and scene (`--scenes`) browser |

## Other Directories

| Directory | Description                                                    |
|-----------|----------------------------------------------------------------|
| data/     | Game data files (E1, E1-DOS, E2 variants)                      |
| decomp/   | Decompilation tooling: wdump parsing, IDA scripts, symbol maps |
| docs/     | Research notes, format documentation, plans                    |
