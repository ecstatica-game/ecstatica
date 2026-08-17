/**
 * openfpga.c
 *
 * Platform backend for openfpgaOS (Analogue Pocket / MiSTer), built
 * against the openfpgaSDK: VexiiRiscv rv32imafc @ 100 MHz, 64 MB SDRAM,
 * 320x240 8-bit indexed video, 32-voice PCM mixer, sample-based MIDI synth.
 *
 * Game data ships as an ISO 9660 image in an APF data slot; it is mounted
 * read-only at /game and the file layer is pointed at that root, so the
 * engine's own case-insensitive path resolution works unchanged.
 *
 * Built by pocket/Makefile, which copies the engine sources flat into an
 * SDK app directory. Sources are flat there, hence "platform.h" and not
 * "../platform.h".
 */

#include "of.h"
#include "platform.h"
#include "asm_f.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Video ──────────────────────────────────────────────────── */

#define PKEY_TABLE_SIZE 256

struct platform_t {
    int render_w, render_h;
    int mode_w, mode_h, mode_stride;

    uint8_t last_pal[768];
    bool pal_valid;

    bool keys_now[PKEY_TABLE_SIZE];
    bool keys_prev[PKEY_TABLE_SIZE];
    bool keys_latch[PKEY_TABLE_SIZE];

    int mouse_x, mouse_y;
    int mouse_buttons;

    uint32_t start_ms;
};

static platform_t g_plat;

/* Pick a scanout mode for a w*h source.
 *
 * Only modes the OS advertises can be set — of_video_get_mode_info shares
 * of_video_set_mode's validation, so an enumerated mode always takes. The
 * core's video.json lists 320x200 alongside 320x240, so the game's VGA mode
 * normally lands exactly and the Pocket scaler fills the panel; anything
 * without an exact match letterboxes into the smallest mode that holds it. */
static void apply_video_mode(platform_t *p, int w, int h) {
    of_video_mode_t modes[16];
    int count = of_video_get_mode_count();
    if (count > (int)(sizeof(modes) / sizeof(modes[0])))
        count = (int)(sizeof(modes) / sizeof(modes[0]));

    int n = 0;
    for (int i = 0; i < count; i++)
        if (of_video_get_mode_info(i, &modes[n]) == 0 &&
            modes[n].color_mode == OF_VIDEO_MODE_8BIT)
            n++;

    int best = -1;
    for (int i = 0; i < n; i++) {
        if (modes[i].width == w && modes[i].height == h) { best = i; break; }
        if (modes[i].width < w || modes[i].height < h)
            continue;
        if (best < 0 ||
            (uint32_t)modes[i].width * modes[i].height <
            (uint32_t)modes[best].width * modes[best].height)
            best = i;
    }

    if (best < 0) {
        /* Source is larger than anything advertised — stay put and let the
         * blit clip. Only reachable if the hi-res data set is installed on
         * a core whose video.json omits 640x480. */
        return;
    }
    if (modes[best].width == p->mode_w && modes[best].height == p->mode_h)
        return;

    if (of_video_set_mode(&modes[best]) != 0)
        return;

    p->mode_w = modes[best].width;
    p->mode_h = modes[best].height;
    p->mode_stride = modes[best].stride ? modes[best].stride : p->mode_w;
    of_video_clear(0);
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)title;
    (void)scale;

    platform_t *p = &g_plat;
    memset(p, 0, sizeof(*p));

    of_video_init();
    of_video_set_display_mode(OF_DISPLAY_FRAMEBUFFER);
    p->mode_w = OF_SCREEN_W;
    p->mode_h = OF_SCREEN_H;
    p->mode_stride = OF_SCREEN_W;

    p->render_w = fb_width;
    p->render_h = fb_height;
    apply_video_mode(p, fb_width, fb_height);

    p->mouse_x = p->render_w / 2;
    p->mouse_y = p->render_h / 2;
    p->start_ms = of_time_ms();

    of_input_set_deadzone(4000);
    of_input_poll();

    return p;
}

