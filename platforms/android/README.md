# Ecstatica on Android

This is an Android build of the C99 port. It uses the same engine as every
other target. The Android-specific code is in `../../src/platforms/android.c`,
which the engine reaches through `platform.h`. It also uses the shared
`mixer.c`, `screen.c`, `touch_controls.c` and `threads.c`.

One APK runs both games. It needs Android 8.0 (API 26) or later, and contains
`arm64-v8a` (phones, tablets) and `x86_64` (emulators, Chromebooks)
libraries.

## Building

You need the Android SDK with an NDK, build-tools and a platform (API 34 or
later) installed, plus a JDK. The build uses the SDK's command-line tools
directly, so you do not need Gradle or Android Studio:

```sh
export ANDROID_HOME=~/Library/Android/sdk     # or wherever the SDK is
sdkmanager "ndk;27.2.12479018" "platforms;android-34" "build-tools;35.0.0"

make android                  # → platforms/android/build/ecstatica.apk
make android-install          # adb install -r
make android-clean
```

`make android` runs `platforms/android/build.sh`. That script:

1. Compiles `libecstatica.so` for each ABI with the NDK's CMake toolchain.
2. Compiles the launcher's Java code with `javac` and `d8`.
3. Links the manifest and icons with `aapt2`.
4. Aligns the APK and signs it with `apksigner`.

To sign with a release key, set `ANDROID_KEYSTORE`, `ANDROID_KEYSTORE_PASS`
and `ANDROID_KEY_ALIAS`. Without them, the script makes a debug key in
`build/`. An APK signed with one key cannot update an install signed with
another.

## Installing the games

Open **Ecstatica** and, for each game, select one of these:

- **Import folder** and pick the folder that holds `CODE/ECSTATIC.FAN`.
- **Import zip** and pick a zip of that folder.

Either way, the game is copied into the app's own storage. A folder or zip
that holds the game one or two levels down also works. The import skips
installers, `DIRECTX/`, `ISHIELD/`, `.EXE` and `.DLL` files.

You can also copy the games by hand, over USB or with `adb push`:

```
/sdcard/Android/data/io.github.ecstaticagame.ecstatica/files/
    e1/             ← Ecstatica 1: the DOS release root, with W/ inside it
        CODE/
        ...
    e2/             ← Ecstatica 2
        CODE/
        ...
    *.sf2           ← optional General MIDI soundfont, for music
```

Saved games are written to `saved/` in each game's folder. **Remove**
deletes the game and its saves, and uninstalling the app deletes all of it.

Music needs a General MIDI soundfont in `.sf2` format, such as GeneralUser
GS. Import it with **Import .sf2 soundfont**. Without a soundfont, sound
effects and speech still play.

## Controls

| Input | Use |
| ----- | --- |
| Touch | On-screen stick, A/B/X/Y, LB/RB, LT/RT, START/SELECT, L3/R3. Tapping the picture clicks, for the menus. |
| Gamepad | The same mapping as the desktop build. The on-screen pad hides when a pad is used and comes back on the next touch. |
| Keyboard, mouse | As on the desktop. |
| Back | Escape (the game menu). |

The on-screen pad sits in the bars beside the 4:3 picture on a phone in
landscape. The scale modes in the settings menu (pillarbox, crop, stretch)
work as on the Vita.

## How it works

- **Activities:** `LauncherActivity` (Java) is the game library.
  `GameActivity` is a `NativeActivity` running in its own process (`:game`).
  The engine keeps its state in globals and quits with `_exit`, so each game
  starts fresh and quitting returns to the launcher.
- **Game thread:** `android_main` runs the engine's `main()` (compiled as
  `ecs_main`) on the `native_app_glue` thread. `platform_pump_events` drains
  the looper and blocks while the app is in the background.
- **Video:** the window buffer has the screen's aspect ratio at no more than
  960 lines, and the compositor scales it to the screen. `screen.c` expands
  the frame into it, and `touch_controls.c` draws the pad on top.
- **Audio:** an AAudio callback pulls the shared software mixer.
