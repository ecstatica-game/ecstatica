/**
 * edit.c
 *
 * Entity management: add/remove things, parts, keys, events.
 * Core game loop (make_thing), entity defaults, scene/action management.
 * 28 functions prefixed with edit_ in the original ASM.
 */

#include "edit.h"
#include "display.h"
#include "ellipse.h"
#include "file.h"
#include "game.h"
#include "icon.h"
#include "init.h"
#include "map.h"
#include "menu.h"
#include "move.h"
#include "music.h"
#include "topo.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static bool quit_flag;

int16_t mask_distance[64];

/* edit_copy_defaults_to_actual  E1: 0x422390 | E2: 0x425FE0 */
void copy_defaults_to_actual(actor_t *actor) {
    if (!actor) return;

    actor->actor_rep_index = actor->default_repert;

    for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
        copy_vector(&part->Rotate, &part->def_rotate);
        copy_vector(&part->Offset, &part->def_offset);
        copy_vector(&part->AbsPosition, &part->def_position);
        copy_vector(&part->displacement_point, &part->def_displacement);
        copy_vector(&part->VECTOR_Squash, &part->def_Squash);
        copy_vector(&part->VECTOR_RelCentre, &part->def_RelCentre);
        copy_vector(&part->field_74, &part->def_vector3);
        part->type = part->def_type;
        part->flags = part->default_flags;
        part->color = part->default_color;

        calculate_squash(part);
        calc_rel_offset(part);
        calc_rel_centre(part);

        for (point_t *point = part->points_list; point; point = point->next)
            copy_vector(&point->offset_point, &point->def_offset_point);
    }

    for (tri_t *tri = actor->polygone_tri_list; tri; tri = tri->next) {
        tri->tri_color_3 = tri->tri_color_1;
        tri->tri_color_4 = tri->tri_color_2;
        tri->triangle_flags = tri->tri_use_flag;
    }
}

/* edit_copy_defaults_to_actual_not_flags  E1: 0x4224A4 | E2: 0x4260F4 */
void copy_defaults_to_actual_not_flags(actor_t *actor) {
    if (!actor) return;

    actor->actor_rep_index = actor->default_repert;

    for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
        copy_vector(&part->Rotate, &part->def_rotate);
        copy_vector(&part->Offset, &part->def_offset);
        copy_vector(&part->AbsPosition, &part->def_position);
        copy_vector(&part->displacement_point, &part->def_displacement);
        copy_vector(&part->VECTOR_Squash, &part->def_Squash);
        copy_vector(&part->VECTOR_RelCentre, &part->def_RelCentre);
        copy_vector(&part->field_74, &part->def_vector3);
        part->type = part->def_type;

        calculate_squash(part);
        calc_rel_offset(part);
        calc_rel_centre(part);

        for (point_t *point = part->points_list; point; point = point->next)
            copy_vector(&point->offset_point, &point->def_offset_point);
    }

    for (tri_t *tri = actor->polygone_tri_list; tri; tri = tri->next) {
        tri->tri_color_3 = tri->tri_color_1;
        tri->tri_color_4 = tri->tri_color_2;
        tri->triangle_flags = tri->tri_use_flag;
    }
}

/* edit_copy_actual_to_defaults  E1: 0x4225A0 | E2: 0x4261F0 */
void copy_actual_to_defaults(actor_t *actor) {
    if (!actor) return;

    actor->default_repert = actor->actor_rep_index;

    for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
        copy_vector(&part->def_rotate, &part->Rotate);
        copy_vector(&part->def_offset, &part->Offset);
        copy_vector(&part->def_position, &part->AbsPosition);
        copy_vector(&part->def_displacement, &part->displacement_point);
        copy_vector(&part->def_Squash, &part->VECTOR_Squash);
        copy_vector(&part->def_RelCentre, &part->VECTOR_RelCentre);
        copy_vector(&part->def_vector3, &part->field_74);
        part->default_flags = part->flags;
        part->def_type = part->type;
        part->default_color = part->color;

        for (point_t *point = part->points_list; point; point = point->next)
            copy_vector(&point->def_offset_point, &point->offset_point);
    }

    for (tri_t *tri = actor->polygone_tri_list; tri; tri = tri->next) {
        tri->tri_color_1 = tri->tri_color_3;
        tri->tri_color_2 = tri->tri_color_4;
        tri->triangle_flags = tri->tri_use_flag;
    }
}

/* edit_add_scene  E1: 0x4226A0 | E2: 0x4262F0 */
scene_t *add_scene(void) {
    scene_t *new_scene = find_free_scene();
    new_scene->scene_index = -1;
    new_scene->scene_script_list = NULL;
    new_scene->camera_index = -1;
    new_scene->scene_music_index = -1;
    new_scene->scene_code_index = -1;
    new_scene->scene_code_2 = -1;
    new_scene->scene_use_flag = 2;
    new_scene->last_scene_direction = -1;
    new_scene->scene_time = game_time;
    new_scene->scene_next = scene_list;
    scene_list = new_scene;

    for (int i = 0; i < 18; i++)
        new_scene->action_indices[i] = -1;

    return new_scene;
}

/* edit_add_action  E1: 0x422790 | E2: 0x4263E0 */
action_t *add_action(void) {
    action_t *new_action = find_free_action();
    new_action->act_duration = 256;
    new_action->key_list = NULL;
    new_action->next = NULL;
    new_action->thing_name_index = -1;
    new_action->next_action_index = -1;
    new_action->action_time = game_time;
    new_action->action_flags = 0x200;

    if (action_list) {
        action_t *last = action_list;
        while (last->next)
            last = last->next;
        last->next = new_action;
    } else {
        action_list = new_action;
    }
    return new_action;
}

