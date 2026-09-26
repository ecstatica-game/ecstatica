# Config / Install Files

Assorted text and small binary configs shipped with the game. None are performance-critical; documented here for completeness.

## `E_CONFIG` / `D_CONFIG`

Small binary game config read at startup — sound driver selection, screen mode, language, etc.

Reader: `setup()` in `src/init.c` — `fopen_ci("e_config", "rb")`, struct `config_t` (`src/init.h`).
Original: read by `init_setup_41007C`, written by `req_handle_ok2_43C5EC` and `req_install_to_disk_43DA80`. Always 32 bytes; a short read is fatal ("Error in configuration file").

```
offset  type    name              notes
  0x00  u8[12]  name              "Ecstatica001", no NUL
  0x0C  u8      Cdrom_path        CD drive letter; '?' when not installed from CD
  0x0D  u8      femaleF           female hero
  0x0E  u8      InstallType
  0x0F  u8      sound_driver
  0x10  u8      SoundCard
  0x11  u8      SoundCardIOAddrl  I/O port, low byte
  0x12  u8      SoundCardIOAddr   I/O port, high byte
  0x13  u8      SoundCardDMA
  0x14  u8      SoundCardIRQ
  0x15  u8      language
  0x16  u8      views
  0x17  u8[9]   reserved          zero-filled by the writer, never read
```

## `CDPATH`

ASCII text file. Contains the drive letter / path where the CD-ROM data is mounted. Single line, DOS newlines.

## `*.STE` — Setup script

ASCII text, CRLF-terminated. Example (`APDLBOAP.STE`):

```
ROOT
DIR E:\ECSTATIC\*
```

Used by the DOS installer / autorun to enumerate CD-side directories. Not consumed by the game runtime.

## `AUTORUN.INF`, `INSTALL.BAT`, `SETUP.BAT`, `DEMO.BAT`

Standard DOS/Windows autorun and installer scripts. Not read by the game.

## Localization text

- `FRENCH.TXT`, `GERMAN.TXT` — string tables for UI localization (line-oriented ASCII, likely index-keyed).
- `LANGUAG.TXT`, `GRAPH.TXT`, `LOWGR.TXT`, `MUSIC.TXT`, `VISIB.TXT` — setup-time metadata (setting menus + valid values).

Format for these is line-based; reverse when localization support lands.

## `TITLE_S.RAW`

Despite the name and location, this is a normal 320 × 200 RAW image (signature `mhwanh`), not a sound. Presumably "S" = "small" or "static" title. See [raw-image.md](raw-image.md).

## `ANTIALIA.DAT`

Anti-aliasing lookup table. Loader `load_anti_alias()` (`init.c:509`) currently returns 1 and doesn't read the file — the shipped Win95 build disables AA. Retained on disc for compatibility with the DOS build.
