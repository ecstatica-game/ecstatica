# Sound Block (embedded in `.FAN`)

Sound effects are stored as tag `0x05` blocks inside `.FAN` archives (see [fan-archive.md](fan-archive.md)). Unlike every other block type, the payload is **little-endian**.

Reader: `file_read_sound()` in `src/file.c:604`. Consumer struct: `sound_heap_t` (`game.h`).

## Layout

```
le16   name_index               # index into sound_names[], SOUND_TAB_SIZE
le16   use_flag                 # OR'd with 1 by loader
le16   field_10                 # opaque; possibly sample-rate class flags
le32   stored_length            # total payload incl. 32-byte header
le16   volume                   # default 100 if ≤ 0
u8[32] header                   # opaque runtime metadata
u8[stored_length - 32] pcm      # unsigned 8-bit PCM, mono
```

`stored_length - 32` bytes of PCM follow the header; `sound->sound_length` records that PCM length.

## PCM specifics

- Format: **unsigned 8-bit PCM** (DirectSound `WAVE_FORMAT_PCM`, 8 bits/sample, 1 channel).
- Sample rate: **22050 Hz** (bug 30 in `PLAN.md` — original C port had a byte-swap error that stored 8790 Hz).
- Byte order and 32-byte header skip both matter: prior port omitted them and produced garbled audio.

## 32-byte opaque header

Not fully reversed. Stored verbatim into `sound_heap_t.header` and skipped on playback. Likely contains loop points / sample-rate override / ADPCM flags. Empirically the sample rate is fixed at 22050 Hz, so the header is probably informational only.

## CoreAudio playback (macOS backend)

`play_sound_win95` wires `AudioPtr` + `sound_length` into the CoreAudio queue path — 22050 Hz, u8, mono. See recent commit `f89484f Sound effects — CoreAudio backend + wire play_sound_win95`.
