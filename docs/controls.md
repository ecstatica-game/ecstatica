# Input Controls

## Ecstatica 1 — Keyboard Controls

### Movement

| Key | Action |
|-----|--------|
| Numpad 8 / Arrow Up / W | Walk forward (speed depends on movement mode) |
| Numpad 2 / Arrow Down / S | Walk backward (speed depends on movement mode) |
| Numpad 4 / Arrow Left / A | Turn left (speed depends on movement mode) |
| Numpad 6 / Arrow Right / D | Turn right (speed depends on movement mode) |
| Up+Left / W+A | Walk forward + turn left |
| Up+Right / W+D | Walk forward + turn right |

### Combat

| Key | Alt Key | Action |
|-----|---------|--------|
| Numpad 7 | Q | Unarmed: left swing. Armed: upper cut |
| Numpad 9 | E | Unarmed: straight punch. Armed: right swing |

### Item Handling

| Key | Alt Key | Action |
|-----|---------|--------|
| Numpad 1 | Z | Left hand pick up / drop (weapons auto-move to right hand if free) |
| Numpad 3 | C | Right hand pick up / drop (weapon hand) |
| Space | Pick up / interact (auto-selects free hand; right first, left if right is full) |
| Left Alt | Use held item (left hand first, then right hand) |

### Other Actions

| Key | Action |
|-----|--------|
| Left Shift | Jump (hold + Up for running jump) |

### Combat (Ctrl held)

| Key | Action |
|-----|--------|
| Ctrl + Up/W | Attack forward (upper cut) |
| Ctrl + Left/A | Attack left (left swing) |
| Ctrl + Right/D | Attack right (right swing) |
| Ctrl + Down/S | Attack low |

### Speed Modes

| Key | Action |
|-----|--------|
| 1 | Sneak mode |
| 2 | Walk mode (default) |
| 3 | Running mode |
| F1-F4 | Sneak mode |
| F5-F8 | Walk mode (default) |
| F9-F12 | Running mode |

### System

| Key | Action |
|-----|--------|
| Escape | Menu (save, load, options, quit) |
| Enter | Open inventory screen |
| I | Toggle HUD icons on/off |
| G | Switch graphics between Original (320×200) and Enhanced (640×480). Needs the Win95 data — no effect otherwise |

### Inventory Screen

| Key | Action |
|-----|--------|
| Escape | Close inventory |
| Enter | Close inventory |

---

## Ecstatica 2 — Keyboard Controls

### Movement (Walk)

| Key | Action |
|-----|--------|
| Arrow Up / Numpad 8 / W | Walk forward |
| Arrow Down / Numpad 2 / S | Walk backward |
| Arrow Left / Numpad 4 / A | Turn left |
| Arrow Right / Numpad 6 / D | Turn right |
| Up+Left / W+A | Walk forward + turn left |
| Up+Right / W+D | Walk forward + turn right |

### Movement (Run)

| Key | Action |
|-----|--------|
| Ctrl + Up/W | Run forward |
| Ctrl + Down/S | Run backward |
| Ctrl + Left/A | Run left |
| Ctrl + Right/D | Run right |

### Actions

| Key | Action |
|-----|--------|
| Space | Pick up / interact |
| Left Shift | Jump (hold + Up for running jump) |
| Left Alt | Use held item / flip / roll |

### Combat (Ctrl held)

| Key | Action |
|-----|--------|
| Ctrl + Up/W | Attack forward |
| Ctrl + Down/S | Attack backward |
| Ctrl + Left/A | Attack left |
| Ctrl + Right/D | Attack right |

### Combat (Alt held)

| Key | Action |
|-----|--------|
| Alt + Up/W | Magic/special forward |
| Alt + Down/S | Magic/special backward |
| Alt + Left/A | Magic/special left |
| Alt + Right/D | Magic/special right |

### Combat (Ctrl + Alt held)

| Key | Action |
|-----|--------|
| Ctrl+Alt + Up/W | Aimed attack up/down (target-dependent) |
| Ctrl+Alt + Down/S | Aimed attack up/down (target-dependent) |
| Ctrl+Alt + Left/A | Aimed attack left (target-dependent) |
| Ctrl+Alt + Right/D | Aimed attack right (target-dependent) |

