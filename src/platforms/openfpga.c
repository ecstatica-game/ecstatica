/**
 * openfpga.c
 *
 * openfpgaOS backend (Analogue Pocket / MiSTer), built by platforms/pocket/Makefile
 * against the openfpgaSDK: rv32imafc @ 100 MHz, 64 MB SDRAM, 320x240 8-bit
 * video, 32-voice PCM mixer, sample-based MIDI synth. Game data is an ISO 9660
 * image in an APF data slot, mounted read-only at /game.
 */

#include "of.h"
#include "platform.h"
#include "asm_f.h"
#include "types.h"

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

/* Only modes the OS advertises can be set. video.json lists 320x200 and
 * 320x240, so VGA lands exactly and the Pocket scaler fills the panel; other
 * sizes letterbox into the smallest mode that holds them. */
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
        /* Nothing big enough (E2 at 640x480 on a 320x240 core): take the
         * largest mode and downscale rather than clip. */
        for (int i = 0; i < n; i++)
            if (best < 0 ||
                (uint32_t)modes[i].width * modes[i].height >
                (uint32_t)modes[best].width * modes[best].height)
                best = i;
        if (best < 0)
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

/* Nearest-neighbour, since these are palette indices. Only runs when the OS
 * cannot scan out the source size; E2 without HIRES/ upscales its 320x200
 * backgrounds and this reduces them again. */
