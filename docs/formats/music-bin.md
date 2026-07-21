# Music BIN / SCC / GUS / SBL / AWE / LAP

Per-driver tune banks under `MUSIC/`. Each file targets one sound-card driver family; the runtime picks one at startup based on `sound_driver`.

**Status: not yet reverse-engineered.** Playback is stubbed in `music.c:64` (`play_tune`) — the file is loaded into `tune_buffer` but the synth loop is deferred.

## Files (E1 DOS `MUSIC/`)

| Pattern | Driver family | Notes |
|---|---|---|
| `E*.SCC` / `EBKGRD1.SCC` | Sierra SCC / General MIDI-ish | Highest-quality bank on shipped media |
| `E*.GUS` | Gravis UltraSound wavetable | Uses on-card patch caching |
| `E*.SBL` | Sound Blaster / OPL FM | |
| `E*.AWE` | SB AWE / EMU8000 wavetable | |
| `E*.LAP` | LAPC / MT-32 native | |
| `AWE0.BIN`, `GUS0.BIN`, `SBL0.BIN`, `SCC0.BIN`, `LAP0.BIN` | Merged bank files | Referenced from `tune_offset[driver][slot]` |
| `BREAK.8`, `DUMMEY.BIN` | Sentinel / silence | |

Naming: `E<tag>.<ext>` where `<tag>` = `BADEND`, `BKGRD1..N`, `ENDING`, etc. Corresponding tune indices are keyed in the `.FAN` code tokens.

## Known runtime shape

From `music.c:53`:

> Custom MIDI-like stream stored per-driver. Cross-platform port needs:
> 1. Reverse-engineer the tune stream format (note on/off, pitch bend, program change, tempo, per-channel volume).
> 2. Build a soft synth (OPL emulation for SBL, wavetable for AWE, or convert to standard MIDI and feed AVAudioUnitSampler with a General MIDI SoundFont).

## Offset table

Located inside `OFFSETS` — see [offsets.md](offsets.md). Layout: `tune_offset[10][96]` be32 values. 10 driver slots × 96 tunes.

## TODO (when reversing)

- Byte-scan a short tune blob (e.g. `EBKGRD1.SCC`) for delta-time / status-byte structure. If close to Standard MIDI, dump as `.mid` for playback via SoundFont.
- Identify per-driver command dialect variance (SBL likely embeds OPL register writes; AWE/GUS likely have patch-select opcodes).
- Locate the E2 dispatcher (in the Win95 binary). E1 dispatcher lives in the DOS `ECSTATIC.EXE` at driver-init time.
