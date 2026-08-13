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
#include "init.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

void *hwnd = NULL;
bool app_active = true;

static platform_t *g_platform = NULL;

/* win_flip_win95_458094
 * Page flip — present the back buffer.
 * Original used DirectDraw IDirectDrawSurface::Flip().
 */
void flip_win95(void) {
    int pitch;
    char *plane_data = (char *)dd_lock(db, &pitch);

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
        _exit(0);
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

    keys_were_pressed_codes[0x31] = platform_key_pressed(g_platform, PKEY_1);
    keys_were_pressed_codes[0x32] = platform_key_pressed(g_platform, PKEY_2);
    keys_were_pressed_codes[0x33] = platform_key_pressed(g_platform, PKEY_3);
    keys_were_pressed_codes[0x41] = platform_key_pressed(g_platform, PKEY_A);
    keys_were_pressed_codes[0x43] = platform_key_pressed(g_platform, PKEY_C);
    keys_were_pressed_codes[0x44] = platform_key_pressed(g_platform, PKEY_D);
    keys_were_pressed_codes[0x4D] = platform_key_pressed(g_platform, PKEY_M);
    keys_were_pressed_codes[0x50] = platform_key_pressed(g_platform, PKEY_P);
    keys_were_pressed_codes[0x51] = platform_key_pressed(g_platform, PKEY_Q);
    keys_were_pressed_codes[0x57] = platform_key_pressed(g_platform, PKEY_W);
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

    /* mouse */
    int mx, my;
    int mb = platform_mouse_state(g_platform, &mx, &my);
    mouse_x = mx;
    mouse_y = my;
    if (mb & 1) mouse = 2;  /* left down */
    if (mb & 2) mouse = 8;  /* right down */

    /* Gamepad — OR into existing key globals.
     *
     * Mapping (matches controls.md):
     *   Left stick / D-pad    → movement (up/down/left/right + diagonals)
     *   A / Cross  (south)    → Space: pick up / interact
     *   B / Circle (east)     → Escape: menu / back
     *   X / Square (west)     → Left Alt: use item / flip / roll / magic
     *   Y / Triangle (north)  → Enter: inventory
     *   LB / L1               → Left Shift: jump
     *   RB / R1               → Left Ctrl: run / attack modifier
     *   LT / L2               → Left Alt: magic/special modifier (E2)
     *   RT / R2               → Left Ctrl: attack modifier (alternative)
     *   Start                 → Escape: pause menu
     *   Select / Back         → I: toggle HUD icons
     *   Left stick click      → speed mode cycle (1→2→3)
     */
    platform_gamepad_state_t gp;
    platform_gamepad_poll(g_platform, &gp);

    if (gp.connected) {
        int dz = GAMEPAD_STICK_DEADZONE;
        bool gp_up    = gp.dpad_up    || gp.left_y >  dz;
        bool gp_down  = gp.dpad_down  || gp.left_y < -dz;
        bool gp_left  = gp.dpad_left  || gp.left_x < -dz;
        bool gp_right = gp.dpad_right || gp.left_x >  dz;

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

        /* A → Space (interact / pick up) */
        space_pressed |= gp.btn_south;
        if (gp.btn_south) space_was_pressed = true;
        extra_keys_pressed[57] |= gp.btn_south;

        /* B / Start → Escape (menu) */
        if (gp.btn_east || gp.btn_start) key_esc_was_pressed = true;

        /* X / LT → Left Alt (use item / magic) */
        alt_pressed |= gp.btn_west || gp.btn_lt;
        extra_keys_pressed[56] |= gp.btn_west || gp.btn_lt;

        /* Y → Enter (inventory) */
        enter_pressed |= gp.btn_north;
        if (gp.btn_north) {
            enter_was_pressed = true;
            key_return_was_pressed = true;
        }

        /* LB → Left Shift (jump) */
        keys_pressed[42] |= gp.btn_lb;

        /* RB / RT → Left Ctrl (run / attack modifier) */
        ctrl_pressed |= gp.btn_rb || gp.btn_rt;

        /* Select → I (toggle HUD) */
        if (gp.btn_select) key_i_was_pressed = true;

        /* E1 combat: Q/E mapped to numpad7/numpad9.
         * Right stick horizontal maps to combat swings. */
        int rs_dz = GAMEPAD_STICK_DEADZONE;
        extra_keys_pressed[71] |= gp.right_x < -rs_dz;  /* Q / Num7 left swing */
        extra_keys_pressed[73] |= gp.right_x >  rs_dz;   /* E / Num9 right swing */

        /* E1 item handling: Z/C mapped to numpad1/numpad3.
         * Right stick vertical maps to hand pick/drop. */
        extra_keys_pressed[79] |= gp.right_y < -rs_dz;   /* Z / Num1 left hand */
        extra_keys_pressed[81] |= gp.right_y >  rs_dz;    /* C / Num3 right hand */

        /* Left stick click: cycle speed mode (sneak → walk → run) */
        static bool lstick_was_pressed = false;
        if (gp.btn_lstick && !lstick_was_pressed) {
            if (movement_speed_mode <= 3)
                extra_keys_were_pressed[0x74] = 1;      /* F5: walk */
            else if (movement_speed_mode <= 7)
                extra_keys_were_pressed[0x78] = 1;      /* F9: run */
            else
                extra_keys_were_pressed[0x70] = 1;      /* F1: sneak */
        }
        lstick_was_pressed = gp.btn_lstick;
    }
}

/* win_do_init_458714
 * Original created Win32 window + DirectDraw surfaces.
 * Now delegates to platform_init().
 */
void do_init(void) {
    const char *title = (game_version == GAME_VERSION_E2) ? "Ecstatica II" : "Ecstatica";
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
