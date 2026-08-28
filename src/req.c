/**
 * req.c
 *
 * Request/dialog system: requester panels, input gathering,
 * file requester, yes/no dialogs, game over screens.
 * 58 functions prefixed with req_ in the original ASM.
 */

#include "req.h"
#include "display.h"
#include "edit.h"
#include "game.h"
#include "init.h"
#include "menu.h"
#include "platform.h"
#include "music.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int16_t active_requester = 0;
int16_t req_finished = 0;
int16_t esc_req_choice = 0;
int32_t settings_choice = 0;
int16_t req_choice = 0;
int32_t itype = 0;
int32_t input_cursor_offset = 0;
int32_t return_pressed = 0;
int32_t request_error = 0;
int16_t gadg_info_num = 0;
char input_name_text[260] = {0};
char string_buffer[52] = {0};
request_t choice_req = {0};
request_t info_req = {0};
request_t start_req = {0};
request_t game_req = {0};
request_t settings_req = {0};
request_t progress_req = {0};
request_t language_req = {0};
request_t difficulty_req = {0};
request_t sound_card_req = {0};
request_t page_req = {0};
request_t install_req = {0};
request_t string_req = {0};

#define MAX_SUBTITLES 20

gadget_t subtitle_gadg = {0};
gadget_t music_gadg = {0};
gadget_t sound_fx_gadg = {0};
gadget_t lod_gadg = {0};
gadget_t lo_hi_res_gadg = {0};

void draw_gadget(gadget_t *gadget);
static void refresh_request(request_t *req);

static request_t active_request;
UNUSED_ATTR static int request_result = 0;
static int32_t input_in_progress = 0;
static gadget_t *input_gadg = NULL;

/* req_find_active_req  E2: 0x43AE2C */
request_t *find_active_req(void) {
    switch (active_requester) {
    case 0x14: return &choice_req;
    case 0x15: return &info_req;
    case 0x27:
    case 0x28: return &start_req;
    case 0x2A: return &game_req;
    case 0x2C: return &string_req;
    case 0x2D: return &sound_card_req;
    case 0x2F: return &page_req;
    case 0x30: return &install_req;
    case 0x31: return &settings_req;
    case 0x32: return &language_req;
    case 0x38: return &progress_req;
    case 0x39: return &difficulty_req;
    default:   return NULL;
    }
}

/* req_show_gadget  E2: 0x43B93C — draws gadget within active request context */
void show_gadget(gadget_t *gadget) {
    if (!gadget) return;
    request_t *req = find_active_req();
    if (!req) return;

    int text_w = 0;
    if (gadget->gadget_text)
        text_w = (int)strlen(gadget->gadget_text) * tx_w;
    if (gadget->width == 0)
        gadget->width = (int16_t)text_w;

    int16_t base_x = (gadget->X >= 0) ? req->pixelX : (int16_t)(req->pixelX + req->width);
    gadget->field_16 = base_x + gadget->X;

    int16_t base_y = (gadget->Y >= 0) ? req->pixelY : (int16_t)(req->pixelY + req->height);
    gadget->field_1A = base_y + gadget->Y;
    gadget->field_1C = gadget->field_1A + gadget->height;
    gadget->field_18 = gadget->field_16 + gadget->width;

    if (gadget->flags & (int16_t)0x8000) {
        a_pen_colour = 10;
        b_pen_colour = 15;
    } else {
        a_pen_colour = 15;
        rect_fill(db, gadget->field_16, gadget->field_1A,
                  gadget->field_18 - 1, gadget->field_1C - 1);

        a_pen_colour = (gadget->flags & 1) ? 14 : 8;
        move_pen(db, gadget->field_16, gadget->field_1A - 1);
        draw(db, gadget->field_18 - 1, gadget->field_1A - 1);
        if (!(gadget->flags & 4))
            draw(db, gadget->field_18 - 1, gadget->field_16 + 1);

        a_pen_colour = (gadget->flags & 1) ? 8 : 14;
        move_pen(db, gadget->field_16, gadget->field_16);
        draw(db, gadget->field_16, gadget->field_1A);
        if (!(gadget->flags & 2))
            draw(db, gadget->field_1A, gadget->field_16 - 1);

        a_pen_colour = 10;
        b_pen_colour = 15;
    }

    if (gadget->flags & 8)
        a_pen_colour = 8;
    if (gadget->flags & 0x80)
        a_pen_colour = 14;

    if (gadget->gadget_text) {
        int text_x;
        if (gadget->flags & 0x40)
            text_x = gadget->field_16 + 4;
        else
            text_x = gadget->field_16 + (gadget->width - text_w) / 2;
        int text_y = gadget->field_1A + (gadget->height - tx_h) / 2;
        move_pen(db, (int16_t)text_x, (int16_t)text_y);
        text(db, gadget->gadget_text, 0);
    }
}

/* req_do_request_42A0B8 — opens in-game pause menu/requester */
void do_request(void) {
    int saved_db = db;

    do_main_menu();

    db = saved_db;
    background_status = 2;
}

