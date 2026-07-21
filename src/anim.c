/**
 * anim.c
 *
 * Animation helpers:
 *   ellipse management for keyframes, choice box, action directory loading.
 *
 * 5 functions prefixed anim_ in the original ASM.
 */

#include "anim.h"
#include "edit.h"
#include "display.h"
#include "game.h"
#include "init.h"
#include "move.h"
#include "asm_f.h"
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 *  Ellipse-to-Key Management
 * ══════════════════════════════════════════════════════════════ */

/* anim_add_ellipse_to_key_430568
 * Appends an ellipse_t to the end of a key's ellipse list.
 */
void add_ellipse_to_key(ellipse_t *new_ellipse, key_t *key) {
    new_ellipse->next = NULL;
    ellipse_t *ellipse = key->ellipses_list;

    if (ellipse) {
        while (ellipse->next)
            ellipse = ellipse->next;
        ellipse->next = new_ellipse;
    } else {
        key->ellipses_list = new_ellipse;
    }
}

/* anim_add_ellipse_43059C
 * Allocates a new ellipse_t.
 */
ellipse_t *add_ellipse(void) {
    ellipse_t *new_ellipse = (ellipse_t *)malloc(sizeof(ellipse_t));
    if (!new_ellipse) {
        beep_message("Out of memory!");
        return NULL;
    }
    return new_ellipse;
}

/* ══════════════════════════════════════════════════════════════
 *  Choice Box (editor feature)
 * ══════════════════════════════════════════════════════════════ */

/* anim_clear_choice_box_4305D0 — editor-only UI feature */
void clear_choice_box(void) {
}

/* anim_draw_choice_box_4305E0 — editor-only UI feature */
void draw_choice_box(void) {
}

/* ══════════════════════════════════════════════════════════════
 *  Action Directory Loading
 * ══════════════════════════════════════════════════════════════ */

/* anim_load_action_directory_430600 — loads action directory index from disk.
 * Not called at runtime; actions loaded via offset table or search functions. */
void load_action_directory(void) {
}

/* anim_draw_view_cone_tri_430744 — E2: 0x430744 */
void draw_view_cone_tri(void) {
}

/* anim_draw_world_square_42FE54 — E2: 0x42FE54 */
void draw_world_square(void) {
}

/* anim_recalc_cam_42FDE0 — E2: 0x42FDE0 */
void recalc_cam(void) {
    calculate_view_matrices();
    if (selected_thing) {
        vector_t input_vec = {{{0, 0, (int16_t)(-selected_thing->actor_box_size * 4)}}};
        vector_t result_vec;
        matrix_vector(&input_vec, &result_vec, &view_matrix);
        copy_vector(&view_pos, &selected_thing->position_vector);
        add_vector(&view_pos, &result_vec);
    }
}

/* anim_check_action_name_exists_42FF60 — E2: 0x42FF60 */
int check_action_name_exists(int16_t index) {
    if (index < 0)
        return 0;
    char *ptr = (char *)action_names;
    for (int16_t i = 0; i < index; i++) {
        int len = (int)strlen(ptr);
        if (len == 0)
            return 0;
        ptr += len + 1;
    }
    if (strlen(ptr) == 0)
        return 0;
    return 1;
}

/* anim_calculate_ellipses_one_key_430474 — E2: 0x430474 */
void calculate_ellipses_one_key(key_t *key, actor_t *actor) {
    ellipse_t *ell = key->ellipses_list;
    part_t *part = actor->actor_parts_list;
    while (part) {
        if (ell) {
            part->vector_persp.X = ell->field_0;
            part->vector_persp.Y = ell->field_2;
            part->mask_distanse_ = ell->field_4;
            part->projected_axes.X = ell->field_6;
            part->projected_axes.Y = ell->field_8;
            part->projected_axes.Z = ell->field_A;
            part->color = (int8_t)ell->field_C;
            part->vector_persp.Z = part->mask_distanse_;
            ell = ell->next;
        }
        part = part->next_in_display_list;
    }
}

