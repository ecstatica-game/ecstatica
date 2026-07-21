# E1WIN95.EXE Decompilation Inventory

Function-level status of the Ecstatica I Win95 decompilation.
Base binary: E1WIN95.EXE.

> **Note:** Functions may be implemented in a different .c file than their
> original E1 module. This inventory searches across all src/ files.

## Summary

| Module | Total | Done | Missing | % | Notes |
|--------|-------|------|---------|---|-------|
| anim.c       |  16   |  15  |   1     |  93% |  |
| display.c    |  67   |  65  |   2     |  97% |  |
| edit.c       |  34   |  34  |   0     | 100% |  |
| ellipse.c    |  13   |   8  |   5     |  61% |  |
| file.c       |  84   |  75  |   9     |  89% |  |
| game.c       |  97   |  86  |  11     |  88% |  |
| icon.c       |   2   |   2  |   0     | 100% |  |
| init.c       | 115   | 100  |  15     |  86% |  |
| map.c        |  22   |  18  |   4     |  81% |  |
| menu.c       |  24   |  18  |   6     |  75% |  |
| move.c       |  53   |  51  |   2     |  96% |  |
| music.c      |  59   |   4  |  55     |   6% | SKIP (platform) |
| pack.c       |   1   |   0  |   1     |   0% | SKIP (platform) |
| req.c        |  73   |  50  |  23     |  68% |  |
| topo.c       |  28   |  22  |   6     |  78% |  |
| tri.c        |   2   |   2  |   0     | 100% |  |
| win.c        |  10   |   4  |   6     |  40% | SKIP (platform) |
| **TOTAL** | **700** | **554** | **146** | **79%** | |

## Modules to Skip

Platform-specific (Win32/DirectSound) code reimplemented via macOS/SDL platform layer:

- **music.c** -- DirectSound/WaveOut audio driver (59 functions)
- **win.c** -- Win32 window management, DirectDraw (10 functions)
- **pack.c** -- Screen unpacking (1 function)
- **asm.ASM** -- x86 assembly routines (reimplemented in asm_f.c)

## Per-Module Details

### anim.c -- 15/16 (93%)

| Function | E1 Address |
|----------|-----------|
| SaveActionDirectory | 0x428788 |

### display.c -- 65/67 (97%)

| Function | E1 Address |
|----------|-----------|
| C_MatrixVector | 0x41EC08 |
| C_MatrixMult | 0x422090 |

### edit.c -- 34/34 (100%)

All functions implemented.

### ellipse.c -- 8/13 (61%)

| Function | E1 Address |
|----------|-----------|
| ShadeEllipseWIN95 | 0x42A08C |
| ShadeEllipseSVGA | 0x42A6DC |
| OldShadeEllipse | 0x42A6E0 |
| ArctanSLOW | 0x42BA9C |
| FindLongEllipse | 0x42C51C |

### file.c -- 75/84 (89%)

| Function | E1 Address |
|----------|-----------|
| WriteThing | 0x436A2C |
| ModifyMovement | 0x4371C4 |
| FindNamedCode | 0x437430 |
| DeleteThingName | 0x43747C |
| DeleteSceneName | 0x437C58 |
| SaveGameThing | 0x4394D4 |
| SaveGameParts | 0x439A94 |
| PrintSubtitles | 0x43D614 |
| MakeFileNameSubDir | 0x43DACC |

### game.c -- 86/97 (88%)

| Function | E1 Address |
|----------|-----------|
| CheckIFStructure | 0x443038 |
| SkipToMatchingENDIF | 0x44318C |
| SkipToMatchingIFType | 0x4431E4 |
| ExecuteSubObjCode | 0x443D4C |
| GetNextChar | 0x4457E4 |
| GetNextToken | 0x4459FC |
| CheckTextureLoaded | 0x446EA8 |
| FindMemoryForSound | 0x447210 |
| FindMemoryForTexture | 0x447310 |
| SaveMatrix | 0x4492C4 |
| LoadMatrix | 0x449314 |

