#ifndef EDIT_H
#define EDIT_H

#include "types.h"

extern int16_t mask_distance[64];

void advance_thing(actor_t *actor, int16_t game_time_arg);
void squash_trace(const char *who, const part_t *part, int blend,
                  const event_t *event, const vector_t *cur,
                  const action_t *action);
void copy_defaults_to_actual(actor_t *actor);
void copy_defaults_to_actual_not_flags(actor_t *actor);
void copy_actual_to_defaults(actor_t *actor);
scene_t *add_scene(void);
script_t *add_script(scene_t *scene);
action_t *add_action(void);
actor_t *add_thing(void);
part_t *add_part(actor_t *parent_core);
tri_t *add_triangle(actor_t *actor, point_t **points);
point_t *add_point(part_t *part);
code_t *add_code(void);
line_of_code_t *add_first_line_of_code(code_t *code);
line_of_code_t *add_line_of_code(line_of_code_t *prev);
char *file_find_code_name(int16_t index);
char *file_find_scene_name(int16_t index);
rephead_t *add_repertoire(void);
void add_sound(void);
void add_texture(void);
void add_map_area(void);
void initialise_act(act_t *act);
void add_event_to_key(event_t *event, key_state_t *key);
key_state_t *insert_key(action_t *action, uint16_t position);
void make_game_screen(void);
void add_to_display_list(actor_t *actor);
void remove_from_display_list(actor_t *actor);
void make_thing(void);
void beep_message(const char *msg);
void beep_error(const char *msg);
void display_message(const char *msg);
void advance_selected_scene_or_action(int16_t game_time_arg);
void init_mask_distances(void);
void set_mask_distance(void);
void show_vector(int16_t *vec);
void message_vector_decimal(const char *label, int16_t *vec);
void message_vector_hex(const char *label, int16_t *vec);

#endif /* EDIT_H */