/* edit_add_thing  E1: 0x4227F8 | E2: 0x426448 */
actor_t *add_thing(void) {
    actor_t *new_thing = find_free_actor();
    new_thing->flags = 1;
    new_thing->time_actor = game_time;
    new_thing->type = 7;
    make_identity(&new_thing->matrix_1);
    make_identity(&new_thing->matrix33_2);
    new_thing->joint_position.Z = 0;
    new_thing->joint_position.Y = 0;
    new_thing->joint_position.X = 0;
    new_thing->actor_parts_list = NULL;
    new_thing->holding_actor = NULL;
    new_thing->move_type = -1;
    new_thing->actor_box_size = 200;
    new_thing->polygone_tri_list = NULL;
    new_thing->default_repert = -1;
    new_thing->actor_hit_code = -1;
    new_thing->code_at_hp_change = -1;
    new_thing->actor_init_code = -1;
    new_thing->picked_up_code = -1;
    new_thing->dead_code_index = -1;
    new_thing->extra_action_index = 6;
    new_thing->area_to_clear = &sub_area_to_clear[0];
    new_thing->last_actor_direction = -1;
    new_thing->end_action_index = -1;
    new_thing->interact_target_index = -1;
    new_thing->interact_state = 0;
    new_thing->actor_Speed_factor = 100;
    new_thing->field_176 = -1;
    new_thing->actor_strength_factor = 100;
    new_thing->actor_magic_factor = 100;
    new_thing->magic_stop_action = -1;
    new_thing->spawner_index = -1;
    new_thing->parent_actor = new_thing;
    new_thing->actor_rep_index = -1;

    new_thing->next_thing1 = thing_list;
    thing_list = new_thing;
    return new_thing;
}

/* edit_add_script  E1: 0x42272C | E2: 0x42637C */
script_t *add_script(scene_t *scene) {
    script_t *new_script = find_free_script();
    new_script->script_actor_index = -1;

    if (scene->scene_script_list) {
        script_t *last = scene->scene_script_list;
        while (last->next_script)
            last = last->next_script;
        last->next_script = new_script;
    } else {
        scene->scene_script_list = new_script;
    }

    new_script->next_script = NULL;
    new_script->script_action.action_index = -1;
    new_script->script_action.act_duration = 0;
    new_script->script_action.key_list = NULL;
    new_script->script_action.action_flags = 2;
    new_script->script_action.next = NULL;

    return new_script;
}

/* edit_add_repertoire  E1: 0x422CF4 | E2: 0x426944 */
rephead_t *add_repertoire(void) {
    rephead_t *rep = find_free_rep();
    rep->rep_index = -1;
    rep->next_rep = repertoire_list;
    repertoire_list = rep;
    memset(rep->action_slots, 0xFF, 416);
    rep->thing_index = -1;
    rep->rep_flags = -1;
    rep->rep_use_flag = 3;
    rep->rep_time = game_time;
    return rep;
}

/* edit_add_part_4265AC — Add a new part as child of parent (actor or part).
 * parent_core is either an actor_t* or part_t* — both share the
 * same core layout (name_index, flags, type, …, actor_parts_list, parent_actor)
 * at matching offsets.  We use actor_t* and rely on layout compatibility. */
part_t *add_part(actor_t *parent_core) {
    if (!parent_core) return NULL;

    part_t *part = find_free_part();
    if (!part) return NULL;

    part->flags = 0;
    part->parent_actor = (actor_t *)((part_t *)parent_core)->parent_actor;
    /* If parent IS the actor itself (type == 7), parent_actor is itself */
    if (parent_core->type == 7)
        part->parent_actor = parent_core;
    part->type = 4;  /* Part type */

    /* Zero out squash vector */
    part->VECTOR_Squash.X = 0;
    part->VECTOR_Squash.Y = 0;
    part->VECTOR_Squash.Z = 0;

    part->field_12E_point_to_point = NULL;
    part->def_pos_flags = 0;
    part->position_flags = 0;

    /* Append to end of parent's part list */
    if (parent_core->actor_parts_list) {
        part_t *last = parent_core->actor_parts_list;
        while (last->next)
            last = last->next;
        last->next = part;
    } else {
        parent_core->actor_parts_list = part;
    }

    part->next = NULL;
    part->actor_parts_list = NULL;
    part->holding_actor = parent_core;

    if (parent_core->type == 7) {
        /* Parent is an actor — set anchored part */
        parent_core->next_in_path = part;
        part->next_in_display_list = part->next;
    } else {
        /* Parent is a part — link into display list */
        part_t *parent_part = (part_t *)parent_core;
        part->next_in_display_list = parent_part->next_in_display_list;
        parent_part->next_in_display_list = part;
    }

    part->color = 1;
    part->color_shade = 0x4000;

    /* Copy current values to defaults */
    copy_vector(&part->def_rotate, &part->Rotate);
    copy_vector(&part->def_offset, &part->Offset);
    copy_vector(&part->def_position, &part->AbsPosition);
    copy_vector(&part->def_Squash, &part->VECTOR_Squash);
    copy_vector(&part->def_RelCentre, &part->VECTOR_RelCentre);
    copy_vector(&part->def_vector3, &part->field_74);
    part->default_flags = part->flags;
    part->def_type = part->type;
    part->default_color = part->color;

    calculate_squash(part);
    calc_rel_offset(part);
    calc_rel_centre(part);

    return part;
}

/* edit_add_triangle_426798 — Add a triangle to an actor using 4 point pointers
 * points[0..2] are the triangle vertices, points[3] is the optional quad point */
tri_t *add_triangle(actor_t *actor, point_t **points) {
    tri_t *tri = find_free_tri();
    if (!tri) return NULL;

    /* Append to end of actor's triangle list */
    if (actor->polygone_tri_list) {
        tri_t *last = actor->polygone_tri_list;
        while (last->next)
            last = last->next;
        last->next = tri;
    } else {
        actor->polygone_tri_list = tri;
    }

    tri->next = NULL;
    tri->point1 = points[0];
    tri->point2 = points[1];
    tri->point3 = points[2];
    tri->tri_color_3 = 15;
    tri->tri_color_4 = 13;
    tri->tri_color_1 = 15;
    tri->tri_color_2 = 13;
    tri->shade_multiplier = 0x4000;
    tri->tri_index = -1;
    tri->tri_shade_name = -1;
    tri->texture_name_index = -1;
    tri->quad_point4 = points[3];
    tri->parent_actor = actor;

    return tri;
}

