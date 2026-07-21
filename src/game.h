#ifndef GAME_H
#define GAME_H

#include "types.h"
#include "anim.h"  /* action_t full definition needed by script_t */

/* ══════════════════════════════════════════════════════════════
 *  Structure Definitions
 * ══════════════════════════════════════════════════════════════ */

/* ── Name text ── */
#pragma pack(push, 1)
typedef struct name_text_s {
    char field_0[26];
} name_text_t;
#pragma pack(pop)

/* ── Token (for script parsing) ── */
#pragma pack(push, 1)
typedef struct token_s {
    int16_t token_code;
    char token_text[21];
} token_t;  /* 23 bytes */
#pragma pack(pop)

/* ── Act entry (per-actor animation state) ── */
#pragma pack(push, 1)
struct act_s {
    action_t *act_action;
    int16_t duration;
    uint16_t key_progress;
    key_t *actor_keys_list;
    uint16_t flags;
    int16_t anim_param;
    int16_t loop_count;
    struct act_s *next;
};  /* 22 bytes */
#pragma pack(pop)

/* ── Actor entry (the main entity struct, 396 bytes) ── */
#pragma pack(push, 1)
struct actor_s {
    /* Part-like core (first 80 bytes match part_t layout) */
    int16_t name_index;                        /* 0x00 */
    uint16_t flags;                            /* 0x02 */
    int16_t type;                              /* 0x04 */
    matrix3x3_t matrix_1;                      /* 0x06 */
    vector_t joint_position;                   /* 0x18 */
    part_t *actor_parts_list;                  /* 0x1E */
    struct actor_s *parent_actor;              /* 0x22 */
    vector_t Rotate;                           /* 0x26 */
    vector_t Offset;                           /* 0x2C */
    matrix3x3_t matrix_2;                      /* 0x32 */
    struct actor_s *holding_actor;             /* 0x44 */
    part_t *next_in_path;                      /* 0x48 */
    struct actor_s *next_in_display_list;      /* 0x4C */

    /* Actor-specific fields */
    struct actor_s *next_thing1;               /* 0x50 */
    part_tab_t *_PartTab;                      /* 0x54 */
    vector_t actor_velocity;                   /* 0x58 */
    int16_t field_5E;                          /* 0x5E */
    int32_t field_60;                          /* 0x60 */
    vector_t previous_position;                /* 0x64 */
    int32_t field_6A;                          /* 0x6A */
    vector_t field_6E_vect;                    /* 0x6E */
    int32_t field_74;                          /* 0x74 */
    int32_t field_78;                          /* 0x78 */
    int32_t field_7C;                          /* 0x7C */
    int16_t field_80;                          /* 0x80 */
    int16_t actor_behavior;                    /* 0x82 */
    vector_t position_vector;                  /* 0x84 */
    vector_t actor_center;                     /* 0x8A */
    vector_t rotate_vector;                    /* 0x90 */
    int16_t move_type;                         /* 0x96 */
    int16_t wander_direction;                  /* 0x98 */
    int16_t actor_box_size;                    /* 0x9A */
    vector_t start_position;                   /* 0x9C */
    act_t *actor_act_list;                /* 0xA2 */
    act_t actor_act;                      /* 0xA6 */
    part_t *field_BC;                          /* 0xBC */
    vector_t field_C0;                         /* 0xC0 */
    matrix3x3_t matrix33_2;                    /* 0xC6 */
    tri_t *polygone_tri_list;             /* 0xD8 */
    triangle_tab_t *_TriangleTab;              /* 0xDC */
    point_tab_t *_PointTab;                    /* 0xE0 */
    subarea_t *area_to_clear;                  /* 0xE4 */
    int16_t action_delay;                      /* 0xE8 */
    int16_t range_threshold;                   /* 0xEA */
    int16_t actor_hitpoints;                   /* 0xEC */
    int16_t full_actor_hp;                     /* 0xEE */
    int16_t action_state;                      /* 0xF0 */
    part_t *part_heap_link;                    /* 0xF2 */
    vector_t held_offset;                      /* 0xF6 */
    vector_t held_rotate;                      /* 0xFC */
    vector_t held_off_left;                    /* 0x102 */
    vector_t held_rot_left;                    /* 0x108 */
    int16_t field_10E;                         /* 0x10E */
    int16_t field_110;                         /* 0x110 */
    subarea_t bounding_box;                    /* 0x112 */
    int32_t time_actor;                        /* 0x11A */
    rephead_t *actor_reperture;                /* 0x11E */
    int16_t actor_rep_index;                   /* 0x122 */
    int16_t default_repert;                    /* 0x124 */
    action_t *force_action_to_execute;         /* 0x126 */
    action_t *queued_action;                   /* 0x12A */
    struct actor_s *target_actor;              /* 0x12E */
    scene_t *actor_scene;                      /* 0x132 */
    int16_t code_at_hp_change;                 /* 0x136 */
    int16_t actor_hit_code;                    /* 0x138 */
    int16_t actor_init_code;                   /* 0x13A */
    int16_t picked_up_code;                    /* 0x13C */
    int16_t dead_code_index;                   /* 0x13E */
    int16_t event_timer;                       /* 0x140 */
    int16_t action_variant;                    /* 0x142 */
    int16_t extra_action_index;                /* 0x144 */
    int16_t state_flags;                       /* 0x146 */
    int32_t field_148;                         /* 0x148 */
    int32_t field_14C;                         /* 0x14C */
    int16_t hold_timer;                        /* 0x150 */
    vector_t target_position;                  /* 0x152 */
    int16_t interact_timer;                    /* 0x158 */
    int16_t field_15A;                         /* 0x15A */
    int32_t field_15C;                         /* 0x15C */
    int16_t field_160;                         /* 0x160 */
    int16_t end_action_index;                  /* 0x162 */
    int16_t hit_angle;                         /* 0x164 */
    int16_t hit_type;                          /* 0x166 */
    int16_t action_index;                      /* 0x168 */
    int16_t interact_target_index;             /* 0x16A */
    int16_t interact_state;                    /* 0x16C */
    int16_t interact_cooldown;                 /* 0x16E */
    int16_t move_direction;                    /* 0x170 */
    int16_t last_actor_direction;              /* 0x172 */
    int16_t actor_Speed_factor;                /* 0x174 */
    int16_t field_176;                         /* 0x176 */
    int32_t field_178;                         /* 0x178 */
    taction_t *tactions_list;                  /* 0x17C */
    int16_t actor_hit_factor;                  /* 0x180 */
    int16_t actor_strength_factor;             /* 0x182 */
    int16_t actor_magic;                       /* 0x184 */
    int16_t actor_magic_factor;                /* 0x186 */
    int16_t magic_stop_action;                 /* 0x188 */
    int16_t spawner_index;                     /* 0x18A */
};  /* 396 bytes */
#pragma pack(pop)

