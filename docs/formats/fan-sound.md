# FAN Sound Data Type

Sound effects are embedded audio samples stored inside `.FAN` archives. Unlike most FAN data which is big-endian, all sound fields are **little-endian**.

Block type tag: `0x05` (tagged-block format). Minimum file version: **16** (name table required).

---

## Field Layout

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `name_index` | le16 index into the sound name table |
| 2 | 2 | `use_flag` | le16 flag, OR'd with 1 by the loader |
| 4 | 2 | `sample_rate` | le16 sample rate value |
| 6 | 4 | `stored_length` | le32 total payload size including the 32-byte header |
| 10 | 2 | `volume` | le16 playback volume; clamped to 100 if <= 0 |
| 12 | 32 | `header` | Opaque 32-byte WAV-like header, stored verbatim |
| 44 | `stored_length - 32` | `pcm_data` | Unsigned 8-bit PCM audio, mono |

Total record size: `12 + stored_length` bytes.

---

## PCM Format

- **Encoding:** unsigned 8-bit PCM, mono, single channel.
- **Conversion:** during load, each sample byte has `0x80` added to convert from unsigned to signed representation.
- **Sample rate:** 22050 Hz at full quality. E1 DOS mode (320x200) downsamples from 21 kHz to 11025 Hz at playback.

---

## 32-Byte Header

Not fully reversed. Stored verbatim and skipped during playback. Likely contains loop points, sample rate overrides, or compression flags based on WAV-header patterns. The header bytes are counted in `stored_length`, so actual PCM length is always `stored_length - 32`.

---

## Reading Paths

There are two distinct code paths that read sound data.

### Tagged-Block Reader (`file_read_sound`)

Reads sound data from block type `0x05` entries in the tagged-block section. Parses the fields sequentially as listed in the layout table above: name index, use flag, sample rate, stored length, volume, then the 32-byte header, followed by `stored_length - 32` bytes of PCM data.

### Event-Stream Reader (`read_sounds`)

Reads a sentinel-delimited stream of sound entries. Each iteration:

1. Read one byte as a sentinel. If zero or EOF, stop.
2. Read the same field sequence as the tagged-block reader: name index (le16), use flag (le16), sample rate (le16), stored length (le32), volume (le16).
3. Read 32 bytes of header data.
4. Read `stored_length - 32` bytes of PCM data.
5. Return to step 1 for the next entry.

The sentinel byte is the only structural difference from the tagged-block reader. A non-zero sentinel means another sound entry follows; zero terminates the list.

---

## Index Remapping

Sound name indices are remapped when loading from files with version >= 16. The `name_index` field indexes into the sound name table, which maps old indices to current ones during scene merges. Event parameters that reference sounds (e.g., PlaySound in action events) use 1-based indexing where 0 means "none."