/* edit_add_point_426858 — Add a point to a part */
point_t *add_point(part_t *part) {
    point_t *pt = find_free_point();
    if (!pt) return NULL;

    /* Append to end of part's point list */
    if (part->points_list) {
        point_t *last = part->points_list;
        while (last->next)
            last = last->next;
        last->next = pt;
    } else {
        part->points_list = pt;
    }

    pt->next = NULL;
    pt->point_index = -1;
    pt->parent_part = part;

    return pt;
}

/* edit_initialise_act */
void initialise_act(act_t *act) {
    if (!act) return;
    action_t *action = act->act_action;
    if (action) {
        act->key_progress = 0;
        act->duration = action->act_duration;
        act->flags = action->action_flags;
        act->anim_param = 0;
        act->actor_keys_list = action->key_list;
        if (action->action_flags & 1)
            act->loop_count = 0;
        else
            act->loop_count = 1;
    }
}

/* edit_remove_from_display_list */
void remove_from_display_list(actor_t *actor) {
    int left_display = 0;
    if (!actor) return;

    if (root_thing) {
        if (actor == root_thing) {
            left_display = 1;
            root_thing = root_thing->next_in_display_list;
        } else {
            for (actor_t *cur = root_thing; cur->next_in_display_list; cur = cur->next_in_display_list) {
                if (actor == cur->next_in_display_list) {
                    left_display = 1;
                    cur->next_in_display_list = cur->next_in_display_list->next_in_display_list;
                    break;
                }
            }
        }
    }

    if (left_display) {
        copy_vector(&actor_position[actor->name_index], &actor->position_vector);
        copy_vector(&actor_orientation[actor->name_index], &actor->rotate_vector);
        actor_rep_name[actor->name_index] = actor->actor_rep_index;
        actor_hit_points[actor->name_index] = actor->actor_hitpoints;
        actor_magic[actor->name_index] = (int16_t)actor->actor_magic;
        if (actor->actor_act.act_action) {
            for (key_state_t *key = actor->actor_act.actor_keys_list; key; key = key->next) {
                for (event_t *event = key->key_event_list; event; event = event->next) {
                    if (event->event_type == INTERACT && event->param1 == 4 && event->param2 != 0) {
                        execute_thing_code(actor, (int16_t)(event->param2 - 1));
                    }
                }
            }
        }
    }

    if (actor->flags & 1) {
        actor->time_actor = game_time - 1;
        actor->flags &= 0xFFFE;
    }
}

/* edit_insert_key_41F80C — insert key sorted by position */
key_state_t *insert_key(action_t *action, uint16_t position) {
    if (!action) return NULL;

    key_state_t *key = find_free_key();
    if (!key) return NULL;

    key->KEY_position = position;

    /* Find insertion point: sorted by position ascending */
    key_state_t **prev_ptr = &action->key_list;
    while (*prev_ptr && (*prev_ptr)->KEY_position <= key->KEY_position) {
        prev_ptr = &(*prev_ptr)->next;
    }

    key->next = *prev_ptr;
    *prev_ptr = key;
    return key;
}

/* edit_add_event_to_key_41F964 — add event sorted by priority */
void add_event_to_key(event_t *event, key_state_t *key) {
    if (!key || !event) return;

    int priority = event_priority[event->event_type & 0x7F];

    event_t **prev_ptr = &key->key_event_list;
    while (*prev_ptr) {
        int existing_priority = event_priority[(*prev_ptr)->event_type & 0x7F];
        if (existing_priority > priority) break;
        prev_ptr = &(*prev_ptr)->next;
    }

    event->next = *prev_ptr;
    *prev_ptr = event;
}

/* edit_add_ellipse_to_key_41FA2C — defined in anim.c */

/* edit_make_thing_4200E8 — the main game loop */
void make_thing(void) {
    program_up_and_running = true;

    /* Set up screen edges and centre (matching assembly) */
    right_edge = screen_width;
    bottom_edge = screen_height;
    left_edge = 0;
    top_edge = 0;
    screen_centre_x = screen_width / 2;
    screen_centre_y = screen_height / 2;
    /* asm clears active_camera; prepare_parts re-sets it each frame in
     * asm via check_camera/check_view. C prepare_parts doesn't yet port
     * that block — keep camera alive from initial check_view. */
    /* active_camera = NULL; */

    for (;;) {
        /* Pump events + read input (assembly: get_mouse handles msg pump) */
        get_mouse();
        get_joystick();

        /* Execute game logic (includes rendering via Phase 3) */
        do_movement();

        /* Check for quit */
        if (quit_flag || !program_up_and_running) break;
    }
}

/* edit_free_all_heaps_420490 — game_free_all_heaps_452D3C
 * Marks every entry in every static heap array as "free" by setting
 * the appropriate flag field, then resets all list pointers. */
