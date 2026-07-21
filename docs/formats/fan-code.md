# FAN Code Block Format

Code blocks are the game's scripting system: tokenized bytecode with embedded text for dialogue and descriptions. Each block has a name index, a token stream (instructions and entity references), and optional text lines.

- Block type tag: `0x04` (tagged-block format)
- Minimum version: 6
- Byte order: big-endian throughout

---

## Tagged Block Layout (file_read_code)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0 | 2 | int16 BE | Name index into global code name table |
| 2 | 2 | int16 BE | Token count (number of 16-bit words) |
| 4 | N*2 | int16 BE[] | Token store (N = token count) |

---

## Event Stream Layout (read_code)

The event stream contains multiple code blocks, each with tokens and text.

### Header

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0 | 2 | int16 BE | Code block count |

### Per-Block Structure

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0 | 2 | int16 BE | File-local code index (remapped via `new_code_name[]`) |
| 2 | var | int16 BE[] | Token stream, terminated by `0x0000` |
| var | 2 | int16 BE | Text line count |
| var | var | char[][] | Null-terminated strings (max 53 chars each) |

Tokens are read as big-endian 16-bit words until a zero terminator is encountered. Each non-zero token is passed through name remapping before storage.

---

## Token Format

Each token is a 16-bit value. The upper nibble (bits 15-12) determines the token type. The lower 12 bits encode an index value that is remapped through the corresponding name table during load.

| Upper Nibble | Value | Type | Lower 12 Bits |
|--------------|-------|------|----------------|
| `0x1` | `0x1000` | Part reference | Part index |
| `0x2` | `0x2000` | Thing reference | Thing index |
| `0x3` | `0x3000` | Action reference | Action index |
| `0x4` | `0x4000` | Scene reference | Scene index |
| `0x5` | `0x5000` | Point reference | Point index |
| `0x6` | `0x6000` | Triangle reference | Triangle index |
| `0x7` | `0x7000` | Code reference | Code index |
| `0x8` | `0x8000` | Repertoire reference | Repertoire index |
| `0x9` | `0x9000` | Map area reference | Map area index |
| `0xA` | `0xA000` | Sound reference | Sound index |
| `0xE` | `0xE000` | Extended array | Element count (see below) |
| `0x0` | `0x0000` | Terminator / literal | Opcode or literal value |
| other | -- | Literal / opcode | Opcode or literal value |

### Extended Array Token (`0xE000`)

When the upper nibble is `0xE`, the token encodes an inline data array:

- Lower 12 bits = `(element_count - 1)`
- Element count = `(lower_12_bits & 0x0FFF) + 1`
- Array size in words = `ceil(element_count / 2)`
- The next `array_size` words (big-endian int16) immediately follow the token

### Terminator

A token value of `0x0000` terminates the token stream for the current code block.

---

## Name Remapping

All entity-reference tokens are remapped during load. The file stores file-local indices; the reader translates them to runtime indices through per-type mapping arrays (`new_part_name[]`, `new_thing_name[]`, `new_code_name[]`, etc.). This allows multiple FAN files to be merged into a single runtime namespace without index collisions.

---

## Text Lines

After the token stream terminator, each code block stores a count of text lines followed by that many null-terminated strings. Each string is at most 53 characters (including the null terminator). These lines hold dialogue, descriptions, and other display text associated with the code block.