/* req_open_requester  E1: ? | E2P: 0x42A188 */
void open_requester(request_t *req) {
    if (!req) return;

    /* Draw requester panel background */
    int panel_x = (screen_width - req->width) / 2;
    int panel_y = (screen_height - req->height) / 2;

    /* White fill + 3D bevel border (matches req_request_43B384) */
    a_pen_colour = 15;
    rect_fill(db, panel_x, panel_y, req->width, req->height);

    /* 3D bevel: top-left dark (8), bottom-right bright (14) */
    a_pen_colour = 8;
    move_pen(db, (int16_t)panel_x, (int16_t)(panel_y + req->height - 1));
    draw(db, panel_x, panel_y);
    draw(db, panel_x + req->width - 1, panel_y);

    a_pen_colour = 14;
    move_pen(db, (int16_t)(panel_x + req->width - 1), (int16_t)(panel_y + 1));
    draw(db, panel_x + req->width - 1, panel_y + req->height - 1);
    draw(db, panel_x + 1, panel_y + req->height - 1);

    /* Title: dark text on white (a=8, b=15) */
    if (req->gadget_header_text && req->gadget_header_text[0]) {
        a_pen_colour = 8;
        int title_len = (int)strlen(req->gadget_header_text);
        move_pen(db, (int16_t)(panel_x + (req->width - title_len * tx_w) / 2),
                (int16_t)(panel_y + tx_h + 2));
        text(db, req->gadget_header_text, title_len);
    }
}

/* req_close_requester  E1: ? | E2P: 0x42A258 */
void close_requester(void) {
    /* Restore screen under requester from background */
    clear_background(db, 0, 0, screen_width, screen_height);
}

/* req_restore_screen  E2: 0x43AD40 */
void restore_screen(request_t *req) {
    clip_blit(1, req->pixelX, req->pixelY, 1 - db, req->pixelX, req->pixelY,
              req->width, req->height, 0);
    clip_blit(3, 0, 0, 2, 0, 0, screen_width, screen_height, 0xC0);
    clip_mask(2, 1, 0, 0, screen_width, screen_height);
    clip_mask(2, 0, 0, 0, screen_width, screen_height);
    background_status = 2;
    for (actor_t *a = root_thing; a; a = a->next_in_display_list)
        a->flags &= ~ACTOR_FLAG_VISIBLE;
    for (int i = 0; i < 20; i++) {
        if (subtitle_status[i] == 2)
            subtitle_status[i] = 1;
    }
}

/* req_check_gadget  E2: 0x43B688 */
void check_gadget(request_t *req, gadget_t *gadget) {
    if (mouse_x < gadget->field_16 || mouse_x >= gadget->field_18 ||
        mouse_y < gadget->field_1A || mouse_y >= gadget->field_1C)
        return;

    if ((gadget->flags & 0x1000) && gadget->gadget_text) {
        input_in_progress = 1;
        input_gadg = gadget;
        show_gadget(gadget);
        return;
    }

    if (gadget->flags & 0x2000) {
        restore_screen(req);
        req_finished = 1;
    }

    if (gadget->handle_click) {
        void (*handler)(void) = (void (*)(void))(intptr_t)gadget->handle_click;
        handler();
    }
    show_gadget(gadget);
}

/* req_request  E2: 0x43B304 */
void request(request_t *req) {
    stop_the_clock = true;

    if (!program_up_and_running)
        set_palette(colour_map);

    turn_mouse_pointer_on();

    req->width = req->max_length;
    req->height = req->field_6;

    if ((int16_t)req->field_0 >= 0) {
        req->pixelX = (int16_t)req->field_0;
    } else {
        req->pixelX = (screen_width - req->width) / 2;
    }
    if ((int16_t)req->field_2 >= 0) {
        req->pixelY = (int16_t)req->field_2;
    } else {
        req->pixelY = (screen_height - req->height) / 2;
    }

    req->field_12 = req->pixelX + req->width;
    req->field_16 = req->pixelY + req->height;

    clip_blit(1 - db, req->pixelX, req->pixelY, 1, req->pixelX, req->pixelY,
              req->width, req->height, 0);

    a_pen_colour = 15;
    rect_fill(db, req->pixelX, req->pixelY, req->field_12 - 1, req->field_16 - 1);

    draw_mode[db] = 1;

    a_pen_colour = 8;
    move_pen(db, req->pixelX, (int16_t)(req->field_16 - 1));
    draw(db, req->pixelX, req->pixelY);
    draw(db, req->field_12 - 1, req->pixelY);

    a_pen_colour = 14;
    move_pen(db, (int16_t)(req->field_12 - 1), (int16_t)(req->pixelY + 1));
    draw(db, req->field_12 - 1, req->field_16 - 1);
    draw(db, req->pixelX + 1, req->field_16 - 1);

    if (req->gadget_header_text && req->gadget_header_text[0]) {
        int title_len = (int)strlen(req->gadget_header_text);
        int text_w = title_len * tx_w;
        int x_center = req->pixelX + (req->width - text_w + 1) / 2;
        int y_pos = req->pixelY + tx_h + 2;
        a_pen_colour = 8;
        b_pen_colour = 15;
        move_pen(db, (int16_t)y_pos, (int16_t)x_center);
        text(db, req->gadget_header_text, 0);
    }

    for (gadget_t *g = req->gadget_list; g; g = g->next_gdg)
        show_gadget(g);

    req_finished = 0;
    input_in_progress = 0;

    while (!req_finished) {
        get_mouse();

        if (mouse & 2) {
            if (input_in_progress) {
                input_in_progress = 0;
                show_gadget(input_gadg);
            }
            for (gadget_t *g = req->gadget_list; g; g = g->next_gdg) {
                check_gadget(req, g);
            }
        } else {
            clear_keys_pressed();
        }
    }

    turn_mouse_pointer_off();
    if (active_requester != 0x38)
        active_requester = 0;
}

