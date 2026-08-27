#ifndef FILE_H
#define FILE_H

#include "types.h"

#pragma pack(push, 1)
struct rephead_s {
    int16_t rep_index;
    int16_t action_slots[208];
    int16_t thing_index;
    int16_t rep_flags;
    struct rephead_s *next_rep;
    uint16_t rep_use_flag;
    int32_t rep_time;
};  /* 432 bytes */
#pragma pack(pop)

extern FILE *file_pointer;
extern FILE *file2_pointer;
extern bool load_by_offset;
extern int32_t file_offsets[MAX_OFFSETS];
extern int32_t number_of_offsets;
extern int16_t file_version;
extern int16_t last_scene_dir;
extern int16_t num_rep_names;

/* Per-type offset arrays for resource loading */
extern int32_t action_offset[ACTION_TAB_SIZE];
extern int32_t scene_offset[SCENE_TAB_SIZE];
extern int32_t actor_offset[THING_TAB_SIZE];
extern int32_t sound_offset[SOUND_TAB_SIZE];
extern int32_t repertoire_offset[REPERTOIRE_TAB_SIZE];

/* Name translation tables (file→global index mapping during load) */
extern int16_t new_part_name[PART_TAB_SIZE];
extern int16_t new_thing_name[THING_TAB_SIZE];
extern int16_t new_action_name[ACTION_TAB_SIZE];
extern int16_t new_scene_name[SCENE_TAB_SIZE];
extern int16_t new_code_name[CODE_TAB_SIZE];
extern int16_t new_rep_name[REPERTOIRE_TAB_SIZE];
extern uint8_t rep_name_flags[REPERTOIRE_TAB_SIZE];
extern uint8_t action_name_flags[ACTION_TAB_SIZE];
extern uint8_t code_name_flags[CODE_TAB_SIZE];
extern uint8_t sound_name_flags[SOUND_TAB_SIZE];
extern uint8_t texture_name_flags[TEXTURE_TAB_SIZE];
extern int16_t new_sound_name[SOUND_TAB_SIZE];
extern int16_t new_map_area_name[MAP_AREA_TAB_SIZE];
extern int16_t new_texture_name[TEXTURE_TAB_SIZE];
extern int16_t new_point_name[POINT_TAB_SIZE];
extern int16_t new_triangle_name[TRIANGLE_TAB_SIZE];
extern rephead_t rep_heap_arr[REP_POOL_SIZE];

void write_parts(actor_t *actor, FILE *f);
void write_an_event(event_t *event, FILE *f);
event_t *read_event(FILE *f);
int32_t getl(FILE *f);
int32_t getlLoHi(FILE *f);
void start_things(void);
void modify_movement(void);
void putl(int32_t val, FILE *f);
int16_t find_part_name(const char *name);
int16_t find_named_code(const char *name);
void delete_thing_name(int index);
actor_t *find_thing_name(const char *name);
int16_t find_thing_name_index(const char *name);
int16_t find_map_area_name_index(const char *name);
int16_t find_rep_name_index(const char *name);
int16_t find_sound_name_index(const char *name);
int16_t find_action_name_index(const char *name);
action_t *find_action_name(const char *name);
scene_t *find_scene_name(const char *name);
int16_t find_scene_name_index(const char *name);
int16_t find_code_name_index(const char *name);
void delete_scene_name(int index);
code_t *find_code_name(const char *name);
rephead_t *find_repertoire_name(const char *name);
sound_t *find_sound_name(const char *name);
texture_t *find_texture_name(const char *name);
int16_t find_point_name(const char *name);
int16_t find_triangle_name(const char *name);
void recursively_calc_rel_offs(part_t *part);
void calc_rel_offset(part_t *part);
void calc_rel_centre(part_t *part);
void print_event(event_t *event, FILE *f);
void print_action(action_t *action, FILE *f);
void set_new_names_to_old(void);
void make_file_name(char *output, const char *name);
void file_write_event(event_t *event, FILE *f);
int get_save_name(int slot, char *buf, int buflen);
int load_saved_thumbnail(int slot, uint8_t *thumb_buf);
void capture_save_thumbnail(void);
void read_actors(FILE *f, int quiet);
void read_actions(FILE *f);
void do_delete_rep(rephead_t *rep);
void read_repertoires(FILE *f);
void read_code(FILE *f);
void read_sounds(FILE *f);
void read_textures(FILE *f);
void open_read_file(const char *name);
void open_read_file2(const char *name);
void merge_a_file(const char *name, int quiet);

/* Port-only preferences (ecstatica.cfg), separate from the save file. */
void load_port_settings(void);
void save_port_settings(void);
void merge_a_file_no_message(const char *name, int quiet);
void load_a_thing(int thing_index);
void merge_sought_file(FILE *f, int quiet);
int16_t add_part_name(const char *name);
int16_t add_thing_name(const char *name);
int16_t add_action_name(const char *name);
int16_t add_scene_name(const char *name);
int16_t add_code_name(const char *name);
int16_t add_repertoire_name(const char *name);
int16_t add_sound_name(const char *name);
int16_t add_map_area_name(const char *name);
int16_t add_texture_name(const char *name);
int16_t add_point_name(const char *name);
int16_t add_triangle_name(const char *name);
void merge_event_names(event_t *event);
void merge_new_map(FILE *f);
void detect_game_version(void);
void read_offsets_file(void);
int16_t find_part_name_index(const char *name);
int16_t find_texture_name_index(const char *name);
int16_t find_point_name_index(const char *name);
int16_t find_triangle_name_index(const char *name);
actor_t *find_named_thing(const char *name);
void calculate_all_relative_offsets(void);
const char *rep1_used(void);
void clear_new_names_and_flags(void);
char *make_file_name_subdir(int index);
char *make_file_dir_name(int index);
void make_dir_if_not_exists(const char *dirname);
char *file_find_action_name(int16_t index);
char *file_find_part_name_str(int16_t index);
char *file_find_point_name_str(int16_t index);
char *file_find_triangle_name_str(int16_t index);
char *file_find_repertoire_name_str(int16_t index);
char *file_find_thing_name(int16_t index);
void write_a_merged_ev(int16_t event_type, int16_t event_index, int16_t *params, FILE *f);
void write_merged_event(event_t *event, FILE *f);
void print_scene(scene_t *scene, FILE *f);
void delete_repertoire_name(int16_t index);
void delete_action_name(int16_t index);
void delete_code_name(int16_t index);

#endif /* FILE_H */
