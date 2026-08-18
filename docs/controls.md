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
| Left Shift | Jump — from a standstill only (see note) |

E1's `BH_JOYSTICK` tests the walk keys before it tests Shift, so holding a
direction wins and the jump never fires. The running-jump form the code can
express (`Shift` + `Up`) is unreachable under E1 for that reason; E2 tests
Shift first and does have it.

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

**Not reachable from the keyboard as currently wired.** Left Alt raises both
scancode 56 and the Alt modifier, and `BH_JOYSTICK` tests scancode 56 first —
so pressing Alt always lands on use-item/flip/roll, and the two tables above
(actions `196`..`199` and `204`..`211`) are never reached. The gamepad splits
the two across X and LT and can reach them; the keyboard would need the
modifier sourced from a second key (Right Alt is the likely original, worth
confirming against the binary in IDA).

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

## Gamepad (Steam Deck / Steam Controller / Xbox / PlayStation / Generic)

Supported on all platforms: macOS (GameController.framework), Linux (joystick API), Windows (XInput).
Auto-detected and hot-pluggable — no configuration needed. Gamepad and keyboard can be
used simultaneously. See [Troubleshooting](#troubleshooting-linux) if a button lands on
the wrong action.

### How the map is laid out

Both games drive a body, not a cursor, and the pad is bound to match it:

| Part of the pad | Part of the body |
|-----------------|------------------|
| Left stick (and D-pad) | **Legs** — where you walk and which way you face |
| Shoulder row — LB/LT on the left, RB/RT on the right | **Arms** — the left column drives the left arm, the right column the right |
| Face buttons | Everything that is not a limb — reach out, use, inventory, back |

E1 and E2 model the arms differently, and the map follows that rather than
forcing one layout onto both. **E1 gives the player two hands to drive
independently** — each has its own pick-up/drop key and its own swing — so E1
puts a hand on each trigger. **E2 collapses that to a single pick-up plus
modifier keys**, so under E2 the triggers become the attack and magic
modifiers instead.

### Shared by both games

| Button | Keyboard | Action |
|--------|----------|--------|
| Left stick | Arrows / WASD | Walk and turn, eight directions |
| D-pad | Arrows / WASD | Same as the left stick |
| A / Cross (south) | Space | Reach out — pick up, interact, confirm a menu or dialogue |
| B / Circle (east) | Escape | Back / cancel |
| X / Square (west) | Left Alt | Use what is held (empty-handed: flip / roll) |
| Y / Triangle (north) | Enter | Open the inventory screen |
| LB / L1 | Left Shift | Jump |
| Start | Escape | Pause menu |
| Select / Back | I | Toggle the HUD icons |
| Right stick click (R3) | G | Switch between the Original and Enhanced graphics sets |

R3 needs a `HIRES` set beside the game data to have anything to switch to —
E2's data ships one, E1 needs the Win95 release. Without it the button is a
no-op. **Left stick click (L3) does something different in each game** and is
listed in the per-game tables below.

The left stick is read radially: distance from centre decides whether it counts
as pushed at all, then the angle picks one of eight 45-degree sectors.
Thresholding each axis on its own instead would hand out a diagonal for most of
the stick's travel — and since left and right *turn* rather than strafe, that
reads as the character veering off instead of walking straight ahead.

### Ecstatica 1 — the two hands

E1 tracks each hand separately: the right hand is the weapon hand, the left is
the free hand, and either can be holding something. Each trigger is that hand.

| Button | Keyboard | Action |
|--------|----------|--------|
| **LT / L2** | Numpad 1 / Z | **Left hand** — pick up what is in front of you, or put down what it holds |
| **RT / R2** | Numpad 3 / C | **Right hand** — same, for the weapon hand |
| Right stick ← | Numpad 7 / Q | Left swing (unarmed) / upper cut (armed) |
| Right stick → | Numpad 9 / E | Straight punch (unarmed) / right swing (armed) |
| RB / R1 + left stick | Ctrl + direction | Directional attack — see below |
| **Left stick click (L3)** | F1 / F5 / F9 | Cycle speed mode: walk → run → sneak → walk |
| A / Cross | Space | Pick up without choosing a hand |

RB is the attack modifier, and the direction you hold picks the strike:

| RB + | Keyboard | Action |
|------|----------|--------|
| ↑ or ← | Ctrl + Up/Left | Upper cut / left swing |
| → | Ctrl + Right | Right swing |
| ↓ | Ctrl + Down | Attack low — **only reachable this way**, the right stick has no equivalent |

The right stick gives the two common swings without taking a thumb off the
movement stick; RB + direction covers the same two plus the low attack. Use
whichever suits the fight.

### Ecstatica 2 — modifiers instead of hands

E2 has one pick-up action and layers everything else onto modifiers, so the
triggers carry the modifiers rather than a hand.

| Button | Keyboard | Action |
|--------|----------|--------|
| **RB / R1**, **RT / R2** | Left Ctrl | Run while walking; with a direction, attack |
| **LT / L2** | — | Magic / special with a direction; with RB held, an aimed attack |
| A / Cross | Space | Pick up / interact |
| X / Square | Left Alt | Use what is held (empty-handed: flip / roll) |
| **Left stick click (L3)** | I | Toggle the HUD icons — E2 has no speed modes for it to cycle |

| Combination | Keyboard | Action |
|-------------|----------|--------|
| RB + left stick | Ctrl + direction | Attack forward / back / left / right (height chosen by target) |
| LT + left stick | Alt + direction | Magic / special in that direction |
| RB + LT + left stick | Ctrl + Alt + direction | Aimed attack, up or down depending on the target |

Both RB and RT produce the run/attack modifier under E2 — in this game they are
the same key, so either trigger finger works. Under E1 they are **not**
interchangeable: RT is the right hand there, and binding it to the modifier as
well would turn every right-hand pick-up into an attack.

### Behaviour worth knowing

These come from the games' own input handling, and apply to the keyboard just
as much as the pad:

- **E1 jumps from a standstill.** `BH_JOYSTICK` tests the walk direction before
  it tests the jump key, so holding a direction wins and LB does nothing while
  you walk. Let go of the stick, then jump.
- **E1 swings suppress diagonals.** The right stick's two swings share the
  scancodes that E1 also uses for its diagonal walk, so a swing cancels a
  diagonal for as long as it is held. This is why the right stick's vertical
  axis is left unbound — it used to carry the hand pick-up keys, where a nudge
  while turning dropped whatever was held.
- **E1's speed cycle is counted on the pad, not read back from the game.**
  `get_joystick` only writes `movement_speed_mode` when the data has no
  `Key_F1_4` / `Key_F5_8` / `Key_F9_12` script code; E1's data has them, so it
  runs the script and leaves the variable at its startup value. Deriving the
  next step from it sent the same F-key on every click, which is why the cycle
  appeared stuck on run.
- **E2 checks pick-up first.** Holding A blocks every other action while it is
  down, so tap it rather than resting a thumb on it.
- **E2's X and LT are deliberately different keys.** `BH_JOYSTICK` tests
  scancode 56 before it tests the Alt modifier, so any control raising both
  always lands on use-item/roll and can never reach the magic (`196`..`199`) or
  aimed-attack (`204`..`211`) actions behind the later branches. X raises both,
  the way Left Alt does on the keyboard; LT raises only the modifier. **Those
  E2 actions are reachable from the pad but not from the keyboard as currently
  wired** — see the note under the E2 keyboard tables.

### Menus, dialogues and the inventory

| Button | Action |
|--------|--------|
| Left stick / D-pad | Move the selection |
| A / Cross, Y / Triangle | Confirm |
| B / Circle, Start | Cancel / close |

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
| `ECSTATICA_GAMEPAD_DEBUG=1` | Print each detected pad, its resolved button/axis slots, and any `/dev/input/js*` node rejected as not-a-gamepad |
| `ECSTATICA_GAMEPAD_DEBUG=2` | Also trace the decoded pad state and the walk direction it resolves to, every time either changes |
| `ECSTATICA_GAMEPAD_DEVICE=js2` | Read only that node (index, `jsN`, or a full path) instead of every pad found |
| `ECSTATICA_GAMEPAD_SWAP_FACE=0` | Force compass-name reading: X is west (use item), Y is north (inventory) |
| `ECSTATICA_GAMEPAD_SWAP_FACE=1` | Force letter-name reading (the default for unrecognised pads) |

### Several pads at once

Every `/dev/input/js*` node that describes a gamepad is opened, up to four, and
their states are merged — buttons and directions OR'd together, each stick axis
taken from whichever pad is pushing it furthest. A Steam Deck or Steam Machine
carries more than one at all times: the physical pad, plus a virtual
`Microsoft X-Box 360 pad` that Steam Input synthesises for each controller it
sees. Only one of those is the node actually carrying input, and it is not
reliably the lowest-numbered one, so reading the first that looks like a pad
lands on a silent node about as often as the live one.

### Steam Input sends nothing outside Steam

Steam Input routes a controller to its virtual pad only for games Steam
launched. Start the game straight from a terminal or file manager with a Steam
Controller — or a Steam Deck's built-in pad — and the controller stays on
Steam's *desktop* layout: the sticks drive the mouse and the buttons send
keyboard keys, so the character does whatever those keys happen to mean instead
of walking, and the game's virtual pad node sits idle. `ECSTATICA_GAMEPAD_DEBUG=2`
shows this as `conn=1` with values that never move.

Either add the binary to Steam as a non-Steam game and launch it from there, or
switch the controller's desktop layout to a gamepad template. A pad on its own
driver — Xbox, DualSense, a generic USB pad — is unaffected either way.
