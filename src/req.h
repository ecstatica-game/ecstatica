#ifndef REQ_H
#define REQ_H

#include "types.h"

#pragma pack(push, 1)
struct gadget_s {
    int16_t X;
    int16_t Y;
    int16_t width;
    int16_t height;
    char *gadget_text;
    int32_t handle_click;
    int16_t flags;
    struct gadget_s *next_gdg;
    int16_t field_16;
    int16_t field_18;
    int16_t field_1A;
    int16_t field_1C;
};
#pragma pack(pop)

struct request_s {
    uint16_t field_0;
    uint16_t field_2;
    int16_t max_length;
    int16_t field_6;
    char *gadget_header_text;
    gadget_t *gadget_list;
    int16_t pixelX;
    int16_t field_12;
    int16_t pixelY;
    int16_t field_16;
    int16_t width;
    int16_t height;
};

extern int16_t active_requester;
extern int16_t req_finished;
extern int16_t esc_req_choice;
extern int32_t settings_choice;
extern int16_t req_choice;
extern int32_t itype;
extern int32_t input_cursor_offset;
extern int32_t return_pressed;
extern int32_t request_error;
extern int16_t gadg_info_num;
extern char input_name_text[260];
extern char string_buffer[52];
extern request_t choice_req;
extern request_t info_req;
extern request_t start_req;
extern request_t game_req;
extern request_t settings_req;
extern request_t progress_req;
extern request_t language_req;
extern request_t difficulty_req;
extern request_t sound_card_req;
extern request_t page_req;
extern request_t install_req;
extern request_t string_req;
extern gadget_t subtitle_gadg;
extern gadget_t music_gadg;
extern gadget_t sound_fx_gadg;
extern gadget_t lod_gadg;
extern gadget_t lo_hi_res_gadg;

void restore_screen(request_t *req);
request_t *find_active_req(void);
void restore_active_req(void);
void request(request_t *req);
void check_gadget(request_t *req, gadget_t *gadget);
void clear_requester(void);
void show_gadget(gadget_t *gadget);
void do_input(void);
void load_saved_screen(void);
void handle_cancel(void);
void handle_ok(void);
int do_choice_req(const char *msg);
void do_choice_page_req(void);
void do_choice2_req(void);
void do_info_req(const char *msg);
void do_info2_req(const char *msg1, const char *msg2);
void do_info3_req(const char *msg1, const char *msg2, const char *msg3);
void do_info_page_req(void);
void do_input_req(char *buffer, int buflen, const char *prompt);
void do_request(void);
void init_gadget(gadget_t *gadget, int16_t x, int16_t y, int16_t w,
                 int16_t h, char *text, void (*handler)(void),
                 int16_t flags, gadget_t *next);
void init_gadgets(void);
void install_to_disk(void);

/* Handler functions */
void handle_dir_gadg(void);
void handle_play_female(void);
void handle_play_male(void);
void handle_configure(void);
void handle_load_game(void);
void handle_quit_to_dos(void);
void handle_save_game(void);
void handle_set_card(void);
void handle_set_install(void);
void handle_subtitle(void);
void handle_language(void);
void handle_music(void);
void handle_sound_fx(void);
void handle_diff_gadg(void);
void handle_lo_hi_res(void);
void handle_difficulty(void);

/* Gadget text update */
void do_subtitle_gadg_text(void);
void do_music_gadg_text(void);
void do_sound_fx_gadg_text(void);
void do_lod_gadg_text(void);
void do_lo_hi_res_gadg_text(void);

/* Gadget refresh */
void refresh_gadgets(gadget_t *gadget);
void refresh_request_res(void);

/* Choice dialogs */
void do_choice3_req(const char *msg1, const char *msg2, const char *msg3);

#endif /* REQ_H */
