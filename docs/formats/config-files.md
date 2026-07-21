# Config / Install Files

Assorted text and small binary configs shipped with the game. None are performance-critical; documented here for completeness.

## `E_CONFIG` / `D_CONFIG`

Small binary game config read at startup — sound driver selection, screen mode, language, etc.

Reader: `init.c:80` — `fopen("e_config", "rb")`.

Layout not yet fully reversed. First bytes on E1 shipped media: `Ecstatica001\0` (13-byte magic), followed by driver / mode / language u8 fields.

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