void platform_set_render_size(platform_t *p, int w, int h) {
    if (!p || w <= 0 || h <= 0)
        return;
    if (w == p->render_w && h == p->render_h)
        return;
    p->render_w = w;
    p->render_h = h;
    if (p->mouse_x >= w) p->mouse_x = w - 1;
    if (p->mouse_y >= h) p->mouse_y = h - 1;
    apply_video_mode(p, w, h);
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer)
        return;

    /* view_cmap is a 256-entry VGA 6-bit RGB table (palette_entry_t, 3
     * bytes). Uploading 256 entries costs a syscall + 1 KB of writes, so
     * only push it when it actually changed — fades aside, it rarely does. */
    if (palette && (!p->pal_valid || memcmp(p->last_pal, palette, 768) != 0)) {
        memcpy(p->last_pal, palette, 768);
        p->pal_valid = true;
        of_video_palette_vga6(palette, 256);
    }

    uint8_t *fb = of_video_surface();
    if (!fb)
        return;

    int stride = p->mode_stride;
    int copy_w = p->render_w < p->mode_w ? p->render_w : p->mode_w;
    int copy_h = p->render_h < p->mode_h ? p->render_h : p->mode_h;
    int dst_x = (p->mode_w - copy_w) / 2;
    int dst_y = (p->mode_h - copy_h) / 2;

    /* With an exact mode match this is a straight row-by-row copy. When the
     * source is letterboxed, clear only the bars — not the whole surface,
     * which would be another 76 KB of writes per frame on a 100 MHz core. */
    if (dst_y > 0) {
        memset(fb, 0, (size_t)dst_y * stride);
        memset(fb + (size_t)(dst_y + copy_h) * stride, 0,
               (size_t)(p->mode_h - dst_y - copy_h) * stride);
    }

    for (int y = 0; y < copy_h; y++) {
        uint8_t *dst = fb + (size_t)(dst_y + y) * stride + dst_x;
        if (dst_x > 0)
            memset(dst - dst_x, 0, (size_t)dst_x);
        memcpy(dst, framebuffer + (size_t)y * p->render_w, (size_t)copy_w);
        if (dst_x + copy_w < stride)
            memset(dst + copy_w, 0, (size_t)(stride - dst_x - copy_w));
    }

    of_video_flip();
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    /* The engine only takes this path for the debug overlay, which the
     * Pocket build does not enable. */
    (void)p;
    (void)framebuffer;
}

void platform_set_title(platform_t *p, const char *title) {
    (void)p;
    (void)title;
}

/* ── Input ──────────────────────────────────────────────────── */

/* USB HID keyboard usage → PKEY, for the Pocket dock / MiSTer USB
 * keyboard. Sparse table; unmapped usages stay 0 and are ignored. */
static const struct { uint8_t usage; uint8_t pkey; } hid_to_pkey[] = {
    { 0x04, PKEY_A }, { 0x06, PKEY_C }, { 0x07, PKEY_D }, { 0x08, PKEY_E },
    { 0x0A, PKEY_G }, { 0x0C, PKEY_I }, { 0x10, PKEY_M }, { 0x13, PKEY_P },
    { 0x14, PKEY_Q }, { 0x16, PKEY_S }, { 0x1A, PKEY_W }, { 0x1D, PKEY_Z },
    { 0x1E, PKEY_1 }, { 0x1F, PKEY_2 }, { 0x20, PKEY_3 },
    { 0x28, PKEY_RETURN }, { 0x29, PKEY_ESCAPE }, { 0x2C, PKEY_SPACE },
    { 0x3A, PKEY_F1 }, { 0x3B, PKEY_F2 }, { 0x3C, PKEY_F3 }, { 0x3D, PKEY_F4 },
    { 0x3E, PKEY_F5 }, { 0x3F, PKEY_F6 }, { 0x40, PKEY_F7 }, { 0x41, PKEY_F8 },
    { 0x42, PKEY_F9 }, { 0x43, PKEY_F10 }, { 0x44, PKEY_F11 }, { 0x45, PKEY_F12 },
    { 0x4F, PKEY_RIGHT }, { 0x50, PKEY_LEFT }, { 0x51, PKEY_DOWN }, { 0x52, PKEY_UP },
    { 0x59, PKEY_NUM1 }, { 0x5A, PKEY_NUM2 }, { 0x5B, PKEY_NUM3 },
    { 0x5C, PKEY_NUM4 }, { 0x5D, PKEY_NUM5 }, { 0x5E, PKEY_NUM6 },
    { 0x5F, PKEY_NUM7 }, { 0x60, PKEY_NUM8 }, { 0x61, PKEY_NUM9 },
    { 0xE0, PKEY_LCTRL }, { 0xE1, PKEY_LSHIFT }, { 0xE2, PKEY_LALT },
    { 0xE3, PKEY_LCMD }, { 0xE5, PKEY_RSHIFT },
};

