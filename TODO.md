# TODO

## Engine
### Bugs
[x] Collisions - player gets stuck on walls when moving diagonally
[x] Collisions - player go through view objects and can walk outside map bounds
[x] Collisions - player go on top of map blocks that are too high
[ ] check_texture_loaded is declared in game.h and defined nowhere. put_a_cuboid
    and tri.c:378 index texture_tab unguarded, so an unloaded texture silently
    becomes a flat fill. Original calls it at 0x42533B; E1 body at 0x446EA8.
[ ] shade_map bit 7 ("skip pixel") is honoured by the ellipsoid span fills
    (asm_f.c:282) but masked off by the flat path (ellipse.c:333, tri.c:354).
    Check against ellipse_draw_triangle_4319FC.

## Ecstatica 1
### Bugs
[x] - Rotation issues when transitions views
[ ] - Player not able to trigger certain events (like reading books, place object that trigger actions)
[x] - Actor walk animations not playing (but they move)
[ ] - CRITICAL PATH: object handling. Four symptoms, one code path
      (look_for_pick_up -> rep slot -> action events -> check_pick_up /
      check_put_down / HoldThingWithPart):

        a) give: object leaves the wrong hand, but the script flag reads as
           if the right one was given
        b) give: object should disappear from the giver and does not
        c) throw: object stays floating in the air, no physics
        d) the parked scene 132 book bug (below)

      Blocker: IDA must have decomp/ida/ECSTATIC.EXE.i64 loaded (E1 Win95,
      = data/e1/W/ECSTATIC.EXE, PE32, md5 cc52177c824a47ebd166bc42f97c80d1).
      With the wrong database open every decompile returns hx:GenPseudo.

      Functions to diff, in order:
        move_check_put_down_42706C
        move_check_pick_up_427308
        move_look_for_pick_up_427128
        display_hold_thing_with_part_41D458
        anim_behaviour_428914          (keyboard pick-up branch)
        topo_update_position_43E804    (only candidate for thrown motion)

      Divergences already located, unfixed:

        1. look_for_pick_up (move.c:2972) — E1 takes the hand PART and
           returns 4 when that part already holds something; the port takes
           the actor and hoisted the check into the callers (move.c:1210,
           1222, 1232), returning 3 for null. The return value IS the rep
           slot offset (+36 / +41), so a wrong return picks a wrong action:
           wrong arm animates while the held-link goes elsewhere. Explains
           (a) and (b).

        2. look_for_pick_up dead-actor branch (move.c:3029) — flags&4 -> 5/6/7
           is an E2-ism; E1 has no such branch.

        3. Hand/slot pairing (move.c:1205-1238) — key 79 pairs _PartTab[1]
           with +36, key 81 pairs _PartTab[0] with +41, but space pairs
           _PartTab[0] with +36. The joystick branch at 0x429586 is +41 with
           _PartTab[0], so +41 is left. Settle the space case against
           anim_behaviour_428914.

        4. e1_pick_up_hand (move.c:29) is write-only: set at three call
           sites, zeroed in check_pick_up (move.c:3613), never read. E1 has
           no hand global in the symbol dump, so hand choice is slot-driven
           only — either wire it up or delete it.

        5. E1 put-down never repositions (move.c:3699, and the same skip in
           check_pick_up at move.c:3640). E2 drops the thing to
           find_height_now()-30 and zeroes rotate_vector.X/Z when the throw
           bit (action_flags&2, move.c:2399) is clear; the port skips the
           whole block for E1. Prime suspect for (c). Note E1 has no
           velocity/gravity function anywhere in the module dump, so thrown
           motion has to come from topo_update_position or from an act
           spawned on the freed thing — trace which.

        6. Give path (move.c:2401 INTERACT sub-type 3, plus the et_flags&0x200
           held-object redirect at move.c:2085) clears the previous holder
           only through a->part_heap_link. A stale link there leaves the
           giver still rendering the object — candidate for (b).

      Order of work: load the E1 db, diff the six functions, then fix 1+2
      (cheapest, largest blast radius), then 5, then re-test the give case,
      then unpark the book bug — it routes action 83 through a rep slot, so
      it may fall out of the slot-selection fix.
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
        joystick pickup branch                0x429586 (+41, _PartTab[0])
        merge_seeked_file header/new_name_sys 0x43B080, 0x43B126
        merge_event_names remaps ADD_PART_TO_THING via new_part_name[]

      Known real divergences found on the way (none explain the symptom):
        - E1 look_for_pick_up takes the hand PART and returns 4 when that part
          already holds something; the port takes the actor and hoisted the
          check into the caller, returning 3 for null
        - E1 has no dead-actor branch; the port's flags&4 -> 5/6/7 is an E2-ism
        - check_texture_loaded is declared in game.h and defined nowhere;
          put_a_cuboid and tri.c:378 index texture_tab without it (orig calls it
          at 0x42533B), so an unloaded texture silently becomes a flat fill
        - shade_map bit 7 ("skip pixel") is honoured by the ellipsoid span fills
          (asm_f.c:282 etc.) but masked off by the flat path (ellipse.c:333,
          tri.c:354). Not yet checked against ellipse_draw_triangle_4319FC
        - material_flags is populated (init.c:822) and never read; the original
          uses it only in topo_find_height_now_material_448998

      Next step: the book's parts carry humanoid names ('Left molet', 'Right
      forearm', 'Left eye', 'Right butt', 'z2'), which is either the model
      reusing a character rig's name slots — in which case part 76 really is a
      book part and the divergence is somewhere not yet examined — or
      new_part_name[] mis-mapping that record. Settle it by dumping the book's
      thing record's own part-name table from the archive and comparing against
      part_names[]. Diagnostics for all of this are committed and env-gated.

      Unpark this after the object critical path above: the first two
      divergences listed here are items 1 and 2 there, and this bug reaches
      part 76 through a rep slot, so the slot-selection fix may resolve it.

## Ecstatica 2
### Bugs
[x] - Polygon Rasterization issue - intro scene flying gargoyle winds are not rendered correctly

