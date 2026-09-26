/**
 * layout.h
 *
 * Compile-time checks that the structs still have the original binary's
 * layout. Offsets and sizes are the ones E2WIN95.EXE uses; a field renamed
 * from field_XX keeps its offset here, so the check survives the rename.
 *
 * Only a 32-bit build can check them: the structs hold pointers, which are
 * 4 bytes in the original. DOS, Win9x, openfpgaOS and the PSP are 32-bit and
 * enforce every line; on 64-bit desktops the macros expand to nothing.
 *
 * Included once, from main.c.
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#include "anim.h"
#include "display.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "music.h"
#include "req.h"
#include "topo.h"
#include "types.h"

/* A negative array size is the one static assertion every compiler here
 * accepts; Open Watcom has no _Static_assert. */
#define LAYOUT_CAT2(a, b) a##b
#define LAYOUT_CAT(a, b)  LAYOUT_CAT2(a, b)

#if UINTPTR_MAX == 0xFFFFFFFFu
#define LAYOUT_OFFSET(type, field, off) \
    typedef char LAYOUT_CAT(layout_offset_, __LINE__)[(offsetof(type, field) == (off)) ? 1 : -1];
#define LAYOUT_SIZE(type, size) \
    typedef char LAYOUT_CAT(layout_size_, __LINE__)[(sizeof(type) == (size)) ? 1 : -1];
#else
#define LAYOUT_OFFSET(type, field, off)
#define LAYOUT_SIZE(type, size)
#endif

/* anim.h */
LAYOUT_OFFSET(struct key_s, field_E, 0xE)

LAYOUT_OFFSET(struct ellipse_s, field_0, 0x0)
LAYOUT_OFFSET(struct ellipse_s, field_2, 0x2)
LAYOUT_OFFSET(struct ellipse_s, field_4, 0x4)
LAYOUT_OFFSET(struct ellipse_s, field_6, 0x6)
LAYOUT_OFFSET(struct ellipse_s, field_8, 0x8)
LAYOUT_OFFSET(struct ellipse_s, field_A, 0xA)
LAYOUT_OFFSET(struct ellipse_s, field_C, 0xC)

/* display.h */
LAYOUT_OFFSET(struct graphic_name_s, field_0, 0x0)

LAYOUT_OFFSET(struct camera_data_s, unused_E, 0xE)
LAYOUT_OFFSET(struct camera_data_s, unused_12, 0x12)
LAYOUT_OFFSET(struct camera_data_s, unused_14, 0x14)
LAYOUT_OFFSET(struct camera_data_s, time, 0x16)
LAYOUT_OFFSET(struct camera_data_s, top_clip, 0x1A)

LAYOUT_OFFSET(struct tri_s, point1, 0x4)
LAYOUT_OFFSET(struct tri_s, tri_color_3, 0x10)
LAYOUT_OFFSET(struct tri_s, unused_1A, 0x1A)
LAYOUT_OFFSET(struct tri_s, tri_shade_name, 0x1C)
LAYOUT_OFFSET(struct tri_s, next, 0x1E)
LAYOUT_OFFSET(struct tri_s, parent_actor, 0x22)
LAYOUT_OFFSET(struct tri_s, shade_multiplier, 0x26)
LAYOUT_OFFSET(struct tri_s, quad_point4, 0x28)
LAYOUT_OFFSET(struct tri_s, texture_name_index, 0x2C)
LAYOUT_OFFSET(struct tri_s, u1, 0x2E)
LAYOUT_OFFSET(struct tri_s, v4, 0x3C)
LAYOUT_OFFSET(struct tri_s, unused_3E, 0x3E)

LAYOUT_OFFSET(struct part_s, field_50, 0x50)
LAYOUT_OFFSET(struct part_s, field_5C, 0x5C)
LAYOUT_OFFSET(struct part_s, field_60, 0x60)
LAYOUT_OFFSET(struct part_s, field_64, 0x64)
LAYOUT_OFFSET(struct part_s, field_74, 0x74)
LAYOUT_OFFSET(struct part_s, field_7A, 0x7A)
LAYOUT_OFFSET(struct part_s, field_7C, 0x7C)
LAYOUT_OFFSET(struct part_s, field_86, 0x86)
LAYOUT_OFFSET(struct part_s, field_88, 0x88)
LAYOUT_OFFSET(struct part_s, field_8C, 0x8C)
LAYOUT_OFFSET(struct part_s, field_90, 0x90)
LAYOUT_OFFSET(struct part_s, field_98, 0x98)
LAYOUT_OFFSET(struct part_s, field_9C, 0x9C)
LAYOUT_OFFSET(struct part_s, field_FE, 0xFE)