/* req_yes_no_requester  E1: ? | E2P: 0x42A328 */
int yes_no_requester(const char *question) {
    request_t req;
    memset(&req, 0, sizeof(req));
    req.width = 300;
    req.height = 80;
    req.gadget_header_text = (char *)question;

    open_requester(&req);

    /* Draw Yes/No buttons */
    int panel_x = (screen_width - req.width) / 2;
    int panel_y = (screen_height - req.height) / 2;
    int selected = 0;

    for (;;) {
        platform_pump_events(NULL);
        get_mouse();

        /* Yes button — gadget style with 3D bevel */
        {
            int bx = panel_x + 40, by = panel_y + 46, bw = 60, bh = tx_h + 4;
            a_pen_colour = 15;
            rect_fill(db, bx, by, bw, bh);
            int tl = (selected == 0) ? 14 : 8;
            int br = (selected == 0) ? 8 : 14;
            a_pen_colour = tl;
            move_pen(db, (int16_t)bx, (int16_t)(by + bh - 1));
            draw(db, bx, by); draw(db, bx + bw - 1, by);
            a_pen_colour = br;
            move_pen(db, (int16_t)(bx + bw - 1), (int16_t)(by + 1));
            draw(db, bx + bw - 1, by + bh - 1); draw(db, bx + 1, by + bh - 1);
            a_pen_colour = (selected == 0) ? 14 : 10;
            move_pen(db, (int16_t)(bx + (bw - 3 * tx_w) / 2), (int16_t)(by + 2));
            text(db, "Yes", 3);
        }

        /* No button */
        {
            int bx = panel_x + 160, by = panel_y + 46, bw = 60, bh = tx_h + 4;
            a_pen_colour = 15;
            rect_fill(db, bx, by, bw, bh);
            int tl = (selected == 1) ? 14 : 8;
            int br = (selected == 1) ? 8 : 14;
            a_pen_colour = tl;
            move_pen(db, (int16_t)bx, (int16_t)(by + bh - 1));
            draw(db, bx, by); draw(db, bx + bw - 1, by);
            a_pen_colour = br;
            move_pen(db, (int16_t)(bx + bw - 1), (int16_t)(by + 1));
            draw(db, bx + bw - 1, by + bh - 1); draw(db, bx + 1, by + bh - 1);
            a_pen_colour = (selected == 1) ? 14 : 10;
            move_pen(db, (int16_t)(bx + (bw - 2 * tx_w) / 2), (int16_t)(by + 2));
            text(db, "No", 2);
        }

        if (key4_pressed || key6_pressed ||
            extra_keys_pressed[75] || extra_keys_pressed[77]) {
            selected = 1 - selected;
            platform_delay(120);
        }

        if (space_pressed || enter_pressed || joy_button) {
            close_requester();
            return selected == 0 ? 1 : 0;
        }

        if (key_esc_was_pressed) {
            key_esc_was_pressed = false;
            close_requester();
            return 0;
        }

        show_parts();
        platform_blit(NULL, (const uint8_t *)bitmap[1 - db], (const uint8_t *)view_cmap);
    }
}

/* req_text_requester  E1: ? | E2P: 0x42A3F8 */
int text_requester(const char *prompt, char *output, int max_len) {
    /* No-op: editor-only text input via gadget StringReq system.
       Zero callers at runtime. */
    (void)prompt; (void)output; (void)max_len;
    return 0;
}

/* req_number_requester  E1: ? | E2P: 0x42A4C8 */
int number_requester(const char *prompt) {
    /* No-op: editor-only numeric input via gadget system.
       Zero callers at runtime.  Ref: uses HandleStringGadg + numeric validation. */
    (void)prompt;
    return 0;
}

/* req_add_gadget  E1: ? | E2P: 0x42A598 */
void add_gadget(gadget_t *gadget) {
    if (!gadget) return;
    /* Prepend gadget to active request's gadget linked list */
    gadget->next_gdg = active_request.gadget_list;
    active_request.gadget_list = gadget;
}

/* req_remove_gadget  E1: ? | E2P: 0x42A668 */
void remove_gadget(gadget_t *gadget) {
    if (!gadget) return;
    /* Unlink gadget from active request's gadget linked list */
    if (active_request.gadget_list == gadget) {
        active_request.gadget_list = gadget->next_gdg;
    } else {
        gadget_t *prev = active_request.gadget_list;
        while (prev && prev->next_gdg != gadget)
            prev = prev->next_gdg;
        if (prev)
            prev->next_gdg = gadget->next_gdg;
    }
    gadget->next_gdg = NULL;
}

