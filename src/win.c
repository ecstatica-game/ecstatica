/**
 * win.c
 *
 * Platform / window management:
 *   Original Win32/DirectDraw code replaced with cross-platform stubs
 *   that delegate to the platform.h abstraction layer.
 *
 * 8 functions prefixed win_ in the original ASM.
 */

#include "win.h"
#include "debug_overlay.h"
#include "display.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "menu.h"
#include "platform.h"
#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void *hwnd = NULL;
bool app_active = true;

static platform_t *g_platform = NULL;

platform_t *win_platform(void) {
    return g_platform;
}

/* win_flip_win95_458094
 * Page flip — present the back buffer.
 * Original used DirectDraw IDirectDrawSurface::Flip().
 */
void flip_win95(void) {
    int pitch;
    char *plane_data = (char *)dd_lock(db, &pitch);

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

    platform_blit(g_platform, (const uint8_t *)plane_data, (const uint8_t *)view_cmap);
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

/* win_window_proc_458110
 * Original Win32 WndProc — keyboard/mouse event handler.
 * Now handled by platform_pump_events() in the platform layer.
 */
void window_proc(void) {
    if (!platform_pump_events(g_platform)) {
        ecs_exit_now(0);
    }

    /* Map platform key states (PKEY scancodes) to game globals.
     * Bug 11 fix: previously called platform_key_* with Win32 VK codes
     * (0x1B, 0x20, 0x41 etc.) but platform stores at PKEY indices (0x01,
     * 0x39, 0x1E etc.) from `macos_vk_to_pkey()` — mismatch meant NO key
     * ever registered. game_t arrays `keys_were_pressed_codes[]` keep VK
     * indexing for compat; translate PKEY→VK at storage. */
    space_pressed       = platform_key_down(g_platform, PKEY_SPACE);
    enter_pressed       = platform_key_down(g_platform, PKEY_RETURN);
    /* Edge-triggered variants (latched, consumer clears). Scripts checking
     * CT_ANY_KEY_PRESSED / CT_SPACE_PRESSED must use these so a held key
     * doesn't fire the token multiple frames → cascading scene skips. */
    if (platform_key_pressed(g_platform, PKEY_SPACE))  space_was_pressed = true;
    if (platform_key_pressed(g_platform, PKEY_RETURN)) enter_was_pressed = true;
    key_esc_was_pressed = platform_key_pressed(g_platform, PKEY_ESCAPE);
    if (platform_key_pressed(g_platform, PKEY_I))      key_i_was_pressed = true;
    if (platform_key_pressed(g_platform, PKEY_RETURN))  key_return_was_pressed = true;
    ctrl_pressed        = platform_key_down(g_platform, PKEY_LCTRL);
    alt_pressed         = platform_key_down(g_platform, PKEY_LALT);

    /* Set, never cleared — the consumer clears its own entry, the way the
     * F-key block below already works. Plain assignment would wipe a press
     * that nothing has read yet whenever window_proc runs twice before the
     * consumer does, which it does through present_delay and the menus. */
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

    /* BH_JOYSTICK reads extra_keys_pressed at DOS scancodes for movement,
     * and keys_pressed[42] for Left Shift jump. Accept arrow keys OR numpad
     * for movement. Scancodes: 72=NUM8/up, 75=NUM4/left, 77=NUM6/right,
     * 80=NUM2/down. Alt at scancode 56, space at 57. */
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

    /* Cmd+D: toggle debug overlay */
    if (platform_key_down(g_platform, PKEY_LCMD) &&
        platform_key_pressed(g_platform, PKEY_D))
        debug_overlay_active ^= 1;

    /* G: switch between the original and enhanced graphics sets.
     * Only while a game is actually running — set_enhanced_graphics rebuilds
     * icons and parts, which have no meaning on the title screen or mid-intro.
     * A no-op when there is no hi-res data to switch to. */
    /* platform_key_hit consumes one latched press. It survives a tap whose
     * down and up land in the same pump — on the title screen and during the
     * intro, pumps are 50ms apart (present_delay), so a level or prev/now
     * comparison drops most presses. Consuming the latch before the call also
     * stops set_enhanced_graphics, which re-enters window_proc through
     * make_game_screen, from seeing the same press again. */
    if (platform_key_hit(g_platform, PKEY_G))
        set_enhanced_graphics(!mode_svga);

    /* mouse */
    int mx, my;
    int mb = platform_mouse_state(g_platform, &mx, &my);
    mouse_x = mx;
    mouse_y = my;
    if (mb & 1) mouse = 2;  /* left down */
    if (mb & 2) mouse = 8;  /* right down */

    /* Gamepad — OR into existing key globals.
     *
     * The pad is laid out the way both games model the body: the left stick
     * is the legs, and the shoulder row is the arms, left side for the left
     * hand and right side for the right. The face buttons carry what is not
     * limb-shaped — reach out, use, inventory, back.
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
     * LT under E2 raises alt_pressed WITHOUT scancode 56. BH_JOYSTICK tests
     * that scancode before it tests alt_pressed, so a control that raises both
     * — X here, Left Alt on the keyboard — always lands on use-item/roll and
     * can never reach the magic (196..199) or aimed-attack (204..211) actions
     * behind the later branches. Splitting them across two pad controls is what
     * makes those actions reachable at all.
     */
    platform_gamepad_state_t gp;
    platform_gamepad_poll(g_platform, &gp);

    /* ECSTATICA_GAMEPAD_DEBUG=2: dump the decoded pad state whenever it
     * changes, so a mis-assigned control can be traced back to the slot it
     * came from rather than guessed at from what the character did. */
    static int pad_debug = -1;
    if (pad_debug < 0) {
        const char *e = getenv("ECSTATICA_GAMEPAD_DEBUG");
        pad_debug = e ? atoi(e) : 0;
    }
    if (pad_debug >= 2) {
        /* Sticks are snapped to a coarse step so analog jitter alone does not
         * reprint the line every frame. */
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

    /* Edge latches live outside the connected test so unplugging a pad with a
     * button held does not leave a latch stuck set, swallowing the first press
     * after it comes back. */
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
        /* Resolve the stick radially, not one axis at a time. Thresholding
         * each axis on its own makes the corners of the square the only place
         * a diagonal lives and hands out a diagonal for anything else past the
         * deadzone on both — under E2 that turns "walk forward" into "walk
         * forward while turning" for most of the stick's travel. Distance
         * decides whether the stick is pushed at all, then the angle picks one
         * of eight 45-degree sectors: a secondary axis under tan(22.5) of the
         * primary is the player aiming straight. */
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

        /* Movement directions (same logic as keyboard arrows) */
        key8_pressed |= gp_up    && !gp_left && !gp_right;
        key2_pressed |= gp_down  && !gp_left && !gp_right;
        key4_pressed |= gp_left  && !gp_up   && !gp_down;
        key6_pressed |= gp_right && !gp_up   && !gp_down;
        key7_pressed |= gp_up    && gp_left;
        key9_pressed |= gp_up    && gp_right;
        key1_pressed |= gp_down  && gp_left;
        key3_pressed |= gp_down  && gp_right;

        /* BH_JOYSTICK movement scancodes */
        extra_keys_pressed[72] |= gp_up;
        extra_keys_pressed[80] |= gp_down;
        extra_keys_pressed[75] |= gp_left;
        extra_keys_pressed[77] |= gp_right;

        /* A → Space (interact / pick up) — level + edge */
        space_pressed |= gp.btn_south;
        if (gp.btn_south && !btn_south_was_pressed) space_was_pressed = true;
        btn_south_was_pressed = gp.btn_south;
        extra_keys_pressed[57] |= gp.btn_south;

        /* B / Start → Escape (menu) — edge-triggered */
        if (gp.btn_east && !btn_east_was_pressed) key_esc_was_pressed = true;
        if (gp.btn_start && !btn_start_was_pressed) key_esc_was_pressed = true;
        btn_east_was_pressed = gp.btn_east;
        btn_start_was_pressed = gp.btn_start;

        /* X → Left Alt: use what is held (or flip / roll with empty hands) */
        alt_pressed |= gp.btn_west;
        extra_keys_pressed[56] |= gp.btn_west;

        /* Y → Enter (inventory) — edge-triggered */
        enter_pressed |= gp.btn_north;
        if (gp.btn_north && !btn_north_was_pressed) {
            enter_was_pressed = true;
            key_return_was_pressed = true;
        }
        btn_north_was_pressed = gp.btn_north;

        /* LB → Left Shift (jump) */
        keys_pressed[42] |= gp.btn_lb;

        /* RB → Left Ctrl: run under E2, attack with a direction under both.
         * RT joins it under E2, where the right trigger has no hand to drive;
         * under E1 the triggers are the hands and RT must not double as Ctrl,
         * or every right-hand pick-up would come out as an attack. */
        ctrl_pressed |= gp.btn_rb || (gp.btn_rt && game_version != GAME_VERSION_E1);

        /* Select → I (toggle HUD) — edge-triggered */
        if (gp.btn_select && !btn_select_was_pressed) key_i_was_pressed = true;
        btn_select_was_pressed = gp.btn_select;

        if (game_version == GAME_VERSION_E1) {
            /* The hands, on the side of the pad they are on the body: each
             * trigger picks up with that hand, or puts down what it holds. */
            extra_keys_pressed[79] |= gp.btn_lt;   /* Num1 / Z — left hand  */
            extra_keys_pressed[81] |= gp.btn_rt;   /* Num3 / C — right hand */

            /* Quick swings, without letting go of the movement stick. Only
             * the horizontal axis: these scancodes suppress BH_JOYSTICK's
             * diagonals, and the vertical axis used to sit on the hand keys,
             * where a stick nudged while turning dropped whatever was held. */
            int rs_dz = GAMEPAD_STICK_DEADZONE;
            extra_keys_pressed[71] |= gp.right_x < -rs_dz;  /* Num7 / Q left swing  */
            extra_keys_pressed[73] |= gp.right_x >  rs_dz;  /* Num9 / E right swing */
        } else {
            /* E2 has no per-hand keys; the left trigger is the magic and
             * aimed-attack modifier instead. Deliberately not scancode 56 —
             * see the header comment. */
            alt_pressed |= gp.btn_lt;
        }

        /* Right stick click: graphics set toggle (same as G) */
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

        /* Left stick click: the legs again — speed mode under E1, where the
         * F-key groups it drives are read, and the HUD toggle under E2, which
         * has no speed modes and would otherwise leave the button dead.
         *
         * The step is counted here rather than read back from
         * movement_speed_mode: get_joystick only writes that variable when the
         * game data has no Key_F1_4 / Key_F5_8 / Key_F9_12 script code, and
         * when it does — E1's data does — it runs the script and leaves the
         * variable at its startup value. Deriving the next step from it sent
         * the same F-key on every click. */
        if (gp.btn_lstick && !lstick_was_pressed) {
            if (game_version == GAME_VERSION_E1) {
                /* F1 sneak, F5 walk, F9 run. The game starts in walk. */
                static const int speed_fkey[3] = { 0x70, 0x74, 0x78 };
                static int speed_step = 1;
                speed_step = (speed_step + 1) % 3;
                extra_keys_were_pressed[speed_fkey[speed_step]] = 1;
                if (pad_debug >= 2)
                    fprintf(stderr, "[PAD] L3: speed step %d (F%d)\n",
                            speed_step, speed_step == 0 ? 1 : speed_step == 1 ? 5 : 9);
            } else {
                key_i_was_pressed = true;   /* I: toggle HUD icons */
                if (pad_debug >= 2)
                    fprintf(stderr, "[PAD] L3: HUD toggle\n");
            }
        }
        lstick_was_pressed = gp.btn_lstick;

        if (pad_debug >= 2) {
            /* The game keys the mapping actually produced — the other half of
             * tracing a control that lands on the wrong action. */
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

/* win_do_init_458714
 * Original created Win32 window + DirectDraw surfaces.
 * Now delegates to platform_init().
 */
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
    /* display_mode switching between VGA/SVGA handled by set_vga/svga_constants.
     * No hardware mode change needed in modern platform layer. */
}

/* win_win_main_game_458B84
 * Original WinMain entry point. Now called from main.c.
 */
void win_main_game(void) {
    /* Resolve the version before do_init(), which picks the window title.
     * init() detects again later; the probe is idempotent. */
    detect_game_version();
    do_init();
    // if_editor_show_cursor();
    setup();
}

/* win_make_code_writable_458000
 * Original patched PE section flags for self-modifying code.
 * Not needed in the C port.
 */
void make_code_writable(void) {
    /* No-op in C port */
}

/* win_get_windows_directory_win95  E1: 0x44A9E8 | E2: 0x458998 */
void get_windows_directory_win95(void) {
    /* Original called GetWindowsDirectory() for path resolution.
     * Now handled by setup_directory_paths() using relative paths. */
}
