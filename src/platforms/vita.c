/**
 * platforms/vita.c
 *
 * PlayStation Vita backend, built against VitaSDK.
 *
 *   Video     SceDisplay, 960x544 32-bit, triple-buffered in CDRAM. The
 *             engine's 8-bit indexed frame is palette-expanded and scaled on
 *             the CPU into a 4:3 area centred on the panel (pillarboxed), with
 *             per-row and per-column source tables so the inner loop is a
 *             lookup and a store.
 *   Input     SceCtrl for the buttons and both sticks. The front panel is a
 *             pointer and click for menus and requesters; the rear panel's
 *             left and right halves are the second shoulder row the Vita
 *             does not have (E1 per-hand pick-up, E2 magic).
 *   Timing    sceKernelGetProcessTimeWide (microseconds).
 *   Audio     One BGM port at 44100 Hz stereo, fed by a mixer thread that sums
 *             the engine's 16 voices in software. Music is silent — the Vita
 *             has no General MIDI synth, see platform_midi_play.
 *
 * Built by vita/CMakeLists.txt through VitaSDK's vita.cmake. The sce* calls
 * are confined to this file; the engine reaches the Vita only through
 * platform.h. The game data lives outside the VPK, in ux0:data/ecstatica, so
 * the package stays small and saves have somewhere writable to go.
 */

#ifdef __vita__

#include "../platform.h"
#include "../asm_f.h"
#include "../types.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/display.h>
#include <psp2/audioout.h>
#include <psp2/power.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Newlib's heap. The archives, the part and event pools and the decoded
 * backgrounds all live in it; the default is far below what a Vita app has. */
int _newlib_heap_size_user = 192 * 1024 * 1024;
/* Deep call chains through the renderers, matrices passed by value. */
unsigned int sceUserMainThreadStackSize = 1024 * 1024;

#define SCREEN_W      960
#define SCREEN_H      544
#define FB_COUNT      3
/* CDRAM blocks are allocated in 256 KB units. */
#define FB_BYTES      ((SCREEN_W * SCREEN_H * 4 + 0x3FFFF) & ~0x3FFFF)

#define MAX_RENDER_W  640
#define MAX_RENDER_H  480

#define PKEY_TABLE_SIZE 256

struct platform_t {
    int render_w, render_h;

    bool keys_now[PKEY_TABLE_SIZE];
    bool keys_prev[PKEY_TABLE_SIZE];
    bool keys_latch[PKEY_TABLE_SIZE];

    int mouse_x, mouse_y;
    int mouse_buttons;

    uint64_t start_us;
};

static platform_t g_plat;

/* ── Video ──────────────────────────────────────────────────── */

static SceUID    s_fb_block = -1;
static uint32_t *s_fb[FB_COUNT];
static int       s_fb_next;

/* view_cmap is 6-bit VGA RGB; the panel wants 0xAABBGGRR. */
static uint32_t s_lut[256];
static uint8_t  s_last_pal[768];
static bool     s_pal_valid;

/* The 4:3 picture area inside the 960x544 panel, and for each of its columns
 * and rows the source pixel it samples. Rebuilt when the render size
 * changes — the graphics toggle switches between 320x200 and 640x480. */
static int      s_dst_x, s_dst_w;
static uint16_t s_col_src[SCREEN_W];
static uint32_t s_row_off[SCREEN_H];
static int      s_map_w, s_map_h;

static void build_scale_maps(int sw, int sh) {
    s_dst_w = SCREEN_H * 4 / 3;                 /* 725 */
    s_dst_x = (SCREEN_W - s_dst_w) / 2;
    for (int x = 0; x < s_dst_w; x++)
        s_col_src[x] = (uint16_t)(x * sw / s_dst_w);
    for (int y = 0; y < SCREEN_H; y++)
        s_row_off[y] = (uint32_t)(y * sh / SCREEN_H) * (uint32_t)sw;
    s_map_w = sw;
    s_map_h = sh;
}