/* req_draw_gadget  E1: ? | E2P: 0x42A738 */
void draw_gadget(gadget_t *gadget) {
    if (!gadget) return;

    /* Draw gadget rectangle */
    a_pen_colour = gadget->flags ? 7 : 5;
    move_pen(db, gadget->X, gadget->Y);
    draw(db, gadget->X + gadget->width, gadget->Y);
    draw(db, gadget->X + gadget->width, gadget->Y + gadget->height);
    draw(db, gadget->X, gadget->Y + gadget->height);
    draw(db, gadget->X, gadget->Y);

    /* Draw label */
    if (gadget->gadget_text && gadget->gadget_text[0]) {
        a_pen_colour = 1;
        int label_len = (int)strlen(gadget->gadget_text);
        move_pen(db, (int16_t)(gadget->X + 4), (int16_t)(gadget->Y + 4));
        text(db, gadget->gadget_text, label_len);
    }
}

/* req_check_gadget_hit  E1: ? | E2P: 0x42A808 */
int check_gadget_hit(gadget_t *gadget, int mx, int my) {
    if (!gadget) return 0;
    return (mx >= gadget->X && mx <= gadget->X + gadget->width &&
            my >= gadget->Y && my <= gadget->Y + gadget->height);
}

/* req_game_over  E1: ? | E2P: 0x42A8D8 */
void game_over(void) {
    do_fade_to_black(0);

    a_pen_colour = 1;
    move_pen(db, (int16_t)(screen_width / 2 - 40), (int16_t)(screen_height / 2));
    text(db, "GAME OVER", 9);

    show_parts();
    platform_blit(NULL, (const uint8_t *)bitmap[1 - db], (const uint8_t *)view_cmap);

    do_fade_in();

    /* Wait for keypress */
    for (;;) {
        platform_pump_events(NULL);
        get_mouse();
        if (space_pressed || key_esc_was_pressed) break;
        platform_delay(16);
    }

    /* Return to main menu */
    do_main_menu();
}

/* req_show_end_sequence  E1: ? | E2P: 0x42A9A8 */
void show_end_sequence(void) {
    /* End-game cutscene: requires knowing the end-scene index.
       Falls back to game_over screen until scene data is identified. */
    game_over();
}

/* req_show_credits  E1: ? | E2P: 0x42AA78 */
void show_credits(void) {
    /* No-op: zero callers. Likely dead/cut code. */
}

/* The subtitle port options (size, hold) exist because the original 6x8 font
 * reads very small once it is stretched over the enhanced 640x480 set. The
 * original VGA mode is meant to look like the release, so both are pinned to
 * their original values there rather than following the saved setting. */
static int subtitle_eff_scale(void) {
    if (!mode_svga) return 1;
    return subtitle_scale < 1 ? 1 : subtitle_scale;
}

static int subtitle_eff_hold(void) {
    if (!mode_svga) return 0;
    return subtitle_hold;
}

/* req_show_subtitle  E1: ? | E2P: 0x42AB48 */
void show_subtitle(const char *text_str, int duration) {
    if (!text_str) return;

    /* Find empty subtitle slot */
    for (int i = 0; i < MAX_SUBTITLES; i++) {
        if (!subtitle_status[i]) {
            subtitle_status[i] = 1;
            subtitle_colour[i] = 6;
            subtitle_text[i] = (char *)text_str;
            subtitle_length[i] = (int16_t)strlen(text_str);
            /* draw_subtitles re-centres for the current scale; this is just a
             * sane initial value. */
            subtitle_offset[i] = (screen_width
                                  - 6 * subtitle_eff_scale() * subtitle_length[i] - 2) / 2;
            subtitles_time = game_time + duration;
            return;
        }
    }
}

/* req_clear_subtitles  E1: ? | E2P: 0x42AC18 */
void req_clear_subtitles(void) {
    for (int i = 0; i < MAX_SUBTITLES; i++) {
        subtitle_status[i] = 0;
        subtitle_colour[i] = 0;
        subtitle_text[i] = NULL;
        subtitle_length[i] = 0;
        subtitle_offset[i] = 0;
    }
    subtitles_time = 0;
    clear_subtitles = 1;
}

/* req_draw_subtitles  E1: ? | E2P: 0x42ACE8 */
/* asm display_prepare_parts_421198+116..+240. Runs from prepare_parts before
 * the background restore, not from the subtitle renderer: the rects
 * clear_a_subtitle appends to clear_tab have to be in place before
 * clear_parts/clear_masking consume them, or they land a frame late.
 *
 * Blitting the background store straight into both draw planes (what this used
 * to do) skips the mask and the clear_tab bookkeeping, so anything composited
 * over the subtitle rect is lost. Stuck actors (flags & 0x0400) are only drawn
 * once and left in the buffer, so any of them overlapping the rect must have
 * its already-drawn bit cleared to make draw_stuck_parts render it again. */
