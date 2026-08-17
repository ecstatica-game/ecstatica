# TODO

## Engine
### Bugs
[x] Collisions - player gets stuck on walls when moving diagonally
[x] Collisions - player go through view objects and can walk outside map bounds
[x] Collisions - player go on top of map blocks that are too high

## Ecstatica 1
### Bugs
[x] - Rotation issues when transitions views
[ ] - Player not able to trigger certain events (like reading books, place object that trigger actions)
[x] - Actor walk animations not playing (but they move)
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

## Ecstatica 2
### Bugs
[x] - Polygon Rasterization issue - intro scene flying gargoyle winds are not rendered correctly

