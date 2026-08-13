/**
 * menu.c
 *
 * Game menu system: main menu, pause menu, settings.
 * 15 functions prefixed with menu_ in the original ASM.
 *
 * Uses the requester visual style: dark blue panel (palette 6),
 * light cyan border (palette 7), white text (palette 1),
 * gray text for unselected items (palette 5), highlight bar
 * for selected item (palette 4 bg).
 */

#include "menu.h"
#include "display.h"
#include "edit.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "music.h"
#include "req.h"
#include "platform.h"
#include "win.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations */
static void handle_main_menu_selection(void);
void do_load_menu(void);
void do_save_menu(void);
void do_settings_menu(void);
int  do_slot_select(const char *title);

/* ══════════════════════════════════════════════════════════════
 *  Menu State
 * ══════════════════════════════════════════════════════════════ */

static int menu_selection = 0;
static int menu_active = 0;
static int menu_result = -1;
bool menu_no_continue = false;

/* ══════════════════════════════════════════════════════════════
 *  Shared menu helpers
 * ══════════════════════════════════════════════════════════════ */

static void menu_frame_start(void) {
    window_proc();
}

static void menu_frame_end(void) {
    show_parts();
    platform_blit(NULL, (const uint8_t *)bitmap[1 - db], (const uint8_t *)colour_map);
    platform_delay(16);
}

static bool menu_nav_up(void) {
    return key8_pressed || extra_keys_pressed[72];
}

static bool menu_nav_down(void) {
    return key2_pressed || extra_keys_pressed[80];
}

static bool menu_nav_left(void) {
    return key4_pressed || extra_keys_pressed[75];
}

static bool menu_nav_right(void) {
    return key6_pressed || extra_keys_pressed[77];
}

static bool menu_confirm(void) {
    return space_pressed || enter_pressed || joy_button;
}

static bool menu_back(void) {
    if (key_esc_was_pressed) {
        key_esc_was_pressed = false;
        return true;
    }
    return false;
}

/* ── Per-version palette indices (from binary disassembly) ──
 *
 * E1 (req_request_431034 / show_gadget_43166C):
 *   Uses palette range 0xC3–0xDD (reds/maroons from PALLETTE.RAW).
 *   0xC3=dark red, 0xC9=mid red (fill), 0xCF=bevel light,
 *   0xD8=text, 0xDD=title text.
 *
 * E2 (req_request_43B304 / show_gadget_43B93C):
 *   Uses standard indices 8/10/14/15 (grays + gold from PALLETTE.RAW).
 *   15=gray (fill), 8=near-white (bevel light), 14=near-black (bevel dark),
 *   10=gold (text), 8=title text.
 */
static int col_fill(void)       { return (game_version == GAME_VERSION_E1) ? 0xC9 : 15; }
static int col_bevel_light(void) { return (game_version == GAME_VERSION_E1) ? 0xCF :  8; }
static int col_bevel_dark(void)  { return (game_version == GAME_VERSION_E1) ? 0xC3 : 14; }
static int col_text(void)       { return (game_version == GAME_VERSION_E1) ? 0xD8 : 10; }
static int col_text_sel(void)   { return (game_version == GAME_VERSION_E1) ? 0xC3 : 14; }
static int col_title(void)      { return (game_version == GAME_VERSION_E1) ? 0xDD :  8; }

/* 3D beveled box — matches original req_request / show_gadget rendering. */
static void draw_bevel(int x, int y, int w, int h, bool pressed) {
    int tl = pressed ? col_bevel_dark()  : col_bevel_light();
    int br = pressed ? col_bevel_light() : col_bevel_dark();

    a_pen_colour = tl;
    move_pen(db, (int16_t)x, (int16_t)(y + h - 1));
    draw(db, x, y);
    draw(db, x + w - 1, y);

    a_pen_colour = br;
    move_pen(db, (int16_t)(x + w - 1), (int16_t)(y + 1));
    draw(db, x + w - 1, y + h - 1);
    draw(db, x + 1, y + h - 1);
}