void free_all_heaps(void) {
    for (int i = 0; i < SOUND_POOL_SIZE; i++) {
        if (!(sound_heap_arr[i].use_flag & 0x8000)) {
            /* release_sound_buffer(&sound_heap_arr[i]); — stubbed */
        }
    }

    /* Mark all actor entries as free (flags = 0x8000) */
    for (int i = 0; i < ACTOR_POOL_SIZE; i++)
        actor_heap_arr[i].flags = 0x8000;

    /* Mark all part entries as free (type = 0x8000) */
    for (int i = 0; i < PART_POOL_SIZE; i++)
        part_heap_arr[i].type = 0x8000;

    /* Mark all action entries as free (action_flags = 0x8000) */
    for (int i = 0; i < ACTION_POOL_SIZE; i++)
        action_heap_arr[i].action_flags = (int16_t)0x8000;

    /* Mark all scene entries as free (scene_use_flag = 0x8000) */
    for (int i = 0; i < SCENE_POOL_SIZE; i++)
        scene_heap_arr[i].scene_use_flag = (int16_t)0x8000;

    /* Mark all script entries as free (last byte = 0x80) */
    for (int i = 0; i < SCRIPT_SIZE; i++)
        script_arr[i].script_actor_index = -1;

    /* Mark all repertoire entries as free (rep_use_flag = 0x8000) */
    for (int i = 0; i < REP_POOL_SIZE; i++)
        rep_heap_arr[i].rep_use_flag = (int16_t)0x8000;

    /* Mark all event entries as free (event_type = 0x8000) */
    for (int i = 0; i < EVENT_POOL_SIZE; i++)
        event_heap_arr[i].event_type = (uint16_t)0x8000;

    /* Mark all key entries as free (field_E = 0x80) */
    for (int i = 0; i < KEY_POOL_SIZE; i++)
        key_heap_arr[i].field_E = (int8_t)0x80;

    /* Mark all triangle entries as free (tri_use_flag = 0x8000) */
    for (int i = 0; i < TRI_SIZE; i++)
        tri_arr[i].tri_use_flag = 0x8000;

    /* Mark all point entries as free (point_use_flag = 1) */
    for (int i = 0; i < POINT_POOL_SIZE; i++)
        point_heap_arr[i].point_use_flag = 1;

    /* Free all sounds */
    for (int i = 0; i < SOUND_POOL_SIZE; i++)
        free_sound(&sound_heap_arr[i]);

    /* Mark all texture entries as free */
    for (int i = 0; i < TEXTURE_POOL_SIZE; i++)
        texture_heap_arr[i].use_flag = (uint16_t)0x8000;

    /* Mark all t-action entries as free (taction_index = -1) */
    for (int i = 0; i < TACTION_POOL_SIZE; i++)
        taction_heap_arr[i].taction_index = -1;

    /* NOTE: The assembly game_free_all_heaps_452D3C does NOT clear list
     * pointers (root_thing, code_list, etc.) or call clear_ptr_tabs().
     * Those are handled separately by initialise_parts() during initial
     * setup, or by new_game()/initialise_game() during game resets.
     * Code and map_area lists persist across game resets. */
}

/* edit_add_code_4268A4 — allocate a code_t and prepend to code_list */
code_t *add_code(void) {
    code_t *code = (code_t *)calloc(1, sizeof(code_t));
    if (!code) return NULL;
    code->index_code = -1;
    code->text_line_of_code = NULL;
    code->next_code = code_list;
    code_list = code;
    return code;
}

/* edit_add_first_line_of_code_4268E4 — allocate a line_of_code_t as first line of a code block */
line_of_code_t *add_first_line_of_code(code_t *code) {
    if (!code) return NULL;
    line_of_code_t *loc = (line_of_code_t *)calloc(1, sizeof(line_of_code_t));
    if (!loc) return NULL;
    memset(loc->field_0, ' ', 53);     /* space-fill text area */
    loc->next_line_code = code->text_line_of_code;
    code->text_line_of_code = loc;
    return loc;
}

/* edit_add_line_of_code_42692C — allocate a line_of_code_t and insert after prev */
line_of_code_t *add_line_of_code(line_of_code_t *prev) {
    if (!prev) return NULL;
    line_of_code_t *loc = (line_of_code_t *)calloc(1, sizeof(line_of_code_t));
    if (!loc) return NULL;
    memset(loc->field_0, ' ', 53);     /* space-fill text area */
    loc->next_line_code = prev->next_line_code;
    prev->next_line_code = loc;
    return loc;
}

/* game_delete_code_4526E4 — unlink a code_t from code_list and free it */
void delete_code(code_t *code) {
    if (!code) return;

    /* Unlink from code_list */
    if (code_list == code) {
        code_list = code->next_code;
    } else {
        for (code_t *prev = code_list; prev; prev = prev->next_code) {
            if (prev->next_code == code) {
                prev->next_code = code->next_code;
                break;
            }
        }
    }

    /* Clear code_tab entry */
    if (code->index_code >= 0 && code->index_code < CODE_TAB_SIZE)
        code_tab[code->index_code] = NULL;

    /* Free text lines */
    line_of_code_t *loc = code->text_line_of_code;
    while (loc) {
        line_of_code_t *next = loc->next_line_code;
        free(loc);
        loc = next;
    }

    free(code);
}

/* edit_write_an_event  E1: ? | E2P: 0x41FDE8 */
void write_an_event(event_t *event, FILE *f) {
    if (!f || !event) return;

    fputc(event->event_type & 0xFF, f);
    fputc((event->event_type >> 8) & 0xFF, f);
    putw_be(event->param1, f);
    putw_be(event->param2, f);
    putw_be(event->param3, f);
}

/* edit_write_parts  E1: ? | E2P: 0x41FF08 */
void write_parts(actor_t *actor, FILE *f) {
    if (!f || !actor) return;

    part_t *part = actor->actor_parts_list;
    while (part) {
        /* Write part data */
        putw_be(part->Offset.X, f);
        putw_be(part->Offset.Y, f);
        putw_be(part->Offset.Z, f);
        putw_be(part->Rotate.X, f);
        putw_be(part->Rotate.Y, f);
        putw_be(part->Rotate.Z, f);
        putw_be(part->VECTOR_Squash.X, f);
        putw_be(part->VECTOR_Squash.Y, f);
        putw_be(part->VECTOR_Squash.Z, f);
        fputc(part->color, f);
        fputc(part->type, f);
        putw_be(part->flags, f);

        part = part->next;
    }
}

/* edit_advance_thing  E1: 0x422340 | E2: 0x425F90 */
void advance_thing(actor_t *actor, int16_t game_time) {
    if (!actor) return;
    if (actor->actor_act.flags & 0x400) return;

    act_t *act = actor->actor_act_list;
    while (act) {
        update_act(act, actor, game_time);
        act = act->next;
    }
    update_act(&actor->actor_act, actor, game_time);
    free_spent_acts(actor);
}

