# Security Policy

## Supported Versions

This project ships rolling edge builds off `main` plus tagged releases. Only
the latest tagged release and the current `main` branch are supported —
please reproduce against one of those before reporting.

## Scope

This is a native C99 reimplementation of a 1990s game engine. The code parses
`.FAN` archives, save files, and other packed binary structures straight into
C structs with minimal validation, matching the original engine's behavior.
Memory-safety issues reachable from **untrusted or malformed game data /
save files** (buffer overflows, out-of-bounds reads/writes, integer overflow
in size calculations, etc.) are in scope and treated as real vulnerabilities,
since game data and saves are routinely shared between players.

Out of scope:

* Bugs or crashes that only reproduce against the original DOS/Win95
  binaries themselves, rather than this port's code.
* Non-exploitable logic bugs, rendering glitches, or behavioral desyncs from
  the original — file those as a regular
  [bug report](https://github.com/ecstatica-game/ecstatica/issues/new/choose)
  instead.

## Reporting a Vulnerability

Please use GitHub's private vulnerability reporting rather than a public
issue: go to the **Security** tab of this repository → **Report a
vulnerability**. This lets a fix land before the details are public.

If that is not available to you, email **xesfnet@gmail.com** with enough
detail to reproduce (a sample archive/save file that triggers the issue is
the most useful thing you can send).

You should get an acknowledgment within a few days. There is no bug bounty —
this is a hobby preservation project — but you will be credited in the fix
unless you ask not to be.
