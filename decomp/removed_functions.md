# Removed functions

Functions and globals from the original executables that the port does not
have, on purpose. When an IDA comparison or the inventory scripts show one of
these as "missing", that is expected: do not port it again.

Every symbol in the 0.9.4 tables was unreachable in the port. A
dead-stripping link of the desktop build (`-Wl,-dead_strip`) dropped it, and
no platform backend (Linux, Windows, DOS, Win9x, PSP, Vita, Pocket) uses it.
Unreachable code that belongs to gameplay (combat, sound, script, camera,
requesters, end sequence) is kept, because it may be wired up later.

## Categories

| Category | Meaning |
|---|---|
| editor | Part of the in-house level/animation editor. The shipped game never runs it. |
| debug | Debug printing, frame-rate display or screenshots. |
| installer | Setup, sound-card selection or install-directory code. The platform layer replaces it. |
| hardware | DOS, VGA mode X, SVGA bank switching, DirectSound or DirectDraw. The platform layer replaces it. |
| uncalled | Has no callers in the original binary either. |

Addresses are from the Watcom symbols in `dump/E1WIN95.EXE.json` and
`dump/E2WIN95.EXE.json`. "port-only" means the symbol was added by the port
and has no Watcom counterpart.

## Removed in 0.9.4

### anim

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `check_action_name_exists` | code | `CheckActionNameExists_` | 0x4280C0 | 0x42FF60 | editor |
| `clear_all_choices` | code | `ClearAllChoices_` | 0x4281F8 | 0x430098 | editor |
| `clear_choice_box_internal` | code | `ClearChoiceBox_` | 0x42825C | 0x4300FC | editor |
| `draw_all_choices` | code | `DrawAllChoices_` | 0x428120 | 0x42FFC0 | editor |
| `draw_choice_box_internal` | code | `DrawChoiceBox_` | 0x428290 | 0x430130 | editor |
| `recalc_cam` | code | `RecalcCam_` | 0x427F40 | 0x42FDE0 | editor |

### asm_f

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `add_timer_int_handler` | code | `AddTimerIntHandler_` | 0x44E9EE | 0x45C99E | hardware |
| `clear_latches` | code | `ClearLatches_` | 0x44ED53 | 0x45CD03 | hardware |
| `dos_mem_alloc` | code | `DOSMEMALLOC_` | 0x45012E | 0x45E0DE | hardware |
| `dos_mem_free` | code | `DOSMEMFREE_` | 0x45016B | 0x45E11B | hardware |
| `remove_timer_handler` | code | `RemoveTimerHandler_` | 0x44EBC5 | 0x45CB75 | hardware |
| `set_320x240_mode` | code | `SET320X240MODE_` | 0x44EC01 | 0x45CBB1 | hardware |
| `set_latches` | code | `SetLatches_` | 0x44ED40 | 0x45CCF0 | hardware |
| `set_plane_mask` | code | `SetPlaneMask_` | 0x44ED24 | 0x45CCD4 | hardware |
| `set_read_plane` | code | `SetReadPlane_` | 0x44ED32 | 0x45CCE2 | hardware |
| `set_svga_bank` | code | `SetSVGABank_` | 0x44EDDA | 0x45CD8A | hardware |
| `show_page` | code | `ShowPage_` | 0x44ED5E | 0x45CD0E | hardware |
| `write_pixel_mode_x` | code | `WritePixelModeX_` | 0x44ECC4 | 0x45CC74 | hardware |
| `xor_pixel_mode_x` | code | `XORPixelModeX_` | 0x44ECEF | 0x45CC9F | hardware |

### display

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `fps` | data | port-only | — | — | debug |
| `max_fps` | data | port-only | — | — | debug |
| `print_matrix` | code | `PrintMatrix_` | 0x41F9F4 | 0x4235D4 | debug |
| `print_vector` | code | `PrintVector_` | 0x41FA40 | 0x423620 | debug |
| `add_a_triangle` | code | `AddATriangle_` | 0x41E3E0 | 0x421F20 | editor |
| `add_actor_polys` | code | `AddActorPolys_` | 0x41E3CC | 0x421F0C | editor |
| `add_polygons` | code | `AddPolygons_` | 0x41E3A0 | 0x421EE0 | editor |
| `making_background` | data | `_MakingBackground` | 0x469B74 | 0x47A6FC | editor |
| `put_parts_select` | code | `PutPartsSelect_` | 0x4210AC | 0x424C8C | editor |
| `xxx_stick_to_background` | code | `xxxStickToBackground_` | 0x421FFC | 0x425C4C | editor |