void clear_expired_subtitles(void) {
    if (!subtitles_on) return;

    /* Finish time of the speech this caption belongs to. Latched when the
     * caption is posted rather than tracked continuously: a paragraph is one
     * long sample with its lines posted over it, and the *next* paragraph
     * starting would otherwise drag the previous line along with it — the
     * line stayed up ~5s past its own audio.
     *
     * The grace window covers the reverse case, where a script posts the text
     * slightly ahead of the sound event (seen at ~110 units), so the caption
     * still adopts the sample that arrives just after it. */
    const int32_t voice_grace_rt = 2 * MY_TIME_PER_SEC;
    int32_t now_rt = my_time();

    static int32_t caption_for = -1;
    static int32_t caption_audio_end_rt = 0;
    static int32_t caption_post_rt = 0;
    if (caption_for != subtitles_time) {
        caption_for = subtitles_time;
        caption_audio_end_rt = sample_end_rt;
        caption_post_rt = now_rt;
    }
    /* Grace window: a script can post the text slightly ahead of the sound
     * event, so the caption still adopts a sample arriving just after it.
     * Outside the window a later sample — the *next* paragraph — must not
     * drag this line along with it. */
    if (now_rt <= caption_post_rt + voice_grace_rt && sample_end_rt > caption_audio_end_rt)
        caption_audio_end_rt = sample_end_rt;

    /* 0x41D853 expires every caption SUBTITLE_HOLD_TICKS after it was posted,
     * flat, with no reference to the speech — which is why a voiced line
     * outlives its text. The longer settings are a port option. */
    int hold = subtitle_eff_hold();
    int32_t expire_at = subtitles_time + SUBTITLE_HOLD_TICKS;
    if (hold == 1)
        expire_at = subtitles_time + SUBTITLE_HOLD_TICKS * 3;

    int expired = (game_time - expire_at) > 0;

    if (hold >= 2) {
        /* Match voice: a line is normally retired by the next CT_SUBTITLE
         * (which sets clear_subtitles); this only decides the last line of a
         * paragraph, which goes when its own audio stops. Compared in real
         * time so a loading stall cannot cut it short. The cap is back in
         * game_time and covers a sample that is missing or muted. */
        if (now_rt < caption_audio_end_rt)
            expired = 0;
        if ((game_time - (subtitles_time + SUBTITLE_HOLD_TICKS * 8)) > 0)
            expired = 1;
    }

    if (expired)
        clear_subtitles = 1;

    if (!clear_subtitles) return;

    for (int i = 0; i < MAX_SUBTITLES; i++) {
        subarea_t *sub = &sub_area_to_clear[i];
        if (sub->left >= sub->right || sub->top >= sub->bottom)
            continue;

        for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
            subarea_t *a = actor->area_to_clear;
            if (!a) continue;
            if (a->left > sub->right || a->right < sub->left) continue;
            if (a->top > sub->bottom || a->bottom < sub->top) continue;
            if (!(actor->flags & 0x0400)) continue;
            actor->flags &= ~0x0800;
        }

        DBG_LOG(2, "[SUBT] clear slot=%d game_time=%d posted=%d visible=%d units (%.2fs)\n",
                i, (int)game_time, (int)subtitles_time,
                (int)(game_time - subtitles_time),
                (game_time - subtitles_time) / 3.0 /
                    ((game_version == GAME_VERSION_E1) ? 60.0 : 70.0));

        clear_a_subtitle(i);

        /* 0x41D925 retires the slot and blanks the rect, and deliberately
         * leaves subtitle_text/subtitle_length alone. The port used to null
         * them here, which destroyed a caption posted in this same frame:
         * CT_SUBTITLE sets clear_subtitles and refills the slot, but the rect
         * still belongs to the outgoing line, so the loop reaches it and wiped
         * the incoming text before draw_subtitles ever saw it. Status stays 1
         * for a fresh post, so only status 2 is retired. */
        if (subtitle_status[i] == 2)
            subtitle_status[i] = 0;
        sub->left = sub->right = sub->top = sub->bottom = 0;
    }
    clear_subtitles = 0;
}

void draw_subtitles(void) {
    if (!subtitles_on) return;

    int scale = subtitle_eff_scale();
    int cw = 6 * scale;          /* advance per character */
    int line_h = 10 * scale;

    /* Slots in the current caption block (a multi-line caption is pushed as
     * consecutive slots before any of it is drawn). */
    int lines = 0;
    for (int i = 0; i < MAX_SUBTITLES; i++)
        if (subtitle_status[i] == 1 || subtitle_status[i] == 2)
            lines = i + 1;

    /* The original stacks lines downward from 240 in SVGA and from 0 in VGA
     * (0x41E18A: mode_svga ? 240 : 0; 240 is screen_height/2 at 640x480).
     * Scaling the line pitch alone would push the block that much closer to
     * the bottom of the screen, so in SVGA anchor the block's bottom edge
     * where the unscaled layout put it and let it grow upward instead. At
     * scale 1 this is exactly the original placement; VGA is always original. */
    int base = 0;
    if (mode_svga) {
        base = screen_height / 2 + lines * 10 - lines * line_h;
        if (base < 0) base = 0;
    }

    for (int i = 0; i < MAX_SUBTITLES; i++) {
        if (subtitle_status[i] != 1) continue;

        /* Re-centre for the current scale rather than trusting the value
         * show_subtitle computed — the size can change between the two. */
        int line_w = cw * subtitle_length[i];
        subtitle_offset[i] = (int16_t)((screen_width - line_w - 2) / 2);
        if (subtitle_offset[i] < 0) subtitle_offset[i] = 0;

        int y = base + i * line_h;
        if (y + line_h > screen_height) y = screen_height - line_h;
        if (y < 0) y = 0;
        draw_mode[2] = 2;
        subtitle_status[i] = 2;
        int16_t col = subtitle_colour[i];
        if (col & 1) {
            a_pen_colour = 16 * col + 8;
            b_pen_colour = 16 * (col - 1) + 2;
        } else {
            b_pen_colour = 16 * col;
            a_pen_colour = 16 * col + 15;
        }
        move_pen(2, subtitle_offset[i], (int16_t)y);
        if (subtitle_text[i]) {
            text_with_mask_scaled(2, subtitle_text[i], subtitle_length[i], scale);
        }

        sub_area_to_clear[i].left   = subtitle_offset[i];
        sub_area_to_clear[i].right  = subtitle_offset[i] + line_w + 1;
        sub_area_to_clear[i].top    = (int16_t)y;
        sub_area_to_clear[i].bottom = (int16_t)(y + line_h - 1);

        int w = line_w + 2;
        int h = line_h;
        clear_background(1 - db, subtitle_offset[i], y, w, h);
        clip_mask(1, 0, subtitle_offset[i], y, w, h);

        if (number_to_clear[1 - db] >= 150) {
            beep_message("ClearTab overflow!");
        } else {
            clear_tab[1 - db][number_to_clear[1 - db]++] = sub_area_to_clear[i];
        }
    }
}