LAYOUT_OFFSET(struct point_s, field_C, 0xC)

/* game.h */
LAYOUT_OFFSET(struct name_text_s, field_0, 0x0)

LAYOUT_OFFSET(struct actor_s, name_index, 0x0)
LAYOUT_OFFSET(struct actor_s, flags, 0x2)
LAYOUT_OFFSET(struct actor_s, type, 0x4)
LAYOUT_OFFSET(struct actor_s, matrix_1, 0x6)
LAYOUT_OFFSET(struct actor_s, joint_position, 0x18)
LAYOUT_OFFSET(struct actor_s, actor_parts_list, 0x1E)
LAYOUT_OFFSET(struct actor_s, parent_actor, 0x22)
LAYOUT_OFFSET(struct actor_s, Rotate, 0x26)
LAYOUT_OFFSET(struct actor_s, Offset, 0x2C)
LAYOUT_OFFSET(struct actor_s, matrix_2, 0x32)
LAYOUT_OFFSET(struct actor_s, holding_actor, 0x44)
LAYOUT_OFFSET(struct actor_s, next_in_path, 0x48)
LAYOUT_OFFSET(struct actor_s, next_in_display_list, 0x4C)
LAYOUT_OFFSET(struct actor_s, next_thing1, 0x50)
LAYOUT_OFFSET(struct actor_s, _PartTab, 0x54)
LAYOUT_OFFSET(struct actor_s, actor_velocity, 0x58)
LAYOUT_OFFSET(struct actor_s, field_5E, 0x5E)
LAYOUT_OFFSET(struct actor_s, field_60, 0x60)
LAYOUT_OFFSET(struct actor_s, previous_position, 0x64)
LAYOUT_OFFSET(struct actor_s, field_6A, 0x6A)
LAYOUT_OFFSET(struct actor_s, field_6E_vect, 0x6E)
LAYOUT_OFFSET(struct actor_s, field_74, 0x74)
LAYOUT_OFFSET(struct actor_s, field_78, 0x78)
LAYOUT_OFFSET(struct actor_s, field_7C, 0x7C)
LAYOUT_OFFSET(struct actor_s, field_80, 0x80)
LAYOUT_OFFSET(struct actor_s, actor_behavior, 0x82)
LAYOUT_OFFSET(struct actor_s, position_vector, 0x84)
LAYOUT_OFFSET(struct actor_s, actor_center, 0x8A)
LAYOUT_OFFSET(struct actor_s, rotate_vector, 0x90)
LAYOUT_OFFSET(struct actor_s, move_type, 0x96)
LAYOUT_OFFSET(struct actor_s, wander_direction, 0x98)
LAYOUT_OFFSET(struct actor_s, actor_box_size, 0x9A)
LAYOUT_OFFSET(struct actor_s, start_position, 0x9C)
LAYOUT_OFFSET(struct actor_s, actor_act_list, 0xA2)
LAYOUT_OFFSET(struct actor_s, actor_act, 0xA6)
LAYOUT_OFFSET(struct actor_s, field_BC, 0xBC)
LAYOUT_OFFSET(struct actor_s, field_C0, 0xC0)
LAYOUT_OFFSET(struct actor_s, matrix33_2, 0xC6)
LAYOUT_OFFSET(struct actor_s, polygone_tri_list, 0xD8)
LAYOUT_OFFSET(struct actor_s, _TriangleTab, 0xDC)
LAYOUT_OFFSET(struct actor_s, _PointTab, 0xE0)
LAYOUT_OFFSET(struct actor_s, area_to_clear, 0xE4)
LAYOUT_OFFSET(struct actor_s, action_delay, 0xE8)
LAYOUT_OFFSET(struct actor_s, range_threshold, 0xEA)
LAYOUT_OFFSET(struct actor_s, actor_hitpoints, 0xEC)
LAYOUT_OFFSET(struct actor_s, full_actor_hp, 0xEE)
LAYOUT_OFFSET(struct actor_s, action_state, 0xF0)
LAYOUT_OFFSET(struct actor_s, part_heap_link, 0xF2)
LAYOUT_OFFSET(struct actor_s, held_offset, 0xF6)
LAYOUT_OFFSET(struct actor_s, held_rotate, 0xFC)
LAYOUT_OFFSET(struct actor_s, held_off_left, 0x102)
LAYOUT_OFFSET(struct actor_s, held_rot_left, 0x108)
LAYOUT_OFFSET(struct actor_s, field_10E, 0x10E)
LAYOUT_OFFSET(struct actor_s, field_110, 0x110)
LAYOUT_OFFSET(struct actor_s, bounding_box, 0x112)
LAYOUT_OFFSET(struct actor_s, time_actor, 0x11A)
LAYOUT_OFFSET(struct actor_s, actor_reperture, 0x11E)
LAYOUT_OFFSET(struct actor_s, actor_rep_index, 0x122)
LAYOUT_OFFSET(struct actor_s, default_repert, 0x124)
LAYOUT_OFFSET(struct actor_s, force_action_to_execute, 0x126)
LAYOUT_OFFSET(struct actor_s, queued_action, 0x12A)
LAYOUT_OFFSET(struct actor_s, target_actor, 0x12E)
LAYOUT_OFFSET(struct actor_s, actor_scene, 0x132)
LAYOUT_OFFSET(struct actor_s, code_at_hp_change, 0x136)
LAYOUT_OFFSET(struct actor_s, actor_hit_code, 0x138)
LAYOUT_OFFSET(struct actor_s, actor_init_code, 0x13A)
LAYOUT_OFFSET(struct actor_s, picked_up_code, 0x13C)
LAYOUT_OFFSET(struct actor_s, dead_code_index, 0x13E)
LAYOUT_OFFSET(struct actor_s, event_timer, 0x140)
LAYOUT_OFFSET(struct actor_s, action_variant, 0x142)
LAYOUT_OFFSET(struct actor_s, extra_action_index, 0x144)
LAYOUT_OFFSET(struct actor_s, state_flags, 0x146)
LAYOUT_OFFSET(struct actor_s, field_148, 0x148)
LAYOUT_OFFSET(struct actor_s, field_14C, 0x14C)
LAYOUT_OFFSET(struct actor_s, hold_timer, 0x150)
LAYOUT_OFFSET(struct actor_s, target_position, 0x152)
LAYOUT_OFFSET(struct actor_s, interact_timer, 0x158)
LAYOUT_OFFSET(struct actor_s, field_15A, 0x15A)
LAYOUT_OFFSET(struct actor_s, field_15C, 0x15C)
LAYOUT_OFFSET(struct actor_s, field_160, 0x160)
LAYOUT_OFFSET(struct actor_s, end_action_index, 0x162)
LAYOUT_OFFSET(struct actor_s, hit_angle, 0x164)
LAYOUT_OFFSET(struct actor_s, hit_type, 0x166)
LAYOUT_OFFSET(struct actor_s, action_index, 0x168)
LAYOUT_OFFSET(struct actor_s, interact_target_index, 0x16A)
LAYOUT_OFFSET(struct actor_s, interact_state, 0x16C)
LAYOUT_OFFSET(struct actor_s, interact_cooldown, 0x16E)
LAYOUT_OFFSET(struct actor_s, move_direction, 0x170)
LAYOUT_OFFSET(struct actor_s, last_actor_direction, 0x172)
LAYOUT_OFFSET(struct actor_s, actor_Speed_factor, 0x174)
LAYOUT_OFFSET(struct actor_s, field_176, 0x176)
LAYOUT_OFFSET(struct actor_s, field_178, 0x178)
LAYOUT_OFFSET(struct actor_s, tactions_list, 0x17C)
LAYOUT_OFFSET(struct actor_s, actor_hit_factor, 0x180)
LAYOUT_OFFSET(struct actor_s, actor_strength_factor, 0x182)
LAYOUT_OFFSET(struct actor_s, actor_magic, 0x184)
LAYOUT_OFFSET(struct actor_s, actor_magic_factor, 0x186)
LAYOUT_OFFSET(struct actor_s, magic_stop_action, 0x188)
LAYOUT_OFFSET(struct actor_s, spawner_index, 0x18A)