/* ── Scene entry ── */
#pragma pack(push, 1)
struct scene_s {
    int16_t scene_index;
    int16_t camera_index;
    script_t *scene_script_list;
    struct scene_s *scene_next;
    int16_t scene_music_index;
    int16_t action_indices[18];
    char scene_name_buf[100];
    int16_t scene_use_flag;
    int16_t scene_code_2;
    int16_t scene_code_index;
    struct scene_s *next_scene;
    int32_t scene_time;
    int16_t scene_camera;
    int16_t last_scene_direction;
};
#pragma pack(pop)

/* ── Script entry ── */
#pragma pack(push, 1)
struct script_s {
    int16_t script_actor_index;
    action_t script_action;
    struct script_s *next_script;
    char script_use_flag;
};  /* 29 bytes */
#pragma pack(pop)

/* ── Line of code (script text) ── */
#pragma pack(push, 1)
struct line_of_code_s {
    char field_0[53];
    struct line_of_code_s *next_line_code;
};
#pragma pack(pop)

/* ── Code structure ── */
#pragma pack(push, 1)
struct code_s {
    int16_t index_code;
    line_of_code_t *text_line_of_code;
    int32_t token_store_index;
    struct code_s *next_code;
};
#pragma pack(pop)

/* ── Timed action ── */
#pragma pack(push, 1)
struct taction_s {
    int16_t taction_index;
    int32_t taction_time;
    struct taction_s *next;
};  /* 10 bytes */
#pragma pack(pop)

/* ══════════════════════════════════════════════════════════════
 *  Game State Globals (extern)
 * ══════════════════════════════════════════════════════════════ */

