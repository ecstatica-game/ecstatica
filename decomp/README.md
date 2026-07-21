# Decomp - Symbol Extraction Pipeline

## Overview

This pipeline extracts debug symbol names from Watcom-compiled executables and
generates IDA Pro IDC scripts to apply those names to a disassembly database.

```
EXE (with Watcom debug info)
  |
  v
wdump (Open Watcom tool)  -->  dump/<EXE>.txt   (raw debug dump)
  |
  v
parse.py                  -->  dump/<EXE>.json   (structured symbols)
                               dump/<EXE>.modules.json
                               dump/<EXE>.idc    (IDA rename script)
```

## Step 1: Generate Dump Files with wdump

`wdump` is the Open Watcom Executable Image Dump utility. It reads the Watcom
debug information embedded in the original game executables (compiled with
Watcom C) and dumps all headers, sections, and debug symbol tables to text.

Run from inside the `dump/` directory:

```bash
wdump -a <EXE_FILE> > <EXE_NAME>.txt
```

The `-a` flag dumps all sections. The key debug sections parsed later are:

- **Module Info (section 0)** - Lists source files (e.g. `C:\e\init.c`,
  `C:\e\display.c`). Each module gets an index used to map symbols back to
  their source file.
- **Global Info (section 0)** - Lists every global symbol with its name,
  segment:offset address, module index, and kind (code or data).
- **Addr Info (section 0)** - Address-to-line mappings (not currently parsed).

### Executables Dumped

| File             | Type   | Notes                             | Symnbols |
|------------------|--------|-----------------------------------|----------|
| ECSTATICA.EXE    | PE/Win | Ecstatica 1 Win95                 | Yes      |
| ECST4MEG.EXE     | LE/DOS | Ecstatica 1 DOS (4MB)             | No       |
| ECST8MEG.EXE     | LE/DOS | Ecstatica 1 DOS (8MB)             | No       |
| E2WIN95.EXE      | PE/Win | Ecstatica 2 Win95                 | Yes      |
| E2DOS.EXE        | LE/DOS | Ecstatica 2 DOS                   | No       |

## Step 2: Parse Dumps with parse.py

`parse.py` reads the wdump text output and extracts structured symbol data.

### Usage

```bash
cd dump/
python ../parse.py                    # process all configured executables
python ../parse.py E2WIN95P.EXE       # process a single executable
```

### What It Does

1. **Parses Module Info** - Extracts the module table (index -> source file
   path) from the `Module Info (section 0)` block.

2. **Parses Global Info** - For each symbol in `Global Info (section 0)`,
   extracts: name, segment:offset address, module index, and kind.

3. **Computes IDA addresses** - Each symbol's segment:offset is translated to
   a flat IDA address using per-executable base address configs:

   | Executable    | Seg 0001 (code) | Seg 0002 (data) |
   |---------------|-----------------|-----------------|
   | ECSTATICA.EXE | 0x410000        | 0x460000        |
   | E2WIN95.EXE   | 0x410000        | 0x470000        |

4. **Generates IDA names** - Symbol names are converted to snake_case and
   prefixed with the source module filename (for code symbols). A hex address
   suffix is appended for uniqueness. Example:
   `_InitMouse` in `init.c` at 0x410100 -> `init_init_mouse_410100`

5. **Filters symbols** - Skips Watcom `TRANSFER CODE` thunks. For E2WIN95P,
   skips modules with index > 18 (external library symbols).

### Outputs

For each executable `<EXE>`:

- **`<EXE>.json`** - All parsed symbols with computed IDA addresses.
- **`<EXE>.modules.json`** - Module index-to-source-file mapping.
- **`<EXE>.idc`** - IDA IDC script with `MakeName()` calls (and
  `MakeUnknown()` for data segment symbols to handle tail-byte conflicts).

## Step 3: Apply IDC in IDA Pro

In IDA Pro with the target executable loaded:

1. File -> Script file... -> select `dump/<EXE>.idc`
2. The script renames all matched addresses with their debug symbol names.

The `MakeUnknown()` calls on data segment (0002) addresses clear any existing
IDA type definitions that would block renaming (tail-byte issues). Code segment
(0001) addresses are never undefined to preserve function definitions.

## Directory Layout

```
decomp/
  parse.py              - Symbol extraction script
  dump/                 - wdump outputs and generated files
    *.EXE.txt           - Raw wdump output
    *.EXE.json          - Parsed symbols (JSON)
    *.EXE.modules.json  - Module table (JSON)
    *.EXE.idc           - IDA rename script
  ida/                  - IDA database files
```
