# Contributing to Ecstatica

This project reverse-engineers Ecstatica 1 & 2 and reimplements them in
portable C99. Contributions fall into two broad categories: decompilation
work (translating original game logic into C) and engine/port work (build
systems, platform backends, bug fixes, new platforms). Both are welcome.

## Before you start

* **Game data is not included.** You need your own legally obtained copy of
  Ecstatica 1 and/or 2 to build and run anything. See the Data Folder
  section of [README.md](README.md) for the expected layout under `data/`.
* **Read [CLAUDE.md](CLAUDE.md) first.** It is the authoritative architecture
  and workflow document — module layout, code style, the decompilation
  process, and a list of original functions that were intentionally dropped
  (dead stubs/wrappers) and should not be re-created. Despite the name, it is
  the most detailed and current reference for human contributors too.
* For questions or discussion before opening an issue, use the
  [Discord](https://discord.gg/msQB6yBP9).

## Building

```bash
make          # cmake build → build/bin/ecstatica
make e2       # build + run E2
make e1       # build + run E1
```

See `CLAUDE.md`'s Build section for the full list of targets, including the
DOS, Win9x, PSP, Vita, Analogue Pocket/MiSTer and Steam Deck builds under
`platforms/`. Each of those has its own README with toolchain setup.

## Code style

* C99, strict. `snake_case`. 4-space indent.
* No comments unless explaining a non-obvious *why* — not what the code does.
* `#pragma pack(push, 1)` for any struct that mirrors the original binary's
  memory layout.
* Module names mirror the original Watcom source files (`init.c`, `game.c`,
  `edit.c`, ...). New, non-original code (the viewer, the pointcloud tool)
  lives under `src/tools/` and says so in its header comment.
* Shared types, pool sizes, and forward typedefs go in `types.h`; full struct
  definitions live in the module header that owns them.

## Decompilation contributions

If you're porting a function from the original binary:

1. Verify the struct layouts and control flow against the original — IDA
   Pro's Hex-Rays output is a starting point, not ground truth; disassembly
   and cross-references are the final check.
2. Match behavior, not just intent — including quirks and bugs in the
   original, unless there's a specific reason to fix them (note the reason
   in the commit message if so).
3. Check `CLAUDE.md`'s "Removed functions" list before adding a function back
   — empty stubs and dead wrappers from the original were deliberately
   dropped, not missed.
4. If you're fixing a behavioral mismatch between this port and the original,
   say what the original actually does and how you confirmed it (an IDA
   address, a Watcom symbol, a side-by-side run), not just what looked wrong.

## Testing your change

There's no automated test suite. Before opening a PR:

* Build and run the platform(s) your change affects, with real game data,
  far enough to exercise the change (not just a clean compile).
* For rendering or behavior changes, compare against the original binary
  running in DOSBox/an emulator where practical, not just against your own
  expectations.
* Mention in the PR what you tested and on which platform(s) — see the PR
  template for the checklist.

## Submitting a pull request

1. Fork the repo and branch from `main`.
2. Keep PRs focused — one fix or feature per PR is easier to review than a
   bundle of unrelated changes.
3. Write commit messages that explain *why*, not just what changed.
4. Open the PR against `main` and fill in the template.

By submitting a contribution, you agree it will be licensed under the
project's [GPLv3 license](LICENSE).