static void blit_downscale(platform_t *p, const uint8_t *src, uint8_t *fb) {
    int dw = p->mode_w, dh = p->mode_h, stride = p->mode_stride;
    int sw = p->render_w, sh = p->render_h;

    /* The 640x480 → 320x240 case: shifts, no divider in the loop. */
    if (sw == dw * 2 && sh == dh * 2) {
        for (int y = 0; y < dh; y++) {
            const uint8_t *s = src + (size_t)(y * 2) * sw;
            uint8_t *d = fb + (size_t)y * stride;
            for (int x = 0; x < dw; x++)
                d[x] = s[x * 2];
        }
        return;
    }

    uint32_t xs = ((uint32_t)sw << 16) / (uint32_t)dw;
    uint32_t ys = ((uint32_t)sh << 16) / (uint32_t)dh;
    uint32_t yf = 0;
    for (int y = 0; y < dh; y++, yf += ys) {
        const uint8_t *s = src + (size_t)(yf >> 16) * sw;
        uint8_t *d = fb + (size_t)y * stride;
        uint32_t xf = 0;
        for (int x = 0; x < dw; x++, xf += xs)
            d[x] = s[xf >> 16];
    }
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

bool platform_hires_supported(platform_t *p) {
    (void)p;
    return true;
}

/* The core's scaler owns the aspect ratio; only PSP and Vita offer fit modes. */
bool platform_scale_mode_supported(platform_t *p) {
    (void)p;
    return false;
}

void platform_set_scale_mode(platform_t *p, int mode) {
    (void)p;
    (void)mode;
}

int platform_crop_inset_y(platform_t *p, int render_h) {
    (void)p;
    (void)render_h;
    return 0;
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

    /* 6-bit VGA RGB; uploaded only when it changed (a syscall + 1 KB). */
    if (palette && (!p->pal_valid || memcmp(p->last_pal, palette, 768) != 0)) {
        memcpy(p->last_pal, palette, 768);
        p->pal_valid = true;
        of_video_palette_vga6(palette, 256);
    }

    uint8_t *fb = of_video_surface();
    if (!fb)
        return;

    if (p->render_w > p->mode_w || p->render_h > p->mode_h) {
        blit_downscale(p, framebuffer, fb);
        of_video_flip();
        return;
    }

    int stride = p->mode_stride;
    int copy_w = p->render_w < p->mode_w ? p->render_w : p->mode_w;
    int copy_h = p->render_h < p->mode_h ? p->render_h : p->mode_h;
    int dst_x = (p->mode_w - copy_w) / 2;
    int dst_y = (p->mode_h - copy_h) / 2;

    /* Letterboxed: clear only the bars, not 76 KB per frame. */
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
    /* Only the debug overlay uses this, and it is off here. */
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
    { 0x05, PKEY_B }, { 0x09, PKEY_F }, { 0x0B, PKEY_H }, { 0x0F, PKEY_L },
    { 0x11, PKEY_N }, { 0x12, PKEY_O }, { 0x15, PKEY_R }, { 0x17, PKEY_T },
    { 0x19, PKEY_V }, { 0x1B, PKEY_X },
    { 0x2B, PKEY_TAB }, { 0x2D, PKEY_MINUS }, { 0x2E, PKEY_EQUALS },
    { 0x2F, PKEY_LBRACKET }, { 0x30, PKEY_RBRACKET },
    { 0x36, PKEY_COMMA }, { 0x37, PKEY_PERIOD },
    { 0x4A, PKEY_HOME }, { 0x4B, PKEY_PGUP }, { 0x4D, PKEY_END }, { 0x4E, PKEY_PGDN },
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

/* A docked USB mouse drives the cursor; otherwise the right stick moves it
 * and R3 clicks, so req.c's gadgets stay reachable. */
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

/* To positive-up. -(-32768) does not fit an int16. */
static int16_t neg_axis(int v) {
    v = -v;
    return (int16_t)(v > 32767 ? 32767 : (v < -32767 ? -32767 : v));
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

    /* The SDK reports positive-down; win.c wants positive-up. */
    state->left_x  = st.joy_lx;
    state->left_y  = neg_axis(st.joy_ly);
    state->right_x = st.joy_rx;
    state->right_y = neg_axis(st.joy_ry);

    state->btn_south  = (st.buttons & OF_BTN_A) != 0;
    state->btn_east   = (st.buttons & OF_BTN_B) != 0;
    state->btn_west   = (st.buttons & OF_BTN_X) != 0;
    state->btn_north  = (st.buttons & OF_BTN_Y) != 0;
    state->btn_start  = (st.buttons & OF_BTN_START) != 0;
    state->btn_lstick = (st.buttons & OF_BTN_L3) != 0;
    /* R3 is the virtual mouse click, not the graphics toggle. */
    state->btn_rstick = false;

    /* A bare Pocket has one shoulder per side and no triggers; Select is the
     * shift for the second row, as in vita.c/psp.c. */
    bool l1 = (st.buttons & OF_BTN_L1) != 0;
    bool r1 = (st.buttons & OF_BTN_R1) != 0;
    bool l2 = (st.buttons & OF_BTN_L2) != 0 || st.trigger_l > 16384;
    bool r2 = (st.buttons & OF_BTN_R2) != 0 || st.trigger_r > 16384;
    bool select = (st.buttons & OF_BTN_SELECT) != 0;

    if (select && (l1 || r1)) {
        state->btn_lt = l1;
        state->btn_rt = r1;
    } else {
        state->btn_lb     = l1;
        state->btn_rb     = r1;
        state->btn_select = select;
    }
    state->btn_lt |= l2;
    state->btn_rt |= r2;

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

/* The mixer wants signed 8-bit; the WAV data is unsigned. check_sound_loaded
 * can reload a buffer, so converting in place is unsafe: signed copies are
 * cached by source pointer. */
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

/* Runs while a blocking read waits on DMA (a background is ~95 KB off the
 * ISO), so effects do not cut out. Must not itself issue a blocking read. */
static void audio_idle_hook(void) {
    of_mixer_pump();
}

void platform_audio_init(void) {
    if (s_audio_ready)
        return;
    of_mixer_init(OF_MIXER_MAX_VOICES, OF_MIXER_OUTPUT_RATE);
    of_midi_init();
    of_file_set_idle_hook(audio_idle_hook);
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

    /* The caller may free the blob, but of_midi plays from it in its ISR. */
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

void platform_set_sfx_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    of_mixer_set_group_volume(OF_MIXER_GROUP_SFX, vol);
    of_mixer_set_group_volume(OF_MIXER_GROUP_VOICE, vol);
}

void platform_set_music_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    of_midi_set_volume(vol);
}

/* ── Platform capabilities ──────────────────────────────────── */

/* No working directory here, so mount before detect_game_version() reads. */
void platform_early_init(void) {
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
        return;
    }

    /* No image: fall back to flat launcher slots (single-archive setups only). */
}

/* APF nonvolatile slots 10..19. The region runs 0x20100000..0x20380000 and
 * butts against shared config, so ten is the ceiling. */
int platform_save_slot_count(void) {
    return 10;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version) {
    /* Must match data_slots in the core's instance JSON; E1 and E2 get
     * separate sets. */
    snprintf(buf, bufsz, "ecstatica%s_%d.sav",
             game_version == GAME_VERSION_E2 ? "2" : "", slot);
}

void platform_save_prepare(void) {
}