/* edit_advance_part_420158 — event-driven part animation update */
void advance_part(event_t *event, int16_t blend, actor_t *actor, action_t *action) {
    if (!event || !actor) return;
    if (event->event_index < 0) return;

    int event_type = event->event_type;
    part_t *work_part = NULL;
    point_t *work_point = NULL;

    /* Resolve target part/point based on event type flags */
    if (event_type_flags[event_type] & 0x10) {
        /* Part-targeted event */
        if (event_type_flags[event_type] & 0x200) {
            /* Look in held actors */
            for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list) {
                if (p->actor_2_held && p->actor_2_held->_PartTab) {
                    work_part = p->actor_2_held->_PartTab->field_0[event->event_index];
                    if (work_part) break;
                }
            }
            if (!work_part) return;
            actor = work_part->parent_actor;
        } else {
            if (actor->_PartTab)
                work_part = actor->_PartTab->field_0[event->event_index];
            if (!work_part) {
                if (!actor->part_heap_link) {
                    for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list) {
                        if (p->actor_2_held && p->actor_2_held->_PartTab) {
                            work_part = p->actor_2_held->_PartTab->field_0[event->event_index];
                            if (work_part) break;
                        }
                    }
                    if (!work_part) return;
                    actor = work_part->parent_actor;
                } else {
                    actor = actor->part_heap_link->parent_actor;
                    if (!(actor->state_flags & 1)) return;
                    if (actor->_PartTab)
                        work_part = actor->_PartTab->field_0[event->event_index];
                    if (!work_part) return;
                }
            }
        }
    } else if (event_type_flags[event_type] & 0x40) {
        /* Point-targeted event */
        if (actor->_PointTab)
            work_point = actor->_PointTab->field_0[event->event_index];
        if (!work_point) return;
    }

    /* Bug 56: use int-intermediate for translations/scales, short for
     * rotations. Two helpers — `mulInt` casts delta to int (32-bit),
     * `mulShort` casts to short first. Was using short-cast for ALL cases →
     * for translations where |param - value| exceeds 32767 the delta wrapped
     * and animation stepped wrong direction or wrong magnitude. Rotations
     * legitimately want short-cast (angles wrap at 16-bit).
     * mulInt: RotateThing/MoveThing/ScriptMove/Offset/Vector1/Vector2/
     * Position/AbsPos/OffsetPoint. mulShort: ScriptTurn/Rotate/AbsoluteRot. */
#define MULINT_X()   ((int16_t)((int)blend * (int)(event->param1 - target.X) >> 14))
#define MULINT_Y()   ((int16_t)((int)blend * (int)(event->param2 - target.Y) >> 14))
#define MULINT_Z()   ((int16_t)((int)blend * (int)(event->param3 - target.Z) >> 14))
#define MULSHORT_X() ((int16_t)((int)blend * (int16_t)(event->param1 - target.X) >> 14))
#define MULSHORT_Y() ((int16_t)((int)blend * (int16_t)(event->param2 - target.Y) >> 14))
#define MULSHORT_Z() ((int16_t)((int)blend * (int16_t)(event->param3 - target.Z) >> 14))

    vector_t temp;
    switch (event_type) {
    case ROTATE_THING: {
        part_t *rp = actor->part_heap_link;
        if (rp && (rp->parent_actor->state_flags & 1))
            actor = rp->parent_actor;
        if (actor->state_flags & 0x20) {
            update_thing(actor);
            actor->state_flags &= ~0x20;
        }
        vector_t target = actor->Rotate;
        temp.X = MULINT_X();
        temp.Y = MULINT_Y();
        temp.Z = MULINT_Z();
        add_vector(&actor->rotate_vector, &temp);
        add_vector(&actor->Rotate, &temp);
        break;
    }
    case MOVE_THING: {
        part_t *mp = actor->part_heap_link;
        if (mp && (mp->parent_actor->state_flags & 1))
            actor = mp->parent_actor;
        if (actor->state_flags & 0x20) {
            update_thing(actor);
            actor->state_flags &= ~0x20;
        }
        vector_t input;
        vector_t target = actor->Offset;
        input.X = MULINT_X();
        input.Y = MULINT_Y();
        input.Z = MULINT_Z();
        add_vector(&actor->Offset, &input);
        vector_t world;
        c_matrix_vector(&world, &actor->matrix33_2, &input);
        update_position(actor, &world);
        check_visibility(actor);
        break;
    }
    case SCRIPT_MOVE: {
        vector_t target = actor->position_vector;
        actor->position_vector.X += MULINT_X();
        actor->position_vector.Y += MULINT_Y();
        actor->position_vector.Z += MULINT_Z();
        break;
    }
    case SCRIPT_TURN: {
        vector_t target = actor->rotate_vector;
        actor->rotate_vector.X += MULSHORT_X();
        actor->rotate_vector.Y += MULSHORT_Y();
        actor->rotate_vector.Z += MULSHORT_Z();
        break;
    }
    case ROTATE: {
        if (!work_part) break;
        if (work_part->flags & 0x200) make_part_relative(work_part);
        vector_t target = work_part->Rotate;
        work_part->Rotate.X += MULSHORT_X();
        work_part->Rotate.Y += MULSHORT_Y();
        work_part->Rotate.Z += MULSHORT_Z();
        work_part->position_flags |= 2;
        break;
    }
    case OFFSET: {
        if (!work_part) break;
        vector_t target = work_part->Offset;
        work_part->Offset.X += MULINT_X();
        work_part->Offset.Y += MULINT_Y();
        work_part->Offset.Z += MULINT_Z();
        calc_rel_offset(work_part);
        work_part->position_flags |= 4;
        break;
    }
    case VECTOR1: {
        if (!work_part) break;
        vector_t target = work_part->VECTOR_Squash;
        work_part->VECTOR_Squash.X += MULINT_X();
        work_part->VECTOR_Squash.Y += MULINT_Y();
        work_part->VECTOR_Squash.Z += MULINT_Z();
        calculate_squash(work_part);
        update_relatives(work_part);
        work_part->position_flags |= 0x10;
        break;
    }
    case VECTOR2: {
        if (!work_part) break;
        vector_t target = work_part->VECTOR_RelCentre;
        work_part->VECTOR_RelCentre.X += MULINT_X();
        work_part->VECTOR_RelCentre.Y += MULINT_Y();
        work_part->VECTOR_RelCentre.Z += MULINT_Z();
        calc_rel_centre(work_part);
        work_part->position_flags |= 8;
        break;
    }
    case DISP_PNT: {
        if (!work_part) break;
        vector_t target = work_part->displacement_point;
        work_part->displacement_point.X += MULINT_X();
        work_part->displacement_point.Y += MULINT_Y();
        work_part->displacement_point.Z += MULINT_Z();
        break;
    }
    case FLAGS:
        if (!work_part) break;
        if (event->param2 & 0x100)
            work_part->flags = event->param1 | (work_part->flags & 0xFEFF);
        break;
    case POSITION: {
        if (!work_part) break;
        if (work_part->flags & 0x200) make_part_relative(work_part);
        vector_t target = work_part->AbsPosition;
        work_part->AbsPosition.X += MULINT_X();
        work_part->AbsPosition.Y += MULINT_Y();
        work_part->AbsPosition.Z += MULINT_Z();
        work_part->position_flags |= 1;
        break;
    }
    case ABSOLUTE_ROT: {
        if (!work_part) break;
        if (!(work_part->flags & 0x200)) {
            if (action && action->action_flags & 2)
                make_part_absolute(work_part);
            else
                make_part_base_relative(work_part);
        }
        vector_t target = work_part->Rotate;
        work_part->Rotate.X += MULSHORT_X();
        work_part->Rotate.Y += MULSHORT_Y();
        work_part->Rotate.Z += MULSHORT_Z();
        break;
    }
    case ABSOLUTE_POS: {
        if (!work_part) break;
        if (!(work_part->flags & 0x200)) {
            if (action && action->action_flags & 2)
                make_part_absolute(work_part);
            else
                make_part_base_relative(work_part);
        }
        vector_t target = work_part->AbsPosition;
        work_part->AbsPosition.X += MULINT_X();
        work_part->AbsPosition.Y += MULINT_Y();
        work_part->AbsPosition.Z += MULINT_Z();
        break;
    }
    case OFFSET_POINT: {
        if (!work_point) break;
        vector_t target = work_point->offset_point;
        work_point->offset_point.X += MULINT_X();
        work_point->offset_point.Y += MULINT_Y();
        work_point->offset_point.Z += MULINT_Z();
        break;
    }
    default:
        break;
    }
