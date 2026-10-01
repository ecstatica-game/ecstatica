# Steam Deck / Steam Library Art

Steam Deck runs the same Linux build as any other desktop — there's no
packaging step, so this isn't a build target like `vita/`, `psp/` or
`pocket/`. What's here is the library art for adding the two games as
non-Steam-game shortcuts, in the sizes Steam's own grid and SteamGridDB
tools expect:

| File | Size | Where Steam shows it |
|---|---|---|
| `grid_portrait.png` | 600x900 | Library grid view, and the default capsule |
| `grid_landscape.png` | 460x215 | Big Picture / grid view's older wide layout |
| `hero.png` | 1920x620 | The banner behind a game's details page |
| `logo.png` | 640x360, transparent | The title logo overlaid on the hero |
| `icon.png` | 256x256 | Shortcut icon, taskbar, Steam Input glyphs |

Generated from real screenshots — see `assets/README.md` for the source
images and the script that builds these.

## Adding a shortcut

In Steam: **Add a Game → Add a Non-Steam Game**, point it at `build/bin/ecstatica`
(with the right `data/` beside it for E1 or E2), then right-click the new
shortcut → **Manage → Set Custom Artwork** and drop in the five files above —
or place them directly under
`steamapps/userdata/<user-id>/config/grid/` following
[Steam's grid naming convention](https://www.pcgamingwiki.com/wiki/Steam#Custom_artwork),
which a tool like SteamGridDB Manager will also do for you.
