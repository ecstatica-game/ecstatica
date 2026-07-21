# FAN Texture Data Type

Textures are 8-bit indexed-color bitmaps stored inside `.FAN` archives, applied to triangular faces as surface textures. This is an E2-only feature; the version thresholds place it beyond E1 DOS standalone files.

Block type tag: `0x08` (tagged-block format). Minimum file version: **37** (name table), **38** (data section).

---

## Field Layout — Tagged-Block Format

All fields **big-endian** (standard FAN byte order).

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `name_index` | be16 index into the texture name table |
| 2 | 2 | `x_size` | be16 texture width in pixels |
| 4 | 2 | `y_size` | be16 texture height in pixels |
| 6 | `x_size * y_size` | `texture_data` | Raw pixel data, 8-bit indexed color |

Total record size: `6 + (x_size * y_size)` bytes.

## Field Layout — Event-Stream Format

All fields **little-endian** (same convention as sounds).

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `name_index` | le16 index into the texture name table |
| 2 | 2 | `flags` | le16 flags/type value, OR'd with 1 by the loader |
| 4 | 2 | `width` | le16 texture width in pixels |
| 6 | 2 | `height` | le16 texture height in pixels |
| 8 | `width * height` | `pixel_data` | Raw pixel data, 8-bit indexed color |

Total record size: `8 + (width * height)` bytes (per entry, excluding sentinel).

---

## Pixel Data

- **Encoding:** 8-bit indexed color, one byte per pixel.
- **Palette:** resolved from the camera or scene palette at render time; not stored in the texture record.
- **Size:** exactly `width * height` bytes, row-major, no padding or alignment.

---

## Reading Paths

There are two distinct code paths that read texture data.

### Tagged-Block Reader (`file_read_texture`)

Reads texture data from block type `0x08` entries in the tagged-block section. Parses three header fields (name index, x size, y size) as big-endian 16-bit integers, then reads `x_size * y_size` bytes of raw pixel data. No flags field in this format.

### Event-Stream Reader (`read_textures`)

Reads a sentinel-delimited stream of texture entries. Each iteration:

1. Read one byte as a sentinel. If zero or EOF, stop.
2. Read name index (le16).
3. Read flags/type (le16); OR'd with 1.
4. Read width (le16) and height (le16).
5. Skip `width * height` bytes of pixel data.
6. Return to step 1 for the next entry.

The sentinel byte is the only structural difference from the tagged-block reader. A non-zero sentinel means another texture entry follows; zero terminates the list.

---

## Endianness Difference

The tagged-block format uses big-endian fields (matching the rest of the FAN tagged-block section), while the event-stream format uses little-endian fields. This mirrors the same split seen in sound data. The event-stream format also includes an extra `flags` field not present in the tagged-block layout.
