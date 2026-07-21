# FAN Action Data Type

Actions define keyframed animation sequences. Each action targets a thing and contains an ordered list of keyframes; each keyframe carries events that drive part transforms, code execution, sound playback, and interaction logic.

Block type tag: `0x02` (tagged-block format). All multi-byte fields are **big-endian**.

---

## Action Header

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `name_index` | Index into the action name table |
| 2 | 2 | `field_E` | Thing name index (the thing this action targets) |
| 4 | 2 | `act_duration` | Total duration of the action in frames |
| 6 | 2 | `field_C_flags` | Action flags |
| 8 | 2 | `num_keys` | Number of keyframes that follow |

After the header, `num_keys` keyframe records follow sequentially.

## Keyframe

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `KEY_position` | Frame position of this keyframe |
| 2 | 1 | `field_E` | Keyframe flags/type byte |
| 3 | 2 | `num_events` | Number of events attached to this keyframe |

After the keyframe header, `num_events` event records follow.

## Event

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | `event_type` | Event type code (see table below) |
| 1 | 1 | `event_index` | Sub-index or part index |
| 2 | 2 | `param1` | Parameter 1 (meaning varies by event type) |
| 4 | 2 | `param2` | Parameter 2 (meaning varies by event type) |
| 6 | 2 | `param3` | Parameter 3 (meaning varies by event type) |

Total event size: 7 bytes.

---

## Reading Paths

There are two distinct code paths for reading actions.

### Tagged-Block Reader

Reads actions from block type `0x02`. Parses the action header, then iterates over keyframes, each of which iterates over its events. This is a straightforward sequential read matching the layout above.

### Event-Stream Reader (`read_actions`)

Reads a flat stream of events terminated by a sentinel (`event_type == 0`). Three pseudo-event types control structure:

| Pseudo-Event | Purpose |
|--------------|---------|
| `PSEUDO_ACTION` | Starts a new action; the event fields define the action header |
| `PSEUDO_ACTION_2` | Extends the current action with `field_10` |
| `PSEUDO_KEY` | Inserts a new keyframe at the current position |

All other event types are appended to the current keyframe. The stream ends when `event_type == 0`.

---

## Event Types

### Part Transform Events

Events that modify per-part visual properties. `event_index` typically selects the target part.

| Category | Description |
|----------|-------------|
| Position | Part X/Y/Z translation |
| Rotation | Part X/Y/Z rotation angles |
| Squash | Part squash/stretch deformation |
| Color | Part color modification |

### Control Events

| Event | Description |
|-------|-------------|
| `THING_CODE` | Assign a code routine to the thing |
| `THING_FLAGS` | Set thing behavior flags |
| `ACTOR_REP` | Set actor representation/model |

### INTERACT Event (type 46)

The INTERACT event uses `event_index` as a sub-type selector:

| Sub-type | Name | Description | Remapping |
|----------|------|-------------|-----------|
| 0 | CheckPartHit | Test if a specific part was hit | None |
| 1 | CheckPickUp | Check if object can be picked up | Code remap on `param2` (version >= 15) |
| 2 | DropObject | Drop the currently held object | None |
| 3 | AttachToHeld | Attach something to the held object | None |
| 4 | ExecutePartCode | Execute code on a part (part optional) | Code remap on `param2` (version >= 15) |
| 5 | PlaySound | Play a sound effect | Sound remap on `param2` (version >= 16) |
| 6 | FireBullet | Fire a projectile (E2 only) | None |
| 7 | BloodSpurt | Spawn blood effect (E2 only) | None |
| 8 | SpawnActor | Spawn an actor instance (version >= 43) | Thing + action remap |
| 9 | SpawnActor2 | Spawn an actor (alternate) (version >= 43) | Thing + action remap |

---

## Index Remapping

When loading actions from older file versions, certain indices are remapped to match the current name tables.

### Code Indices

Used by CheckPickUp (sub-type 1) and ExecutePartCode (sub-type 4) when version >= 15. Indices are 1-based; 0 means "none."

    remapped = new_code_name[param2 - 1] + 1

### Sound Indices

Used by PlaySound (sub-type 5) when version >= 16. Indices are 1-based; 0 means "none."

    remapped = new_sound_name[param2 - 1] + 1

### Thing and Action Indices

Used by SpawnActor (sub-type 8) and SpawnActor2 (sub-type 9) when version >= 43.

    thing_remapped  = new_thing_name[original_thing]
    action_remapped = new_action_name[original_action]