static void draw_panel(int px, int py, int pw, int ph) {
    draw_mode[db] = 1;
    a_pen_colour = col_fill();
    rect_fill(db, px, py, pw, ph);
    draw_bevel(px, py, pw, ph, false);
}

static void draw_panel_title(const char *title, int px, int py, int pw) {
    int len = (int)strlen(title);
    int tx = px + (pw - len * tx_w) / 2;
    a_pen_colour = col_title();
    b_pen_colour = col_fill();
    move_pen(db, (int16_t)tx, (int16_t)(py + tx_h + 2));
    text(db, title, len);
}

static void draw_menu_item(const char *label, int x, int y, int item_w, int item_h, bool selected) {
    a_pen_colour = col_fill();
    rect_fill(db, x, y, item_w, item_h);
    draw_bevel(x, y, item_w, item_h, selected);

    a_pen_colour = selected ? col_text_sel() : col_text();
    b_pen_colour = col_fill();
    int len = (int)strlen(label);
    int tx = x + (item_w - len * tx_w) / 2;
    int ty = y + (item_h - tx_h) / 2;
    move_pen(db, (int16_t)tx, (int16_t)ty);
    text(db, label, len);
}

/* ══════════════════════════════════════════════════════════════
 *  Main Menu — version-specific items and layout
 *
 *  E1 original (from binary at 0x43CDD8 / init_gadgets):
 *    Panel 210×180, gadgets 170×10 at x=20
 *    Items: Start game, Start game (Female), Save game...,
 *           Load game..., Settings..., Quit
 *    Y offsets: 20, 35, 50, 65, 80, 130
 *
 *  E2 (SVGA-scaled: w×2, h×1.5):
 *    Panel 420×270, gadgets 340×15 at x=40
 *    Items: Continue, New Game, Load Game, Save Game,
 *           Settings, Quit
 * ══════════════════════════════════════════════════════════════ */

/* E1 menu items */
#define E1_MENU_ITEMS 6
static const char *e1_menu_items[E1_MENU_ITEMS] = {
    "Start game (Male)",
    "Start game (Female)",
    "Save game...",
    "Load game...",
    "Settings...",
    "Quit"
};
static const int e1_menu_y[E1_MENU_ITEMS] = { 20, 35, 50, 65, 80, 130 };

/* E2 menu items */
#define E2_MENU_ITEMS 6
static const char *e2_menu_items[E2_MENU_ITEMS] = {
    "Continue",
    "New Game",
    "Load Game",
    "Save Game",
    "Settings",
    "Quit"
};
static const int e2_menu_y[E2_MENU_ITEMS] = { 20, 35, 50, 65, 80, 130 };

static int menu_item_count(void) {
    return (game_version == GAME_VERSION_E1) ? E1_MENU_ITEMS : E2_MENU_ITEMS;
}

static const char **menu_items(void) {
    return (game_version == GAME_VERSION_E1) ? e1_menu_items : e2_menu_items;
}

static const int *menu_y_offsets(void) {
    return (game_version == GAME_VERSION_E1) ? e1_menu_y : e2_menu_y;
}

/* menu_do_main_menu  E1: ? | E2P: 0x42B210 */
void do_main_menu(void) {
    capture_save_thumbnail();
    menu_active = 1;
    menu_result = -1;
    int num_items = menu_item_count();
    int first_item = (menu_no_continue && game_version != GAME_VERSION_E1) ? 1 : 0;
    menu_selection = first_item;

    while (menu_active) {
        menu_frame_start();

        draw_main_menu();

        if (menu_nav_up()) {
            if (menu_selection > first_item) menu_selection--;
            platform_delay(120);
        }
        if (menu_nav_down()) {
            if (menu_selection < num_items - 1) menu_selection++;
            platform_delay(120);
        }

        if (menu_confirm()) {
            platform_delay(150);
            handle_main_menu_selection();
        }

        if (menu_back() && !menu_no_continue) {
            menu_active = 0;
        }

        menu_frame_end();
    }

    menu_no_continue = false;

    /* Execute deferred actions after menu loop exits,
     * matching original where actions run outside req_request. */
    if (menu_result >= 0) {
        if (game_version == GAME_VERSION_E1) {
            switch (menu_result) {
            case 0: female = 0; start_game_medium(0, 0); break;
            case 1: female = 1; start_game_medium(0, 0); break;
            }
        } else {
            if (menu_result == 1)
                start_game_medium(0, 0);
        }
    }
}

