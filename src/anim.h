#ifndef ANIM_H
#define ANIM_H

#include "types.h"

/* ══════════════════════════════════════════════════════════════
 *  Structure Definitions
 * ══════════════════════════════════════════════════════════════ */

/* ── Event parameter ── */
#pragma pack(push, 1)
typedef struct event_param_s {
    int16_t param1;
    int16_t param2;
    int16_t param3;
} event_param_t;
#pragma pack(pop)

/* ── Key entry ── */
#pragma pack(push, 1)
struct key_s {
    uint16_t KEY_position;
    struct key_s *next;
    event_t *key_event_list;
    ellipse_t *ellipses_list;
    char field_E;
};
#pragma pack(pop)

/* ── Event entry ── */
#pragma pack(push, 1)
struct event_s {
    int16_t event_index;
    uint16_t event_type;
    int16_t param1;
    int16_t param2;
    int16_t param3;
    struct event_s *next;
};  /* 14 bytes */
#pragma pack(pop)

/* ── Ellipse structure ── */
#pragma pack(push, 1)
struct ellipse_s {
    int16_t field_0;
    int16_t field_2;
    int16_t field_4;
    int16_t field_6;
    int16_t field_8;
    int16_t field_A;
    int16_t field_C;
    struct ellipse_s *next;
};
#pragma pack(pop)

/* ── Action entry ── */
#pragma pack(push, 1)
struct action_s {
    int16_t action_index;
    int16_t act_duration;
    key_state_t *key_list;
    struct action_s *next;
    uint16_t action_flags;
    int16_t thing_name_index;
    int16_t next_action_index;
    int32_t action_time;
};  /* 22 bytes */
#pragma pack(pop)

/* ══════════════════════════════════════════════════════════════
 *  Anim Arrays (extern)
 * ══════════════════════════════════════════════════════════════ */

extern action_t action_heap_arr[ACTION_POOL_SIZE];
extern event_t event_heap_arr[EVENT_POOL_SIZE];
extern key_state_t key_heap_arr[KEY_POOL_SIZE];

/* ══════════════════════════════════════════════════════════════
 *  Function Declarations
 * ══════════════════════════════════════════════════════════════ */

void clear_choice_box(void);
void draw_choice_box(void);
void add_ellipse_to_key(ellipse_t *ell, key_state_t *key);
ellipse_t *add_ellipse(void);
void load_action_directory(void);
void draw_view_cone_tri(void);
void draw_world_square(void);
void recalc_cam(void);
int check_action_name_exists(int16_t index);
void calculate_ellipses_one_key(key_state_t *key, actor_t *actor);
void calculate_ellipses(key_state_t *key, actor_t *actor, int16_t factor);
void clear_all_choices(int16_t x, int16_t y);
void draw_all_choices(int16_t x, int16_t y);
void position_external_act(act_t *act, uint16_t game_time, actor_t *actor);

#endif /* ANIM_H */
