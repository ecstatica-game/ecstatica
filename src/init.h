#ifndef INIT_H
#define INIT_H

#include "types.h"

#pragma pack(push, 1)
typedef struct config_s {
    char name[12];
    char Cdrom_path;
    char femaleF;
    char InstallType;
    char sound_driver;
    char SoundCard;
    char SoundCardIOAddrl;
    char SoundCardIOAddr;
    char SoundCardDMA;
    char SoundCardIRQ;
    char language;
    char views;
    char field_17;
    char field_18;
    char field_19;
    char field_1A;
    char field_1B;
    char field_1C;
    char field_1D;
    char field_1E;
    char field_1F;
} config_t;  /* 32 bytes */
#pragma pack(pop)

extern char keys_pressed[256];
extern char keys_were_pressed[256];
extern char extra_keys_pressed[256];
extern char extra_keys_were_pressed[256];
extern char keys_were_pressed_codes[256];
extern int16_t mouse;
extern int16_t mouse_x;
extern int16_t mouse_y;
extern bool space_pressed;
extern bool space_was_pressed;
extern bool enter_was_pressed;
extern bool enter_pressed;
extern bool ctrl_pressed;
extern bool alt_pressed;
extern bool key1_pressed, key2_pressed, key3_pressed, key4_pressed;
extern bool key5_pressed, key6_pressed, key7_pressed, key8_pressed, key9_pressed;
extern bool key_esc_was_pressed;
extern bool key_i_was_pressed;
extern bool key_return_was_pressed;
extern int16_t joystick;
extern bool joystick_control;
extern int16_t joy_button;
extern int16_t movement_speed_mode;
extern int16_t mcursor_x;
extern int16_t mcursor_y;
extern bool mouse_pointer_on;
extern char mouse_buffer[2][8][8];
extern int16_t sine_table[65536];
extern int16_t cosn_table[65536];
extern int16_t arcsin_tab[32768];
extern int8_t atan_tab0[256];
extern int8_t atan_tab1[256];
extern char shade_tab[17][128][128];
extern char shade_texture[8][128][128];
extern char shadow_tab[3][16][256];
extern char shade_map[128][128];
extern int16_t profile[128][128];
extern int16_t material_flags[30];
extern int16_t event_type_flags[80];
extern int16_t event_priority[80];
extern palette_entry_t view_cmap[256];
extern palette_entry_t colour_map[256];
extern palette_entry_t all_black_cmap[256];
extern palette_entry_t spare_cmap[256];
extern palette_entry_t fade_cmap[256];
extern palette_entry_t edit_map_cmap[256];

/* setup & initialization */
void setup(void);
void init(void);
void setup_directory_paths(void);
void set_up_sound_driver(void);
void set_up_bitmaps(void);
void setup_long_screen(void);
void setup_hi_res_long_screen(void);
void load_logo(const char *file_name);
void load_def_palette(void);
void load_background_title(void);
void load_motion_file(void);

/* Quit / shutdown */
void quit(const char *info);
void quit2(const char *msg1, const char *msg2);

/* Sound */
void stop_samples(void);
void start_playing_sample(sound_t *sound, int a2, int volume);
int  start_playing_sound(void);
void start_recording(void);
int  find_recorded_len(void);
void load_tune(int sound_index);
void select_sound_card_win95(void);
void select_sound_card(void);

/* Display timing */
void wait_vert_blank(void);
void select_video_page(void);

/* Math table init */
void fill_in_sin_tables(void);
void fill_in_shadow_tab(void);
void load_shadow_tab(void);
int  load_anti_alias(void);
int  load_hires_path(void);

/* Pointer table / flags init */
void clear_ptr_tabs(void);
void init_material_flags(void);
void init_event_type_flags(void);

/* Binary I/O helpers */
int16_t getw_be(FILE *f);
void    putw_be(int16_t val, FILE *f);
int16_t putwLoHi(int16_t val, FILE *f);
int16_t getwLoHi(FILE *f);

