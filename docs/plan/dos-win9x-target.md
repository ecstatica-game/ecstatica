# Retargeting to DOS / Win9x (Open Watcom)

Bringing the port back to the hardware the games were written for: 386/486/Pentium,
4–16 MB RAM, VGA mode 13h and VESA, Sound Blaster, built with Open Watcom and
DOS/4GW.

The engine itself is already close. It is C99 with no VLAs, no designated
initialisers, no threads, and the platform seam in `platform.h` is 26 functions
wide. What blocks the target is memory footprint and a handful of hot-loop
patterns that modern optimisers hide and Watcom will not.

All figures below are measured from the tree at the time of writing — a Release
build on x86-64 plus a `sizeof` probe over every global array. They are recorded
so later work can tell whether it actually moved anything.

## Contents

- [1. Memory budget](#1-memory-budget)
- [2. Hot loops](#2-hot-loops)
- [3. Open Watcom compatibility](#3-open-watcom-compatibility)
- [4. New platform backend](#4-new-platform-backend)
- [5. Work order](#5-work-order)

---

## 1. Memory budget

Measured footprint, Release build, x86-64:

| Segment | Bytes |
|---|---|
| `__common` + `__bss` | 7,763,373 |
| heap (`set_up_bitmaps` + name tables) | ~3,614,000 |
| **total** | **~11.4 MB** |

On ilp32 the static side shrinks — `part_t` (390 B) and `actor_t` (488 B) are
pointer-heavy, and the rv32 `pocket/` build measures 6.3 MB of bss — giving
roughly **10 MB** in a flat model. DOS/4GW on top of that wants a 16 MB machine.
E2's own DOS release ran in far less.

Top static consumers:

```
1560000  part_heap_arr           4000 × 390
 888000  part_tab_heap_arr
 800000  triangle_tab_heap_arr
 800000  point_tab_heap_arr
 720000  map_elements            60000 × 12    map.c:25
 720000  event_heap_arr          40000 × 18
 278528  shade_tab
 165600  tri_arr
 131072  sine_table              init.c:59
 131072  cosn_table              init.c:60
 131072  shade_texture
  65536  arcsin_tab              init.c:61
```

### 1.1 Size the framebuffers from the active video mode

`set_up_bitmaps()` (`init.c:451-460`) allocates four bitmaps and three mask
planes at a hardcoded `BITMAP_SIZE` of `0x4B281` — 640×480 plus header slack —
regardless of the mode that was actually selected.

| Mode | Allocated now | Actually needed |
|---|---|---|
| bitmaps ×4 | 1,231,364 | 256,000 at 320×200 |
| masks ×3 (int16) | 1,847,046 | 384,000 at 320×200 |

**Saves 2.44 MB in VGA mode.** `set_vga_constants()` / `set_svga_constants()`
(`icon.c:21`, `icon.c:39`) already set `hires_width` / `hires_height` correctly,
so the fix is to order `set_up_bitmaps()` after the mode is chosen and size from
those. No behavioural change — the extra bytes are never addressed, because
every accessor strides by `hires_width`.

### 1.2 Trigonometric tables

`sine_table` and `cosn_table` are 65,536 × `int16_t` = 128 KB each; `arcsin_tab`
is 32,768 × `int16_t` = 64 KB. That is 320 KB of lookup tables against a 486's
8 KB L1, and `find_ellipse` (`ellipse.c:678-751`) performs roughly thirty of
these lookups per part. So this is a cache problem as much as a size one.

Check the original's table sizes in IDA before shrinking. If it really did carry
64K entries, keep them; if not, fold to a quarter table with quadrant reflection,
or drop to 16,384 entries and mask the index. Up to 224 KB.

### 1.3 Pool high-water marks

`map_elements[60000]` (`map.c:25`), `event_heap_arr[40000]`, and
`part_heap_arr[4000]` should be instrumented at runtime to find their actual
peaks across both games. Several of these look like modern safety margins rather
than the original's dimensions.

---

## 2. Hot loops

### 2.1 Globals reloaded on every pixel — the largest single win

The six inner rasterisers — `ellipse_line_win95` (`asm_f.c:280`),
`beam_line_win95`, `shadow_line_win95`, `smoke_line_win95`, `tri_line_win95`
(`asm_f.c:242`) and `tex_tri_line_win95` (`asm_f.c:392`) — all write the
framebuffer through a `char *`. A `char` store may alias anything, so the
compiler is obliged to reload every global in the loop on every iteration.
Confirmed in codegen:

```asm
LBB6_4:                          ; per-pixel tail
	movq	_fb_pitch@GOTPCREL(%rip), %r9
	movslq	(%r9), %r9
	...
	movq	_screen_width@GOTPCREL(%rip), %r9
	movswl	(%r9), %r9d
	addl	(%r12), %r8d          ; shade_dy
	addl	(%r13), %edx          ; z_dy
; and in the body: z_scale, depth_mask, shade_lut all reloaded
```

Five extra loads per pixel out of roughly fifteen total operations. Watcom has no
more information than clang does here and will emit the same reloads.

The fix is mechanical and provably behaviour-identical — nothing in these loops
writes any of the hoisted globals:

```c
void ellipse_line_win95(int mask_idx, int col_height, int z_interp,
                        char *draw_ptr, int shade_idx) {
    const int             sw    = screen_width;
    const int             pitch = fb_pitch;
    const int32_t         zs    = z_scale;
    const int32_t         sdy   = shade_dy;
    const int32_t         zdy   = z_dy;
    int16_t *const        dm    = depth_mask;
    const char *const     lut   = shade_lut;
    const char *const     sm    = &shade_map[0][0];
    const int16_t *const  pr    = &profile[0][0];
    ...
}
```

`shade_map[0][shade_offset]` is additionally loaded twice per pixel — once to
test bit 7, once as the palette index. One local kills the second load.

The same hoist applies to `screen_width` in the column loops of `tri.c` and
`ellipse.c`.

Estimated 25–40 % off the inner rasteriser on an in-order x86.

### 2.2 `check_fade()` runs per primitive

Called from `ellipse.c:29` (every ellipsoid), `ellipse.c:228` (every triangle)
and `tri.c:249` (every textured triangle).

`check_fade()` (`game.c:2036`) calls `my_time()` (`init.c:727`), which is
`platform_ticks() * rate / 1000` — a 32-bit `idiv`, about 40 cycles on a 486.
While a fade is active it additionally runs a 256-iteration, 768-multiply palette
rebuild (`game.c:2051-2055`). Per primitive. At a few hundred parts per frame
that is hundreds of divides, and during fades roughly 200K multiplies per frame.

Hoist to once per frame — `prepare_parts` or `show_parts` — or gate on a frame
counter.

**Verify against IDA first.** `ellipse_shade_ellipse_win95_430FD8` may genuinely
call the fade check inline, in which case moving it is a divergence and needs to
be justified rather than assumed.

### 2.3 Four integer divides per vertex

`perspective_transform` (`display.c:1063`):

```c
int32_t coeff_x = zoom_factor / point->Z;
int32_t coeff_y = coeff_x - (coeff_x >> 3);
coeff_x = coeff_x * screen_width / 320;
coeff_y = coeff_y * screen_height / 200;
```

Three of the four divisors are constant for the whole frame. At ~40 cycles per
`idiv` and ~1800 points, the three wasted divides cost roughly 216K cycles, about
3 ms at 66 MHz.

Reassociating to `(zoom_factor * screen_width / 320) / Z` is **not** safe — it
changes truncation and will shift pixels. The safe form keeps the `/Z` first and
replaces the two constant divides with reciprocal multiply-shifts computed once
per mode change:

```c
/* set in set_vga_constants / set_svga_constants */
coeff_x = (int32_t)(((int64_t)coeff_x * sw_recip) >> 16);
```

That is still not bit-identical to `/320`. Validate with the `ENABLE_FRAME_DUMP`
PPM path (`win.c:43`) before and after.

### 2.4 64-bit arithmetic on a 32-bit target

`matrix_long_vector` (`asm_f.c:73`) is nine 64-bit multiply-accumulates. Watcom
will emit `__I8M` helper calls unless it is written as assembly. The original
used `imul` → `edx:eax` → `shrd 14`, one instruction pair per term — the comment
at `asm_f.c:68-72` records exactly this. On DOS this wants a `#pragma aux`
intrinsic:

```c
#pragma aux fixmul14 = "imul edx" "shrd eax,edx,14" \
    parm [eax] [edx] value [eax] modify [eax edx];
```

The other `int64_t` sites (`ellipse.c:56-90`, `tri.c:99-104`) are per-primitive
rather than per-pixel, so they matter less.

### 2.5 O(pool) free-slot scans

- `find_free_event` / `look_for_free_event` (`game.c:3556`, `game.c:3567`) scan
  40,000 slots — 720 KB — linearly.
- `find_free_part` (`game.c:3634`) scans 4,000 × 390 = 1.56 MB.
- `find_free_actor` (`game.c:3612`) and `find_free_script` (`game.c:3592`)
  likewise.

Each full scan flushes an 8 KB L1 several times over. The obvious fix is a
rotating cursor per pool — resume where the last allocation succeeded, wrap
once, which still covers the whole pool and amortises to O(1).

**Tried and reverted, but the reason for reverting did not survive scrutiny.**

The original conclusion was that a rotating cursor on the event pool and one on
the part pool each independently change rendered output, measured by frame-dump
comparison. That measurement is invalid — see
[Validating a change](#validating-a-change--frame-dumps-do-not-work-for-this).
Cross-build frame comparison does not distinguish a real behavioural change from
build-to-build noise, so those runs showed nothing either way.

The change is therefore **unevaluated**, not disproven. It stays out until there
is a way to tell, because shipping it would mean asserting something unverified,
not because it is known to be wrong.

Worth noting for whoever picks this up: nothing outside the allocators addresses
either pool by slot number, so there is no direct index dependency to point at.
If a real difference does turn up once verification works, the likely mechanism
is a live pointer to a freed slot — the linear allocator hands low slots
straight back and overwrites them immediately, which would mask it, while
rotation leaves freed slots stale much longer. That is the same smell as the
uninitialised-read problem above and may well be the same bug.

### 2.6 Per-pixel clipping in the blitters

`put_graphic_win95` (`init.c:1092-1104`) and `text()` (`init.c:1172-1187`)
bounds-check inside the innermost loop. Pre-clip the rectangle, then run the copy
unchecked. `text()` also compares each glyph cell against `'#'` per pixel;
precomputing the font as a bitmask at init turns that into a shift and test.

### 2.7 Cache behaviour of the column rasteriser — structural, leave alone

`mask_ptr += screen_width` walks the z-buffer with a 1280-byte stride and the
framebuffer with a 640-byte stride. A 200-pixel column touches 400 distinct cache
lines, about 12.8 KB, against a 486's 8 KB L1. `shade_map` (16 KB) and `profile`
(32 KB) are randomly indexed per pixel on top of that.

This is inherent to the original algorithm. Converting to scanline order would be
a genuine divergence and is out of scope.

One faithful mitigation is worth measuring: interleaving `shade_map` and
`profile` into a single array so a visible pixel fetches both from one cache
line instead of two. The trade is worse locality for the `shade_map`-only
rejection test, so measure before committing.

---

## 3. Open Watcom compatibility

| Item | Where | Status |
|---|---|---|
| Anonymous struct/union in `vector_t` | `types.h:165-172` | C11, not C99. OW supports it as an extension — confirm for the target version, otherwise name the union. |
| Variadic macro `DBG_LOG` | `types.h:298` | C99, fine on OW 2.0 |
| `snprintf` (57 call sites) | throughout | C99, fine on OW 2.0; OW 1.9 needs a shim |
| `<dirent.h>`, `<strings.h>`, `<sys/stat.h>` | `file.c:29-33`, `game.c:30`, `move.c:26` | Absent on OW. `compat.h:25-61` already carries a Win32 `dirent` shim; add a DOS branch over `_dos_findfirst`/`_dos_findnext`. `strcasecmp` → `stricmp`. |
| `<unistd.h>`, `<execinfo.h>`, `_exit`, `backtrace` | `main.c:15-20`, `win.c:23` | Already gated behind `HAVE_EXECINFO`. `_exit` becomes `exit`. |
| `double` / `atan2` | `init.c:612-687`, `ellipse.c:786` | Init-time only, but drags in the FP library — emulated on a 386/486SX. Generate the tables with integer maths, or ship them as static data. `arctan_slow` (`ellipse.c:784`) is the only runtime `atan2`; check whether it is reachable at all and delete it if not. |
| `float` parameters | `platform.h:289`, `platform.h:294` | Change to `int` 0–127. Two call sites. |
| `#pragma pack(push,1)` | throughout | Supported |
| `-fwrapv` semantics | `CMakeLists.txt:51` | OW wraps signed overflow and shifts arithmetically — matches. Confirm alongside `-zp1`. |

Compiler invocation: `wcc386 -6s -otexan -zp1 -zq -bt=dos` (or `-bt=nt` for
Win9x), linked with `wlink` against DOS/4GW.

---

## 4. New platform backend

`src/platforms/dos.c`, implementing the 26 functions in `platform.h`.

- **Video.** Mode 13h at 320×200 makes `platform_blit` a straight `memcpy` of
  `bitmap[db]` to `0xA0000` — no palette expansion and no scaling, both of which
  the desktop path currently performs (`win.c:76`). VESA linear framebuffer for
  the 640×480 mode.
- **Keyboard.** An IRQ9 handler. The `PKEY_*` table (`platform.h:18-60`) is
  already raw scancodes, so this maps directly.
- **Mouse.** int 33h.
- **Audio.** Sound Blaster DMA behind `platform_audio_play_pcm`.
- **Music.** MPU-401 or OPL3. Note that the engine currently converts the native
  GEN2 tune banks to SMF and hands over a blob (`music.c`); on DOS it would be
  better to drive the original driver format directly and skip the conversion.

Build system: a `wmake` fragment. The `pocket/` build — a flat compile of
`../src/*.c` against a fixed toolchain, bypassing CMake entirely — is the right
template.

Release builds should also drop `ENABLE_FRAME_DUMP` (`win.c:43-74`) and the
unconditional `fopen("ecstatica_debug.log", "w")` with `setvbuf(_IONBF)`
(`main.c:73-77`). Unbuffered writes to a DOS filesystem are expensive and
`DBG_LOG` has 62 call sites.

---

## 5. Work order

Sequenced so that each step is verifiable on the existing desktop build before
the DOS toolchain enters the picture.

| # | Task | Risk | Section |
|---|---|---|---|
| 0 | **Find the uninitialised read** (Linux MSan run) | — | [Validating a change](#validating-a-change--frame-dumps-do-not-work-for-this) — blocks everything below that needs verification |
| 1 | Size framebuffers from the active video mode | none | [1.1](#11-size-the-framebuffers-from-the-active-video-mode) ✅ done |
| 2 | Hoist globals out of the six rasteriser loops | none | [2.1](#21-globals-reloaded-on-every-pixel--the-largest-single-win) ✅ done |
| 8 | Blitter pre-clipping | none | [2.6](#26-per-pixel-clipping-in-the-blitters) ✅ done |
| — | Volume API off `float`, POSIX shims via `compat.h` | none | [3](#3-open-watcom-compatibility) ✅ done |
| 3 | Rotating cursors on the find-free scans | unevaluated | [2.5](#25-opool-free-slot-scans) ⛔ held, needs step 0 |
| 4 | Hoist `check_fade` to per frame | needs IDA check | [2.2](#22-check_fade-runs-per-primitive) |
| 5 | `platforms/dos.c`, DOS `dirent` shim, wmake; build and measure | — | [3](#3-open-watcom-compatibility), [4](#4-new-platform-backend) |
| 6 | Divide elimination and `#pragma aux` fixed-point | changes pixels | [2.3](#23-four-integer-divides-per-vertex), [2.4](#24-64-bit-arithmetic-on-a-32-bit-target) |
| 7 | Trig table and pool sizing | needs IDA check | [1.2](#12-trigonometric-tables), [1.3](#13-pool-high-water-marks) |
| 9 | shade_map/profile interleave | measure first | [2.7](#27-cache-behaviour-of-the-column-rasteriser--structural-leave-alone) |

The four items marked done are ones whose correctness can be argued from the
code without needing a behavioural diff: hoisting loop-invariant reads, sizing
an allocation to the mode that will actually be addressed, clipping a rectangle
once instead of per pixel, and moving a 0..255 volume across an interface as an
int instead of a float. Everything below the line either changes output by
design or cannot currently be checked, which is why step 0 comes first.

`arctan_slow` (`ellipse.c:784`) has no callers. It is the only *runtime* use of
`atan2`, but deleting it would not drop `<math.h>` from the build — `init.c:612-687`
still builds the trig tables with `atan`/`sin`/`cos` at startup. Left in place;
the FP dependency to attack is the table generation, not this function.

Steps 4, 6 and 7 change output or diverge from the original and need frame-dump
comparison plus, where noted, confirmation against the disassembly.

### Validating a change — frame dumps do NOT work for this

This was tried and does not hold up. Recorded so nobody repeats it.

The `ENABLE_FRAME_DUMP` path (`win.c:43`) auto-dumps frames 60, 120, 200, 300,
500 and 800 as PPM. Two problems, in order of discovery:

1. A stock build is not reproducible across runs — `my_time()` reads the wall
   clock, so everything past the title sequence diverges between two runs of the
   same binary. `MY_TIME_DETERMINISTIC` (`types.h:28`) fixes that much: it makes
   `my_time()` a frame counter, and the same binary then reproduces all six
   frames byte-for-byte, run after run.

2. **It is still not reproducible across builds.** Two from-scratch builds of
   *identical source*, differing only in build directory, produce different
   output from frame 2 onward. Verified directly: `main` built clean in two
   separate directories, both with `MY_TIME_DETERMINISTIC`, diverge.

So the dumps measure per-binary determinism only. Any cross-build `cmp` verdict
— which is what "did my change alter behaviour" needs — is noise. Comparisons
made this way, including several recorded earlier in this document's history,
do not support the conclusions drawn from them.

That the rendered output depends on binary layout at all points at an
uninitialised read somewhere in the render or scene-setup path: same source,
same logical state, different memory contents. AddressSanitizer over the full
boot-to-frame-800 sequence reports nothing, which fits — ASan catches
out-of-bounds and use-after-free, not reads of uninitialised memory. That needs
MemorySanitizer, which is not available on macOS; a Linux MSan run is the
obvious next move, and finding that bug is worth more than any of the
performance work here, because it is what is standing between this project and
a usable regression test.

Until then, treat behavioural verification as **unsolved**. Changes have to be
argued from the code — that a transformation is provably value-preserving —
rather than demonstrated by diffing frames.
