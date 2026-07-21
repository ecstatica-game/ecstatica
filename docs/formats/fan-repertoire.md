# FAN Repertoire Data Type

Repertoires map action types to specific actions for an actor. Each actor has a repertoire that defines which action to play for each behavior (idle, walk, run, attack, etc.), providing up to 208 action slots.

Block type tag: `0x06` (tagged-block format). All multi-byte fields are **big-endian**.

---

## Repertoire Header

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `name_index` | Index into the repertoire name table |
| 2 | 2 | `field_1A2` | Thing name index (the thing this repertoire targets) |
| 4 | 2 | `field_1A4` | Repertoire flags |
| 6 | 2 | `num_actions` | Number of action slots that follow |

After the header, `num_actions` action slot entries follow sequentially.

## Action Slot Entry

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 2 | `action_index` | Index into the action name table |

Slots are stored in order starting from slot 0. Only the first 208 entries are retained; any beyond that are read but discarded.

---

## Action Slot Layout

The `field_2` array holds 208 entries (`field_2[0..207]`). Each slot index corresponds to an action type:

| Slot Range | Purpose |
|------------|---------|
| 0-32 | Primary action types (idle, walk, run, attack, etc.) |
| 33-35 | Movement variation seeds (E1 only; see below) |
| 50-76 | Directional movement variations (9 directions x 3 types) |

### E1 Movement Variation Expansion

In Ecstatica 1, action types 33-35 represent movement variations. After loading, the reader replicates each across 9 directional slots:

```
For each variation v in {0, 1, 2}:     (action types 33, 34, 35)
    source = field_2[33 + v]
    For each direction d in {0..8}:
        field_2[50 + v + d * 3] = source
```

This fills slots 50-76 with directional copies, giving 9 directions for each of the 3 movement types.

---

## Reading Paths

There are two distinct code paths for reading repertoires.

### Tagged-Block Reader

**Min version:** 12 (name table), 13 (data section).

Reads repertoires from block type `0x06`. Parses the header, then reads `num_actions` slot entries sequentially. Each 2-byte value is stored into the corresponding `field_2` index. After loading, the E1 movement variation expansion runs if applicable.

### Event-Stream Reader (`read_repertoires`)

Reads a flat stream of events terminated by a sentinel (`event_type == 0`). Two event types control the data:

| Event Type | Purpose |
|------------|---------|
| `PSEUDO_REP` | Starts a new repertoire; sets header fields |
| `REP_ENTRY` | Assigns an action to a specific slot |

**PSEUDO_REP** sets:
- `rep_index` from `event_index`
- `field_1A2` from `param1 - 1` (thing index; 1-based in file, converted to 0-based)
- `field_1A4` from `param2 - 1` (flags; 1-based in file, converted to 0-based)

**REP_ENTRY** writes:
- `field_2[param1] = param2` (slot index = action index)

The stream ends when `event_type == 0`.

---

## Notes

- Repertoires are per-actor: each actor references a repertoire that determines its available behaviors.
- The action index stored in each slot refers to the action name table, linking to the corresponding action data.
- In the event-stream format, thing index and flags use 1-based encoding; the reader subtracts 1 to produce 0-based values.
