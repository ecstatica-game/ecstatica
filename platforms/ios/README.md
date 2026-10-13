# Ecstatica on iOS and iPadOS

This is an iPhone and iPad build of the C99 port. It uses the same engine as
every other target. The iOS-specific code is in `../../src/platforms/ios.m`,
which the engine reaches through `platform.h`. It also uses the shared
`mixer.c`, `screen.c`, `touch_controls.c` and `threads.c`.

One app runs both games. It needs iOS or iPadOS 15 or later.

## Building

You need Xcode and CMake 3.20 or later:

```sh
make ios-sim      # simulator build → platforms/ios/build/Release-iphonesimulator/Ecstatica.app
make ios          # unsigned device build → platforms/ios/build/ecstatica.ipa
make ios-clean
```

The targets configure `platforms/ios/` with CMake's Xcode generator
(`-G Xcode -DCMAKE_SYSTEM_NAME=iOS`) and build it with `xcodebuild`. To work
in Xcode, open `platforms/ios/build/ecstatica_ios.xcodeproj`.

### Installing on a device

`ecstatica.ipa` is unsigned. To install it, use one of these:

- A sideloading tool, such as AltStore or Sideloadly, which signs the app
  with your own Apple ID.
- Your own signed build: set your team with
  `cmake -S platforms/ios -B platforms/ios/build -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DDEVELOPMENT_TEAM=<team ID>`,
  then open the project in Xcode and run it on your device.

### Running in the simulator

```sh
xcrun simctl install booted platforms/ios/build/Release-iphonesimulator/Ecstatica.app
xcrun simctl launch booted io.github.ecstaticagame.ecstatica -game e2
```

The `-game e1` or `-game e2` argument skips the library screen.

## Installing the games

The app's Documents folder is visible in the Files app as
**On My iPhone › Ecstatica** (or **On My iPad**):

```
Ecstatica/
    e1/             ← Ecstatica 1: the DOS release root, with W/ inside it
        CODE/       ← CODE/ECSTATIC.FAN marks the folder as the game
        ...
    e2/             ← Ecstatica 2
        CODE/
        ...
    *.sf2           ← optional General MIDI soundfont, for music
```

You can put the games there in either of these ways:

- In the app, select **Import folder…** and pick the game folder from
  anywhere the Files app reaches, such as iCloud Drive, a USB drive or a
  server. The import skips installers, `DIRECTX/`, `ISHIELD/`, `.EXE` and
  `.DLL` files.
- In the Files app, copy the game folder's contents into `e1` or `e2`.
  Finder on a Mac connected over USB can also copy files into the app.

Saved games are written to `saved/` in each game's folder. Music needs a
General MIDI soundfont in `.sf2` format, such as GeneralUser GS, in the
`Ecstatica` folder. Without a soundfont, sound effects and speech still play.

## Controls

| Input | Use |
| ----- | --- |
| Touch | On-screen stick, A/B/X/Y, LB/RB, LT/RT, START/SELECT, L3/R3. Tapping the picture clicks, for the menus. |
| Game controller | The same mapping as the desktop build (MFi, Xbox, PlayStation). The on-screen pad hides when a controller is used and comes back on the next touch. |
| Keyboard | As on the desktop. |

## How it works

- **Game thread:** UIKit owns the main thread, so the engine's `main()`
  (compiled as `ecs_main`) runs on a thread of its own with an 8 MB stack.
  Its blocking loops work unchanged. The main thread shows frames and
  receives input. The engine keeps its state in globals, so quitting the
  game closes the app.
- **Video:** the game thread expands each frame with `screen.c` into one of
  three RGBA buffers. The buffers have the view's aspect ratio at no more
  than 960 lines, with the on-screen pad drawn on top. A `CADisplayLink`
  copies the newest one into the view's layer, which scales it to the screen.
- **Audio:** a RemoteIO audio unit pulls the shared software mixer, and
  TinySoundFont renders the tunes.
- **Background:** the game thread pauses in `platform_pump_events` while the
  app is not active.