static void upload_lut(const uint8_t *palette) {
    for (int i = 0; i < 256; i++) {
        uint32_t r = (uint32_t)(palette[i * 3 + 0] & 0x3F) << 2;
        uint32_t g = (uint32_t)(palette[i * 3 + 1] & 0x3F) << 2;
        uint32_t b = (uint32_t)(palette[i * 3 + 2] & 0x3F) << 2;
        s_lut[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
}

static bool video_init(void) {
    s_fb_block = sceKernelAllocMemBlock("ecs_fb", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                        FB_BYTES * FB_COUNT, NULL);
    if (s_fb_block < 0)
        return false;

    void *base = NULL;
    sceKernelGetMemBlockBase(s_fb_block, &base);
    for (int i = 0; i < FB_COUNT; i++) {
        s_fb[i] = (uint32_t *)((uint8_t *)base + (size_t)i * FB_BYTES);
        /* The pillarbox bars are never drawn again, so they have to start
         * black in every buffer. */
        memset(s_fb[i], 0, SCREEN_W * SCREEN_H * 4);
    }

    SceDisplayFrameBuf param;
    memset(&param, 0, sizeof(param));
    param.size = sizeof(param);
    param.base = s_fb[0];
    param.pitch = SCREEN_W;
    param.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    param.width = SCREEN_W;
    param.height = SCREEN_H;
    sceDisplaySetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
    s_fb_next = 1;
    return true;
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)title;
    (void)scale;

    platform_t *p = &g_plat;
    memset(p, 0, sizeof(*p));

    /* The engine is a software renderer; give it the fastest ARM clock. */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);

    p->render_w = fb_width;
    p->render_h = fb_height;
    p->mouse_x = fb_width / 2;
    p->mouse_y = fb_height / 2;
    p->start_us = sceKernelGetProcessTimeWide();

    video_init();
    build_scale_maps(fb_width, fb_height);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

    return p;
}

bool platform_hires_supported(platform_t *p) {
    (void)p;
    return true;
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
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || s_fb_block < 0)
        return;

    int sw = p->render_w, sh = p->render_h;
    if (sw <= 0 || sh <= 0 || sw > MAX_RENDER_W || sh > MAX_RENDER_H)
        return;
    if (sw != s_map_w || sh != s_map_h)
        build_scale_maps(sw, sh);

    if (palette && (!s_pal_valid || memcmp(s_last_pal, palette, 768) != 0)) {
        memcpy(s_last_pal, palette, 768);
        s_pal_valid = true;
        upload_lut(palette);
    }

    /* Triple buffering: the buffer written here is neither the one on screen
     * nor the one queued for the next vblank, so nothing waits and nothing
     * tears. */
    uint32_t *fb = s_fb[s_fb_next];
    const uint16_t *cols = s_col_src;
    const int dw = s_dst_w;

    for (int y = 0; y < SCREEN_H; y++) {
        const uint8_t *src = framebuffer + s_row_off[y];
        uint32_t *dst = fb + y * SCREEN_W + s_dst_x;
        for (int x = 0; x < dw; x++)
            dst[x] = s_lut[src[cols[x]]];
    }

    SceDisplayFrameBuf param;
    memset(&param, 0, sizeof(param));
    param.size = sizeof(param);
    param.base = fb;
    param.pitch = SCREEN_W;
    param.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    param.width = SCREEN_W;
    param.height = SCREEN_H;
    sceDisplaySetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);

    s_fb_next = (s_fb_next + 1) % FB_COUNT;
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    /* Only the debug overlay takes this path, and the Vita build does not
     * enable it. */
    (void)p;
    (void)framebuffer;
}

void platform_set_title(platform_t *p, const char *title) {
    (void)p;
    (void)title;
}

/* ── Input ──────────────────────────────────────────────────── */

/* Analog sticks: 0..255 per axis, centred at 128. Scale to the int16 range the
 * gamepad interface uses; win.c reads Y as positive-up. */
static int16_t stick_axis(unsigned char raw) {
    int v = ((int)raw - 128) * 258;
    /* Symmetric on purpose: the Y axis is negated by the caller, and -(-32768)
     * does not fit an int16 — full forward wrapped round to full back. */
    if (v >  32767) v =  32767;
    if (v < -32767) v = -32767;
    return (int16_t)v;
}