LAYOUT_OFFSET(struct line_of_code_s, field_0, 0x0)

/* init.h */
LAYOUT_OFFSET(struct config_s, views, 0x16)
LAYOUT_OFFSET(struct config_s, reserved, 0x17)

/* music.h */
LAYOUT_OFFSET(struct wave_s, format_tag, 0x0)
LAYOUT_OFFSET(struct wave_s, channels, 0x2)
LAYOUT_OFFSET(struct wave_s, samples_per_sec, 0x4)
LAYOUT_OFFSET(struct wave_s, avg_bytes_per_sec, 0x8)
LAYOUT_OFFSET(struct wave_s, block_align, 0xC)
LAYOUT_OFFSET(struct wave_s, bits_per_sample, 0xE)
LAYOUT_OFFSET(struct wave_s, extra_size, 0x10)

/* req.h */
LAYOUT_OFFSET(struct gadget_s, pixel_left, 0x16)
LAYOUT_OFFSET(struct gadget_s, pixel_right, 0x18)
LAYOUT_OFFSET(struct gadget_s, pixel_top, 0x1A)
LAYOUT_OFFSET(struct gadget_s, pixel_bottom, 0x1C)

LAYOUT_OFFSET(struct request_s, pos_x, 0x0)
LAYOUT_OFFSET(struct request_s, pos_y, 0x2)
LAYOUT_OFFSET(struct request_s, max_height, 0x6)
LAYOUT_OFFSET(struct request_s, pixel_right, 0x12)
LAYOUT_OFFSET(struct request_s, pixel_bottom, 0x16)

