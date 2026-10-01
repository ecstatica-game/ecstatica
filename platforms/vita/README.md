# Ecstatica on the PlayStation Vita

This is a PS Vita build of the C99 port. It uses the same engine as every
other target. All the Vita-specific code is in `../../src/platforms/vita.c`,
which the engine reaches through `platform.h`.

The build makes two VPKs from the same code, one for each game. Each one is a
separate LiveArea bubble and reads its own data folder:

| VPK                | Bubble       | Title ID  | Data folder              |
| ------------------ | ------------ | --------- | ------------------------ |
| `ecstatica-e1.vpk` | Ecstatica    | ECST00001 | `ux0:data/ecstatica/e1/` |
| `ecstatica-e2.vpk` | Ecstatica II | ECST00002 | `ux0:data/ecstatica/e2/` |

## Building

You need [VitaSDK](https://vitasdk.org) with `VITASDK` set:

```sh
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH

make vita                              # → platforms/vita/build/ecstatica-e1.vpk, -e2.vpk
cmake -S platforms/vita -B platforms/vita/build -DPROFILE=ON && cmake --build platforms/vita/build
                                       # the same, and they write prof.log
```

The build uses only the system libraries (`SceDisplay`, `SceCtrl`, `SceTouch`,
`SceAudio`, `ScePower`), so you do not need vita2d or SDL.

## Installing

1. Install one or both VPKs with VitaShell.
2. Copy each game's data into its folder, exactly as the desktop build reads
   it:

```
ux0:data/ecstatica/
    e1/             ← Ecstatica 1: the DOS release root, with W/ inside it
        CODE/       ← CODE/ECSTATIC.FAN marks the folder as the game
        FILES/
        VIEWS/
        W/
        ...
    e2/             ← Ecstatica 2
        CODE/
        FILES/
        HIRES/
        ...
```

You do not need the installers, `DIRECTX/`, `ISHIELD/` or the `.EXE` and `.DLL`
files. If you have only one game, you can put its data straight into
`ux0:data/ecstatica/`, and either bubble opens it. The game also looks in
`uma0:data/ecstatica`.

Ecstatica 1 needs its `W/` folder for the enhanced 640x480 graphics. It starts
in enhanced graphics, and you can switch to the original 320x200 set from the
settings menu.

The game writes saves to `saved/` in each game's folder, with eleven slots. The
save format is the same on every platform, so a save copied from a desktop
install works here, and a save from the Vita works on the desktop.

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