static SceCtrlData  s_pad;
static SceTouchData s_front, s_back;
static SceTouchPanelInfo s_front_info, s_back_info;
static bool s_touch_info_valid;

static bool s_rear_left, s_rear_right;

static void pump_pointer(platform_t *p) {
    /* Front panel: the finger is the pointer, and touching is holding the
     * left button, so a tap is a click. Mapped through the 4:3 picture area;
     * the bars clamp to the nearest edge. */
    if (s_front.reportNum > 0) {
        int span_x = s_front_info.maxAaX - s_front_info.minAaX;
        int span_y = s_front_info.maxAaY - s_front_info.minAaY;
        if (span_x <= 0) span_x = SCREEN_W * 2;
        if (span_y <= 0) span_y = SCREEN_H * 2;

        int sx = (s_front.report[0].x - s_front_info.minAaX) * SCREEN_W / span_x;
        int sy = (s_front.report[0].y - s_front_info.minAaY) * SCREEN_H / span_y;

        p->mouse_x = (sx - s_dst_x) * p->render_w / (s_dst_w > 0 ? s_dst_w : 1);
        p->mouse_y = sy * p->render_h / SCREEN_H;
        p->mouse_buttons = PMOUSE_LEFT;
    } else {
        /* Same stick-driven pointer as the PSP, for playing without touching
         * the screen: the stick walks in play and moves the cursor in the
         * requesters, which are the only place req.c reads it. */
        int dx = (int)s_pad.lx - 128;
        int dy = (int)s_pad.ly - 128;
        const int dz = 24;

        if (dx > dz || dx < -dz) p->mouse_x += dx / 16;
        if (dy > dz || dy < -dz) p->mouse_y += dy / 16;
        p->mouse_buttons = (s_pad.buttons & SCE_CTRL_CROSS) ? PMOUSE_LEFT : 0;
    }

    if (p->mouse_x < 0) p->mouse_x = 0;
    if (p->mouse_y < 0) p->mouse_y = 0;
    if (p->mouse_x >= p->render_w) p->mouse_x = p->render_w - 1;
    if (p->mouse_y >= p->render_h) p->mouse_y = p->render_h - 1;
}

static void pump_rear(void) {
    s_rear_left = s_rear_right = false;
    int mid_x = (s_back_info.minAaX + s_back_info.maxAaX) / 2;
    if (mid_x <= 0) mid_x = SCREEN_W;
    for (unsigned i = 0; i < s_back.reportNum && i < SCE_TOUCH_MAX_REPORT; i++) {
        if (s_back.report[i].x < mid_x) s_rear_left = true;
        else s_rear_right = true;
    }
}