static void pump_keyboard(platform_t *p) {
    memcpy(p->keys_prev, p->keys_now, sizeof(p->keys_now));
    memset(p->keys_now, 0, sizeof(p->keys_now));

    of_keyboard_state_t kb;
    of_input_keyboard_state(&kb);
    if (kb.present) {
        for (size_t i = 0; i < sizeof(hid_to_pkey) / sizeof(hid_to_pkey[0]); i++)
            if (of_keyboard_key(&kb, hid_to_pkey[i].usage))
                p->keys_now[hid_to_pkey[i].pkey] = true;
    }

    for (int i = 0; i < PKEY_TABLE_SIZE; i++)
        if (p->keys_now[i] && !p->keys_prev[i])
            p->keys_latch[i] = true;
}

/* Pointer input: a docked USB mouse drives the cursor directly; without
 * one the right stick moves it and R3 clicks, so the requester gadgets in
 * req.c stay reachable on a bare Pocket. The d-pad and left stick are left
 * alone — win.c binds those to movement. */
static void pump_pointer(platform_t *p) {
    of_mouse_state_t ms;
    of_input_mouse_state(&ms);

    if (ms.present) {
        p->mouse_x += ms.dx;
        p->mouse_y += ms.dy;
        p->mouse_buttons = 0;
        if (ms.buttons & 1) p->mouse_buttons |= PMOUSE_LEFT;
        if (ms.buttons & 2) p->mouse_buttons |= PMOUSE_RIGHT;
        if (ms.buttons & 4) p->mouse_buttons |= PMOUSE_MIDDLE;
    } else {
        of_input_state_t st;
        of_input_state(0, &st);
        const int dz = 6000;
        if (st.joy_rx > dz || st.joy_rx < -dz)
            p->mouse_x += st.joy_rx / 4000;
        if (st.joy_ry > dz || st.joy_ry < -dz)
            p->mouse_y -= st.joy_ry / 4000;
        p->mouse_buttons = (st.buttons & OF_BTN_R3) ? PMOUSE_LEFT : 0;
    }

    if (p->mouse_x < 0) p->mouse_x = 0;
    if (p->mouse_y < 0) p->mouse_y = 0;
    if (p->mouse_x >= p->render_w) p->mouse_x = p->render_w - 1;
    if (p->mouse_y >= p->render_h) p->mouse_y = p->render_h - 1;
}

bool platform_pump_events(platform_t *p) {
    if (!p)
        return false;

    of_input_poll();
    pump_keyboard(p);
    pump_pointer(p);
    of_mixer_pump();

    return true;
}

bool platform_key_down(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE)
        return false;
    return p->keys_now[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE)
        return false;
    return p->keys_now[keycode] && !p->keys_prev[keycode];
}

bool platform_key_hit(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE)
        return false;
    bool hit = p->keys_latch[keycode];
    p->keys_latch[keycode] = false;
    return hit;
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y) {
    if (!p)
        return 0;
    if (out_x) *out_x = p->mouse_x;
    if (out_y) *out_y = p->mouse_y;
    return p->mouse_buttons;
}

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    if (!p)
        return;

    of_input_state_t st;
    of_input_state(0, &st);

    state->connected   = true;
    state->dpad_up     = (st.buttons & OF_BTN_UP)    != 0;
    state->dpad_down   = (st.buttons & OF_BTN_DOWN)  != 0;
    state->dpad_left   = (st.buttons & OF_BTN_LEFT)  != 0;
    state->dpad_right  = (st.buttons & OF_BTN_RIGHT) != 0;

    /* win.c reads left_y as "positive is up", matching the desktop
     * backends; the SDK reports positive-down, so invert here. */
    state->left_x  = st.joy_lx;
    state->left_y  = (int16_t)-st.joy_ly;
    state->right_x = st.joy_rx;
    state->right_y = (int16_t)-st.joy_ry;

    state->btn_south  = (st.buttons & OF_BTN_A) != 0;
    state->btn_east   = (st.buttons & OF_BTN_B) != 0;
    state->btn_west   = (st.buttons & OF_BTN_X) != 0;
    state->btn_north  = (st.buttons & OF_BTN_Y) != 0;
    state->btn_lb     = (st.buttons & OF_BTN_L1) != 0;
    state->btn_rb     = (st.buttons & OF_BTN_R1) != 0;
    state->btn_lt     = (st.buttons & OF_BTN_L2) != 0 || st.trigger_l > 16384;
    state->btn_rt     = (st.buttons & OF_BTN_R2) != 0 || st.trigger_r > 16384;
    state->btn_start  = (st.buttons & OF_BTN_START) != 0;
    state->btn_select = (st.buttons & OF_BTN_SELECT) != 0;
    state->btn_lstick = (st.buttons & OF_BTN_L3) != 0;
    /* R3 drives the virtual mouse click; don't also fire the graphics
     * toggle win.c binds to it. */
    state->btn_rstick = false;

    return;
}

