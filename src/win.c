/**
 * win.c
 *
 * Window glue: page flip, event pump and input mapping onto the game's key
 * globals. The original's Win32/DirectDraw code is replaced by platform.h.
 */

#include "win.h"
#include "debug_overlay.h"
#include "display.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "menu.h"
#include "platform.h"
#include "render.h"
#include "compat.h"
#include "prof.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void *hwnd = NULL;
bool app_active = true;
int16_t display_scale_mode = SCALE_PILLARBOX;

static platform_t *g_platform = NULL;

platform_t *win_platform(void) {
    return g_platform;
}

void win_set_scale_mode(int mode) {
    display_scale_mode = (int16_t)mode;
    if (g_platform)
        platform_set_scale_mode(g_platform, mode);
}

/* E1 speed mode (sneak/walk/run), driven by simulating the F-key groups the
 * game's script reads (see the L3 handler in window_proc()). Shared by the
 * Settings menu row and the pad's L3, which PSP and handheld Vita lack. */
static const int e1_speed_fkey[3] = { 0x70, 0x74, 0x78 };  /* F1, F5, F9 */
static int e1_speed_step = 1;                              /* game starts in walk */

void e1_cycle_speed_mode(int dir) {
    e1_speed_step = (e1_speed_step + (dir > 0 ? 1 : 2)) % 3;
    extra_keys_were_pressed[e1_speed_fkey[e1_speed_step]] = 1;
}

int e1_speed_mode_step(void) {
    return e1_speed_step;
}

/* win_flip_win95_458094 */
void flip_win95(void) {
    int pitch;
    char *plane_data = (char *)dd_lock(db, &pitch);

    if (render_backend == RENDER_HARDWARE) {
        /* The hardware path presents the composited frame itself; the software
         * plane it reads is the same one dd_lock just handed back. */
        render_frame_end();
        dd_unlock(db, plane_data);
        return;
    }

#ifdef ENABLE_FRAME_DUMP
    /* F12 — manual dump. Also auto-dump frames 60, 120, 180 for headless validation. */
    static bool f12_was_pressed = false;
    bool f12_now = platform_key_down(g_platform, PKEY_F12);
    static int s_auto_n = 0;
    s_auto_n++;
    bool auto_dump = (s_auto_n == 60 || s_auto_n == 120 || s_auto_n == 200 || s_auto_n == 300 || s_auto_n == 500 || s_auto_n == 800);
    if (((f12_now && !f12_was_pressed) || auto_dump) && plane_data) {
        static int dump_counter = 0;
        char filename[64];
        snprintf(filename, sizeof(filename), "frame_dump_%03d.ppm", dump_counter++);
        FILE *ppm = fopen(filename, "wb");
        if (ppm) {
            const uint8_t *pal = (const uint8_t *)view_cmap;
            int fb_total = screen_width * screen_height;
            fprintf(ppm, "P6\n%d %d\n255\n", screen_width, screen_height);
            for (int i = 0; i < fb_total; i++) {
                uint8_t idx = (uint8_t)plane_data[i];
                /* VGA 6-bit DAC to 8-bit */
                uint8_t r = (uint8_t)(pal[idx * 3 + 0] << 2);
                uint8_t g = (uint8_t)(pal[idx * 3 + 1] << 2);
                uint8_t b = (uint8_t)(pal[idx * 3 + 2] << 2);
                fwrite(&r, 1, 1, ppm);
                fwrite(&g, 1, 1, ppm);
                fwrite(&b, 1, 1, ppm);
            }
            fclose(ppm);
            fprintf(stderr, "[DUMP] Saved %s\n", filename);
        }
    }
    f12_was_pressed = f12_now;
#endif /* ENABLE_FRAME_DUMP */

    PROF_BEGIN(PROF_BLIT);
    platform_blit(g_platform, (const uint8_t *)plane_data, (const uint8_t *)view_cmap);
    PROF_END(PROF_BLIT);
    dd_unlock(db, plane_data);
}