/* menu_draw_main_menu  E1: ? | E2P: 0x42B2E0 */
void draw_main_menu(void) {
    int num_items = menu_item_count();
    const char **items = menu_items();
    const int *y_offsets = menu_y_offsets();

    /* Original E1 requester: panel 210×180, gadgets 170×10 at x=20.
     * SVGA (E2): doubled width, 1.5× height via init_gadget scaling. */
    int panel_w, panel_h, gadget_w, gadget_h, gadget_x;
    if (mode_svga) {
        panel_w = 420; panel_h = 270;
        gadget_w = 340; gadget_h = 15; gadget_x = 40;
    } else {
        panel_w = 210; panel_h = 180;
        gadget_w = 170; gadget_h = 10; gadget_x = 20;
    }

    int px = (screen_width - panel_w) / 2;
    int py = (screen_height - panel_h) / 2;

    draw_panel(px, py, panel_w, panel_h);

    const char *title = (game_version == GAME_VERSION_E1) ? "Ecstatica" : "Ecstatica II";
    draw_panel_title(title, px, py, panel_w);

    int first_item = (menu_no_continue && game_version != GAME_VERSION_E1) ? 1 : 0;
    for (int i = first_item; i < num_items; i++) {
        int y_off = y_offsets[i];
        if (mode_svga)
            y_off = (3 * y_off + (y_off < 0 ? 1 : 0)) / 2;
        draw_menu_item(items[i], px + gadget_x, py + y_off,
                       gadget_w, gadget_h, i == menu_selection);
    }
}