#undef MULINT_X
#undef MULINT_Y
#undef MULINT_Z
#undef MULSHORT_X
#undef MULSHORT_Y
#undef MULSHORT_Z
}

/* edit_advance_act  E1: ? | E2P: 0x420198 */
void advance_act(act_t *act, actor_t *actor, int game_time) {
    if (!act->act_action) {
        /* Bug 45: asm at 0x42AFEE gates flags |= 0x400 on `moving_camera == 0`.
         * When moving_camera set (cam interpolating between two views), don't
         * mark actor as finished-frame. */
        if (!moving_camera)
            actor->flags |= 0x400;
        return;
    }
    action_t *action = act->act_action;
    int some_time = act->duration * (0x10000 - act->key_progress) >> 16;
    uint16_t some_duration;

    if (game_time > some_time && !(action->next_action_index >= 0 || (act->flags & 1)))
        some_duration = 0xFFFF;
    else {
        while (game_time > some_time) {
            game_time -= some_time;
            complete_act(act, actor);
            if (action->next_action_index >= 0) {
                check_action_loaded(action->next_action_index);
                action_t *next_action = action_tab[action->next_action_index];
                if (next_action) {
                    action = next_action;
                    act->act_action = action;
                    act->duration = action->act_duration;
                    if (actor->actor_Speed_factor != 100)
                        act->duration = 100 * act->duration / actor->actor_Speed_factor;
                    act->key_progress = 0;
                    act->actor_keys_list = action->key_list;
                    act->flags = (act->flags & 0x40) | action->action_flags;
                    if (act->flags & 1)
                        act->flags &= ~0x40;
                } else {
                    beep_message("following action not found");
                }
            }
            some_time = act->duration * (0x10000 - act->key_progress) >> 16;
        }
        int candidate = act->key_progress + (game_time << 16) / act->duration;
        some_duration = (candidate < 0xFFFF) ? (uint16_t)candidate : 0xFFFF;
    }
    position_act(act, some_duration, actor);
    if (actor->flags & 0x400)
        clear_a_stuck_thing(actor);
    actor->flags &= ~0x400;
}

/* edit_advance_act_position  E1: ? | E2P: 0x4201D8 */
void advance_act_position(act_t *act, actor_t *actor, int delta) {
    action_t *action = act->act_action;
    if (!action) return;

    int progress;
    if (action->action_flags & 2) {
        progress = act->key_progress + delta;
        if (progress > act->duration) {
            complete_act(act, actor);
            act->key_progress = act->duration;
            act->flags |= 0x0400;
            return;
        }
        position_act(act, (uint16_t)progress, actor);
        return;
    }

    progress = act->key_progress + delta;
    while (progress > 0xFFFF) {
        int16_t loops = act->loop_count;
        if (loops > 1) {
            complete_act(act, actor);
            progress -= 0x10000;
            --act->loop_count;
        } else if (loops == 0) {
            complete_act(act, actor);
            progress -= 0x10000;
        } else {
            complete_act(act, actor);
            act->key_progress = -1;
            act->flags |= 0x0400;
            goto done;
        }
    }
done:
    if (!(act->flags & 0x0400))
        position_act(act, (uint16_t)progress, actor);
}

