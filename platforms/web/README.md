# Ecstatica in the browser

This is a WebAssembly build of the C99 port. It uses the same engine as every
other target. The browser-specific code is in `../../src/platforms/web.c`,
which the engine reaches through `platform.h`. It also uses the shared
`mixer.c`, `screen.c` and `threads.c`.

One page runs both games. The player imports their own copy of each game into
the browser's private storage once. The files are never uploaded.

## Building

You need [Emscripten](https://emscripten.org) 3.1.53 or later, with
`emcmake` on `PATH`:

```sh
make web          # → platforms/web/build/dist/
make web-serve    # serves it on http://localhost:8080 with the headers it needs
make web-clean
```

`make web` runs the same commands as:

```sh
emcmake cmake -S platforms/web -B platforms/web/build -DCMAKE_BUILD_TYPE=Release
cmake --build platforms/web/build
```

The output in `build/dist/` is a static site:

| File | Purpose |
| ---- | ------- |
| `index.html` | Game library, import, on-screen pad and the game canvas |
| `ecstatica.js`, `ecstatica.wasm`, `ecstatica.worker.js` | The engine |
| `coi-sw.js` | Service worker that adds the cross-origin isolation headers |
| `icon.png`, `banner.png` | Page art, from `assets/generate_packaging_art.py web` |

## Hosting

The game uses threads, and browsers give threads (`SharedArrayBuffer`) only
to a cross-origin isolated page. The page must be served over HTTPS, or from
`localhost`, with these headers:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

`serve.py` sends them. If your host cannot send headers (GitHub Pages, for
example), `coi-sw.js` adds them. The page registers it and reloads once, and
after that **Play** is enabled.

## Importing the games

For each game, select **Import folder…** and pick the folder that holds
`CODE/ECSTATIC.FAN`. You can also select **Import .zip…** and pick a zip of
that folder. Most phones and tablets cannot pick a folder, so use a zip there.
A folder or zip that holds the game one level down also works.

The page copies the game into the
[Origin Private File System](https://developer.mozilla.org/docs/Web/API/File_System_API/Origin_private_file_system):

```
/ecstatica/
    e1/             ← Ecstatica 1: the DOS release root, with W/ inside it
    e2/             ← Ecstatica 2
    *.sf2           ← optional General MIDI soundfont, for music
```

The import skips installers, `DIRECTX/`, `ISHIELD/`, `.EXE` and `.DLL` files.
Saved games are written to `saved/` in the game's folder, so they persist
with it. **Remove** deletes the game and its saves.

Music needs a General MIDI soundfont in `.sf2` format, such as GeneralUser
GS. Import it with **Import .sf2 soundfont…**. Without a soundfont, sound
effects and speech still play.

## Controls

The keyboard, mouse and gamepad work as in the desktop build. Gamepads use
the browser's standard mapping.

On a touch screen the page shows an on-screen pad with the same layout as
the Android and iOS builds. Tapping the picture is a mouse click, for the
menus.

## How it works

- **Threads:** the build uses `-sPROXY_TO_PTHREAD`, so `main()` runs on a
  worker. The engine's blocking loops work unchanged. The browser's main
  thread only draws frames, plays audio and receives input.
- **Files:** WASMFS mounts the Origin Private File System at `/opfs`. The
  game runs in `/opfs/ecstatica/e1` or `e2`.
- **Video:** the game thread palette-expands each frame into one of three
  RGBA buffers. Every animation frame, the main thread copies the newest one
  onto the canvas. CSS scales the canvas to 4:3.
- **Audio:** a `ScriptProcessorNode` pulls the shared software mixer, and
  TinySoundFont renders the tunes.

Supported browsers are current Chrome, Edge, Firefox and Safari. The import
runs in a worker with synchronous access handles, so it also works in Safari,
which has no `createWritable`.
