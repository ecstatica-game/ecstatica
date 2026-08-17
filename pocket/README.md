# Ecstatica on Analogue Pocket / MiSTer (openfpgaOS)

A platform backend for [openfpgaSDK](https://github.com/openfpgaOS/openfpgaSDK),
which runs C apps on a VexiiRiscv soft CPU inside an openFPGA core. The same
`app.elf` runs on Analogue Pocket and on MiSTer (DE10-Nano / SuperStation One).

Both games work from the one build — the engine detects E1 vs E2 at runtime
from the archives, so the game you get is decided by which data ISO you load.

## Target

| | |
|---|---|
| CPU | VexiiRiscv rv32imafc @ 100 MHz, 8 KB I$ / 32 KB D$ |
| RAM | 64 MB SDRAM (54 MB app static window; this build uses ~6.3 MB bss) |
| Video | 320x240 8-bit indexed, triple-buffered |
| Audio | 48 kHz stereo, 32-voice PCM mixer, sample-based MIDI synth |
| Input | 2 controllers; dock USB keyboard and mouse when docked |

Ecstatica 1 runs in its 320x200 VGA mode; Ecstatica 2 always runs at 640x480
(see "E2 has no low-resolution path"). `apply_video_mode()` enumerates the
modes the OS advertises and takes an exact match where one exists — the stock
`video.json` lists 320x200, so the Pocket scaler fills the panel at 4:3 with no
black bars. Sizes with no exact match letterbox into the smallest mode that
holds them.

## Build

```bash
git clone https://github.com/openfpgaOS/openfpgaSDK ../../openfpgaSDK
cd pocket
make OF_SDK=../../openfpgaSDK              # -> $OF_SDK/.obj/sdk/ecstatica/app.elf
```

The SDK compiles RISC-V inside a Docker image by default. Without Docker, use a
host toolchain (`riscv64-unknown-elf-gcc`, e.g. `brew install riscv-gnu-toolchain`)
and pass `USE_SDK_CONTAINER=0`.

`make` stages a flat copy of `../src` plus `../src/platforms/openfpga.c` into
`$OF_SDK/src/apps/ecstatica/` and builds there — the SDK's build container only
mounts its own checkout, so the sources are copied rather than symlinked. Edit in
`src/`, never in the staged copy; `make` overwrites it.

Other targets:

```bash
make test        # desktop SDL2 build of the same app (SDK PC backend)
make debug       # push the ELF to a Pocket over UART (PHDP)
make clean       # drop objects and build/
make distclean   # also drop the staged app dir and the ISOs
```

## Build and deploy, end to end

```bash
make iso                 # pack whichever of E1 / E2 data is present
make deploy              # build ELF, assemble build/, rsync to the SD card
```

`make release` assembles `build/` as a literal SD card tree:

```
Cores/<author>.Ecstatica/     bitstream.rbf_r + loader.bin (SDK runtime), core JSON
Platforms/ecstatica.json
Assets/ecstatica/common/      os.bin, os.ini, ecstatica.elf, bank.ofsf, *.iso
Assets/ecstatica/<core id>/   Ecstatica.json, Ecstatica2.json
```

`make deploy` then rsyncs it onto the card. `deploy.sh` finds the card by
looking for a mount with both `Cores/` and `Assets/` at its root; override with
`SDCARD=/Volumes/AP`. It syncs each core/asset subdirectory individually —
`--delete` never runs against the parent `Cores/` or `Assets/`, which would take
every other core on the card with it. Comparison is by checksum, not mtime,
because FAT32 timestamps are too coarse to tell a stale ELF from a fresh one.

The ISOs are copied into `build/` only if they exist, so `make release` stays
quick while iterating on the ELF; run `make iso` once up front. With both games
packed, `build/` is ~340 MB.

Useful overrides: `AUTHOR=` (sets the core id, default `xesf`), `E1_DATA=`,
`E2_DATA=`, `SDCARD=`, `VERSION=`.

`core/icon.bin` is the 36x36 core icon; `release` generates it with
`tools/mkicon.py` if absent. `core/platform_image.bin` is the 521x165 platform
banner — supply one for custom art, otherwise the generic openfpgaOS banner is
taken from the SDK, which is exactly what the stock cores ship.

## Game data

APF data slots are flat files, but the engine wants a directory tree with
case-insensitive lookup. The data therefore ships as an ISO 9660 image, mounted
read-only at `/game`; `file_set_data_root()` points the file layer at the mount
so `fopen_ci()` works unchanged.

```bash
./mkiso.sh ../data/e1 ecstatica.iso             # Ecstatica 1  (~200 MB)
./mkiso.sh ../data/e2 ecstatica2.iso            # Ecstatica 2  (~140 MB)
./mkiso.sh ../data/e1 ecstatica.iso --dos-only  # E1 without the enhanced set
./mkiso.sh ../data/e2 ecstatica2.iso --hires    # + E2's 640x480 backgrounds
```

The script copies only the asset directories and root data files — the source
trees also hold IDA databases, disassembly listings and installers, which would
add gigabytes.

### E1's two roots

`data/e1` is the CD layout: a DOS install with the Win95 one nested in `W/`.
`init_data_roots()` probes `W/CODE/ECSTATIC.FAN`, and when that resolves `W`
becomes a second search root, with `enhanced_graphics` deciding which root wins
for the swappable presentation directories.

The root database stays authoritative either way. Its FAN is version 30, which
puts `vga_data` true, so E1 boots at 320x200 and the in-game graphics toggle
(G, or R3 on the pad) switches the backgrounds to the 640x480 set at runtime.
That is why `E1_DATA` defaults to `data/e1` and not `data/e1-dos` — the latter
has no `W/`, so `hires_available` stays false and the toggle does nothing.

Only the swappable directories are ever read from `W`, so `W/FILES` is left out
of the image: ~40 MB of database that is never consulted cross-root.

Note that enhanced mode means the renderer works at 640x480 — four times the
pixels, on a 100 MHz soft core. Expect it to be a slideshow until the
rasterizers are optimised. It is off at boot, so it costs nothing until asked
for.

### E2 has no low-resolution path

`init()` derives `vga_data` as `game_version == GAME_VERSION_E1 && fan_ver <= 30`,
so **E2 always takes `set_svga_constants()` and runs at 640x480**, whether or
not `HIRES/` is in the image. `--hires` only controls which backgrounds are
available, not the render size. E2 is therefore the much heavier of the two on
this hardware, and `apply_video_mode()` will pick the 640x480 scanout mode for
it. Untested on hardware.

The mount code tries `ecstatica.iso`, `ecstatica2.iso`, `game.iso`, `e1.iso`,
`e2.iso`, `data.iso` in that order and uses the first slot that exists.

### Why an ISO, when no other openfpgaOS core uses one

Every shipped openfpgaOS core (Doom, Quake, Diablo, LBA2) binds one data slot
per game file, because those games keep their assets in a handful of big
archives — LBA2 declares 20 slots and calls it done. Ecstatica can't: it has
~3450 loose files across ten directories, and `of_file_slot_register` caps at
32 registrations. Hence the image.

The kernel's iso9660 filesystem is real, not just an SDK header — `iso9660`
appears in `os.bin`'s VFS type table next to `slot:` and `save:`. Note that the
SDK's own `isodemo` reads an ISO with raw async DMA rather than `of_iso_mount`,
so this port is the first user of the mount path; if the data root comes up
empty, check that first with `slotdemo`.

## Packaging a core

Scaffold a custom core with the SDK, then overlay the files in `core/`:

```bash
cd $OF_SDK
./scripts/customize.sh --batch --name ecstatica --short "Ecstatica" \
                       --author "<you>" --platform ecstatica --variant os25
```

`core/` holds the pieces that differ from the scaffold: `data.json`,
`platform.json`, `os.ini`, and an instance file per game. `video.json`,
`audio.json` and `variants.json` are the stock openfpgaOS set, copied from a
working core for reference.

On the SD card the layout is:

```
Cores/<author>.Ecstatica/     data.json, video.json, core.json, bitstream…
Platforms/ecstatica.json      platform.json, renamed
Assets/ecstatica/common/      os.bin, os.ini, ecstatica.elf, bank.ofsf, *.iso
Assets/ecstatica/<author>.Ecstatica/   instance.e1.json / instance.e2.json
```

The slots that matter:

- **4** — the game data ISO (`deferload`, streamed from SD, not resident)
- **3** — `ecstatica.elf`, **2** — `os.ini` (`ELF=ecstatica.elf`; the OS reads
  this to know what to launch, and the core will not boot without it)
- **7** — `bank.ofsf`, the MIDI soundfont bank (`tools/sf2_to_ofsf` in the SDK
  converts an SF2; the SDK ships an SC-55-derived bank). Without it the tunes
  are silent — sound effects and speech are unaffected.
- **10–19** — ten 256 KB nonvolatile save slots. `make_save_filename()` emits
  `ecstatica_<n>.sav` for E1 and `ecstatica2_<n>.sav` for E2, so the two
  instances never share a slot, and `SAVE_MAX_SLOTS` drops from 11 to 10 to
  match. Ten is the ceiling: the slots run 0x20100000..0x20380000 and butt
  straight up against the shared-config region.

## Controls

`window_proc()` in `src/win.c` already maps a gamepad; this backend fills the
same `platform_gamepad_state_t`, so the bindings match every other platform:

| Pocket | Action |
|---|---|
| D-pad / left stick | move (8-way) |
| A | interact / pick up |
| B, Start | menu / back |
| X | use item / magic |
| Y | inventory |
| L | jump |
| R | run / attack |
| Select | toggle HUD |
| L3 | cycle speed mode |
| Right stick | combat swings, hand pick/drop |

The requester dialogs in `req.c` are mouse-driven. A docked USB mouse drives the
cursor directly; without one the right stick moves it and **R3** clicks. A docked
USB keyboard is mapped to the full `PKEY_*` set, so the debug and hotkey paths
are reachable when docked.

## Known limits

- **Frame rate is the open question.** The renderer is a software ellipsoid and
  polygon rasterizer that shipped for a 486/Pentium; a 100 MHz soft core with a
  32 KB D$ is in that neighbourhood but has not been profiled on hardware. Expect
  to need work in `ellipse.c` / `tri.c` before this is comfortable.
- **Save size is unverified.** APF nonvolatile slots cap at 256 KB each and
  `save_game()` writes every live actor, part and event; a large scene may not
  fit. Nothing truncates gracefully yet.
- **`-fsigned-char` is required.** `char` is unsigned on RISC-V, signed on the
  x86 the original Watcom binary targeted, and the engine keeps signed data in
  plain `char` arrays. The flag is set in the Makefile; do not drop it.
- **Misaligned access.** The FAN parsers cast raw buffers to packed structs.
  This is fine on x86 and on the packed members GCC knows about, but any
  unaligned wide load the compiler assumes is aligned depends on how the core
  handles misaligned addresses.
- `platform_blit_rgba()` is a stub — it only feeds the debug overlay, which this
  build does not enable.
