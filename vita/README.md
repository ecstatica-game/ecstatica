# Ecstatica on the PlayStation Vita

This is a PS Vita build of the C99 port. It uses the same engine as every
other target. All the Vita-specific code is in `../src/platforms/vita.c`, which
the engine reaches through `platform.h`. One VPK runs both Ecstatica 1 and
Ecstatica 2: the engine detects which game from the data it finds.

## Building

You need [VitaSDK](https://vitasdk.org) with `VITASDK` set:

```sh
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH

make vita                              # → vita/build/ecstatica.vpk
cmake -S vita -B vita/build -DPROFILE=ON && cmake --build vita/build
                                       # the same, and it writes prof.log
```

The build uses only the system libraries (`SceDisplay`, `SceCtrl`, `SceTouch`,
`SceAudio`, `ScePower`), so you do not need vita2d or SDL.

## Installing

1. Install `ecstatica.vpk` with VitaShell.
2. Copy the game data to `ux0:data/ecstatica/`, exactly as the desktop build
   reads it:

```
ux0:data/ecstatica/
    CODE/           ← CODE/ECSTATIC.FAN marks the folder as the game
    FILES/
    VIEWS/
    ...
```

To keep both games on the card, put them in `ux0:data/ecstatica/e2/` and
`ux0:data/ecstatica/e1/` instead. If both folders are there, E2 starts. The
game also looks in `uma0:data/ecstatica`.

For Ecstatica 1, copy the DOS release root with its `W/` folder inside it.
The game starts in the enhanced 640x480 graphics and switches to the original
320x200 set from the settings menu.

The game writes saves to `saved/` beside the data, with eleven slots. The save
format is the same on every platform, so a save copied from a desktop install
works here, and a save from the Vita works on the desktop.

## Controls

The mapping follows `docs/controls.md` and matches the PSP build. The Vita adds
a second stick and two touch panels:

| Vita                   | Action                                        |
| ---------------------- | --------------------------------------------- |
| D-pad / left stick     | walk and turn, eight ways                     |
| Cross                  | reach out: pick up, interact, confirm         |
| Circle                 | back / cancel                                 |
| Triangle               | inventory                                     |
| Square                 | use what is held                              |
| L                      | jump                                          |
| R                      | attack with the stick (E1), run (E2)          |
| Right stick            | E1 quick swings left and right                |
| Rear touch, left/right | E1: left/right hand pick up and drop. E2: magic |
| Select + L / R         | the same as rear touch                        |
| Start                  | pause menu                                    |
| Select                 | toggle the HUD icons                          |
| Front touch            | pointer; a tap clicks, for menus              |

The handheld Vita has no stick clicks, so the graphics toggle (R3 on other
gamepads) is only available from the settings menu. A PS TV with a DualShock
has L3 and R3 as usual.

## Video

The engine renders 640x480 or 320x200 in 8-bit colour. The CPU converts the
palette and scales the picture to a 4:3 area in the middle of the 960x544
screen, with black bars at the sides. The Vita has three framebuffers, so
drawing never waits for the vblank.

## What is not there yet

- **Music.** `music.c` hands a Standard MIDI File to `platform_midi_play`,
  which needs a General MIDI synth. The Vita does not have one, so music is
  silent. The PSP and DOS builds have the same limit. Sound effects and speech
  work.
- **Smooth scaling.** The CPU scaler uses the nearest pixel. Bilinear
  filtering would need the GPU (sceGxm).
- **Tested in Vita3K only.** Both games boot and play in Vita3K, but nobody
  has run this build on a real Vita yet.