/* Note: draw_polygon, find_ellipse, shade_ellipse, add_ellipse
 * are defined in tri.c and ellipse.c/anim.c respectively */

/* req_do_choice_req  E2: 0x43C890 */
int do_choice_req(const char *msg) {
    active_requester = 0x14;
    char buf[51];
    strncpy(buf, msg, 50);
    buf[50] = '\0';
    choice_req.gadget_header_text = buf;
    int len = (int)strlen(buf);
    if (len < 23)
        choice_req.max_length = 150;
    else
        choice_req.max_length = (int16_t)(len * 6 + 20);
    esc_req_choice = 0;
    request(&choice_req);
    return esc_req_choice;
}

/* req_do_choice2_req  E2: 0x43C9A0 */
void do_choice2_req(void) {
    esc_req_choice = 0;
    request(&choice_req);
}

/* req_do_choice_page_req  E2: 0x43C918 */
void do_choice_page_req(void) {
    active_requester = 0x2F;
    esc_req_choice = 0;
    request(&page_req);
}

/* req_do_info_page_req  E2: 0x43C9EC */
void do_info_page_req(void) {
    esc_req_choice = 0;
    request(&page_req);
}

/* req_do_info_req  E2: 0x43CA40 */
void do_info_req(const char *msg) {
    if (!msg) return;
    active_requester = 0x15;
    int len = (int)strlen(msg);
    if (len > 50) {
        int split = 49;
        while (split > 0 && msg[split] != ' ') split--;
        if (split <= 0) {
            do_info_req("DoInfoReq overflow");
            return;
        }
        char line1[52];
        strncpy(line1, msg, split);
        line1[split] = '\0';
        const char *line2 = msg + split + 1;
        if ((int)strlen(line2) > 50) {
            do_info_req("DoInfoReq overflow");
            return;
        }
        do_info_page_req();
        return;
    }
    char buf[52];
    strncpy(buf, msg, 50);
    buf[50] = '\0';
    info_req.gadget_header_text = buf;
    len = (int)strlen(buf);
    if (len < 23)
        info_req.max_length = 150;
    else
        info_req.max_length = (int16_t)(len * 6 + 20);
    request(&info_req);
}

/* req_do_info2_req  E2: 0x43CB34 */
void do_info2_req(const char *msg1, const char *msg2) {
    (void)msg2;
    do_info_req(msg1);
}

/* req_do_info3_req  E2: 0x43CB50 */
void do_info3_req(const char *msg1, const char *msg2, const char *msg3) {
    (void)msg2; (void)msg3;
    do_info_req(msg1);
}

/* req_clear_requester  E2: 0x43B8EC */
void clear_requester(void) {
    request_t *req = find_active_req();
    if (req && req->pixelX >= 0) {
        clip_blit(1 - db, req->pixelX, req->pixelY, db,
                  req->pixelX, req->pixelY, req->width, req->height, 0);
    }
    turn_mouse_pointer_off();
    active_requester = 0;
}

/* req_handle_cancel  E2: 0x43C468 */
void handle_cancel(void) {
    switch (active_requester) {
    case 0x00:
    case 0x14:
    case 0x2D:
    case 0x2F:
        break;
    case 0x27:
    case 0x28:
        esc_req_choice = 5;
        return;
    case 0x2A:
        saved_game_num = 0;
        return;
    case 0x2C:
    case 0x36:
        req_choice = 0;
        return;
    case 0x30:
        itype = 0;
        return;
    case 0x31:
        break;
    default:
        quit("Error: Bad requester id");
        return;
    }
}

/* req_handle_ok  E2: 0x43C514 */
void handle_ok(void) {
    switch (active_requester) {
    case 0x00:
    case 0x15:
    case 0x2E:
    case 0x30:
        break;
    case 0x14:
    case 0x2C:
    case 0x2F:
    case 0x36:
        req_choice = 1;
        return;
    case 0x29:
        if (saved_game_num >= 0)
            load_game(saved_game_num);
        return;
    case 0x2A:
        if (saved_game_num >= 0)
            save_game(saved_game_num);
        return;
    case 0x31:
        settings_choice = 0;
        return;
    default:
        quit("Error: Bad requester id");
        return;
    }
}