### System

| Key | Action |
|-----|--------|
| Escape | Pause menu |
| Enter | Open inventory screen |
| I | Toggle HUD icons on/off |

### Inventory Screen

| Key | Action |
|-----|--------|
| Escape | Close inventory |
| Enter | Close inventory |

---

## Menu / Dialog Navigation

| Key | Action |
|-----|--------|
| Space / Enter | Confirm selection |
| Escape | Cancel / back |
| Mouse click | Select menu option |

---

## Mouse

| Input | Action |
|-------|--------|
| Mouse movement | Menu cursor |
| Left click | Select / confirm (menus) |
| Right click | Context action (menus) |

---

## Gamepad (Steam Deck / Xbox / PlayStation / Generic)

Supported on all platforms: macOS (GameController.framework), Linux (joystick API), Windows (XInput).
Auto-detected and hot-pluggable — no configuration needed. Gamepad and keyboard can be
used simultaneously. See [Troubleshooting](#troubleshooting-linux) if a button lands on
the wrong action.

### Movement

| Button | Action |
|--------|--------|
| Left Stick / D-pad | Walk (8-direction) |
| Left Stick Click | Cycle speed mode (sneak → walk → run) — E1 only |

### Actions

| Button | Keyboard Equivalent | Action |
|--------|---------------------|--------|
| A / Cross (south) | Space | Pick up / interact |
| B / Circle (east) | Escape | Cancel / back |
| X / Square (west) | Left Alt | Use item / flip / roll / magic |
| Y / Triangle (north) | Enter | Open inventory |
| LB / L1 | Left Shift | Jump |
| RB / R1 | Left Ctrl | Run modifier / attack modifier |
| LT / L2 | Left Alt | Magic/special modifier (E2) |
| RT / R2 | Left Ctrl | Attack modifier (alternative) |
| Start | Escape | Pause menu |
| Select / Back | I | Toggle HUD icons |
| Right stick click (R3) | G | Original / Enhanced graphics (E1 with Win95 data) |

### Combat (E1)

| Input | Action |
|-------|--------|
| Right Stick Left | Left swing / upper cut (Numpad 7 / Q) |
| Right Stick Right | Straight punch / right swing (Numpad 9 / E) |
| RB/RT + Left Stick | Directional attacks (Ctrl + direction) |

### Combat (E2)

| Input | Action |
|-------|--------|
| RB/RT + Left Stick | Directional attacks (Ctrl + direction) |
| X or LT + Left Stick | Magic/special (Alt + direction) |
| RB/RT + X/LT + Left Stick | Aimed attacks (Ctrl + Alt + direction) |

### Item Handling (E1)

| Input | Action |
|-------|--------|
| Right Stick Down | Left hand pick up / drop (Numpad 1 / Z) |
| Right Stick Up | Right hand pick up / drop (Numpad 3 / C) |
| A / Cross | Auto pick up (Space — right hand first) |

The right stick is E1-only: E2 has no numpad actions, and under E1 the same
scancodes suppress the diagonal walk, so E2 leaves the stick unbound.

### Troubleshooting (Linux)

The joystick API reports button *codes*, not physical positions, and the two
are not the same thing. `BTN_X` and `BTN_Y` are aliases for `BTN_NORTH` and
`BTN_WEST`, so a driver that names its buttons in Xbox letter order (xpad,
hid-steam — the Steam Deck's own pad — and every virtual pad Steam Input
creates) reports the physical *left* button as north and the physical *top*
button as west. Drivers written against the compass names (hid-playstation,
hid-nintendo) do not. The device name decides which reading applies; override
it if a pad guesses wrong:

| Variable | Effect |
|----------|--------|
| `ECSTATICA_GAMEPAD_DEBUG=1` | Print the detected pad, its resolved button/axis slots, and any `/dev/input/js*` node rejected as not-a-gamepad |
| `ECSTATICA_GAMEPAD_SWAP_FACE=0` | Force compass-name reading: X is west (use item), Y is north (inventory) |
| `ECSTATICA_GAMEPAD_SWAP_FACE=1` | Force letter-name reading (the default for unrecognised pads) |