### edit

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `display_message` | code | `DisplayMessage_` | 0x423308 | 0x426FDC | debug |
| `message_vector_decimal` | code | `MessageVectorDecimal_` | 0x423298 | 0x426F6C | debug |
| `message_vector_hex` | code | `MessageVectorHex_` | 0x4232CC | 0x426FA0 | debug |
| `show_vector` | code | `ShowVector_` | 0x423230 | 0x426F04 | debug |
| `init_mask_distances` | code | `InitMaskDistances_` | 0x4231B4 | 0x426E88 | editor |
| `mask_distance` | data | `_MaskDistance` | 0x4F13F0 | 0x6372F0 | editor |
| `set_mask_distance` | code | `SetMaskDistance_` | 0x4231D4 | 0x426EA8 | editor |
| `write_an_event` | code | `WriteAnEvent_` | 0x436FF8 | 0x441278 | editor |
| `write_parts` | code | `WriteParts_` | 0x436BF4 | 0x440E74 | editor |

### ellipse

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `ellipse_half_size` | code | `EllipseHalfSize_` | 0x42A6DC | 0x4316F4 | hardware |

### file

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `print_action` | code | `PrintAction_` | 0x438F70 | 0x4431EC | debug |
| `print_event` | code | `PrintEvent_` | 0x4389A8 | 0x442C24 | debug |
| `print_scene` | code | `PrintScene_` | 0x438FE8 | 0x443264 | debug |
| `rep1_used` | code | `Rep1Used_` | 0x436A14 | 0x440C94 | debug |
| `action_name_flags` | data | `_ActionNameFlags` | 0x503130 | 0x674B80 | editor |
| `clear_new_names_and_flags` | code | `ClearNewNamesAndFlags_` | 0x43911C | 0x4433A4 | editor |
| `code_name_flags` | data | `_CodeNameFlags` | 0x5018F8 | 0x66F1A8 | editor |
| `delete_action_name` | code | `DeleteActionName_` | 0x4378D4 | 0x441B54 | editor |
| `delete_code_name` | code | `DeleteCodeName_` | 0x437E90 | 0x442114 | editor |
| `delete_repertoire_name` | code | `DeleteRepertoireName_` | 0x438318 | 0x442594 | editor |
| `merge_file_contents` | code | port-only | — | — | editor |
| `rep_name_flags` | data | `_RepNameFlags` | 0x505670 | 0x65D610 | editor |
| `sound_name_flags` | data | `_SoundNameFlags` | 0x4FF5D0 | 0x65EE48 | editor |
| `texture_name_flags` | data | `_TextureNameFlags` | 0x505B2C | 0x65ECB8 | editor |
| `write_a_merged_ev` | code | `WriteAMergedEv_` | 0x437030 | 0x4412B0 | editor |
| `write_merged_event` | code | `WriteMergedEvent_` | 0x4370B8 | 0x441338 | editor |
| `make_dir_if_not_exists` | code | `MakeDirIfNotExists_` | 0x43DB94 | 0x447EC4 | installer |
| `make_file_dir_name` | code | `MakeFileDirName_` | 0x43DB2C | 0x447E5C | installer |
| `make_file_name_subdir` | code | `MakeFileNameSubDir_` | 0x43DACC | 0x447DFC | installer |
| `wave_load_file` | code | `WaveLoadFile_` | 0x44E3C4 | 0x45C374 | uncalled |
| `wave_open_file` | code | `WaveOpenFile_` | 0x44DD00 | 0x45BCB0 | uncalled |