/* ── Timing ─────────────────────────────────────────────────── */

uint32_t platform_ticks(platform_t *p) {
    return of_time_ms() - (p ? p->start_ms : 0);
}

void platform_delay(uint32_t ms) {
    if (ms)
        usleep(ms * 1000u);
}

void platform_shutdown(platform_t *p) {
    (void)p;
    of_mixer_stop_all();
    of_midi_stop();
}

/* ── Audio ──────────────────────────────────────────────────── */

/* The mixer's 8-bit path wants signed samples; the engine's WAV data is
 * unsigned. Converting in place is not safe (check_sound_loaded can reload
 * a buffer from the archive), so keep signed copies in a small cache keyed
 * by source pointer. Speech is the big consumer — a few hundred KB each. */
#define PCM_CACHE_ENTRIES 48
#define PCM_CACHE_BUDGET  (4 * 1024 * 1024)

typedef struct {
    const void *src;
    int len;
    uint8_t *conv;
} pcm_cache_t;

static pcm_cache_t s_pcm_cache[PCM_CACHE_ENTRIES];
static int s_pcm_cache_next;
static size_t s_pcm_cache_bytes;

#define VOICE_SLOTS 16

typedef struct {
    bool used;
    int voice;
    of_mixer_handle_t handle;
} voice_slot_t;

static voice_slot_t s_voices[VOICE_SLOTS];
static uint8_t *s_midi_blob;
static bool s_audio_ready;

static void pcm_cache_evict(int idx) {
    if (!s_pcm_cache[idx].conv)
        return;
    s_pcm_cache_bytes -= (size_t)s_pcm_cache[idx].len;
    free(s_pcm_cache[idx].conv);
    s_pcm_cache[idx].conv = NULL;
    s_pcm_cache[idx].src = NULL;
    s_pcm_cache[idx].len = 0;
}

static const uint8_t *pcm_to_signed(const void *src, int len) {
    for (int i = 0; i < PCM_CACHE_ENTRIES; i++)
        if (s_pcm_cache[i].conv && s_pcm_cache[i].src == src &&
            s_pcm_cache[i].len == len)
            return s_pcm_cache[i].conv;

    uint8_t *conv = (uint8_t *)malloc((size_t)len);
    if (!conv)
        return NULL;
    const uint8_t *in = (const uint8_t *)src;
    for (int i = 0; i < len; i++)
        conv[i] = (uint8_t)(in[i] ^ 0x80);

    /* Round-robin eviction; also drop entries until the budget fits. */
    int idx = s_pcm_cache_next;
    s_pcm_cache_next = (s_pcm_cache_next + 1) % PCM_CACHE_ENTRIES;
    pcm_cache_evict(idx);
    while (s_pcm_cache_bytes + (size_t)len > PCM_CACHE_BUDGET) {
        int victim = s_pcm_cache_next;
        s_pcm_cache_next = (s_pcm_cache_next + 1) % PCM_CACHE_ENTRIES;
        if (victim == idx)
            continue;
        if (!s_pcm_cache[victim].conv)
            break;
        pcm_cache_evict(victim);
    }

    s_pcm_cache[idx].src = src;
    s_pcm_cache[idx].len = len;
    s_pcm_cache[idx].conv = conv;
    s_pcm_cache_bytes += (size_t)len;
    return conv;
}