/* edit_advance_def_modifieds  E1: ? | E2P: 0x420218 */
void advance_def_modifieds(actor_t *actor, int blend) {
    if (!actor) return;
    for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
        if (part->def_pos_flags) {
            part->def_pos_flags = ~part->position_flags & part->def_pos_flags;
            int flags = part->def_pos_flags;
            if (flags & 1) {
                part->AbsPosition.X += (int16_t)(blend * (part->def_position.X - part->AbsPosition.X) >> 14);
                part->AbsPosition.Y += (int16_t)(blend * (part->def_position.Y - part->AbsPosition.Y) >> 14);
                part->AbsPosition.Z += (int16_t)(blend * (part->def_position.Z - part->AbsPosition.Z) >> 14);
            }
            if (flags & 2) {
                part->Rotate.X += (int16_t)(blend * (part->def_rotate.X - part->Rotate.X) >> 14);
                part->Rotate.Y += (int16_t)(blend * (part->def_rotate.Y - part->Rotate.Y) >> 14);
                part->Rotate.Z += (int16_t)(blend * (part->def_rotate.Z - part->Rotate.Z) >> 14);
            }
            if (flags & 4) {
                part->Offset.X += (int16_t)(blend * (part->def_offset.X - part->Offset.X) >> 14);
                part->Offset.Y += (int16_t)(blend * (part->def_offset.Y - part->Offset.Y) >> 14);
                part->Offset.Z += (int16_t)(blend * (part->def_offset.Z - part->Offset.Z) >> 14);
                calc_rel_offset(part);
            }
            if (flags & 8) {
                part->VECTOR_RelCentre.X += (int16_t)(blend * (part->def_RelCentre.X - part->VECTOR_RelCentre.X) >> 14);
                part->VECTOR_RelCentre.Y += (int16_t)(blend * (part->def_RelCentre.Y - part->VECTOR_RelCentre.Y) >> 14);
                part->VECTOR_RelCentre.Z += (int16_t)(blend * (part->def_RelCentre.Z - part->VECTOR_RelCentre.Z) >> 14);
                calc_rel_centre(part);
            }
            if (flags & 0x10) {
                part->VECTOR_Squash.X += (int16_t)(blend * (part->def_Squash.X - part->VECTOR_Squash.X) >> 14);
                part->VECTOR_Squash.Y += (int16_t)(blend * (part->def_Squash.Y - part->VECTOR_Squash.Y) >> 14);
                part->VECTOR_Squash.Z += (int16_t)(blend * (part->def_Squash.Z - part->VECTOR_Squash.Z) >> 14);
                calculate_squash(part);
                update_relatives(part);
            }
        }
    }
}

/* helper for update_thing */
static void update_thing_sub(actor_t *actor, int is_part_heap_link) {
    int condition = actor->state_flags & 1;
    if (is_part_heap_link) condition = !condition;
    if (!condition) {
        for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
            if (part->flags & 0x200)
                make_part_relative(part);
            part->def_pos_flags |= part->position_flags;
            part->position_flags = 0;
            part->flags &= 0xFEFF;
        }
        make_identity(&actor->matrix_1);
        uint16_t angle = (uint16_t)actor->rotate_vector.Y;
        if (angle) rotate_about_y(&actor->matrix_1, angle);
        angle = (uint16_t)actor->rotate_vector.X;
        if (angle) rotate_about_x(&actor->matrix_1, angle);
        angle = (uint16_t)actor->rotate_vector.Z;
        if (angle) rotate_about_z(&actor->matrix_1, angle);

        copy_matrix(&actor->matrix33_2, &actor->matrix_1);
        set_vector(&actor->Offset, 0, 0, 0);
        set_vector(&actor->Rotate, 0, 0, 0);
        copy_vector(&actor->actor_center, &actor->position_vector);
    }
}

/* edit_update_thing  E1: ? | E2P: 0x420268 */
void update_thing(actor_t *actor) {
    if (!actor) return;
    update_thing_sub(actor, 0);
    if (actor->part_heap_link)
        update_thing_sub(actor->part_heap_link->parent_actor, 1);
}

/* edit_update_act  E1: ? | E2P: 0x4202A8 */
void update_act(act_t *act, actor_t *actor, int some_time) {
    if (!act || !actor) return;

    if (!act->act_action) {
        if (!moving_camera)
            actor->flags |= 0x0400;
        return;
    }
    if (act->act_action->action_flags & 2) {
        int some_duration = act->key_progress + some_time;
        if (actor->name_index == 33)
        if (some_duration > act->duration) {
            complete_act(act, actor);
            act->key_progress = act->duration;
            act->flags |= 0x0400;
            if (actor->flags & 0x0400)
                clear_a_stuck_thing(actor);
            actor->flags &= ~0x0400;
            return;
        }
        position_act(act, (uint16_t)some_duration, actor);
        key_state_t *key = act->actor_keys_list;
        if (!key || key->key_event_list) {
            if (actor->flags & 0x0400)
                clear_a_stuck_thing(actor);
            actor->flags &= ~0x0400;
            return;
        }
        if (!moving_camera)
            actor->flags |= 0x0400;
        return;
    }

    int progress = (some_time << 16) / act->duration + act->key_progress;
    /* Non-scene loop-through — asm move_update_act_42AD60 +42ADFA..+42AE51.
     * Three-way branch on act->loop_count (int16 loop remaining):
     *   > 1: complete_act, dec loop_count, progress -= 0x10000, loop while progress > 0xFFFF
     *   ==1: complete_act, key_progress = 0xFFFF, flags |= 0x400, exit
     *   ==0: complete_act, progress -= 0x10000, loop */
    while (progress > 0xFFFF) {
        int16_t loops = act->loop_count;
        if (loops > 1) {
            complete_act(act, actor);
            act->loop_count = (int16_t)(loops - 1);
            progress -= 0x10000;
        } else if (loops == 1) {
            complete_act(act, actor);
            act->key_progress = (int16_t)0xFFFF;
            act->flags |= 0x0400;
            break;
        } else { /* loop_count == 0 */
            complete_act(act, actor);
            progress -= 0x10000;
        }
    }
    if (!(act->flags & 0x0400))
        position_act(act, (uint16_t)progress, actor);
    if (actor->flags & 0x0400)
        clear_a_stuck_thing(actor);
    actor->flags &= ~0x0400;
}

/* edit_update_relatives  E1: ? | E2P: 0x4202E8 */
void update_relatives(part_t *part) {
    if (!part) return;
    if (!(part->flags & 8)) {
        if (part->rel_offset.X)
            part->VECTOR_RelCentre.X = (int16_t)((part->VECTOR_Squash.X * part->rel_offset.X) >> 14);
        if (part->rel_offset.Y)
            part->VECTOR_RelCentre.Y = (int16_t)((part->VECTOR_Squash.Y * part->rel_offset.Y) >> 14);
        if (part->rel_offset.Z)
            part->VECTOR_RelCentre.Z = (int16_t)((part->VECTOR_Squash.Z * part->rel_offset.Z) >> 14);
    }
    for (part_t *child = part->actor_parts_list; child; child = child->next) {
        if (!(child->flags & 4)) {
            if (child->offset_squash_ratio.X)
                child->Offset.X = (int16_t)((part->VECTOR_Squash.X * child->offset_squash_ratio.X) >> 14);
            if (child->offset_squash_ratio.Y)
                child->Offset.Y = (int16_t)((part->VECTOR_Squash.Y * child->offset_squash_ratio.Y) >> 14);
            if (child->offset_squash_ratio.Z)
                child->Offset.Z = (int16_t)((part->VECTOR_Squash.Z * child->offset_squash_ratio.Z) >> 14);
        }
    }
}

