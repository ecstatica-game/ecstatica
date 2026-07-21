# .PAK Archive (Ecstatica 1 DOS)

Simple sequential name+size+payload container. Found under `data/e1-dos/ARCHIVE/`:
`ACTIONS.PAK`, `ACTORS.PAK`, `GRAPHICS.PAK`, `MUSIC.PAK`, `REP.PAK`, `SCENES.PAK`, `SOUNDS1.PAK`, `SOUNDS2.PAK`, `VIEWS.PAK`.

The Win95 build (E2) replaced this scheme with the `.FAN` merged archive plus `OFFSETS` index. The DOS runtime is not yet fully ported, so this spec is reconstructed from hex inspection, not from a C reader.

## Layout

```
loop until EOF:
    u8[12] filename          # NUL-padded ASCII, uppercase, DOS 8.3
    le32   payload_size      # size of payload in bytes (excludes header)
    u8[payload_size] payload # opaque; content type depends on outer PAK
```

## Example (`GRAPHICS.PAK` first entry)

```
offset 0x00: "TITLE1.RAW\0\0"                 12 bytes
offset 0x0C: A0 14 00 00                      le32 = 0x14A0 (5280)
offset 0x10: 6D 68 77 61 6E 68 ...            payload starts (mhwanh RAW)
next entry at 0x10 + 0x14A0 = 0x14B0
```

The 12-byte name field fits DOS 8.3 (`NAME.EXT` + trailing NULs). Files longer than 8.3 do not appear in shipped PAKs.

## Payload types by archive

| PAK | Payload type |
|---|---|
| `GRAPHICS.PAK` | `.RAW` image files (see [raw-image.md](raw-image.md)) |
| `VIEWS.PAK` | Per-camera background RAW + palettes (analog of E2's `hires/` + `views/`) |
| `MUSIC.PAK` | Per-driver `.BIN` tune blobs (see [music-bin.md](music-bin.md)) |
| `SOUNDS1/2.PAK` | Raw PCM sound effects (`.FAN` naming preserved inside) |
| `ACTIONS.PAK`, `ACTORS.PAK`, `SCENES.PAK`, `REP.PAK` | Individual `.FAN` blocks per entry; format equivalent to a single tagged block from [fan-archive.md](fan-archive.md) |

## Notes

- No global header, no directory, no checksum. Reader must walk sequentially.
- No compression at the container level; payload may itself be packed (RAW backgrounds use the same `unpack_bitmap`/`unpack_mask` scheme as E2 — see [packed-background.md](packed-background.md)).
- Byte order inside payloads varies; typically big-endian for graphics headers, matching E2 conventions.