### game

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `develop_mode` | data | port-only | — | — | debug |
| `save_size_of_heaps` | code | `SaveSizeOfHeaps_` | 0x449CCC | 0x457C68 | debug |
| `show_rate` | data | `_ShowRate` | 0x469770 | 0x47A2F8 | debug |
| `selected_action` | data | `_selected_action` | 0x46988C | 0x47A414 | editor |
| `source_thing` | data | `_source_thing` | 0x469888 | 0x47A410 | editor |
| `tokenize_code` | code | `TokenizeCode_` | 0x445530 | 0x450B08 | editor |
| `allocate_ds_buffer` | code | `AllocateDSBuffer_` | 0x44AB48 | 0x458AF8 | hardware |
| `init_sound_driver_win95` | code | `InitSoundDriverWIN95_` | 0x44B60C | 0x4595BC | hardware |

### init

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `find_recorded_len` | code | `FindRecordedLen_` | 0x411418 | 0x414244 | debug |
| `start_playing_sound` | code | `StartPlayingSound_` | 0x41108C | 0x413EB8 | debug |
| `start_recording` | code | `StartRecording_` | 0x41120C | 0x414038 | debug |
| `expand_colour_map_a_bit` | code | `ExpandColourMapABit_` | 0x4197AC | 0x41CD28 | editor |
| `if_editor_show_cursor` | code | `IfEditorShowCursor_` | 0x4184C4 | 0x41BA3C | editor |
| `load_motion_file` | code | `LoadMotionFile_` | — | 0x42054C | editor |
| `pack_bitmap` | code | `PackBitmap_` | 0x419AD8 | 0x41D058 | editor |
| `pack_mask` | code | `PackMask_` | 0x419D10 | 0x41D290 | editor |
| `add_keyboard_handler` | code | `AddKeyboardHandler_` | 0x4184D4 | 0x41BA4C | hardware |
| `biostime` | code | `biostime_` | 0x41999C | 0x41CF18 | hardware |
| `clear_mouse_cursor_win95` | code | `ClearMouseCursorWIN95_` | 0x418BDC | 0x41C158 | hardware |
| `draw_db_mouse_cursor_win95` | code | `DrawDBMouseCursorWIN95_` | 0x418A54 | 0x41BFD0 | hardware |
| `draw_mouse_cursor` | code | `DrawMouseCursor_` | 0x418778 | 0x41BCF0 | hardware |
| `draw_mouse_cursor_win95` | code | `DrawMouseCursorWIN95_` | 0x4188EC | 0x41BE68 | hardware |
| `mcursor_x` | data | `_MCursorX` | 0x46934A | 0x479E2A | hardware |
| `mcursor_y` | data | `_MCursorY` | 0x4F0998 | 0x636580 | hardware |
| `mouse_buffer` | data | `_MouseBuffer` | 0x470010 | 0x480010 | hardware |
| `old_clip_blit` | code | `OldClipBlit_` | 0x413EB8 | 0x417430 | hardware |
| `select_video_page` | code | `SelectVideoPage_` | 0x411874 | 0x414694 | hardware |
| `wait_vert_blank` | code | `WaitVertBlank_` | 0x411844 | 0x414664 | hardware |
| `check_directories` | code | `CheckDirectories_` | 0x41C53C | 0x41FABC | installer |
| `select_sound_card` | code | `SelectSoundCard_` | 0x412F38 | 0x4164A0 | installer |
| `select_sound_card_win95` | code | `SelectSoundCardWIN95_` | 0x412EA8 | 0x416410 | installer |
| `set_up_sub_directories` | code | `SetUpSubDirectories_` | 0x41B584 | 0x41EB04 | installer |

### menu

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `delete_point` | code | `DeletePoint_` | 0x4303EC | 0x43A6C4 | editor |
| `delete_script` | code | `DeleteScript_` | 0x4307BC | 0x43AA94 | editor |
| `find_svga_card` | code | `FindSVGACard_` | 0x430A58 | 0x43AD30 | hardware |

### req

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `handle_configure` | code | `HandleConfigure_` | 0x433104 | 0x43D3F0 | installer |
| `handle_dir_gadg` | code | `HandleDirGadg_` | 0x432160 | 0x43C430 | installer |
| `handle_set_card` | code | `HandleSetCard_` | 0x433370 | 0x43D62C | installer |
| `handle_set_install` | code | `HandleSetInstall_` | 0x43337C | 0x43D638 | installer |
| `install_to_disk` | code | `InstallToDisk_` | 0x4337D8 | 0x43DA80 | installer |