/* anim_calculate_ellipses_430304 — E2: 0x430304 */
void calculate_ellipses(key_t *key, actor_t *actor, int16_t factor) {
    ellipse_t *cur = key->ellipses_list;
    ellipse_t *nxt = key->next ? key->next->ellipses_list : NULL;
    part_t *part = actor->actor_parts_list;
    while (part) {
        if (!cur || !nxt)
            break;
        part->vector_persp.X = cur->field_0;
        part->vector_persp.Y = cur->field_2;
        part->mask_distanse_ = cur->field_4;
        part->projected_axes.X = cur->field_6;
        part->projected_axes.Y = cur->field_8;
        part->projected_axes.Z = cur->field_A;
        part->color = (int8_t)cur->field_C;

        part->vector_persp.X += (int16_t)(((int)(nxt->field_0 - cur->field_0) * factor) >> 14);
        part->vector_persp.Y += (int16_t)(((int)(nxt->field_2 - cur->field_2) * factor) >> 14);
        part->mask_distanse_ += (int16_t)(((int)(nxt->field_4 - cur->field_4) * factor) >> 14);
        part->projected_axes.X += (int16_t)(((int)(nxt->field_6 - cur->field_6) * factor) >> 14);
        part->projected_axes.Y += (int16_t)(((int)(nxt->field_8 - cur->field_8) * factor) >> 14);
        part->projected_axes.Z += (int16_t)(((int)(nxt->field_A - cur->field_A) * factor) >> 14);

        part->vector_persp.Z = part->mask_distanse_;

        cur = cur->next;
        nxt = nxt->next;
        part = part->next_in_display_list;
    }
}

static void clear_choice_box_internal(int16_t x, int16_t y, int16_t w, int16_t h) {
    int plane = 1 - db;
    clip_blit(2, x, y, plane, x, y, w + 1, h + 1, 0xC0);
}

static void draw_choice_box_internal(int16_t x, int16_t y, int16_t w, int16_t h, const char *str) {
    int plane = 1 - db;
    int x2 = x + w;
    int y2 = y + h;

    a_pen_colour = 0;
    rect_fill(plane, x, y, x2, y2);

    a_pen_colour = 1;
    move_pen(plane, x, y);
    draw(plane, x, y2);
    draw(plane, x2, y2);
    draw(plane, x2, y);
    draw(plane, x, y);

    a_pen_colour = 1;
    b_pen_colour = 0;
    draw_mode[plane] = 2;
    move_pen(plane, x + 8, y + 4);
    int len = (int)strlen(str);
    if (len > 8) len = 8;
    text(plane, str, len);

    if ((int)strlen(str) > 8) {
        int len2 = (int)strlen(str + 8);
        if (len2 > 8) len2 = 8;
        move_pen(plane, x + 16, y + 4);
        text(plane, str + 8, len2);
    }
}

/* anim_clear_all_choices_430098 — E2: 0x430098 */
void clear_all_choices(int16_t x, int16_t y) {
    clear_choice_box_internal(x, y, 0x48, 0x18);
    clear_choice_box_internal(x + 0x4B, y, 0x48, 0x18);
    clear_choice_box_internal(x + 0x96, y, 0x48, 0x18);
    clear_choice_box_internal(x + 0xE1, y, 0x48, 0x18);
}

/* anim_draw_all_choices_42FFC0 — E2: 0x42FFC0 */
void draw_all_choices(int16_t x, int16_t y) {
    if (!selected_scene) return;
    char *buf = selected_scene->scene_name_buf;
    if (strlen(&buf[0]) > 0)
        draw_choice_box_internal(x, y, 0x48, 0x18, &buf[0]);
    if (strlen(&buf[25]) > 0)
        draw_choice_box_internal(x + 0x4B, y, 0x48, 0x18, &buf[25]);
    if (strlen(&buf[50]) > 0)
        draw_choice_box_internal(x + 0x96, y, 0x48, 0x18, &buf[50]);
    if (strlen(&buf[75]) > 0)
        draw_choice_box_internal(x + 0xE1, y, 0x48, 0x18, &buf[75]);
}

/* anim_position_external_act_42FE58 — E2: 0x42FE58 */
void position_external_act(act_t *act, uint16_t game_time, actor_t *actor) {
    if (game_time < act->key_progress)
        return;

    key_t *cur_key = act->actor_keys_list;
    if (cur_key) {
        while (game_time >= cur_key->KEY_position) {
            event_t *event = cur_key->key_event_list;
            while (event) {
                modify_part(event, actor, 0, act->act_action);
                event = event->next;
            }
            if (cur_key == act->actor_keys_list) {
                if (!(act->act_action->action_flags & 2))
                    default_modifieds(actor);
            }
            act->key_progress = cur_key->KEY_position;
            cur_key = cur_key->next;
            if (!cur_key) break;
        }
    }

    if (cur_key) {
        uint16_t prev_pos = act->key_progress;
        uint16_t next_pos = cur_key->KEY_position;
        if (next_pos != prev_pos) {
            int factor = ((int)(game_time - prev_pos) << 14) / (int)(next_pos - prev_pos);
            if (factor != 0) {
                event_t *event = cur_key->key_event_list;
                while (event) {
                    advance_part(event, (int16_t)factor, actor, act->act_action);
                    event = event->next;
                }
                if (!(act->act_action->action_flags & 2))
                    advance_def_modifieds(actor, factor);
            }
        }
    }

    act->actor_keys_list = cur_key;
    act->key_progress = game_time;
}
