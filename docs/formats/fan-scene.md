# FAN Scene Data Type

Scenes group action references into gameplay scenarios such as cutscenes, area encounters, or interaction setups. A scene holds up to 18 action indices that define which animations play when the scene is triggered.

## Tagged-Block Format (Block Type 0x03)

All fields are big-endian signed 16-bit integers.

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | name_index | Index into the name table identifying this scene |
| 0x02 | 2 | scene_use_flag | Usage flag controlling scene behavior |
| 0x04 | 2 | field_C | Unknown purpose |
| 0x06 | 2 | num_actions | Number of action references that follow (max stored: 18) |
| 0x08 | 2 * num_actions | action_indices | Array of action indices; only the first 18 are retained |

Total size: 8 + (2 * num_actions) bytes.

If `num_actions` exceeds 18, the extra entries are read from the stream but discarded.

## Event-Stream Format (PSEUDO_SCENE)

In the event stream (used by the merge/seek reader), scenes appear after actions and actors. A `PSEUDO_SCENE` event encodes the scene header, followed by action-reference events.

### PSEUDO_SCENE event fields

| Field | Content |
|-------|---------|
| event_index | Scene name index |
| param1 | scene_use_flag |
| param2 | field_C |

Action references follow as subsequent events in the stream. The reader collects them into the scene's action array (up to 18 entries).

## Runtime

Scenes are stored in `scene_tab[]`. When a scene is started, the referenced actions are loaded and played back in sequence.