### topo

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `save_screen_shot` | code | `SaveScreenShot_` | 0x43FBC4 | 0x44A268 | debug |
| `add_arc` | code | `AddArc_` | 0x43E080 | 0x4483B0 | editor |
| `check_block_needs_rendering` | code | `CheckBlockNeedsRendering_` | 0x43FBBC | 0x44A260 | editor |
| `compare_bitmaps_0_and_1` | code | `CompareBitmaps0and1_` | 0x43FB5C | 0x44A200 | editor |
| `init_topography` | code | `InitTopography_` | 0x43DFF8 | 0x448328 | editor |
| `left_ang` | data | `_LeftAng` | 0x505B88 | 0x67BCEC | editor |
| `mask_bit_map` | data | `_MaskBitMap` | 0x505F1C | 0x67C080 | editor |
| `next_mask_tab_offset` | data | `_NextMaskTabOffset` | 0x505F48 | 0x67C0AC | editor |
| `num_arcs` | data | `_NoOfArcs` | 0x505F4A | 0x67C0AE | editor |
| `right_ang` | data | `_RightAng` | 0x505C50 | 0x67BDB4 | editor |
| `spare_bit_map` | data | `_SpareBitMap` | 0x505F24 | 0x67C08C | editor |

### win

| Port name | Kind | Watcom symbol | E1 (`E1WIN95.EXE`) | E2 (`E2WIN95.EXE`) | Category |
|---|---|---|---|---|---|
| `change_screen_mode_win95` | code | `ChangeScreenModeWIN95_` | 0x44A7C0 | 0x458770 | hardware |
| `get_windows_directory_win95` | code | `GetWindowsDirectoryWIN95_` | 0x44A9E8 | 0x458998 | hardware |

## Removed before 0.9.4

These were removed one by one before the list above. Most are empty in the
original, or are wrappers that call another function with the same arguments.

**Empty stubs or no callers:**

- `init`: `analyse_view`, `archive_all`, `archive_all_fast`, `display_beep`, `expand_palette`, `set_grey_palette`, `zeroise_bitmap_pointers`, `read_from_dsp`, `write_to_dsp`, `load_hires_path` (fills `hires_path`, never read), `setup_hi_res_long_screen` (no callers), `clear_background_svga`, `rect_fill_svga`, `text_svga`, `clip_blit_svga`, `draw_mouse_cursor_svga`, `draw_db_mouse_cursor_svga`, `clear_mouse_cursor_svga`, `clear_db_mouse_cursor_svga`, `load_backmask` (SVGA bank-switched video path, no callers on any platform including DOS/Win9x)
- `anim`: `clear_choice_box`, `draw_choice_box`, `load_action_directory`, `draw_view_cone_tri`, `draw_world_square`
- `map`: `reposition_fixed_parts`, `reposition_thing` (declared, never defined, no callers), `go_to_location`, `load_packed_views` (no callers)
- `req`: `handle_ok2`, `handle_test`, `handle_strings_gadg`, `handle_file_gadg`, `handle_uninstall`
- `topo`: `show_topography`, `make_mask_map`, `paint_word`, `draw_tri`, `find_normal`, `draw_clipped_line`, `load_long_screen`, `save_raw_for_printing` (same dead terrain-editor debug subsystem as `show_topography`, no callers)
- `tri`: `clip_tri`

**Dead wrappers (the call is made directly instead):**

- `init`: `clip_mask2` → `clip_mask`, `text_win95` → `small_text_win95`, `do_text_with_mask_win95_japan` → `text_with_mask`, `old_draw` → `draw`, `draw_clipped` → `draw`, `clear_rectangle` → `rect_fill`
- `display`: `transpose_matrix` → `c_transpose_matrix`, `x_view_transform` → `long_view_transform`, `c_matrix_mult_display` → `c_matrix_mult`
- `edit`: `write_event` → `fwrite`
- `game`: `save_word` → `fwrite`
- `move`: `find_direction_and_distance_zx` → `find_direction_and_distance`
- `music`: `init_sound_channels` → `platform_audio_init`, `update_sound_mix` → `platform_audio_mix`
- `win`: `finish_win95` → `platform_shutdown`
