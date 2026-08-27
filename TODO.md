# TODO

## Engine
### Bugs
[x] Event suppression was never implemented. supress_events_469BF0 gates both
    modify_part (0x425000) and advance_part (0x425D26): while complete_act
    winds an act forward during scene setup / swap-in, only events flagged
    NO_SUPRESS (0x100 — the pose events) may apply. The port had the flag in
    event_type_flags and the four writes in map_swap_in_actor, but the variable
    was a map.c file-static that nothing read, so every INTERACT in a scene
    script — CheckPickUp, CheckPutDown, HoldThingWithPart — fired during setup.
[x] The held-object fallback in modify_part / advance_part is a port invention.
    E1 (0x425075..0x4250D5, 0x425D99) simply drops a part-targeted event when
    the owning actor has no part with that index. The port instead redirected
    it through part_heap_link and then a search of every held object, so events
    aimed at a part the actor does not own landed on whatever it was holding.
    That is the mechanism behind the scene-132 book bug and behind scene
    scripts acting on the wrong held item. Now E1-gated to the drop.
    NOTE: the old TODO claimed this was "verified faithful to
    0x42B8C8..0x42B8F4". That address range is inside ellipse_draw_triangle
    (0x42A9E4, size 0x1081) and has nothing to do with event dispatch — the
    verification was never valid. E2 behaviour here is still unchecked.
[x] Stuck things were never erased from the background store. flags 0x0800
    means "currently painted into the background", set once by draw_stuck_parts
    (0x41DF19) and cleared only by the un-stick pass at the top of prepare_parts
    (0x41D978). prepare_parts in the port was clearing 0x0800 for every actor
    every frame, so stuck things were re-baked continuously and a thing that
    stopped being stuck — a ground item that was just picked up — kept its
    painted-in copy until the whole view was re-rendered. Ported the un-stick
    pass (erase via clear_a_stuck_thing, force overlapping stuck neighbours to
    re-bake, force overlapping subtitles to redraw) and dropped the per-frame
    reset. E1 draw_parts (0x41E441) confirms 0x0800 gates per-frame drawing.
[x] check_texture_loaded retried a permanently-missing texture on every
    put_a_cuboid call and re-set stop_the_clock each time, which zeroes
    local_game_time (move.c) and freezes all action playback — pick-up
    animations never reached their CheckPickUp keyframe. Now attempts each
    texture once, gate cleared in remove_all_textures, and only touches
    stop_the_clock when the file actually exists.
[x] Collisions - player gets stuck on walls when moving diagonally
[x] Collisions - player go through view objects and can walk outside map bounds
[x] Collisions - player go on top of map blocks that are too high
[x] check_texture_loaded was declared in game.h and defined nowhere. Defined it
    (game.c, after E1 0x446EA8) and called it from put_a_cuboid at the point the
    original does (0x421757, right after the `tex_idx >= 0 && !select_flag`
    test). Only the by-name branch is ported: texture_offset_50566C has exactly
    one xref in E1 (the read at 0x446F29) and no writer, so the load_by_offset
    branch is dead there. tri.c:378 is the rasterizer, past the load point —
    it needs no call.
[x] shade_map bit 7 — NOT a bug. Verified at 0x42AF12..0x42AF29: the original
    flat path clamps row/col to 0..127, reads the byte via `unk_4DF52D`
    (= shade_map_4DF530 - 3, hence the dword read + `sar 24`), and does
    `and edx, 7Fh` before indexing shade_tab. Identical to ellipse.c:333 /
    tri.c:354. Bit 7 only means "skip" in the ellipsoid span fills, which index
    a 256-entry shade_lut; the flat rasterizer (asm_triangle_line_44F733) never
    touches shade_map at all — it writes one precomputed colour byte.
    The xref list for shade_map_4DF530 misses the flat-path read because of the
    -3 alias; don't trust it alone.

## Ecstatica 1
### Bugs
[x] - Rotation issues when transitions views
[ ] - Player not able to trigger certain events (like reading books, place object that trigger actions)
      Likely fixed by the check_pick_up code-index off-by-one below — retest.