bool platform_pump_events(platform_t *p) {
    if (!p)
        return false;

    if (!s_touch_info_valid) {
        sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &s_front_info);
        sceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &s_back_info);
        s_touch_info_valid = true;
    }

    /* Peek, not read: the read calls block until the next sample, which would
     * pace the whole game off the controller. */
    sceCtrlPeekBufferPositive(0, &s_pad, 1);
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &s_front, 1);
    sceTouchPeek(SCE_TOUCH_PORT_BACK, &s_back, 1);

    /* No keyboard, so the key table stays empty and the latches with it;
     * everything the player can press arrives through the pad. */
    memcpy(p->keys_prev, p->keys_now, sizeof(p->keys_now));

    pump_pointer(p);
    pump_rear();

    /* The PS button suspends the app at the system's level; there is no quit
     * request to report. */
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

    unsigned int b = s_pad.buttons;

    state->connected  = true;
    state->dpad_up    = (b & SCE_CTRL_UP)    != 0;
    state->dpad_down  = (b & SCE_CTRL_DOWN)  != 0;
    state->dpad_left  = (b & SCE_CTRL_LEFT)  != 0;
    state->dpad_right = (b & SCE_CTRL_RIGHT) != 0;

    state->left_x  = stick_axis(s_pad.lx);
    state->left_y  = (int16_t)-stick_axis(s_pad.ly);
    state->right_x = stick_axis(s_pad.rx);
    state->right_y = (int16_t)-stick_axis(s_pad.ry);

    state->btn_south = (b & SCE_CTRL_CROSS)    != 0;
    state->btn_east  = (b & SCE_CTRL_CIRCLE)   != 0;
    state->btn_west  = (b & SCE_CTRL_SQUARE)   != 0;
    state->btn_north = (b & SCE_CTRL_TRIANGLE) != 0;
    state->btn_start = (b & SCE_CTRL_START)    != 0;
    state->btn_lstick = (b & SCE_CTRL_L3) != 0;   /* PS TV pads only */
    state->btn_rstick = (b & SCE_CTRL_R3) != 0;

    /* The Vita's shoulders report as the trigger bits; a PS TV pad's L1/R1
     * as their own. */
    bool l = (b & (SCE_CTRL_LTRIGGER | SCE_CTRL_L1)) != 0;
    bool r = (b & (SCE_CTRL_RTRIGGER | SCE_CTRL_R1)) != 0;
    bool select = (b & SCE_CTRL_SELECT) != 0;

    /* The engine wants two shoulder rows: LB/RB (jump, attack) and LT/RT
     * (E1's per-hand pick-up, E2's magic). The rear panel halves are the
     * second row. Select + shoulder does the same, as on the PSP, for anyone
     * holding the Vita where the rear panel is awkward to reach. */
    if (select && (l || r)) {
        state->btn_lt = l;
        state->btn_rt = r;
    } else {
        state->btn_lb = l;
        state->btn_rb = r;
        state->btn_select = select;
    }
    state->btn_lt |= s_rear_left;
    state->btn_rt |= s_rear_right;
}

/* ── Timing ─────────────────────────────────────────────────── */

uint32_t platform_ticks(platform_t *p) {
    uint64_t now = sceKernelGetProcessTimeWide();
    uint64_t start = p ? p->start_us : 0;
    return (uint32_t)((now - start) / 1000u);
}

void platform_delay(uint32_t ms) {
    if (ms)
        sceKernelDelayThread(ms * 1000u);
}

#ifdef ECS_PROFILE
#include "../prof.h"

uint32_t prof_clock_us(void) {
    return sceKernelGetProcessTimeLow();
}
#endif

/* ── Audio ──────────────────────────────────────────────────────
 * One BGM port at 44100 Hz stereo, with the engine's 16 voices summed in
 * software by a dedicated thread. Voices are 8-bit unsigned mono at whatever
 * rate the WAV carried, usually 22050, so each one is point-resampled as it
 * is mixed. Same mixer as the PSP backend.
 */

#define VITA_VOICES       16
#define VITA_AUDIO_RATE   44100
#define VITA_AUDIO_FRAMES 1024

typedef struct {
    const uint8_t *data;
    uint32_t       len;
    uint32_t       idx;      /* whole sample index into data */
    uint32_t       frac;     /* fractional position, 0..0xFFFF */
    uint32_t       step;     /* 16.16 increment: rate / VITA_AUDIO_RATE */
    int            vol;      /* 0..127 */
    int            pan;      /* -128..127 */
    bool           loop;
    volatile bool  active;
} vita_voice_t;

static vita_voice_t s_voices[VITA_VOICES];
static SceUID s_voice_sema = -1;
static SceUID s_audio_thread = -1;
static int    s_audio_port = -1;
static volatile bool s_audio_ready;
static volatile bool s_audio_quit;
static int s_sfx_vol = 255;            /* 0..255 master */

static int16_t __attribute__((aligned(64))) s_out[VITA_AUDIO_FRAMES * 2];
static int s_accum[VITA_AUDIO_FRAMES * 2];

static void voices_lock(void) {
    if (s_voice_sema >= 0)
        sceKernelWaitSema(s_voice_sema, 1, NULL);
}