/* Present current frame and pump events for the given duration (ms). */
void present_delay(int ms) {
    int frames = ms / 50;
    if (frames < 1) frames = 1;
    for (int i = 0; i < frames; i++) {
        flip_win95();
        window_proc();
        if (space_pressed || key_esc_was_pressed) break;
        platform_delay(50);
    }
}

/* win_window_proc_458110 */
void window_proc(void) {
    if (!platform_pump_events(g_platform)) {
        ecs_exit_now(0);
    }

    /* Game key arrays keep the original VK indexing; the platform layer
     * reports PKEY scancodes, so translate here. */
    space_pressed       = platform_key_down(g_platform, PKEY_SPACE);
    enter_pressed       = platform_key_down(g_platform, PKEY_RETURN);
    /* Latched edges: a held key must not fire CT_ANY_KEY_PRESSED /
     * CT_SPACE_PRESSED on several frames and cascade scene skips. */
    if (platform_key_pressed(g_platform, PKEY_SPACE))  space_was_pressed = true;
    if (platform_key_pressed(g_platform, PKEY_RETURN)) enter_was_pressed = true;
    key_esc_was_pressed = platform_key_pressed(g_platform, PKEY_ESCAPE);
    if (platform_key_pressed(g_platform, PKEY_I))      key_i_was_pressed = true;
    if (platform_key_pressed(g_platform, PKEY_RETURN))  key_return_was_pressed = true;
    ctrl_pressed        = platform_key_down(g_platform, PKEY_LCTRL);
    alt_pressed         = platform_key_down(g_platform, PKEY_LALT);

    /* Set, never cleared — the consumer clears its own entry. window_proc can
     * run twice before the consumer reads it (present_delay, menus). */
    static const struct { int pk, vk; } edge_keys[] = {
        { PKEY_1, 0x31 }, { PKEY_2, 0x32 }, { PKEY_3, 0x33 },
        { PKEY_A, 0x41 }, { PKEY_C, 0x43 }, { PKEY_D, 0x44 },
        { PKEY_M, 0x4D }, { PKEY_P, 0x50 }, { PKEY_Q, 0x51 },
        { PKEY_W, 0x57 },
    };
    for (int i = 0; i < (int)(sizeof(edge_keys) / sizeof(edge_keys[0])); i++)
        if (platform_key_pressed(g_platform, edge_keys[i].pk))
            keys_were_pressed_codes[edge_keys[i].vk] = 1;
    keys_pressed[0x5A]            = platform_key_down(g_platform,    PKEY_Z);

    int arrow_up    = platform_key_down(g_platform, PKEY_UP)    || platform_key_down(g_platform, PKEY_W);
    int arrow_down  = platform_key_down(g_platform, PKEY_DOWN)  || platform_key_down(g_platform, PKEY_S);
    int arrow_left  = platform_key_down(g_platform, PKEY_LEFT)  || platform_key_down(g_platform, PKEY_A);
    int arrow_right = platform_key_down(g_platform, PKEY_RIGHT) || platform_key_down(g_platform, PKEY_D);

    key7_pressed = platform_key_down(g_platform, PKEY_NUM7) || (arrow_up && arrow_left);
    key9_pressed = platform_key_down(g_platform, PKEY_NUM9) || (arrow_up && arrow_right);
    key1_pressed = platform_key_down(g_platform, PKEY_NUM1) || (arrow_down && arrow_left);
    key3_pressed = platform_key_down(g_platform, PKEY_NUM3) || (arrow_down && arrow_right);
    key8_pressed = platform_key_down(g_platform, PKEY_NUM8) || (arrow_up && !arrow_left && !arrow_right);
    key2_pressed = platform_key_down(g_platform, PKEY_NUM2) || (arrow_down && !arrow_left && !arrow_right);
    key4_pressed = platform_key_down(g_platform, PKEY_NUM4) || (arrow_left && !arrow_up && !arrow_down);
    key6_pressed = platform_key_down(g_platform, PKEY_NUM6) || (arrow_right && !arrow_up && !arrow_down);
    key5_pressed = platform_key_down(g_platform, PKEY_NUM5);

    /* BH_JOYSTICK reads DOS scancodes: 72 up, 75 left, 77 right, 80 down,
     * 56 Alt, 57 space, and keys_pressed[42] for Left Shift. */
    extra_keys_pressed[72] = platform_key_down(g_platform, PKEY_UP)    || platform_key_down(g_platform, PKEY_NUM8) || platform_key_down(g_platform, PKEY_W);
    extra_keys_pressed[75] = platform_key_down(g_platform, PKEY_LEFT)  || platform_key_down(g_platform, PKEY_NUM4) || platform_key_down(g_platform, PKEY_A);
    extra_keys_pressed[77] = platform_key_down(g_platform, PKEY_RIGHT) || platform_key_down(g_platform, PKEY_NUM6) || platform_key_down(g_platform, PKEY_D);
    extra_keys_pressed[80] = platform_key_down(g_platform, PKEY_DOWN)  || platform_key_down(g_platform, PKEY_NUM2) || platform_key_down(g_platform, PKEY_S);
    extra_keys_pressed[56] = platform_key_down(g_platform, PKEY_LALT);
    extra_keys_pressed[57] = platform_key_down(g_platform, PKEY_SPACE);
    extra_keys_pressed[71] = platform_key_down(g_platform, PKEY_NUM7) || platform_key_down(g_platform, PKEY_Q);
    extra_keys_pressed[73] = platform_key_down(g_platform, PKEY_NUM9) || platform_key_down(g_platform, PKEY_E);
    extra_keys_pressed[79] = platform_key_down(g_platform, PKEY_NUM1) || platform_key_down(g_platform, PKEY_Z);
    extra_keys_pressed[81] = platform_key_down(g_platform, PKEY_NUM3) || platform_key_down(g_platform, PKEY_C);
    keys_pressed[42]       = platform_key_down(g_platform, PKEY_LSHIFT);

    static const int fn_pk[12] = { PKEY_F1, PKEY_F2, PKEY_F3, PKEY_F4,
                                    PKEY_F5, PKEY_F6, PKEY_F7, PKEY_F8,
                                    PKEY_F9, PKEY_F10, PKEY_F11, PKEY_F12 };
    static const int fn_vk[12] = { 0x70, 0x71, 0x72, 0x73,
                                    0x74, 0x75, 0x76, 0x77,
                                    0x78, 0x79, 0x7A, 0x7B };
    for (int i = 0; i < 12; i++)
        if (platform_key_pressed(g_platform, fn_pk[i]))
            extra_keys_were_pressed[fn_vk[i]] = 1;

    if (platform_key_down(g_platform, PKEY_LCMD) &&
        platform_key_pressed(g_platform, PKEY_D))
        debug_overlay_active ^= 1;

    /* G: original / enhanced graphics, only in a running game. The latch
     * survives taps within one 50ms present_delay pump, and consuming it first
     * stops set_enhanced_graphics, which re-enters window_proc, from seeing
     * the press again. */
    if (platform_key_hit(g_platform, PKEY_G))
        graphics_mode_cycle(1);

    int mx, my;
    int mb = platform_mouse_state(g_platform, &mx, &my);
    mouse_x = mx;
    mouse_y = my;
    if (mb & 1) mouse = 2;  /* left down */
    if (mb & 2) mouse = 8;  /* right down */

    /* Gamepad, ORed into the key globals. Left stick is the legs, the shoulder
     * row the arms (left side, left hand); face buttons do the rest.
     *
     * Shared (matches controls.md):
     *   Left stick / D-pad    → legs: walk and turn, eight ways
     *   A / Cross  (south)    → Space: reach out — pick up, interact, confirm
     *   B / Circle (east)     → Escape: back / cancel
     *   Y / Triangle (north)  → Enter: inventory
     *   X / Square (west)     → Left Alt: use what is held
     *   LB / L1               → Left Shift: jump
     *   Start                 → Escape: pause menu
     *   Select / Back         → I: toggle HUD icons
     *   Right stick click     → G: original / enhanced graphics
     *
     * E1 — two hands the player drives independently:
     *   LT / L2               → Numpad 1 / Z: LEFT hand pick up / drop
     *   RT / R2               → Numpad 3 / C: RIGHT hand pick up / drop
     *   RB / R1               → Left Ctrl: attack with the stick (incl. low)
     *   Right stick ← / →     → Numpad 7 / 9: quick left and right swing
     *   Left stick click      → speed mode cycle (F1 / F5 / F9)
     *
     * E2 — one pick-up action, modifiers instead of per-hand keys:
     *   RB / R1, RT / R2      → Left Ctrl: run, and attack with the stick
     *   LT / L2               → magic / special with the stick, and with
     *                           RB held, an aimed attack
     *
     * E2's LT raises alt_pressed without scancode 56: BH_JOYSTICK tests 56
     * first, so a control raising both could never reach the magic (196..199)
     * or aimed-attack (204..211) actions.
     */
    platform_gamepad_state_t gp;
    platform_gamepad_poll(g_platform, &gp);

    /* ECSTATICA_GAMEPAD_DEBUG=2: dump the decoded pad state when it changes. */
    static int pad_debug = -1;
    if (pad_debug < 0) {
        const char *e = getenv("ECSTATICA_GAMEPAD_DEBUG");
        pad_debug = e ? atoi(e) : 0;
    }
    if (pad_debug >= 2) {
        /* Coarse steps, so analog jitter does not reprint every frame. */
        int lx = gp.left_x / 4096, ly = gp.left_y / 4096;
        int rx = gp.right_x / 4096, ry = gp.right_y / 4096;
        int buttons =
            (gp.btn_south << 0) | (gp.btn_east << 1) | (gp.btn_west << 2) |
            (gp.btn_north << 3) | (gp.btn_lb << 4) | (gp.btn_rb << 5) |
            (gp.btn_lt << 6) | (gp.btn_rt << 7) | (gp.btn_start << 8) |
            (gp.btn_select << 9) | (gp.btn_lstick << 10) | (gp.btn_rstick << 11) |
            (gp.dpad_up << 12) | (gp.dpad_down << 13) | (gp.dpad_left << 14) |
            (gp.dpad_right << 15) | (gp.connected << 16);

        static int prev_buttons = -1, prev_lx, prev_ly, prev_rx, prev_ry;
        if (buttons != prev_buttons || lx != prev_lx || ly != prev_ly ||
            rx != prev_rx || ry != prev_ry) {
            fprintf(stderr, "[PAD] conn=%d L=%6d,%6d R=%6d,%6d dpad=%d%d%d%d "
                    "S=%d E=%d W=%d N=%d LB=%d RB=%d LT=%d RT=%d "
                    "start=%d sel=%d L3=%d R3=%d\n",
                    gp.connected, gp.left_x, gp.left_y, gp.right_x, gp.right_y,
                    gp.dpad_up, gp.dpad_down, gp.dpad_left, gp.dpad_right,
                    gp.btn_south, gp.btn_east, gp.btn_west, gp.btn_north,
                    gp.btn_lb, gp.btn_rb, gp.btn_lt, gp.btn_rt,
                    gp.btn_start, gp.btn_select, gp.btn_lstick, gp.btn_rstick);
        }
        prev_buttons = buttons;
        prev_lx = lx; prev_ly = ly; prev_rx = rx; prev_ry = ry;
    }

    /* Outside the connected test, so unplugging with a button held cannot
     * leave a latch stuck. */
    static bool btn_south_was_pressed = false;
    static bool btn_east_was_pressed = false;
    static bool btn_start_was_pressed = false;
    static bool btn_north_was_pressed = false;
    static bool btn_select_was_pressed = false;
    static bool rstick_was_pressed = false;
    static bool lstick_was_pressed = false;

    if (!gp.connected) {
        btn_south_was_pressed = btn_east_was_pressed = btn_start_was_pressed =
            btn_north_was_pressed = btn_select_was_pressed =
            rstick_was_pressed = lstick_was_pressed = false;
    } else {
        /* Resolve the stick radially: per-axis thresholds hand out a diagonal
         * for most of the travel, which under E2 turns "walk forward" into
         * "walk while turning". Distance decides whether it is pushed, then
         * the angle picks one of eight 45-degree sectors. */
        int dz = GAMEPAD_STICK_DEADZONE;
        int ax = gp.left_x < 0 ? -gp.left_x : gp.left_x;
        int ay = gp.left_y < 0 ? -gp.left_y : gp.left_y;
        bool pushed = (int64_t)ax * ax + (int64_t)ay * ay > (int64_t)dz * dz;
        /* 24/10 approximates tan(67.5) = 2.414 — the sector boundary. */
        bool stick_vert = pushed && ay * 10 > ax * 24;
        bool stick_horz = pushed && ax * 10 > ay * 24;
        bool stick_diag = pushed && !stick_vert && !stick_horz;

        bool gp_up    = gp.dpad_up    || ((stick_vert || stick_diag) && gp.left_y > 0);
        bool gp_down  = gp.dpad_down  || ((stick_vert || stick_diag) && gp.left_y < 0);
        bool gp_left  = gp.dpad_left  || ((stick_horz || stick_diag) && gp.left_x < 0);
        bool gp_right = gp.dpad_right || ((stick_horz || stick_diag) && gp.left_x > 0);

        if (pad_debug >= 2) {
            static int prev_dir = -1;
            int dir = (gp_up << 3) | (gp_down << 2) | (gp_left << 1) | gp_right;
            if (dir != prev_dir) {
                fprintf(stderr, "[PAD] dir up=%d down=%d left=%d right=%d "
                        "(L=%d,%d)\n", gp_up, gp_down, gp_left, gp_right,
                        gp.left_x, gp.left_y);
                prev_dir = dir;
            }
        }

        key8_pressed |= gp_up    && !gp_left && !gp_right;
        key2_pressed |= gp_down  && !gp_left && !gp_right;
        key4_pressed |= gp_left  && !gp_up   && !gp_down;
        key6_pressed |= gp_right && !gp_up   && !gp_down;
        key7_pressed |= gp_up    && gp_left;
        key9_pressed |= gp_up    && gp_right;
        key1_pressed |= gp_down  && gp_left;
        key3_pressed |= gp_down  && gp_right;

        extra_keys_pressed[72] |= gp_up;
        extra_keys_pressed[80] |= gp_down;
        extra_keys_pressed[75] |= gp_left;
        extra_keys_pressed[77] |= gp_right;

        space_pressed |= gp.btn_south;
        if (gp.btn_south && !btn_south_was_pressed) space_was_pressed = true;
        btn_south_was_pressed = gp.btn_south;
        extra_keys_pressed[57] |= gp.btn_south;

        if (gp.btn_east && !btn_east_was_pressed) key_esc_was_pressed = true;
        if (gp.btn_start && !btn_start_was_pressed) key_esc_was_pressed = true;
        btn_east_was_pressed = gp.btn_east;
        btn_start_was_pressed = gp.btn_start;

        alt_pressed |= gp.btn_west;
        extra_keys_pressed[56] |= gp.btn_west;

        enter_pressed |= gp.btn_north;
        if (gp.btn_north && !btn_north_was_pressed) {
            enter_was_pressed = true;
            key_return_was_pressed = true;
        }
        btn_north_was_pressed = gp.btn_north;

        keys_pressed[42] |= gp.btn_lb;

        /* RB → Left Ctrl. RT joins it under E2 only: under E1 the triggers are
         * the hands, and every right-hand pick-up would become an attack. */
        ctrl_pressed |= gp.btn_rb || (gp.btn_rt && game_version != GAME_VERSION_E1);

        if (gp.btn_select && !btn_select_was_pressed) key_i_was_pressed = true;
        btn_select_was_pressed = gp.btn_select;

        if (game_version == GAME_VERSION_E1) {
            /* Each trigger picks up with, or puts down from, its own hand. */
            extra_keys_pressed[79] |= gp.btn_lt;   /* Num1 / Z — left hand  */
            extra_keys_pressed[81] |= gp.btn_rt;   /* Num3 / C — right hand */

            /* Quick swings. Horizontal axis only: these scancodes suppress
             * BH_JOYSTICK's diagonals. */
            int rs_dz = GAMEPAD_STICK_DEADZONE;
            extra_keys_pressed[71] |= gp.right_x < -rs_dz;  /* Num7 / Q left swing  */
            extra_keys_pressed[73] |= gp.right_x >  rs_dz;  /* Num9 / E right swing */
        } else {
            /* Not scancode 56 — see the gamepad comment above. */
            alt_pressed |= gp.btn_lt;
        }

        bool rstick_edge = gp.btn_rstick && !rstick_was_pressed;
        rstick_was_pressed = gp.btn_rstick;   /* latch before the call */
        if (rstick_edge) {
            if (pad_debug >= 2)
                fprintf(stderr, "[PAD] R3: hires_available=%d mode_svga=%d -> %d\n",
                        (int)hires_available, (int)mode_svga, (int)!mode_svga);
            set_enhanced_graphics(!mode_svga);
            if (pad_debug >= 2)
                fprintf(stderr, "[PAD] R3: now mode_svga=%d\n", (int)mode_svga);
        }

        /* Left stick click: speed mode under E1, HUD toggle under E2. The step
         * is counted here because get_joystick leaves movement_speed_mode
         * untouched when E1's Key_F* script codes exist. */
        if (gp.btn_lstick && !lstick_was_pressed) {
            if (game_version == GAME_VERSION_E1) {
                e1_cycle_speed_mode(1);
                if (pad_debug >= 2) {
                    int step = e1_speed_mode_step();
                    fprintf(stderr, "[PAD] L3: speed step %d (F%d)\n",
                            step, step == 0 ? 1 : step == 1 ? 5 : 9);
                }
            } else {
                key_i_was_pressed = true;   /* I: toggle HUD icons */
                if (pad_debug >= 2)
                    fprintf(stderr, "[PAD] L3: HUD toggle\n");
            }
        }
        lstick_was_pressed = gp.btn_lstick;

        if (pad_debug >= 2) {
            /* The game keys the mapping produced. */
            static int prev_keys = -1;
            int keys =
                (space_pressed << 0) | (ctrl_pressed << 1) | (alt_pressed << 2) |
                (extra_keys_pressed[56] << 3) | (keys_pressed[42] << 4) |
                (extra_keys_pressed[71] << 5) | (extra_keys_pressed[73] << 6) |
                (extra_keys_pressed[79] << 7) | (extra_keys_pressed[81] << 8) |
                (enter_pressed << 9);
            if (keys != prev_keys) {
                fprintf(stderr, "[PAD] keys space=%d ctrl=%d alt=%d sc56=%d "
                        "shift=%d sc71=%d sc73=%d sc79=%d sc81=%d enter=%d\n",
                        space_pressed, ctrl_pressed, alt_pressed,
                        extra_keys_pressed[56], keys_pressed[42],
                        extra_keys_pressed[71], extra_keys_pressed[73],
                        extra_keys_pressed[79], extra_keys_pressed[81],
                        enter_pressed);
                prev_keys = keys;
            }
        }
    }
}

/* win_do_init_458714 */
void do_init(void) {
    const char *title = (game_version == GAME_VERSION_E2) ? "Ecstatica II" : "Ečstatica";
    g_platform = platform_init(title, 640, 480, 1);
    if (!g_platform) {
        quit("Platform initialization failed");
    }
    if (screen_width != 640 || screen_height != 480)
        platform_set_render_size(g_platform, screen_width, screen_height);
    app_active = 1;
}

void win_set_render_size(int w, int h) {
    if (g_platform)
        platform_set_render_size(g_platform, w, h);
}

/* win_change_screen_mode_win95  E1: 0x44A7C0 | E2: 0x458770 */
void change_screen_mode_win95(void) {
}

/* win_win_main_game_458B84 — the original WinMain body. */
void win_main_game(void) {
    /* Resolve the version before do_init(), which picks the window title.
     * init() detects again later; the probe is idempotent. */
    detect_game_version();
    do_init();
    setup();
}

/* win_make_code_writable_458000 — patched PE section flags; not needed. */
void make_code_writable(void) {
}

/* win_get_windows_directory_win95  E1: 0x44A9E8 | E2: 0x458998 */
void get_windows_directory_win95(void) {
}