extern bool game_up_and_running;
extern bool program_up_and_running;
extern int32_t game_timer;
extern int32_t game_timer_start;
extern int32_t game_time;
extern int32_t interval;
extern int32_t x_time;
extern int32_t break_do_movement;
extern int32_t num_info_lines;
extern int32_t key_esc_was_forced;
extern bool intro_flag;
extern bool female;
extern int16_t difficulty;
extern int16_t language;
extern bool stop_the_clock;
extern bool no_wanderers;
extern int16_t no_icons;
extern bool slow_motion;
extern int16_t kill_count;
extern int16_t treasure_count;
extern int16_t map_count;
extern int16_t armour_factor;
extern int32_t poison_time;
extern int16_t hero_material;
extern int32_t mode_svga;
extern int32_t chosen_svga;
extern int32_t select_flag;
extern int32_t eagle_card;
extern int16_t height_shift;
extern bool fade_to_black;
extern bool fade_to_white;
extern bool fade_in;
extern int32_t fade_start;
extern int32_t fade_time;
extern int32_t check_time;
extern int16_t last_fade_factor;
extern int32_t lightning;
extern actor_t *root_thing;
extern actor_t *thing_list;
extern actor_t *stuck_thing_list;
extern actor_t *selected_thing;
extern actor_t *source_thing;
extern action_t *selected_action;
extern scene_t *selected_scene;
extern int16_t last_actor_dir;
extern action_t *action_list;
extern scene_t *root_scene;
extern scene_t *scene_list;
extern code_t *code_list;
extern rephead_t *repertoire_list;
extern sound_t *sound_list;
extern map_area_t *map_area_list;
extern texture_t *texture_list;
extern point_t *point_list;
extern tri_t *triangle_list;
extern script_t *script_list;
extern actor_t *thing_tab[THING_TAB_SIZE];
extern action_t *action_tab[ACTION_TAB_SIZE];
extern scene_t *scene_tab[SCENE_TAB_SIZE];
extern code_t *code_tab[CODE_TAB_SIZE];
extern rephead_t *repertoire_tab[REPERTOIRE_TAB_SIZE];
extern sound_t *sound_tab[SOUND_TAB_SIZE];
extern map_area_t *map_area_tab[MAP_AREA_TAB_SIZE];
extern texture_t *texture_tab[TEXTURE_TAB_SIZE];
extern actor_t actor_heap_arr[ACTOR_POOL_SIZE];
extern part_tab_t part_tab_heap_arr[ACTOR_POOL_SIZE];
extern triangle_tab_t triangle_tab_heap_arr[ACTOR_POOL_SIZE];
extern point_tab_t point_tab_heap_arr[ACTOR_POOL_SIZE];
extern scene_t scene_heap_arr[SCENE_POOL_SIZE];
#define ACT_SIZE         50

extern script_t script_arr[SCRIPT_SIZE];
extern taction_t taction_heap_arr[TACTION_POOL_SIZE];
extern act_t act_arr[ACT_SIZE];
extern name_text_t *thing_names;
extern name_text_t *action_names;
extern name_text_t *scene_names;
extern name_text_t *code_names;
extern name_text_t *repertoire_names;
extern name_text_t *sound_names;
extern name_text_t *part_names;
extern name_text_t *point_names;
extern name_text_t *triangle_names;
extern name_text_t *map_area_names;
extern name_text_t *texture_names;
extern int16_t thing_name_flags[THING_TAB_SIZE];
extern int16_t scene_name_flags[SCENE_TAB_SIZE];
extern int16_t actor_rep_name[THING_TAB_SIZE];
extern int16_t actor_magic[THING_TAB_SIZE];
extern int16_t actor_hit_points[THING_TAB_SIZE];
extern vector_t actor_position[THING_TAB_SIZE];
extern vector_t actor_orientation[THING_TAB_SIZE];
extern int16_t actor_last_act[THING_TAB_SIZE];
extern int16_t actor_held_by_part[THING_TAB_SIZE];
extern int16_t actor_held_by_actor[THING_TAB_SIZE];
extern int16_t actor_flags[THING_TAB_SIZE];
extern int16_t *token_store;
extern int32_t top_of_tokens;
extern token_t tokens_table[];
extern bool show_rate;
extern bool develop_mode;
extern bool is_god_mode;
extern int16_t saved_game_num;
extern int32_t extra_life_time;

/* ══════════════════════════════════════════════════════════════
 *  Function Declarations
 * ══════════════════════════════════════════════════════════════ */

/* Actor management */
actor_t *load_wanderer(int base_type, int num_variation);
void try_to_add_actor_to_world(void);
void remove_actor_from_world(actor_t *actor);
void check_encounter(void);
void invalidate_drawn_graphics(void);
void remove_all_graphics(void);
int  check_if_structure(actor_t *actor);