static void voices_unlock(void) {
    if (s_voice_sema >= 0)
        sceKernelSignalSema(s_voice_sema, 1);
}

static void mix_block(void) {
    memset(s_accum, 0, sizeof(s_accum));

    voices_lock();
    for (int v = 0; v < VITA_VOICES; v++) {
        vita_voice_t *vo = &s_voices[v];
        if (!vo->active)
            continue;

        const uint8_t *data = vo->data;
        uint32_t idx = vo->idx, frac = vo->frac, step = vo->step, len = vo->len;

        int scale = (vo->vol * s_sfx_vol * 256) / (127 * 255);
        /* pan is -128 (left) .. 0 (centre) .. 127 (right). Attenuate the far
         * side only, so a centred sound is not quieter than a panned one. */
        int scale_l = vo->pan > 0 ? scale * (127 - vo->pan) / 127 : scale;
        int scale_r = vo->pan < 0 ? scale * (128 + vo->pan) / 128 : scale;

        for (int i = 0; i < VITA_AUDIO_FRAMES; i++) {
            if (idx >= len) {
                if (!vo->loop) { vo->active = false; break; }
                idx = 0;
                frac = 0;
            }

            int s = ((int)data[idx] - 128) << 8;
            s_accum[i * 2 + 0] += (s * scale_l) >> 8;
            s_accum[i * 2 + 1] += (s * scale_r) >> 8;

            /* Index and fraction apart: a single 16.16 position caps the
             * sample at 64 KB, and speech lines are longer. */
            frac += step;
            idx += frac >> 16;
            frac &= 0xFFFFu;
        }

        vo->idx = idx;
        vo->frac = frac;
    }
    voices_unlock();

    for (int i = 0; i < VITA_AUDIO_FRAMES * 2; i++) {
        int s = s_accum[i];
        if (s < -32768) s = -32768;
        if (s >  32767) s =  32767;
        s_out[i] = (int16_t)s;
    }
}

static int audio_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (!s_audio_quit) {
        mix_block();
        sceAudioOutOutput(s_audio_port, s_out);   /* blocks for one block */
    }
    return 0;
}

void platform_audio_init(void) {
    if (s_audio_ready)
        return;

    memset((void *)s_voices, 0, sizeof(s_voices));

    s_audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,
                                       VITA_AUDIO_FRAMES, VITA_AUDIO_RATE,
                                       SCE_AUDIO_OUT_MODE_STEREO);
    if (s_audio_port < 0)
        return;

    int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(s_audio_port,
                         SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);

    s_voice_sema = sceKernelCreateSema("ecs_voices", 0, 1, 1, NULL);
    if (s_voice_sema < 0) {
        sceAudioOutReleasePort(s_audio_port);
        s_audio_port = -1;
        return;
    }

    s_audio_quit = false;
    /* Higher priority (lower number) than the main thread's default, so a
     * long frame in the renderer cannot starve the mixer into an underrun. */
    s_audio_thread = sceKernelCreateThread("ecs_audio", audio_thread,
                                           0x10000100 - 16, 64 * 1024, 0, 0, NULL);
    if (s_audio_thread < 0) {
        sceKernelDeleteSema(s_voice_sema);
        s_voice_sema = -1;
        sceAudioOutReleasePort(s_audio_port);
        s_audio_port = -1;
        return;
    }

    s_audio_ready = true;
    sceKernelStartThread(s_audio_thread, 0, NULL);
}

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop) {
    if (!s_audio_ready || !data || length <= 0)
        return -1;
    if (rate <= 0)
        rate = 22050;

    voices_lock();

    int slot = -1;
    for (int v = 0; v < VITA_VOICES; v++) {
        if (!s_voices[v].active) { slot = v; break; }
    }
    if (slot < 0)
        slot = 0;                       /* steal slot 0, as documented */

    s_voices[slot].active = false;
    s_voices[slot].data = (const uint8_t *)data;
    s_voices[slot].len = (uint32_t)length;
    s_voices[slot].idx = 0;
    s_voices[slot].frac = 0;
    s_voices[slot].step = (uint32_t)(((uint64_t)rate << 16) / VITA_AUDIO_RATE);
    s_voices[slot].vol = volume < 0 ? 0 : (volume > 127 ? 127 : volume);
    s_voices[slot].pan = pan;
    s_voices[slot].loop = loop;
    s_voices[slot].active = true;

    voices_unlock();
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= VITA_VOICES)
        return;
    s_voices[slot].active = false;
}