/* Blit / drawing primitives */
void clip_blit(int src_plane, int src_x, int src_y, int dst_plane,
              int dst_x, int dst_y, int width, int height, int minterm);
void clip_blit_win95(int src_plane, int src_x, int src_y, int dst_plane,
                   int dst_x, int dst_y, int width, int height, int minterm);
void old_clip_blit(int src_plane, int src_x, int src_y, int dst_plane,
                 int dst_x, int dst_y, int width, int height, int minterm);
void put_graphic(char *data, int plane, int x, int y, int sx, int sy);
void put_graphic_win95(char *data, int plane, int x, int y, int sx, int sy);
void clear_background(int plane, int x, int y, int sx, int sy);
void clear_background_win95(int plane, int x, int y, int sx, int sy);

/* Mask operations */
void clip_mask(int src, int dst, int x, int y, int sx, int sy);

/* text rendering */
void text(int plane, const char *text, int length);
void small_text_win95(int plane, const char *text, int length);
void anti_aliased_text(int plane, const char *text, int length);
int  convert_ascii(int ascii_code);
void text_with_mask(int plane, const char *text, int length);
void text_with_mask_win95(int plane, const char *text, int length);
void text_with_mask_scaled(int plane, const char *text, int length, int scale);

/* Rectangle fill */
void rect_fill(int plane, int x, int y, int w, int h);
void rect_fill_win95(int plane, int x, int y, int w, int h);

/* Palette */
void set_palette(palette_entry_t *new_palette);
void show_timer(int timer_val, char reset, int unused);

/* Line drawing */
void move_pen(int indx, int16_t x, int16_t y);
void draw(int plane, int x, int y);
void draw_win95(int plane, int x, int y);

/* Pixel operations */
void write_pixel(int plane, int x, int y, int not_used);
int  read_pixel(int plane, int not_used, int x, int y);
void xor_pixel(int plane, int x, int y);

/* Input */
void add_keyboard_handler(void);
void get_mouse(void);
void get_joystick(void);
void clear_keys_pressed(void);

/* mouse cursor */
void turn_mouse_pointer_on(void);
void turn_mouse_pointer_off(void);
void draw_mouse_cursor(void);
void draw_mouse_cursor_win95(void);
void draw_db_mouse_cursor_win95(void);
void clear_mouse_cursor_win95(void);
void clear_db_mouse_cursor_win95(void);

/* Color / shade */
void init_colours0to8(palette_entry_t *palette);
void expand_colour_map_a_bit(void);
void load_shade_map(void);

/* Timing */
int32_t biostime(void);
int32_t my_time(void);

/* Clear operations */
void clear_mask_rect(int mask, int x, int y, int w, int h);

/* Pack/unpack views */
void pack_bitmap(char *output, char *input);
void pack_mask(int16_t *output, char *input);

/* Misc init */
void set_load_by_offset(void);
void reverse_char_word(void);
void shift_char_word(void);
int16_t reverse_char_word_val(int16_t val);
void change_character_set(void);
void change_subtitles(void);
void change_text_jap(void);
void set_up_sub_directories(void);
void check_directories(void);
void if_editor_show_cursor(void);

/* SVGA stubs (bank-switched video, not used in SDL port) */
void clear_background_svga(int plane, int x, int y, int sx, int sy);
void rect_fill_svga(int plane, int x, int y, int w, int h);
void text_svga(int plane, const char *text, int length);
void clip_blit_svga(int src_plane, int src_x, int src_y, int dst_plane,
                    int dst_x, int dst_y, int width, int height, int minterm);
void draw_mouse_cursor_svga(void);
void draw_db_mouse_cursor_svga(void);
void clear_mouse_cursor_svga(void);
void clear_db_mouse_cursor_svga(void);
void load_backmask(void);

/* DD lock (platform-specific, now stubs) */
char *dd_lock(int plane, int *pitch);
void dd_unlock(int plane, char *data);

#endif /* INIT_H */
