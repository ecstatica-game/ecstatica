# Ecstatica on the PlayStation Portable

A PSP build of the C99 port. Same engine as every other target — the whole
platform-specific surface is `../src/platforms/psp.c`, reached through
`platform.h`, the way `macos.m` reaches Cocoa.

## Building

Needs the [pspdev toolchain](https://github.com/pspdev/pspdev) (`psp-gcc`,
newlib, pspsdk):

```sh
brew install pspdev/pspdev/pspdev      # macOS; or build psptoolchain yourself
export PSPDEV=/usr/local/pspdev
export PATH=$PSPDEV/bin:$PATH

make -C psp                            # → psp/EBOOT.PBP
make -C psp release GAME_DATA=data/e2  # → psp/build/PSP/GAME/ECSTATICA/
make -C psp install MSTICK=/Volumes/PSP
```

From the repository root, `make psp`, `make psp-release`, `make psp-install`
and `make psp-clean` do the same.

## Installing by hand

```
ms0:/PSP/GAME/ECSTATICA/
    EBOOT.PBP
    CODE/           ← the game data, exactly as the desktop build reads it
    VIEWS/
    ...
```

The engine reads every asset relative to the folder the EBOOT was launched
from, so the data sits beside it. `platform_early_init` also accepts the data
in a `data/` subfolder, and looks under `ef0:` for a PSP Go.

Saves go to `saved/` beside the data, eleven slots, same format as everywhere
else — a save copied off a desktop install works here and back again.

## Controls

The PSP has no keyboard, so everything reaches the engine through the gamepad
path in `win.c`. The mapping follows `docs/controls.md`:

| PSP              | Action                                    |
| ---------------- | ----------------------------------------- |
| D-pad / analog   | walk and turn, eight ways                 |
| Cross            | reach out — pick up, interact, confirm    |
| Circle           | back / cancel                             |
| Triangle         | inventory                                 |
| Square           | use what is held                          |
| L                | jump                                      |
| R                | attack with the stick (E1), run (E2)      |
| Start            | pause menu                                |
| Select           | toggle the HUD icons                      |
| Select + L / R   | E1: left / right hand pick up and drop. E2: magic |
| Analog + Cross   | pointer and click, for the menus          |

Two consoles' worth of controls do not fit on one: the PSP has a single stick
and one shoulder button per side, so Select acts as a shift for the second
shoulder row, and E1's right-stick quick swings and the graphics toggle are
unbound. The stick drives both the legs and the menu pointer — the cursor is
not drawn in play, and nothing is walking while a requester is up.

## What is not there yet

- **Music.** `music.c` converts the tune banks to a Standard MIDI File and
  hands it to `platform_midi_play`, which wants a General MIDI synth. The PSP
  has none (`sceMidi` is a UART to the remote port, not a synth), so tunes are
  silent until a software synth and a bank are linked in — the same position
  the DOS target is in. Sound effects and speech work; they go through the
  mixer in `psp.c`.
- **Untested on hardware.** This is a scaffold: it compiles the engine against
  the SDK and implements every entry point in `platform.h`, but nothing here
  has been run on a real PSP or on PPSSPP yet. The video path in particular
  (T8 texture + CLUT, drawn in 64-pixel slices) is the part most likely to
  need adjusting.

## Notes on the target

- 333 MHz MIPS, 32 MB RAM on a PSP-1000 (~20 MB of it usable), 480x272.
  `platform_init` clocks the CPU up; at the 222 MHz default the software
  renderer is not playable.
- **Prefer the 320x200 data.** E2's Win95 set renders 640x480, which is four
  times the pixels for a screen that cannot show them and is then reduced to
  480x272 by the GPU. The DOS data at 320x200 is both faster and, upscaled
  rather than downscaled, sharper.
- Palette expansion and scaling are done by the GE, not the CPU: the frame goes
  over as a `GU_PSM_T8` texture with a 256-entry CLUT.
- The PSP-2000's extra 32 MB needs a kernel-mode module flag the SDK's user
  mode EBOOT does not carry, so the build is sized for a PSP-1000.