/* menu_handle_main_menu_selection  E1: ? | E2P: 0x42B3B0 */
static void handle_main_menu_selection(void) {
    if (game_version == GAME_VERSION_E1) {
        switch (menu_selection) {
        case 0: /* Start game (Male) */
        case 1: /* Start game (Female) */
            menu_result = menu_selection;
            menu_active = 0;
            break;
        case 2: /* Save game */
            do_save_menu();
            break;
        case 3: /* Load game */
            do_load_menu();
            break;
        case 4: /* Settings */
            do_settings_menu();
            break;
        case 5: /* Quit */
            program_up_and_running = false;
            menu_active = 0;
            break;
        }
    } else {
        switch (menu_selection) {
        case 0: /* Continue */
            if (!menu_no_continue)
                menu_active = 0;
            break;
        case 1: /* New Game */
            menu_result = 1;
            menu_active = 0;
            break;
        case 2: /* Load Game */
            do_load_menu();
            break;
        case 3: /* Save Game */
            do_save_menu();
            break;
        case 4: /* Settings */
            do_settings_menu();
            break;
        case 5: /* Quit */
            program_up_and_running = false;
            menu_active = 0;
            break;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Load / Save Menus
 * ══════════════════════════════════════════════════════════════ */

/* menu_do_load_menu  E1: ? | E2P: 0x42B480 */
void do_load_menu(void) {
    int slot = do_slot_select("Load Game");
    if (slot >= 0) {
        if (!check_saved_game(slot)) {
            beep_message("No saved game");
            return;
        }
        load_game(slot);
        menu_active = 0;
    }
}

/* menu_do_save_menu  E1: ? | E2P: 0x42B550 */
void do_save_menu(void) {
    int slot = do_slot_select("Save Game");
    if (slot >= 0) {
        save_game(slot);
        beep_message("Game Saved");
    }
}

#define NUM_SLOTS 11
#define THUMB_W 80
#define THUMB_H 60

static void draw_thumbnail(int dx, int dy, const uint8_t *thumb) {
    int scale = mode_svga ? 2 : 1;
    int tw = THUMB_W * scale;
    int th = THUMB_H * scale;
    uint8_t *dst = (uint8_t *)bitmap[db];
    for (int sy = 0; sy < THUMB_H; sy++) {
        for (int sx = 0; sx < THUMB_W; sx++) {
            uint8_t pixel = thumb[sy * THUMB_W + sx];
            for (int ry = 0; ry < scale; ry++) {
                int py2 = dy + sy * scale + ry;
                if (py2 < 0 || py2 >= screen_height) continue;
                for (int rx = 0; rx < scale; rx++) {
                    int px2 = dx + sx * scale + rx;
                    if (px2 < 0 || px2 >= screen_width) continue;
                    dst[py2 * screen_width + px2] = pixel;
                }
            }
        }
    }
    draw_bevel(dx - 1, dy - 1, tw + 2, th + 2, true);
}

/* menu_do_slot_select  E1: ? | E2P: 0x42B620 */
int do_slot_select(const char *title) {
    int selected = 0;
    int scroll_top = 0;
    int visible = NUM_SLOTS < 8 ? NUM_SLOTS : 8;

    int item_h = tx_h + 8;
    int item_w = 22 * tx_w;
    int thumb_scale = mode_svga ? 2 : 1;
    int thumb_disp_w = THUMB_W * thumb_scale;
    int thumb_disp_h = THUMB_H * thumb_scale;
    int thumb_area_w = thumb_disp_w + 16;
    int panel_w = item_w + thumb_area_w + 16;
    int list_h = visible * item_h;
    int min_h = thumb_disp_h + 8;
    int content_h = list_h > min_h ? list_h : min_h;
    int panel_h = content_h + 28;
    int px = (screen_width - panel_w) / 2;
    int py = (screen_height - panel_h) / 2;

    uint8_t thumb_buf[THUMB_W * THUMB_H];
    int thumb_loaded = -1;

    for (;;) {
        menu_frame_start();

        draw_panel(px, py, panel_w, panel_h);
        draw_panel_title(title, px, py, panel_w);

        int item_x = px + 8;
        int start_y = py + 22;

        for (int i = 0; i < visible && (scroll_top + i) < NUM_SLOTS; i++) {
            int slot = scroll_top + i;
            int y = start_y + i * item_h;
            char label[32];
            char name[28];
            if (get_save_name(slot, name, sizeof(name)))
                snprintf(label, sizeof(label), "%d: %s", slot + 1, name);
            else
                snprintf(label, sizeof(label), "%d: ---", slot + 1);
            draw_menu_item(label, item_x, y, item_w, item_h, slot == selected);
        }

        if (thumb_loaded != selected) {
            thumb_loaded = selected;
            if (!load_saved_thumbnail(selected, thumb_buf))
                memset(thumb_buf, 0, sizeof(thumb_buf));
        }
        int thumb_x = px + item_w + 16 + 4;
        int thumb_y = start_y + (content_h - thumb_disp_h) / 2;
        draw_thumbnail(thumb_x, thumb_y, thumb_buf);

        if (menu_nav_up()) {
            if (selected > 0) {
                selected--;
                if (selected < scroll_top) scroll_top = selected;
            }
            platform_delay(120);
        }
        if (menu_nav_down()) {
            if (selected < NUM_SLOTS - 1) {
                selected++;
                if (selected >= scroll_top + visible) scroll_top = selected - visible + 1;
            }
            platform_delay(120);
        }

        if (menu_confirm()) {
            platform_delay(150);
            return selected;
        }
        if (menu_back()) {
            return -1;
        }

        menu_frame_end();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Settings Menu
 * ══════════════════════════════════════════════════════════════ */

static const char *difficulty_names[] = {"Easy", "Medium", "Hard"};

/* Language indices match script token mapping:
 * 0=English, 1=German, 2=French, 3=(unused), 4=Italian, 5=Spanish, 6=Polish.
 * E1 only has English/German/French. E2 adds Italian/Spanish/Polish. */
static const char *language_names[] = {
    "English", "German", "French", "", "Italian", "Spanish", "Polish"
};
#define NUM_LANGUAGES_E1 3
#define NUM_LANGUAGES_E2 7
#define SETTINGS_ITEMS 5

/* Settings row: gadget-style with label left, value right */
static void draw_setting_row(const char *label, const char *value,
                             int x, int y, int w, bool selected, bool has_arrows) {
    int item_h = tx_h + 4;
    int ty = y + (item_h - tx_h) / 2;

    a_pen_colour = col_fill();
    rect_fill(db, x, y, w, item_h);
    draw_bevel(x, y, w, item_h, selected);

    /* Label on the left */
    a_pen_colour = selected ? col_text_sel() : col_text();
    move_pen(db, (int16_t)(x + 4), (int16_t)ty);
    text(db, label, (int)strlen(label));

    /* Value on the right */
    if (has_arrows) {
        char buf[48];
        snprintf(buf, sizeof(buf), "< %s >", value);
        int buf_len = (int)strlen(buf);
        int vx = x + w - buf_len * tx_w - 4;
        a_pen_colour = selected ? col_title() : col_text();
        move_pen(db, (int16_t)vx, (int16_t)ty);
        text(db, buf, buf_len);
    } else {
        int val_len = (int)strlen(value);
        int vx = x + w - val_len * tx_w - 4;
        a_pen_colour = selected ? col_title() : col_text();
        move_pen(db, (int16_t)vx, (int16_t)ty);
        text(db, value, val_len);
    }
}

/* Settings item IDs */
enum {
    SETT_DIFFICULTY = 0,
    SETT_LANGUAGE,
    SETT_MUSIC,
    SETT_SOUNDFX,
    SETT_SUBTITLES,
    SETT_MAX
};

static void settings_get_value(int id, char *buf, int bufsz) {
    switch (id) {
    case SETT_DIFFICULTY: {
        int d = difficulty;
        if (d < 0) d = 0; if (d > 2) d = 2;
        snprintf(buf, bufsz, "%s", difficulty_names[d]);
        break;
    }
    case SETT_LANGUAGE: {
        int l = language;
        int mx = (game_version == GAME_VERSION_E1) ? NUM_LANGUAGES_E1 : NUM_LANGUAGES_E2;
        if (l < 0) l = 0; if (l >= mx) l = mx - 1; if (l == 3) l = 2;
        snprintf(buf, bufsz, "%s", language_names[l]);
        break;
    }
    case SETT_MUSIC:     snprintf(buf, bufsz, "%s", music_on ? "On" : "Off"); break;
    case SETT_SOUNDFX:   snprintf(buf, bufsz, "%s", sound_fx_on ? "On" : "Off"); break;
    case SETT_SUBTITLES: snprintf(buf, bufsz, "%s", subtitles_on ? "On" : "Off"); break;
    }
}

static void settings_adjust(int id, int dir) {
    switch (id) {
    case SETT_DIFFICULTY:
        difficulty += dir;
        if (difficulty < 0) difficulty = 0;
        if (difficulty > 2) difficulty = 2;
        break;
    case SETT_LANGUAGE: {
        int mx = (game_version == GAME_VERSION_E1) ? NUM_LANGUAGES_E1 : NUM_LANGUAGES_E2;
        language += dir;
        if (language == 3) language += dir;
        if (language < 0) language = 0;
        if (language >= mx) language = mx - 1;
        break;
    }
    case SETT_MUSIC:
        music_on = !music_on;
        if (music_on) start_tune(); else stop_tune();
        break;
    case SETT_SOUNDFX:
        sound_fx_on = !sound_fx_on;
        if (!sound_fx_on) stop_samples();
        break;
    case SETT_SUBTITLES:
        subtitles_on = !subtitles_on;
        break;
    }
}

static const char *settings_labels[] = {
    "Difficulty", "Language", "Music", "Sound FX", "Subtitles"
};

/* menu_do_settings_menu  E1: ? | E2P: 0x42B6F0 */
void do_settings_menu(void) {
    /* Build visible items list — E1 has no difficulty */
    int items[SETT_MAX];
    int num_items = 0;
    if (game_version != GAME_VERSION_E1)
        items[num_items++] = SETT_DIFFICULTY;
    items[num_items++] = SETT_LANGUAGE;
    items[num_items++] = SETT_MUSIC;
    items[num_items++] = SETT_SOUNDFX;
    items[num_items++] = SETT_SUBTITLES;

    int sel = 0;

    int item_h = tx_h + 8;
    int item_w = 30 * tx_w;
    int panel_w = item_w + 16;
    int panel_h = num_items * item_h + 28;
    int px = (screen_width - panel_w) / 2;
    int py = (screen_height - panel_h) / 2;

    for (;;) {
        menu_frame_start();

        draw_panel(px, py, panel_w, panel_h);
        draw_panel_title("Settings", px, py, panel_w);

        int item_x = px + 8;
        int start_y = py + 22;

        for (int i = 0; i < num_items; i++) {
            char val[32];
            settings_get_value(items[i], val, sizeof(val));
            draw_setting_row(settings_labels[items[i]], val,
                             item_x, start_y + i * item_h, item_w,
                             i == sel, true);
        }

        if (menu_nav_up()) {
            if (sel > 0) sel--;
            platform_delay(120);
        }
        if (menu_nav_down()) {
            if (sel < num_items - 1) sel++;
            platform_delay(120);
        }

        if (menu_nav_left() || menu_nav_right()) {
            int dir = menu_nav_right() ? 1 : -1;
            settings_adjust(items[sel], dir);
            platform_delay(150);
        }

        if (menu_confirm()) {
            int id = items[sel];
            if (id == SETT_MUSIC || id == SETT_SOUNDFX || id == SETT_SUBTITLES)
                settings_adjust(id, 1);
            platform_delay(150);
        }

        if (menu_back()) return;

        menu_frame_end();
    }
}

/* menu_do_difficulty  E1: ? | E2P: 0x42B7C0 */
void do_difficulty(void) {
    do_settings_menu();
}

/* menu_do_language  E1: ? | E2P: 0x42B890 */
void do_language(void) {
    do_settings_menu();
}

/* menu_do_sound_settings  E1: ? | E2P: 0x42B960 */
void do_sound_settings(void) {
    do_settings_menu();
}

/* ══════════════════════════════════════════════════════════════
 *  Scene Playback
 * ══════════════════════════════════════════════════════════════ */

/* menu_delete_triangle  E1: 0x430650 | E2: 0x43A928 */
void delete_triangle(tri_t *tri) {
    if (!tri) return;
    actor_t *actor = tri->parent_actor;
    if (!actor) { free_event((event_t *)tri); return; }

    /* Unlink from actor's polygon triangle list */
    if (tri == actor->polygone_tri_list) {
        actor->polygone_tri_list = tri->next;
    } else {
        for (tri_t *t = actor->polygone_tri_list; t && t->next; t = t->next) {
            if (tri == t->next) {
                t->next = t->next->next;
                break;
            }
        }
    }

    /* Clear from triangle table */
    if (actor->_TriangleTab && tri->tri_index >= 0 && tri->tri_index < 500)
        actor->_TriangleTab->field_0[tri->tri_index] = NULL;

    free_event((event_t *)tri);
}

/* menu_delete_parts  E1: 0x4305A0 | E2: 0x43A878 */
void delete_parts(part_t *part) {
    if (!part) return;

    /* Clear from part table */
    if (part->parent_actor && part->parent_actor->_PartTab &&
        part->name_index >= 0 && part->name_index < 500)
        part->parent_actor->_PartTab->field_0[part->name_index] = NULL;

    /* Unlink from actor's display list */
    if (part->parent_actor) {
        part_t *dp = (part_t *)part->parent_actor->actor_parts_list;
        if (dp) {
            for (; dp->next_in_display_list; dp = dp->next_in_display_list) {
                if (part == dp->next_in_display_list) {
                    dp->next_in_display_list = dp->next_in_display_list->next_in_display_list;
                    break;
                }
            }
        }
    }

    /* Recursively delete child parts */
    part_t *child = (part_t *)part->actor_parts_list;
    while (child) {
        part_t *next_child = child->next;
        delete_parts(child);
        child = next_child;
    }

    /* Delete all triangles referencing this part's points */
    if (part->parent_actor) {
        for (point_t *pt = part->points_list; pt; pt = pt->next) {
            tri_t *tri = part->parent_actor->polygone_tri_list;
            while (tri) {
                tri_t *next_tri = tri->next;
                if (pt == tri->point1 || pt == tri->point2 ||
                    pt == tri->point3 || pt == tri->quad_point4)
                    delete_triangle(tri);
                tri = next_tri;
            }
            free_point(pt);
        }
    }

    free_part(part);
}

/* menu_remove_part  E1: 0x43081C | E2: 0x43AAF4 */
void remove_part(part_t *part) {
    if (!part) {
        do_info_req("No part to delete!");
        return;
    }

    actor_t *actor = part->holding_actor;

    /* Clear TwoPartsLimb flag if this part is the limb root */
    if (actor && actor->type != 7 && (actor->flags & 0x20) &&
        part == (part_t *)actor->actor_parts_list)
        actor->flags &= ~0x20;

    /* Also check grandparent */
    if (actor && actor->type != 7) {
        actor_t *grandparent = (actor_t *)actor->holding_actor;
        if (grandparent && grandparent->type != 7 &&
            (grandparent->flags & 0x20) &&
            grandparent->actor_parts_list &&
            part == ((part_t *)grandparent->actor_parts_list)->actor_parts_list)
            grandparent->flags &= ~0x20;
    }

    if (!actor || actor->type == 7) {
        do_info_req("Can't delete root part!");
        return;
    }

    /* Unlink from parent's child list */
    part_t *first = (part_t *)actor->actor_parts_list;
    if (part == first) {
        actor->actor_parts_list = (struct part_s *)first->next;
    } else {
        for (part_t *fp = first; fp; fp = fp->next_in_display_list) {
            if (part == fp->next) {
                fp->next = fp->next->next;
                break;
            }
            if (part == (part_t *)fp->actor_parts_list) {
                fp->actor_parts_list = ((part_t *)fp->actor_parts_list)->next;
                break;
            }
        }
    }

    delete_parts(part);
}

/* menu_play_dead_scene  E1: 0x430388 | E2: 0x43A660 */
void play_dead_scene(int sceneIndex) {
    stop_the_clock = true;
    break_do_movement = 1;
    stop_samples();
    initialise_game();

    check_scene_loaded((int16_t)sceneIndex);
    scene_t *scene = scene_tab[sceneIndex];
    if (!scene) return;

    active_camera = NULL;
    check_actors_in_scene_loaded(scene);

    script_t *first_script = scene->scene_script_list;
    if (first_script) {
        int16_t actorIndex = first_script->script_actor_index;
        if (actorIndex >= 0 && actorIndex < THING_TAB_SIZE) {
            selected_thing = thing_tab[actorIndex];
        }
    }

    start_scene(scene);
}

/* menu_delete_point  E2: 0x43A6C4 */
void delete_point(point_t *point) {
    part_t *part = point->parent_part;
    if (point == part->points_list) {
        part->points_list = point->next;
    } else {
        point_t *prev = part->points_list;
        while (prev) {
            if (prev->next == point) {
                prev->next = point->next;
                free_point(point);
                return;
            }
            if (!prev->next) break;
            prev = prev->next;
        }
    }
    free_point(point);
}

/* menu_delete_script  E2: 0x43AA94 */
void delete_script(script_t *script) {
    key_state_t *key = script->script_action.key_list;
    while (key) {
        key_state_t *next = key->next;
        delete_key(key);
        key = next;
    }
    free_script(script);
}

/* menu_find_svga_card  E2: 0x43AD30 */
void find_svga_card(void) {
    do_info_req("FindSVGACard not implemented for Win95");
}