void platform_audio_init(void) {
    if (s_audio_ready)
        return;
    of_mixer_init(OF_MIXER_MAX_VOICES, OF_MIXER_OUTPUT_RATE);
    of_midi_init();
    s_audio_ready = true;
}

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop) {
    if (!s_audio_ready || !data || length <= 0)
        return -1;

    const uint8_t *pcm = pcm_to_signed(data, length);
    if (!pcm)
        return -1;

    int slot = -1;
    for (int i = 0; i < VOICE_SLOTS; i++) {
        if (!s_voices[i].used) { slot = i; break; }
    }
    if (slot < 0) {
        /* All slots taken — recycle the oldest still-running one. */
        slot = 0;
        of_mixer_stop(s_voices[0].voice);
    }

    int vol = volume * 2;
    if (vol > 255) vol = 255;
    if (vol < 0) vol = 0;

    int voice = of_mixer_play_8bit(pcm, (uint32_t)length, (uint32_t)rate, 0, vol);
    if (voice < 0)
        return -1;

    of_mixer_set_group(voice, OF_MIXER_GROUP_SFX);
    if (loop)
        of_mixer_set_loop(voice, 0, length);

    /* pan is -128..127; convert to per-channel 0..255 gains. */
    if (pan != 0) {
        int right = 128 + pan;
        int left = 255 - right;
        if (left < 0) left = 0;
        if (right > 255) right = 255;
        of_mixer_set_vol_lr(voice, vol * left / 255, vol * right / 255);
    }

    s_voices[slot].used = true;
    s_voices[slot].voice = voice;
    s_voices[slot].handle = OF_MIXER_HANDLE_INVALID;
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= VOICE_SLOTS || !s_voices[slot].used)
        return;
    of_mixer_stop(s_voices[slot].voice);
    s_voices[slot].used = false;
}

void platform_audio_stop_all(void) {
    of_mixer_stop_all();
    for (int i = 0; i < VOICE_SLOTS; i++)
        s_voices[i].used = false;
}

void platform_audio_shutdown(void) {
    platform_audio_stop_all();
    of_midi_stop();
    for (int i = 0; i < PCM_CACHE_ENTRIES; i++)
        pcm_cache_evict(i);
    free(s_midi_blob);
    s_midi_blob = NULL;
    s_audio_ready = false;
}

int platform_midi_play(const void *smf_data, int length, bool loop) {
    if (!smf_data || length <= 0)
        return -1;

    of_midi_stop();

    /* platform.h promises the caller may free immediately, and of_midi
     * plays straight out of the buffer from its timer ISR — keep a copy
     * alive for the lifetime of the tune. */
    uint8_t *blob = (uint8_t *)malloc((size_t)length);
    if (!blob)
        return -1;
    memcpy(blob, smf_data, (size_t)length);

    int rc = of_midi_play(blob, (uint32_t)length, loop ? 1 : 0);
    if (rc != OF_MIDI_OK) {
        free(blob);
        return -1;
    }

    free(s_midi_blob);
    s_midi_blob = blob;
    return 0;
}

void platform_midi_stop(void) {
    of_midi_stop();
}

void platform_set_sfx_volume(float vol) {
    int v = (int)(vol * 255.0f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    of_mixer_set_group_volume(OF_MIXER_GROUP_SFX, v);
    of_mixer_set_group_volume(OF_MIXER_GROUP_VOICE, v);
}

void platform_set_music_volume(float vol) {
    int v = (int)(vol * 255.0f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    of_midi_set_volume(v);
}

/* ── Boot: mount the game data ISO ──────────────────────────── */

/* Called from main() before any engine code runs — detect_game_version()
 * reads from the archives, so the root has to be live first. */
int platform_openfpga_mount_data(void) {
    static const char *const candidates[] = {
        "ecstatica.iso", "ecstatica2.iso", "game.iso",
        "e1.iso", "e2.iso", "data.iso",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        uint32_t slot;
        if (of_file_slot_find(candidates[i], &slot) != 0)
            continue;
        if (of_iso_mount(candidates[i], "/game") != 0)
            continue;
        file_set_data_root("/game");
        return 0;
    }

    /* No ISO: fall back to whatever the launcher exposed as flat slots.
     * Only the smallest single-archive setups will work this way. */
    return -1;
}