void platform_audio_stop_all(void) {
    for (int v = 0; v < VITA_VOICES; v++)
        s_voices[v].active = false;
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready)
        return;
    s_audio_ready = false;

    platform_audio_stop_all();
    s_audio_quit = true;

    if (s_audio_thread >= 0) {
        sceKernelWaitThreadEnd(s_audio_thread, NULL, NULL);
        sceKernelDeleteThread(s_audio_thread);
        s_audio_thread = -1;
    }
    if (s_audio_port >= 0) {
        sceAudioOutReleasePort(s_audio_port);
        s_audio_port = -1;
    }
    if (s_voice_sema >= 0) {
        sceKernelDeleteSema(s_voice_sema);
        s_voice_sema = -1;
    }
}

/* music.c hands over a Standard MIDI File and expects a General MIDI synth on
 * the other side. The Vita has none, so tunes are silent until a software
 * synth and a bank are linked in, the same position as the PSP and DOS
 * targets. Sound effects and speech go through the mixer above. */
int platform_midi_play(const void *smf_data, int length, bool loop) {
    (void)smf_data;
    (void)length;
    (void)loop;
    return -1;
}

void platform_midi_stop(void) {
}

void platform_set_sfx_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 255) vol = 255;
    s_sfx_vol = vol;
}

void platform_set_music_volume(int vol) {
    (void)vol;
}

/* ── Capabilities ───────────────────────────────────────────── */

static char s_data_root[256];

/* Each VPK is built for one game (vita/CMakeLists.txt), so both can be
 * installed as their own LiveArea bubbles and each opens its own folder. */
#ifndef VITA_GAME_DIR
#define VITA_GAME_DIR "e2"
#endif

void platform_early_init(void) {
    /* app0: is the read-only VPK mount, so the data goes on a memory card:
     * ux0:data/ecstatica/e1 or /e2 for this bubble's game. The bare folder is
     * the fallback for a card that carries just one game. */
    static const char *const candidates[] = {
        "ux0:data/ecstatica/" VITA_GAME_DIR,
        "uma0:data/ecstatica/" VITA_GAME_DIR,
        "ux0:data/ecstatica",
        "uma0:data/ecstatica",
        "app0:data",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (!file_dir_has_database(candidates[i]))
            continue;
        snprintf(s_data_root, sizeof(s_data_root), "%s", candidates[i]);
        file_set_data_root(s_data_root);
        file_flush_path_cache();
        /* The debug log and prof.log are opened by bare name; the process
         * starts in app0:, which is read-only. */
        chdir(s_data_root);
        return;
    }

    /* No console to print to; let the engine fail with its own message. */
}

int platform_save_slot_count(void) {
    return 11;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version) {
    (void)game_version;   /* one install per folder, so no need to split */
    if (s_data_root[0])
        snprintf(buf, bufsz, "%s/saved/%04d.ecs", s_data_root, slot);
    else
        snprintf(buf, bufsz, "saved/%04d.ecs", slot);
}

void platform_save_prepare(void) {
    char dir[sizeof(s_data_root) + 8];
    if (s_data_root[0])
        snprintf(dir, sizeof(dir), "%s/saved", s_data_root);
    else
        snprintf(dir, sizeof(dir), "saved");
    sceIoMkdir(dir, 0777);
}

/* ── Shutdown ───────────────────────────────────────────────── */

void platform_shutdown(platform_t *p) {
    (void)p;
    platform_audio_shutdown();
    if (s_fb_block >= 0) {
        sceKernelFreeMemBlock(s_fb_block);
        s_fb_block = -1;
    }
    sceKernelExitProcess(0);
}

#endif /* __vita__ */