[x] - Actor walk animations not playing (but they move)
[ ] - CRITICAL PATH: object handling. Four symptoms, one code path
      (look_for_pick_up -> rep slot -> action events -> check_pick_up /
      check_put_down / HoldThingWithPart):

        a) give: object leaves the wrong hand, but the script flag reads as
           if the right one was given
        b) give: object should disappear from the giver and does not
        c) throw: object stays floating in the air, no physics
        d) the parked scene 132 book bug (below)

      Note on tooling: the installed IDA is IDA *Free* 9.1, whose bundled
      decompiler (hexcx) is x86-64 only. Every `decompile` on these 32-bit PEs
      returns hx:GenPseudo regardless of which database is open — that error is
      not a wrong-database signal. Use `disasm`; it works fine. The E1 database
      (decomp/ida/ECSTATIC.EXE.i64) is the one currently loaded.

      All six functions have now been diffed against the binary. Fixed:

        1. [x] look_for_pick_up — added look_for_pick_up_e1(part) (move.c),
               matching 0x427128: takes the hand PART, returns 4 for a null
               part and 4 when the part already holds something. The three
               caller-side hoists are gone.

        2. [x] dead-actor branch removed for E1. 0x4272C4 has only 0/1/2.

        3. [x] Hand/slot pairing settled. The two E1 call sites are
               0x429364 (key3 / numpad 3 fallthrough): _PartTab[0], +41, and
               0x429563 (key1 / numpad 1): _PartTab[1], +36. The earlier note
               that 0x429586 was "+41 with _PartTab[0]" was wrong — that
               address is the key3 range test, not a call. Space has no E1
               equivalent at all (the original reads numpad only), so it now
               mirrors the numpad-1 branch: _PartTab[1] with +36.

        4. [x] e1_pick_up_hand deleted. E1 has no hand global; slot-driven only.

        5. [x] Confirmed faithful, no change. E1 check_put_down (0x42706C) is
               passed action_flags&2 from the INTERACT dispatch at 0x425A31
               and never reads edx — the throw bit is dead in E1 and there is
               no reposition. Not the cause of (c). See (c) below.

        6. [x] HoldThingWithPart (0x425A4F) was missing its first half: when
               the receiving part already holds something else, E1 releases it
               (chain fixup -> find_positions_on_path -> hold_thing_with_part
               -> clear part_heap_link) before taking the new thing. Added,
               plus the update_game_icons call the original makes when the
               holder is the selected thing.

      New divergences found in the same pass, all fixed:

        7. [x] hold_thing_with_part (0x41D4A2) selects the left-hand
               offset/rotation set on `part->name_index == 1`. The port
               hardcoded 7 (an E2 part numbering), so in E1 every held object
               used the right-hand offsets. This is the direct cause of (a):
               the object renders on the wrong side while the script flag,
               which follows the rep slot, reads correctly.

        8. [x] hold_thing_with_part tail: E1 stops at 0x41D5B8 — no
               find_relative_rot_vector, and no clear_a_stuck_thing (that
               function does not exist in E1) and no else-branch clearing the
               holder's stuck flag. Gated to E2.

        9. [x] check_pick_up code lookup was off by one. 0x4273E6 indexes
               dword_4EBCBC, and code_tab is code_tab_4EBCC0, so the slot is
               param-1 — corroborated by the error path, which names code
               param-1. The port used code_tab[param]. Also, E1 runs it via
               execute_code (0x443D1C), which zeroes actor and part; the port
               was passing the target and the arm part. Both corrected, and
               the port-only execute_code_with_part helper is now gone.

       10. [x] look_for_pick_up's `!t->actor_parts_list` skip is E2-only —
               E1 (0x4271D2) filters on flags&0x20, t!=self and part_heap_link
               alone, and never dereferences the parts list.

      Still open — (c) throw physics. E1 has no gravity: update_position has
      exactly two callers in the whole binary, modify_part (0x42520B) and
      advance_part (0x425EFC), both event-driven, and topo_update_position
      (0x43E804) is only a recursive step-splitter, not motion. So a thrown
      object in E1 must be moved by an action running on the object itself.
      Next step: find what gives the thrown thing an act. Candidates are the
      et_flags&0x200 held-object event redirect (move.c:2085) and
      ExecutePartCode (INTERACT sub-type 4, 0x425B04) firing while the object
      is still held. Retest (a)/(b)/(d) first — 7 and 9 land squarely on them.
[ ] - PARKED: book (bok_mon3) in scene 132 opens on pickup and shows a grey slab
      at the hand. It should stay closed; it opens later via a scene action.

      Cause chain, all traced and confirmed:
        - pickup picks look_for_pick_up()+41 -> hero rep slot 41 -> action 83
          (dur 110, flags 0200, next 82, thing -7)
        - action 83 carries OFFSET(2)/COLOUR(3)/VECTOR1(4) events for part index
          76 on all 11 keys (pos 7424..65280), CheckPickUp at pos 33536
        - hero has parts 0..42, no 76, so modify_part falls through to the
          held-object search and applies them to the book
        - book part 76 ('z2') gets squash (4,44,56)->(36,20,40) and colour ->6:
          the flat-open pose and the grey slab, one cause

      Verified faithful to the E1 binary, so none of these is the bug:
        modify_part held-object fallback      0x42B8C8..0x42B8F4
        look_for_pick_up classification       0x4272D3 (< -bs ->0, <= bs ->1, else 2)
        look_for_pick_up geometry             found_height = Y - 2*box - thing.Y,
                                              pre_filter 2*box, dist <= box
        merge_seeked_file header/new_name_sys 0x43B080, 0x43B126
        merge_event_names remaps ADD_PART_TO_THING via new_part_name[]

      Correction: the "joystick pickup branch 0x429586 (+41, _PartTab[0])" line
      that used to sit in the list above was wrong. 0x429586 is the key3 range
      test. The two real call sites are 0x429364 (_PartTab[0], +41) and
      0x429563 (_PartTab[1], +36) — so slot 41 belongs to _PartTab[0].

      Known real divergences found on the way — ALL NOW FIXED, see the critical
      path above (items 1, 2, 9, 10) and the Engine section:
        - look_for_pick_up part-vs-actor argument and the return-4 cases
        - the dead-actor branch (E2-ism)
        - check_texture_loaded missing
        - shade_map bit 7 — turned out not to be a divergence at all
        - material_flags is populated (init.c:822) and never read; the original
          uses it only in topo_find_height_now_material_448998 (still open,
          harmless)

      Next step: the book's parts carry humanoid names ('Left molet', 'Right
      forearm', 'Left eye', 'Right butt', 'z2'), which is either the model
      reusing a character rig's name slots — in which case part 76 really is a
      book part and the divergence is somewhere not yet examined — or
      new_part_name[] mis-mapping that record. Settle it by dumping the book's
      thing record's own part-name table from the archive and comparing against
      part_names[]. Diagnostics for all of this are committed and env-gated.

      Unpark this now: the critical path above is fixed. This bug reaches part
      76 through rep slot 41, and slot 41 is reached via _PartTab[0], whose
      selection just changed — retest before doing any more archive work.

## Ecstatica 2
### Bugs
[x] - Polygon Rasterization issue - intro scene flying gargoyle winds are not rendered correctly