### icon.c -- 2/2 (100%)

All functions implemented.

### init.c -- 100/115 (86%)

| Function | E1 Address |
|----------|-----------|
| WriteToDSP | 0x411014 |
| ReadFromDSP | 0x411034 |
| ClipBlitSVGA | 0x414798 |
| ClearBackgroundSVGA | 0x414FE8 |
| TextSVGA | 0x415818 |
| TextWithMaskWIN95 | 0x4162D8 |
| RectFillSVGA | 0x4178B4 |
| DrawMouseCursorSVGA | 0x418DFC |
| DrawDBMouseCursor | 0x418F74 |
| DrawDBMouseCursorSVGA | 0x419100 |
| ClearMouseCursorSVGA | 0x419418 |
| ClearDBMouseCursor | 0x4194E8 |
| ClearDBMouseCursorSVGA | 0x41964C |
| ExpandPallette | 0x4197A8 |
| SetGreyPallette | 0x4198DC |

### map.c -- 18/22 (81%)

| Function | E1 Address |
|----------|-----------|
| CopyVGAtoSVGA | 0x44177C |
| GoToLocation | 0x442184 |
| RepositionThing | 0x4422FC |
| LoadPackedViews | 0x442BE8 |

### menu.c -- 18/24 (75%)

| Function | E1 Address |
|----------|-----------|
| CheckMenu | 0x42F430 |
| DrawSubItemsAndSaveArea | 0x42FC68 |
| ClearSubItems | 0x42FE20 |
| ShowMenuBar | 0x42FF8C |
| GoSVGA | 0x430908 |
| GoVGA | 0x430994 |

### move.c -- 51/53 (96%)

| Function | E1 Address |
|----------|-----------|
| oFindDirectionAndDistance | 0x4234F4 |
| Find2PartRotZ | 0x426E40 |

### music.c -- 4/59 (6%) (SKIP)

55 platform-specific functions -- not decompiling.

### pack.c -- 0/1 (0%) (SKIP)

1 platform-specific functions -- not decompiling.

### req.c -- 50/73 (68%)

Missing (mostly install/file-browser UI, not needed for gameplay):

| Function | E1 Address | Notes |
|----------|-----------|-------|
| SetupRequesterImages | 0x431E50 | |
| HandleOK | 0x432244 | |
| HandleOK2 | 0x43231C | |
| WriteE_CONFIG | 0x43233C | |
| handle_dir_prop | 0x432410 | |
| HandleMyDirGadg | 0x432474 | |
| HandlePathGadg | 0x432568 | |
| NewDirectory | 0x4325AC | |
| DoInput2Req | 0x432A3C | |
| HandleSoundFX | 0x433464 | |
| DoSoundFXGadgText | 0x433664 | |
| DoLODGadgText | 0x4336C8 | |
| CopyAFile | 0x4348C0 | install |
| CopyADirectory | 0x434C64 | install |
| RemoveADirectory | 0x435028 | install |
| ArchiveADirectory | 0x43535C | install |
| HandleSoundCardGadg | 0x435644 | |
| HandleInstallGadg | 0x435724 | install |
| HandleSavedGadg | 0x435804 | |
| HandleInputGadg | 0x435880 | |
| HandleDirUpDown | 0x435954 | |
| DrawCopyingBar | 0x435A18 | install |
| ClearCopyingBar | 0x435AEC | install |

### topo.c -- 22/28 (78%)

| Function | E1 Address |
|----------|-----------|
| SwapXY | 0x43DC14 |
| FindNormal | 0x43E0C4 |
| DoFindHeightNow | 0x43E650 |
| DrawClippedLine | 0x43F178 |
| LoadLongScreen | 0x43F8C4 |
| SaveRawForPrinting | 0x4408D0 |

### tri.c -- 2/2 (100%)

All functions implemented.

### win.c -- 4/10 (40%) (SKIP)

6 platform-specific functions -- not decompiling.