/* Script execution */
void skip_to_matching_endif(int16_t **pp);
void skip_to_matching_if_type(int16_t **pp);
int  execute_boolean(int16_t **pp, actor_t *actor);
void execute_code(code_t *code, actor_t *actor);
void execute_thing_code(actor_t *actor, int16_t code_index);
void execute_part_code(part_t *part, int16_t code_index);
void execute_sub_obj_code(void);
void do_execute_code(code_t *code, actor_t *actor);
void tokenize_code(code_t *code);
int  get_next_char(void);
int  get_next_token(void);

/* Action management */
void force_action(actor_t *actor, action_t *action, int set_some_flag);
int  check_action_loaded(int16_t index);
int  check_action_loaded_no_msg(int16_t index);
void check_actor_loaded_by_index(int16_t actor_index);
int  check_rep_loaded(int16_t index);
void div_vector(vector_t *v, int16_t divisor);
void free_taction(taction_t *taction);
void check_sound_loaded(int16_t sound_index);
void search_scene_dirs_and_load(const char *name);
void search_rep_dirs_and_load(const char *name);
void search_actor_dirs_and_load(const char *name);
void search_action_dirs_and_load(const char *name);
int  check_actor_loaded(const char *name);
void check_actors_in_scene_loaded(scene_t *scene);
void check_scene_loaded(int16_t scene_index);
int  check_scene_ok_to_start(scene_t *scene);
void start_scene(scene_t *scene);
int  check_texture_loaded(const char *name);

/* Graphics subsystem */
void init_graphics(void);
void delete_code(code_t *code);
void remove_scene(scene_t *scene);
void remove_actor(actor_t *actor);
void find_memory_for_sound(void);
void remove_sound(sound_t *sound);
void find_memory_for_texture(void);
void remove_texture(texture_t *texture);
void remove_rep(rephead_t *rep);
void remove_action(action_t *action);
void free_all_heaps(void);
void try_to_remove_scene(void);
void try_to_remove_scene_or_action(void);
void try_to_remove_action(void);
void try_to_remove_rep(void);
void try_to_remove_actor(void);
void try_to_remove_sound(void);
void try_to_remove_texture(void);

/* Heap allocators */
void free_event(event_t *event);
event_t *find_free_event(void);
event_t *look_for_free_event(void);
void free_action(action_t *action);
action_t *find_free_action(void);
void free_script(script_t *script);
script_t *find_free_script(void);
actor_t *find_free_actor(void);
void free_part(part_t *part);
part_t *find_free_part(void);
void free_scene(scene_t *scene);
scene_t *find_free_scene(void);
void free_rep(rephead_t *rep);
rephead_t *find_free_rep(void);
void free_point(point_t *point);
point_t *find_free_point(void);
tri_t *find_free_tri(void);
void free_key(key_t *key);
key_t *find_free_key(void);
void free_sound(sound_t *sound);
sound_t *find_free_sound(void);
texture_t *find_free_texture(void);
void free_t_action(taction_t *ta);
taction_t *find_free_t_action(void);

/* Save/Load */
void save_game(int slot);
void load_game(int slot);
void save_matrix(matrix3x3_t *m, FILE *f);
void load_matrix(matrix3x3_t *m, FILE *f);
void save_vector(int16_t *vec, FILE *f);
void load_vector(int16_t *vec, FILE *f);
void save_word(int16_t *val, FILE *f);
void load_word(int16_t *val, FILE *f);
int check_saved_game(int slot);
void remove_all_sounds(void);
void remove_all_textures(void);
void check_lightning(void);
void save_size_of_heaps(void);

/* Graphics overlay */
void draw_graphics(void);
void clear_graphics(void);
void load_a_graphic(const char *name);
void put_a_graphic(const char *name, int pos_x, int pos_y, int intro_graphic);
void put_a_temp_graphic(const char *name, int x, int y);
void clear_a_graphic(const char *name);

/* Icons / HUD */
void update_game_icons(void);
void clear_game_icons(void);
void show_icon_page(void);
void put_number(int value, int x, int y);
void adjust_magic(actor_t *actor, int amount);
void draw_life_bar(void);
void draw_magic_bar(void);
void draw_weapon_magic(void);
void check_hero_rep(void);
void play_ambients(void);

/* Fade effects */
void check_fade(void);
void do_fade_to_black(int fade_factor);
void do_fade_to_white(int fade_factor);
void do_fade_in(void);
void force_timed(int actor_index, int taction_index, int ticks);

#endif /* GAME_H */
