# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

### Added
- Enhanced menu: port-only options (graphics set, subtitle size, subtitle hold) moved out of Settings into their own main-menu entry. The graphics toggle only appears for E1.

### Changed
- Subtitles keep their original layout and timing in the original 320x200 mode: no scaling, block anchored at the top of the screen, original hold.
- ESC opens the menu during the intro instead of only after it. Intro skipping stays on space/enter; saving is refused while the intro runs.

## [0.4.0] - 2026-08-27

### Added
- Analogue Pocket / MiSTer port targeting openfpgaOS via the openfpgaSDK (`pocket/`).
- Model and animation viewer (`--viewer`) and scripted-scene browser (`--scenes`).
- This changelog.

### Fixed
- Edge rasterizer bleeding on polygon boundaries.
- Hand swapping corrupting script state during object interaction.
- Object grabbing accuracy and held-object placement.
- Steam Deck / Steam Machine gamepad handling and DOS-version speech playback.
- Subtitle rendering and enhanced-mode visuals.

## [0.3.1] - 2026-08-17

### Fixed
- E1 combat: damage model, close-range attacks, and removal of E2-only code paths.
- E1 walk animations restarting mid-cycle; restored `next_move` re-decision cooldown.
- Off-grid player positions wrongly triggering the dragon death scene.
- Subtitle cleanup dropping composited actors; full refresh on graphics toggle.
- Zero-volume and malformed cuboid parts painting solid quads.
- Linux gamepad buttons now resolved semantically instead of by raw index.
- Objects overflowing outside the camera view.

## [0.3.0] - 2026-08-15

### Added
- Enhanced-mode toggle to switch between DOS and Win95 presentation.

### Fixed
- DOS bundle version detection.

## [0.2.1] - 2026-08-15

### Added
- Save game persistence.

### Fixed
- Scene 146 (lady at table) repeating.

## [0.2.0] - 2026-08-15

### Fixed
- E1 prop collision: `file_read_map_area` read big-endian from a little-endian stream, byte-swapping every map area name and element index.
- Direction conventions: `find_direction_and_distance` was mirrored against the original binary, breaking `blocked_dirs` and `move_angle`.

## [0.1.3] - 2026-08-13

### Added
- Gamepad support with improved bindings.
- MIDI music on Linux via runtime-loaded FluidSynth.
- Initial save/load groundwork.
- Intro skipping.

### Fixed
- Player hand interaction triggers, controls, and camera/map positioning.
- E1 terrain collisions and E2 idle animation.
- Rasterizer output.
- Music volume on Linux.

## [0.1.2] - 2026-07-21

### Fixed
- Windows build.

## [0.1.1] - 2026-07-21

### Fixed
- Cross-platform build compatibility for Linux and Windows.

## [0.1.0] - 2026-07-21

### Added
- Initial public release: C99 reimplementation of Ecstatica 1 & 2 with macOS, Linux, and Windows platform layers.

[Unreleased]: https://github.com/ecstatica-game/ecstatica/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/ecstatica-game/ecstatica/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/ecstatica-game/ecstatica/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/ecstatica-game/ecstatica/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/ecstatica-game/ecstatica/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/ecstatica-game/ecstatica/compare/v0.1.3...v0.2.0
[0.1.3]: https://github.com/ecstatica-game/ecstatica/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/ecstatica-game/ecstatica/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/ecstatica-game/ecstatica/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/ecstatica-game/ecstatica/releases/tag/v0.1.0