/* topo.h */
LAYOUT_OFFSET(struct profile_height_s, field_0, 0x0)
LAYOUT_OFFSET(struct profile_height_s, field_2, 0x2)
LAYOUT_OFFSET(struct profile_height_s, field_4, 0x4)
LAYOUT_OFFSET(struct profile_height_s, field_6, 0x6)
LAYOUT_OFFSET(struct profile_height_s, field_8, 0x8)
LAYOUT_OFFSET(struct profile_height_s, field_A, 0xA)
LAYOUT_OFFSET(struct profile_height_s, field_C, 0xC)
LAYOUT_OFFSET(struct profile_height_s, field_E, 0xE)

LAYOUT_OFFSET(struct bitmap_hdr_s, magic, 0x0)
LAYOUT_OFFSET(struct bitmap_hdr_s, version, 0x6)
LAYOUT_OFFSET(struct bitmap_hdr_s, size_x, 0x8)
LAYOUT_OFFSET(struct bitmap_hdr_s, size_y, 0xA)
LAYOUT_OFFSET(struct bitmap_hdr_s, palette_size, 0xC)
LAYOUT_OFFSET(struct bitmap_hdr_s, h_dpi, 0xE)
LAYOUT_OFFSET(struct bitmap_hdr_s, v_dpi, 0x10)
LAYOUT_OFFSET(struct bitmap_hdr_s, gamma, 0x12)
LAYOUT_OFFSET(struct bitmap_hdr_s, reserved, 0x14)

/* types.h */
LAYOUT_OFFSET(struct part_tab_s, field_0, 0x0)

LAYOUT_OFFSET(struct triangle_tab_s, field_0, 0x0)

LAYOUT_OFFSET(struct point_tab_s, field_0, 0x0)

LAYOUT_OFFSET(struct action_dir_s, field_0, 0x0)

LAYOUT_SIZE(struct event_s, 14)
LAYOUT_SIZE(struct action_s, 22)
LAYOUT_SIZE(struct tri_s, 68)
LAYOUT_SIZE(struct part_s, 350)
LAYOUT_SIZE(struct point_s, 42)
LAYOUT_SIZE(struct texture_s, 20)
LAYOUT_SIZE(struct rephead_s, 432)
LAYOUT_SIZE(struct token_s, 23)
LAYOUT_SIZE(struct act_s, 22)
LAYOUT_SIZE(struct actor_s, 396)
LAYOUT_SIZE(struct script_s, 29)
LAYOUT_SIZE(struct taction_s, 10)
LAYOUT_SIZE(struct config_s, 32)
LAYOUT_SIZE(struct map_area_element_s, 12)
LAYOUT_SIZE(struct bitmap_hdr_s, 32)
LAYOUT_SIZE(struct wave_s, 18)
LAYOUT_SIZE(struct camera_data_s, 28)
LAYOUT_SIZE(struct matrix3x3_s, 18)
LAYOUT_SIZE(struct vector_s, 6)
LAYOUT_SIZE(struct long_vector_s, 12)
LAYOUT_SIZE(struct palette_entry_s, 3)

#endif /* LAYOUT_H */