/* req_init_gadget  E2: 0x43FDA4 */
void init_gadget(gadget_t *gadget, int16_t x, int16_t y, int16_t w,
                 int16_t h, char *text_str, void (*handler)(void),
                 int16_t flags, gadget_t *next) {
    if (mode_svga) {
        gadget->X = x * 2;
        gadget->Y = (int16_t)((3 * (int)y + ((3 * y < 0) ? 1 : 0)) / 2);
        gadget->width = w * 2;
        gadget->height = (int16_t)((3 * (int)h + ((3 * h < 0) ? 1 : 0)) / 2);
    } else {
        gadget->X = x;
        gadget->Y = y;
        gadget->width = w;
        gadget->height = h;
    }
    gadget->gadget_text = text_str;
    gadget->handle_click = (int32_t)(intptr_t)handler;
    gadget->flags = flags;
    gadget->next_gdg = next;
}

/* req_do_input_req  E2: 0x43CC48 */
void do_input_req(char *buffer, int buflen, const char *prompt) {
    char header_buf[52];
    active_requester = 0x2C;
    strncpy(header_buf, prompt, 50);
    header_buf[50] = '\0';

    string_req.gadget_header_text = header_buf;

    int len = (int)strlen(header_buf);
    if (len < 30)
        string_req.max_length = 200;
    else
        string_req.max_length = (int16_t)(len * 6 + 20);

    strncpy(string_buffer, buffer, 50);
    string_buffer[50] = '\0';

    req_choice = 0;
    request(&string_req);

    if (req_choice != 0) {
        strncpy(buffer, string_buffer, buflen - 1);
        buffer[buflen - 1] = '\0';
    }
}

/* req_do_input  E2: 0x43BCCC */
void do_input(void) {
    /* Keyboard input handler for text gadgets in requester system.
       Not needed — simplified menu system handles input directly. */
}

/* req_load_saved_screen  E2: 0x43C138 */
void load_saved_screen(void) {
    /* Loads saved screenshot from "saved/XXXX.ecs" files.
       264 instructions of file I/O + screen restoration. */
}

/* req_install_to_disk  E2: 0x43DA80 */
void install_to_disk(void) {
    /* Full CD-to-disk install routine.
       0x1098 bytes — copies directories, shows progress bar.
       Not needed for modern platforms. */
}

/* req_handle_dir_gadg  E2: 0x43C430 */
void handle_dir_gadg(void) {
    quit("Error: Bad requester id");
}

/* req_handle_play_female  E2: 0x43D3E4 */
void handle_play_female(void) {
    esc_req_choice = 1;
}

/* req_handle_play_male  E2: 0x43D3D8 */
void handle_play_male(void) {
    esc_req_choice = 0;
}

/* req_handle_configure  E2: 0x43D3F0 */
void handle_configure(void) {
    esc_req_choice = 2;
}

/* req_handle_load_game  E2: 0x43D410 */
void handle_load_game(void) {
    esc_req_choice = 4;
}

/* req_handle_quit_to_dos  E2: 0x43D41C */
void handle_quit_to_dos(void) {
    int result = do_choice_req("Are you sure you want to quit Ecstatica II?");
    if (result)
        esc_req_choice = 6;
    else
        esc_req_choice = 5;
}

/* req_handle_save_game  E2: 0x43D3FC */
void handle_save_game(void) {
    if (!intro_flag)
        esc_req_choice = 3;
}

/* req_handle_set_card  E2: 0x43D62C */
void handle_set_card(void) {
    settings_choice = 1;
}

/* req_handle_set_install  E2: 0x43D638 */
void handle_set_install(void) {
    settings_choice = 2;
}

/* req_do_choice3_req  E2: 0x43CA20 */
void do_choice3_req(const char *msg1, const char *msg2, const char *msg3) {
    choice_req.gadget_header_text = (char *)msg1;
    /* msg2 and msg3 would populate additional gadget lines in the page req */
    do_choice_page_req();
}

/* req_refresh_gadgets  E2: 0x43AEEC */
void refresh_gadgets(gadget_t *gadget) {
    if (!gadget) return;
    for (gadget_t *g = gadget; g; g = g->next_gdg)
        show_gadget(g);
}

/* req_refresh_request — inner drawing: draws box, title, gadgets  E2: 0x43AF2C */
static void refresh_request(request_t *req) {
    int plane = db;
    a_pen_colour = 15;
    rect_fill(plane, req->pixelX, req->pixelY, req->field_12 - 1, req->field_16 - 1);
    draw_mode[plane] = 1;
    move_pen(plane, (int16_t)(req->field_16 - 1), req->pixelX);
    a_pen_colour = 8;
    draw(plane, req->pixelY, req->pixelX);
    draw(plane, req->pixelY, req->field_12);
    a_pen_colour = 14;
    move_pen(plane, (int16_t)(req->pixelY + 1), (int16_t)(req->field_12 - 1));
    draw(plane, (int16_t)(req->field_16 - 1), (int16_t)(req->field_12 - 1));
    draw(plane, (int16_t)(req->field_16 - 1), req->pixelX);

    if (req->gadget_header_text) {
        int text_len = (int)strlen(req->gadget_header_text);
        int text_w = text_len * tx_w;
        int y_pos = req->pixelY + tx_h + 2;
        int x_center = req->pixelX + (req->width - text_w + 1) / 2;
        move_pen(plane, (int16_t)y_pos, (int16_t)x_center);
        a_pen_colour = 8;
        b_pen_colour = 15;
        text(plane, req->gadget_header_text, 0);
    }

    gadget_t *g = req->gadget_list;
    while (g) {
        show_gadget(g);
        g = g->next_gdg;
    }
}

