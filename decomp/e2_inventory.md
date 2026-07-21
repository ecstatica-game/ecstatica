# E2WIN95.EXE Decompilation Inventory

Function-level status of the Ecstatica II Win95 decompilation.
Base binary: E2WIN95.EXE (unpatched).

> **Note:** Functions may be implemented in a different .c file than their
> original E2 module. This inventory searches across all src/ files.

## Summary

| Module | Total | Done | Missing | % | Notes |
|--------|-------|------|---------|---|-------|
| anim.c       |  15   |  14  |   1     |  93% |  |
| display.c    |  67   |  66  |   1     |  98% |  |
| edit.c       |  34   |  34  |   0     | 100% |  |
| ellipse.c    |  13   |  10  |   3     |  76% |  |
| file.c       |  84   |  76  |   8     |  90% |  |
| game.c       | 111   |  97  |  14     |  87% |  |
| icon.c       |   2   |   2  |   0     | 100% |  |
| init.c       | 119   | 116  |   3     |  97% |  |
| map.c        |  22   |  19  |   3     |  86% |  |
| menu.c       |  24   |  20  |   4     |  83% |  |
| move.c       |  59   |  57  |   2     |  96% |  |
| music.c      |  59   |   4  |  55     |   6% | SKIP (platform) |
| pack.c       |   1   |   0  |   1     |   0% | SKIP (platform) |
| req.c        |  73   |  54  |  19     |  73% |  |
| topo.c       |  31   |  26  |   5     |  83% |  |
| tri.c        |   4   |   4  |   0     | 100% |  |
| win.c        |  10   |   4  |   6     |  40% | SKIP (platform) |
| **TOTAL** | **728** | **603** | **125** | **82%** | |

## Modules to Skip

Platform-specific (Win32/DirectSound) code reimplemented via macOS/SDL platform layer:

- **music.c** -- DirectSound/WaveOut audio driver (59 functions)
- **win.c** -- Win32 window management, DirectDraw (10 functions)
- **pack.c** -- Screen unpacking (1 function)
- **asm.ASM** -- x86 assembly routines (reimplemented in asm_f.c)

## Per-Module Details

### anim.c -- 14/15 (93%)

| Function | E2 Address |
|----------|-----------|
| SaveActionDirectory | 0x430628 |

### display.c -- 66/67 (98%)

| Function | E2 Address |
|----------|-----------|
| C_MatrixVector | 0x4227E8 |

### edit.c -- 34/34 (100%)

All functions implemented.

### ellipse.c -- 10/13 (76%)

| Function | E2 Address |
|----------|-----------|
| ShadeEllipseWIN95 | 0x430F58 |
| OldShadeEllipse | 0x4316F8 |
| FindLongEllipse | 0x43360C |

### file.c -- 76/84 (90%)

| Function | E2 Address |
|----------|-----------|
| WriteThing | 0x440CAC |
| ModifyMovement | 0x441444 |
| FindNamedCode | 0x4416B0 |
| DeleteThingName | 0x4416FC |
| DeleteSceneName | 0x441EDC |
| SaveGameThing | 0x443770 |
| SaveGameParts | 0x443D3C |
| PrintSubtitles | 0x447AD4 |

### game.c -- 97/111 (87%)

| Function | E2 Address |
|----------|-----------|
| CheckIFStructure | 0x44E49C |
| SkipToMatchingENDIF | 0x44E5F0 |
| SkipToMatchingIFType | 0x44E648 |
| ExecuteSubObjCode | 0x44F1C4 |
| GetNextChar | 0x450DBC |
| GetNextToken | 0x450FD4 |
| CheckSceneOKToStart | 0x452288 |
| CheckTextureLoaded | 0x452488 |
| FindMemoryForSound | 0x4527F0 |
| FindMemoryForTexture | 0x4528F0 |
| SaveMatrix | 0x454E70 |
| LoadMatrix | 0x454EC0 |
| PutNumber | 0x456700 |
| PlayAmbiants | 0x45760C |

### icon.c -- 2/2 (100%)

All functions implemented.

### init.c -- 116/119 (97%)

| Function | E2 Address |
|----------|-----------|
| TextWithMaskWIN95 | 0x419850 |
| DrawDBMouseCursor | 0x41C4EC |
| ClearDBMouseCursor | 0x41CA64 |

### map.c -- 19/22 (86%)

| Function | E2 Address |
|----------|-----------|
| GoToLocation | 0x44CE44 |
| RepositionThing | 0x44CFE4 |
| LoadPackedViews | 0x44D900 |

### menu.c -- 20/24 (83%)

| Function | E2 Address |
|----------|-----------|
| CheckMenu | 0x439580 |
| DrawSubItemsAndSaveArea | 0x439DB8 |
| ClearSubItems | 0x439F70 |
| ShowMenuBar | 0x43A0DC |

### move.c -- 57/59 (96%)

| Function | E2 Address |
|----------|-----------|
| oFindDirectionAndDistance | 0x4271E4 |
| Find2PartRotZ | 0x42D8BC |

### music.c -- 4/59 (6%) (SKIP)

55 platform-specific functions -- not decompiling.

### pack.c -- 0/1 (0%) (SKIP)

1 platform-specific functions -- not decompiling.

### req.c -- 54/73 (73%)

Missing (mostly install/file-browser UI, not needed for gameplay):

| Function | E2 Address | Notes |
|----------|-----------|-------|
| SetupRequesterImages | 0x43C120 | |
| HandleOK2 | 0x43C5EC | |
| WriteE_CONFIG | 0x43C60C | |
| handle_dir_prop | 0x43C6E0 | |
| HandleMyDirGadg | 0x43C744 | |
| HandlePathGadg | 0x43C838 | |
| NewDirectory | 0x43C87C | |
| DoInput2Req | 0x43CD0C | |
| CopyAFile | 0x43EB18 | install |
| CopyADirectory | 0x43EEBC | install |
| RemoveADirectory | 0x43F280 | install |
| ArchiveADirectory | 0x43F5B4 | install |
| HandleSoundCardGadg | 0x43F89C | |
| HandleInstallGadg | 0x43F97C | install |
| HandleSavedGadg | 0x43FA5C | |
| HandleInputGadg | 0x43FAD8 | |
| HandleDirUpDown | 0x43FBAC | |
| DrawCopyingBar | 0x43FC70 | install |
| ClearCopyingBar | 0x43FD44 | install |

### topo.c -- 26/31 (83%)

| Function | E2 Address |
|----------|-----------|
| FindNormal | 0x4483F4 |
| DoFindHeightNow | 0x448998 |
| DrawClippedLine | 0x4497DC |
| LoadLongScreen | 0x449F68 |
| SaveRawForPrinting | 0x44B49C |

### tri.c -- 4/4 (100%)

All functions implemented.

### win.c -- 4/10 (40%) (SKIP)

6 platform-specific functions -- not decompiling.
