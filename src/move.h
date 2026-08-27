#ifndef MOVE_H
#define MOVE_H

#include "types.h"

void find_direction_and_distance(int16_t *dir, int16_t *dist, int16_t dx, int16_t dz);
void find_dirn_and_dist(int16_t *dir, int16_t *dist, int16_t dx, int16_t dz);
void make_dead(actor_t *actor);
void behaviour(actor_t *actor, int game_time_arg);
int16_t do_wander(actor_t *actor);
int16_t do_new_wander(actor_t *actor);
void do_movement(void);
void advance_act_position(act_t *act, actor_t *actor, int a3);
void update_act(act_t *act, actor_t *actor, int some_time);
void advance_act(act_t *act, actor_t *actor, int game_time_arg);
void complete_act(act_t *act, actor_t *actor);
void turn_actor(actor_t *actor, int16_t angle);
void update_thing(actor_t *actor);
void position_act(act_t *act, uint16_t some_duration, actor_t *actor);
void advance_def_modifieds(actor_t *actor, int a2);
void default_modifieds(actor_t *actor);
void modify_part(event_t *event, actor_t *actor, int some_time, action_t *action);
void advance_part(event_t *event, int16_t a2, actor_t *actor, action_t *action);
void update_relatives(part_t *part);
act_t *find_free_act(void);
void spawn_action(event_t *event, actor_t *actor, int some_time);
void free_spent_acts(actor_t *actor);
void anchor_part(actor_t *actor);
void loosen_joint(actor_t *actor);
void unloosen_joint(actor_t *actor);
void find_inverse_of_attitude(part_t *part, matrix3x3_t *output);
void find_relative_rotations(part_t *part, matrix3x3_t *mtx);
void find_relative_rot_vector(vector_t *v, const matrix3x3_t *m);
void make_2_part_limb(part_t *part);
void unmake_2_part_limb(part_t *part);
void make_part_absolute(part_t *part);
void make_part_base_relative(part_t *part);
void make_part_relative(part_t *part);
void find_2_part_rot_z(part_t *part);
void check_put_down(part_t *part, int flags);
int look_for_pick_up(actor_t *actor);
int look_for_pick_up_e1(part_t *hand);
void check_pick_up(part_t *part, int16_t param);
actor_t *look_for_a_fight(actor_t *actor);
int find_best_target_up_down(actor_t *actor);
void smart_bomb(actor_t *actor, int range, int damage);
void interpolate_pos(actor_t *actor, int game_time_arg, int value);
void see_if_anything_hit(part_t *part, int16_t hit_param);
void fire_bullet(part_t *part);
void blood_spurt(part_t *part);
void spawn_actor(part_t *part, int actor_index, int action_index, int a4, int a5);
void check_part_hit(part_t *part, int16_t hit_param);
int find_footstep_sound(void);
void play_sound_ecstatica(actor_t *actor, int sound_index, int volume_flags, int direct_volume);
void check_steps(void);
void recover_hit_points(void);
void add_act_to_act_list(act_t *act, actor_t *actor);
void init_act_heap(void);
void make_path(part_t *part);
void fix_part(void);
void unfix_part(void);
void reorient_thing(actor_t *actor);
void likely_target_up_or_down(actor_t *actor, int *out_dir, int *out_updown);
void check_fight_rep(actor_t *actor);

#endif /* MOVE_H */