/* edit_anchor_part  E1: ? | E2P: 0x420428 */
void anchor_part(actor_t *actor) {
    if (!actor) return;
    part_t *part = actor->actor_parts_list;
    if (!part) return;
    part->flags |= 0x200;  /* Mark as world-space part */
    copy_vector(&part->AbsPosition, &part->ellipse_center);
}

/* edit_unloosen_joint  E1: ? | E2P: 0x420458 */
void unloosen_joint(actor_t *actor) {
    if (!actor) return;
    part_t *part = actor->actor_parts_list;
    if (!part) return;
    part->flags &= ~0x200;  /* Clear world-space flag */
}

/* edit_beep_error  E1: 0x423280 | E2: 0x426F54 */
void beep_error(const char *msg) {
    if (msg)
        fprintf(stderr, "[ERROR] %s\n", msg);
}

/* edit_find_relative_rotations_42D36C */
void find_relative_rotations(part_t *part, matrix3x3_t *mtx) {
    matrix3x3_t tmp = *mtx;

    int16_t y_angle = arctan(tmp._13, tmp._33);
    if (y_angle != 0)
        pre_rotate_about_y(&tmp, -y_angle);

    int16_t x_angle = arctan(-tmp._23, tmp._33);
    if (x_angle != 0)
        pre_rotate_about_x(&tmp, -x_angle);

    int16_t z_angle = arctan(tmp._21, tmp._11);

    int counter = 0;
    if ((x_angle + 0x4000) & 0x8000) counter++;
    if ((y_angle + 0x4000) & 0x8000) counter++;
    if ((z_angle + 0x4000) & 0x8000) counter++;

    if (counter < 2) {
        part->Rotate.X = x_angle;
        part->Rotate.Y = y_angle;
        part->Rotate.Z = z_angle;
    } else {
        part->Rotate.X = 0x8000 - x_angle;
        part->Rotate.Y = y_angle - (int16_t)0x8000;
        part->Rotate.Z = z_angle - (int16_t)0x8000;
    }
}

/* edit_advance_selected_scene_or_action  E2: 0x425ED0 */
void advance_selected_scene_or_action(int16_t game_time_arg) {
    if (script_mode != 0 && selected_scene != NULL) {
        script_t *script = selected_scene->scene_script_list;
        while (script) {
            int16_t idx = script->script_actor_index;
            actor_t *actor = thing_tab[idx];
            advance_thing(actor, game_time_arg);
            script = script->next_script;
        }
        return;
    }

    actor_t *actor = selected_thing;
    if (!actor) return;
    if (!actor->actor_act.act_action) return;
    if (actor->actor_act.flags & 0x0400) return;

    int16_t act_dur = actor->actor_act.act_action->act_duration;
    int edi = ((int)act_dur * (int)(uint16_t)game_time_arg) >> 16;

    act_t *act = actor->actor_act_list;
    while (act) {
        update_act(act, actor, edi);
        act = act->next;
    }

    advance_act_position(&actor->actor_act, actor, (uint16_t)game_time_arg);
    free_spent_acts(actor);
}

/* edit_init_mask_distances  E2: 0x426E88 */
void init_mask_distances(void) {
    for (int i = 0; i < 64; i++) {
        mask_distance[i] = 0x7FFF;
    }
}

/* edit_set_mask_distance  E2: 0x426EA8 */
void set_mask_distance(void) {
    if (!selected_thing) return;
    if (!selected_thing->actor_parts_list) return;

    int pixel = read_pixel(3, 0, mouse_x, mouse_y);
    if (pixel < 0) return;

    int16_t dist = selected_thing->actor_parts_list->mask_distanse_;
    mask_distance[pixel] = dist;
    if (dist < 0)
        mask_distance[pixel] = 0;
}

/* edit_show_vector  E2: 0x426F04 */
void show_vector(int16_t *vec) {
    char buf[32];
    sprintf(buf, "x %4.4x y %4.4x z %4.4x",
            (unsigned)vec[0], (unsigned)vec[1], (unsigned)vec[2]);
    display_message(buf);
}

/* edit_message_vector_decimal  E2: 0x426F6C */
void message_vector_decimal(const char *label, int16_t *vec) {
    char buf[80];
    sprintf(buf, "%s %d %d %d",
            label, (int)vec[0], (int)vec[1], (int)vec[2]);
    display_message(buf);
}

/* edit_message_vector_hex  E2: 0x426FA0 */
void message_vector_hex(const char *label, int16_t *vec) {
    char buf[80];
    sprintf(buf, "%s %4.4x %4.4x %4.4x",
            label, (unsigned)vec[0], (unsigned)vec[1], (unsigned)vec[2]);
    display_message(buf);
}

/* edit_display_message  E2: 0x426FDC */
void display_message(const char *msg) {
    draw_mode[db] = 2;
    a_pen_colour = 0;

    if (msg) {
        char buf[56];
        strncpy(buf, msg, max_message_len);
        buf[max_message_len] = '\0';
        int len = (int)strlen(buf);
        while (len < max_message_len) {
            buf[len] = ' ';
            len++;
        }

        a_pen_colour = 7;
        b_pen_colour = 0;
        move_pen(db, vector_box_left, vector_box_top);
        text(db, buf, 0);
    } else {
        rect_fill(db, vector_box_left, vector_box_top,
                  max_message_len * 6, 8);
    }

    clip_blit(1 - db, vector_box_left, vector_box_top,
              db, vector_box_left, vector_box_top,
              max_message_len * 6, 8, 0xC0);
}

/* edit_make_game_screen  E2: 0x426BAC */
void make_game_screen(void) {
    left_edge = 0;
    right_edge = screen_width;
    top_edge = 0;
    bottom_edge = screen_height;
    screen_centre_x = right_edge / 2;
    screen_centre_y = bottom_edge / 2;
    active_camera = NULL;
}