/* req_restore_active_req  E2: 0x43AF18 */
void restore_active_req(void) {
    if (active_requester == 0) return;
    request_t *req = find_active_req();
    if (!req) return;
    refresh_request(req);
}

/* req_refresh_request_res  E2: 0x43D82C */
void refresh_request_res(void) {
    request_t *req = find_active_req();
    if (!req) return;
    if (mode_svga) {
        req->width = req->max_length * 2;
        int h = req->max_length >> 16;
        req->height = (int16_t)((3 * abs(h)) / 2);
    }
    refresh_request(req);
}

/* req_do_subtitle_gadg_text  E2: 0x43D86C */
void do_subtitle_gadg_text(void) {
    if (subtitles_on)
        subtitle_gadg.gadget_text = "Subtitles               ON";
    else
        subtitle_gadg.gadget_text = "Subtitles              OFF";
}

/* req_do_music_gadg_text  E2: 0x43D8B4 */
void do_music_gadg_text(void) {
    if (music_on)
        music_gadg.gadget_text = "Music                   ON";
    else
        music_gadg.gadget_text = "Music                  OFF";
    if (sound_driver == 0 || sound_driver == 6)
        music_gadg.flags |= 0x80;
    else
        music_gadg.flags &= ~0x80;
}

/* req_do_sound_fx_gadg_text  E2: 0x43D920 */
void do_sound_fx_gadg_text(void) {
    if (sound_fx_on)
        sound_fx_gadg.gadget_text = "Sound effects           ON";
    else
        sound_fx_gadg.gadget_text = "Sound effects          OFF";
    if (sound_driver == 0)
        sound_fx_gadg.flags |= 0x80;
    else
        sound_fx_gadg.flags &= ~0x80;
}

/* req_do_lod_gadg_text  E2: 0x43D984 */
void do_lod_gadg_text(void) {
    if (difficulty == 0)
        lod_gadg.gadget_text = "Difficulty level      EASY";
    else if (difficulty == 1)
        lod_gadg.gadget_text = "Difficulty level    MEDIUM";
    else
        lod_gadg.gadget_text = "Difficulty level      HARD";
}

/* req_do_lo_hi_res_gadg_text  E2: 0x43DA00 */
void do_lo_hi_res_gadg_text(void) {
    if (mode_svga)
        lo_hi_res_gadg.gadget_text = "Resolution            HIGH";
    else
        lo_hi_res_gadg.gadget_text = "Resolution             LOW";
}

/* req_handle_subtitle  E2: 0x43D664 */
void handle_subtitle(void) {
    subtitles_on = !subtitles_on;
    do_subtitle_gadg_text();
    for (gadget_t *g = &subtitle_gadg; g; g = g->next_gdg)
        show_gadget(g);
}

/* req_handle_language  E2: 0x43D644 */
void handle_language(void) {
    language = (int16_t)(req_finished >> 16);
}

/* req_handle_music  E2: 0x43D6A8 */
void handle_music(void) {
    if (sound_driver != 0 && sound_driver != 6) {
        music_on = !music_on;
        if (!music_on) {
            stop_tune();
        } else if (tune_playing) {
            load_tune(tune_playing);
            if (tune_buffer_length > 0)
                start_tune();
        }
    }
    do_music_gadg_text();
    for (gadget_t *g = &music_gadg; g; g = g->next_gdg)
        show_gadget(g);
}

/* req_handle_sound_fx  E2: 0x43D720 */
void handle_sound_fx(void) {
    if (sound_driver != 0)
        sound_fx_on = !sound_fx_on;
    do_sound_fx_gadg_text();
    for (gadget_t *g = &sound_fx_gadg; g; g = g->next_gdg)
        show_gadget(g);
}

/* req_handle_diff_gadg  E2: 0x43D770 */
void handle_diff_gadg(void) {
    difficulty++;
    if (difficulty > 2) difficulty = 0;
    do_lod_gadg_text();
    for (gadget_t *g = &lod_gadg; g; g = g->next_gdg)
        show_gadget(g);
}

/* req_handle_lo_hi_res  E2: 0x43D7BC */
void handle_lo_hi_res(void) {
    remove_all_graphics();
    if (mode_svga)
        go_vga();
    else
        go_svga();
    chosen_svga = mode_svga;
    remove_all_graphics();
    update_game_icons();
    draw_magic_bar();
    prepare_parts();
    draw_stuck_parts();
    draw_parts();
    show_parts();
    do_subtitle_gadg_text();
    do_music_gadg_text();
    do_sound_fx_gadg_text();
    do_lod_gadg_text();
    do_lo_hi_res_gadg_text();
    restore_active_req();
}

/* req_handle_difficulty  E2: 0x43D654 */
void handle_difficulty(void) {
    difficulty = (int16_t)(req_finished >> 16);
}
