#ifndef TOPO_H
#define TOPO_H

#include "types.h"

/* ══════════════════════════════════════════════════════════════
 *  Structure Definitions
 * ══════════════════════════════════════════════════════════════ */

/* ── Profile height ── */
#pragma pack(push, 1)
typedef struct profile_height_s {
    int16_t field_0;
    int16_t field_2;
    int16_t field_4;
    int16_t field_6;
    int16_t field_8;
    int16_t field_A;
    int16_t field_C;
    int16_t field_E;
} profile_height_t;
#pragma pack(pop)

/* ── Bitmap header ── */
#pragma pack(push, 1)
typedef struct bitmap_hdr_s {
    int16_t field_0;
    int16_t field_2;
    int16_t field_4;
    int16_t field_6;
    int16_t size_x;
    int16_t size_y;
    int16_t field_C;
    int16_t field_E;
    int16_t field_10;
    int16_t field_12;
    int16_t field_14;
    int16_t field_16;
    int16_t field_18;
    int16_t field_1A;
    int16_t field_1C;
    int16_t field_1E;
} bitmap_hdr_t;  /* 32 bytes */
#pragma pack(pop)

/* ══════════════════════════════════════════════════════════════
 *  Topo / Terrain Globals (extern)
 * ══════════════════════════════════════════════════════════════ */

extern profile_height_t profile_height;
extern int16_t topography;
extern int16_t editor_mode;
extern int16_t script_mode;
extern int16_t make_backgrounds;
extern int16_t moving_camera;
extern bool no_die;
extern int16_t demo_option;
extern camera_data_t *old_camera;
extern int32_t last_camera_change;
extern int16_t loaded_background[4];
extern int16_t selected_camera;
extern int16_t num_cameras;
extern char palette_control[24];
extern int32_t palette_offset[1200];
extern int32_t visib_offset[1200];
extern int16_t num_arcs;
extern int32_t next_mask_tab_offset;
extern int16_t left_ang[100];
extern int16_t right_ang[100];
extern char *spare_bit_map;
extern char *mask_bit_map;

/* ══════════════════════════════════════════════════════════════
 *  Function Declarations
 * ══════════════════════════════════════════════════════════════ */

void init_profile_heights(void);
int find_map_element(vector_t *pos);
signed int find_map_element_vis(vector_t *pos);
int16_t find_height_now(vector_t *pos, actor_t *actor);
int16_t find_height_now_material(vector_t *pos, actor_t *actor, int *material);
int16_t find_height_now_vis(vector_t *pos);
void update_position(actor_t *actor, vector_t *increment);
void do_update_position(actor_t *actor, vector_t *increment);
void update_velocity(actor_t *actor);
int  do_update_velocity(actor_t *actor, int time_interval);
void flush_backgrounds(void);
int load_raw(void);
char *load_raw_graphic(const char *source, int *size_x, int *size_y);
void save_screen_shot(void);
void load_palette(const char *filename);
void load_visibility_map(void);
void show_topography(void);
void make_mask_map(void);
void swap_xy(int32_t *a, int32_t *b);
int check_block_needs_rendering(void);
int16_t find_def_height(vector_t *pos);
void add_arc(int16_t left_angle, int16_t right_angle_val);
void init_topography(void);
int compare_bitmaps_0_and_1(void);

#endif /* TOPO_H */
