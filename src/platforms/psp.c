/**
 * platforms/psp.c
 *
 * PlayStation Portable backend (PSPSDK), built by psp/Makefile.
 *
 *   Video     sceGu, 480x272 16-bit. The 8-bit frame is a GU_PSM_T8 texture
 *             with a 256-entry CLUT, so palette expansion and scaling happen
 *             on the GPU.
 *   Input     sceCtrl only; everything reaches the engine through the gamepad
 *             path in win.c, and the stick doubles as the requester pointer.
 *   Timing    sceKernelGetSystemTimeWide.
 *   Audio     One 44100 Hz stereo channel fed by a software mixer thread.
 *             Music is silent (no OS synth).
 */

#ifdef __PSP__

#include "../platform.h"
#include "../asm_f.h"
#include "../types.h"

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspaudio.h>
#include <psppower.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("ECSTATICA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
/* The engine keeps deep call chains through the renderers and hands whole
 * matrices by value; the 256 KB default main-thread stack is not enough. */
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
/* All but a megabyte: a PSP-1000 has ~20 MB of user memory for the archives,
 * pools and backgrounds. */
PSP_HEAP_SIZE_KB(-1024);

#define SCREEN_W     480
#define SCREEN_H     272
#define SCREEN_STRIDE 512          /* VRAM line width, fixed by the hardware */

#define PKEY_TABLE_SIZE 256

struct platform_t {
    int render_w, render_h;

    uint8_t last_pal[768];
    bool pal_valid;

    bool keys_now[PKEY_TABLE_SIZE];
    bool keys_prev[PKEY_TABLE_SIZE];
    bool keys_latch[PKEY_TABLE_SIZE];

    int mouse_x, mouse_y;
    int mouse_buttons;

    uint64_t start_us;
};

static platform_t g_plat;
static volatile bool s_running = true;
static int s_scale_mode = SCALE_PILLARBOX;

int platform_crop_inset_y(platform_t *p, int render_h) {
    (void)p;
    if (s_scale_mode != SCALE_CROP) return 0;
    int full_h = SCREEN_W * 3 / 4;
    int off = (full_h - SCREEN_H) / 2;
    return off * render_h / full_h;
}

/* ── Video ──────────────────────────────────────────────────── */

/* Display list; generous for a handful of sprites. */
static unsigned int __attribute__((aligned(16))) s_gu_list[64 * 1024 / 4];

/* CLUT, 16-byte aligned as the GE requires, in the PSP's 0xAABBGGRR order. */
static unsigned int __attribute__((aligned(16))) s_clut[256];

/* The GE needs 16-byte-aligned textures. The engine framebuffer normally is,
 * so this is only allocated for a misaligned frame. */
static uint8_t *s_stage;
static size_t   s_stage_size;

typedef struct {
    unsigned short u, v;
    short x, y, z;
} blit_vertex_t;

static void gu_init(void) {
    /* Two draw buffers and an unused depth buffer: 835 KB of 2 MB VRAM. */
    void *buf0 = (void *)0;
    void *buf1 = (void *)(SCREEN_STRIDE * SCREEN_H * 2);
    void *zbuf = (void *)(SCREEN_STRIDE * SCREEN_H * 4);

    sceGuInit();
    sceGuStart(GU_DIRECT, s_gu_list);
    sceGuDrawBuffer(GU_PSM_5650, buf0, SCREEN_STRIDE);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, buf1, SCREEN_STRIDE);
    sceGuDepthBuffer(zbuf, SCREEN_STRIDE);
    sceGuOffset(2048 - (SCREEN_W / 2), 2048 - (SCREEN_H / 2));
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_FLAT);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

/* 6-bit VGA components, << 2. Re-uploaded only when the palette changed. */
static void upload_clut(const uint8_t *palette) {
    for (int i = 0; i < 256; i++) {
        unsigned int r = (unsigned int)(palette[i * 3 + 0] & 0x3F) << 2;
        unsigned int g = (unsigned int)(palette[i * 3 + 1] & 0x3F) << 2;
        unsigned int b = (unsigned int)(palette[i * 3 + 2] & 0x3F) << 2;
        s_clut[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
    sceKernelDcacheWritebackRange(s_clut, sizeof(s_clut));
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)title;
    (void)scale;

    platform_t *p = &g_plat;
    memset(p, 0, sizeof(*p));

    /* The engine is a software renderer; the CPU is the whole budget. Both
     * games are unplayable at the 222 MHz default. */
    scePowerSetClockFrequency(333, 333, 166);

    p->render_w = fb_width;
    p->render_h = fb_height;
    p->mouse_x = fb_width / 2;
    p->mouse_y = fb_height / 2;
    p->start_us = sceKernelGetSystemTimeWide();

    gu_init();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    return p;
}

bool platform_hires_supported(platform_t *p) {
    (void)p;
    return true;
}

bool platform_scale_mode_supported(platform_t *p) {
    (void)p;
    return true;
}

void platform_set_scale_mode(platform_t *p, int mode) {
    (void)p;
    if (mode < SCALE_PILLARBOX || mode > SCALE_STRETCH)
        return;
    s_scale_mode = mode;
}

/* Screen x-range and texel-row range for the scale mode. The panel is wider
 * than 4:3, so cropping is only ever vertical. */
static void compute_dst_rect(int sw, int sh, int *dst_x0, int *dst_x1, int *v0, int *v1) {
    (void)sw;
    switch (s_scale_mode) {
    case SCALE_CROP: {
        int full_h = SCREEN_W * 3 / 4;
        int off = (full_h - SCREEN_H) / 2;
        *dst_x0 = 0;
        *dst_x1 = SCREEN_W;
        *v0 = off * sh / full_h;
        *v1 = (off + SCREEN_H) * sh / full_h;
        break;
    }
    case SCALE_STRETCH:
        *dst_x0 = 0;
        *dst_x1 = SCREEN_W;
        *v0 = 0;
        *v1 = sh;
        break;
    default: /* SCALE_PILLARBOX */ {
        int dst_w = SCREEN_H * 4 / 3;
        *dst_x0 = (SCREEN_W - dst_w) / 2;
        *dst_x1 = *dst_x0 + dst_w;
        *v0 = 0;
        *v1 = sh;
        break;
    }
    }
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
    if (!p || !framebuffer)
        return;

    int sw = p->render_w, sh = p->render_h;
    if (sw <= 0 || sh <= 0)
        return;

    if (palette && (!p->pal_valid || memcmp(p->last_pal, palette, 768) != 0)) {
        memcpy(p->last_pal, palette, 768);
        p->pal_valid = true;
        upload_clut(palette);
    }

    /* The GE addresses T8 textures in 16-byte units; both games render 320 or
     * 640 wide. */
    if ((sw & 15) != 0 || sh > 512)
        return;

    const uint8_t *src = framebuffer;
    size_t frame_bytes = (size_t)sw * (size_t)sh;
    if (((uintptr_t)src & 15) != 0) {
        if (s_stage_size < frame_bytes) {
            uint8_t *grown = (uint8_t *)malloc(frame_bytes);
            if (!grown)
                return;
            free(s_stage);
            s_stage = grown;
            s_stage_size = frame_bytes;
        }
        memcpy(s_stage, framebuffer, frame_bytes);
        src = s_stage;
    }

    /* The GE is not coherent with the CPU data cache. */
    sceKernelDcacheWritebackRange(src, frame_bytes);

    int dst_x0, dst_x1, v0, v1;
    compute_dst_rect(sw, sh, &dst_x0, &dst_x1, &v0, &v1);
    int dst_w = dst_x1 - dst_x0;

    sceGuStart(GU_DIRECT, s_gu_list);

    /* Re-black the pillarbox bars, which a stretch/crop frame may have painted. */
    if (dst_x0 > 0) {
        sceGuClearColor(0);
        sceGuClear(GU_COLOR_BUFFER_BIT);
    }

    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(32, s_clut);          /* 32 blocks of 8 entries = 256 */
    sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    /* Filtering applies after the CLUT lookup, on colour, not on indices. */
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexScale(1.0f, 1.0f);          /* uv given in texels, not normalised */
    sceGuTexOffset(0.0f, 0.0f);

    /* Textures are capped at 512x512, so the frame is drawn in 64-pixel columns
     * by moving the texture base. Each slice after the first starts 16 texels
     * early, so bilinear filtering does not wrap to column 511 and draw seams. */
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    const int slice = 64;
    const int lead = 16;
    for (int sx = 0; sx < sw; sx += slice) {
        int sw_slice = (sw - sx) < slice ? (sw - sx) : slice;
        int u0 = sx > 0 ? lead : 0;

        blit_vertex_t *v = (blit_vertex_t *)sceGuGetMemory(2 * sizeof(blit_vertex_t));
        v[0].u = (unsigned short)u0;
        v[0].v = (unsigned short)v0;
        v[0].x = (short)(dst_x0 + sx * dst_w / sw);
        v[0].y = 0;
        v[0].z = 0;
        v[1].u = (unsigned short)(u0 + sw_slice);
        v[1].v = (unsigned short)v1;
        v[1].x = (short)(dst_x0 + (sx + sw_slice) * dst_w / sw);
        v[1].y = SCREEN_H;
        v[1].z = 0;

        sceGuTexImage(0, 512, 512, sw, src + sx - u0);
        sceGuDrawArray(GU_SPRITES,
                       GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                       2, 0, v);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    /* Latches at vblank on its own; no need to block. */
    sceGuSwapBuffers();
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

/* ── Profiler hooks (ECS_PROFILE only) ─────────────────────── */

#ifdef ECS_PROFILE
#include "../prof.h"

uint32_t prof_clock_us(void) {
    return sceKernelGetSystemTimeLow();
}

int __real_sceIoRead(SceUID fd, void *data, SceSize size);
int __wrap_sceIoRead(SceUID fd, void *data, SceSize size) {
    uint32_t t = sceKernelGetSystemTimeLow();
    int r = __real_sceIoRead(fd, data, size);
    prof_io_read(sceKernelGetSystemTimeLow() - t, r);
    return r;
}
SceOff __real_sceIoLseek(SceUID fd, SceOff offset, int whence);
SceOff __wrap_sceIoLseek(SceUID fd, SceOff offset, int whence) {
    prof_io_seek();
    return __real_sceIoLseek(fd, offset, whence);
}
int __real_sceIoLseek32(SceUID fd, int offset, int whence);
int __wrap_sceIoLseek32(SceUID fd, int offset, int whence) {
    prof_io_seek();
    return __real_sceIoLseek32(fd, offset, whence);
}
#endif

/* ── Input ──────────────────────────────────────────────────── */

/* 0..255 centred at 128, scaled to int16; Y is inverted (win.c reads positive-up). */
static int16_t stick_axis(unsigned char raw) {
    int v = ((int)raw - 128) * 258;
    /* Symmetric: the caller negates Y, and -(-32768) does not fit int16. */
    if (v >  32767) v =  32767;
    if (v < -32767) v = -32767;
    return (int16_t)v;
}

static SceCtrlData s_pad;

static void pump_pointer(platform_t *p) {
    /* The one stick drives both legs and pointer; req.c reads the pointer
     * only in menus, where nothing walks. Cross clicks. */
    int dx = (int)s_pad.Lx - 128;
    int dy = (int)s_pad.Ly - 128;
    const int dz = 24;

    if (dx > dz || dx < -dz) p->mouse_x += dx / 16;
    if (dy > dz || dy < -dz) p->mouse_y += dy / 16;

    if (p->mouse_x < 0) p->mouse_x = 0;
    if (p->mouse_y < 0) p->mouse_y = 0;
    if (p->mouse_x >= p->render_w) p->mouse_x = p->render_w - 1;
    if (p->mouse_y >= p->render_h) p->mouse_y = p->render_h - 1;

    p->mouse_buttons = (s_pad.Buttons & PSP_CTRL_CROSS) ? PMOUSE_LEFT : 0;
}

bool platform_pump_events(platform_t *p) {
    if (!p)
        return false;

    /* Peek: the Read variant blocks until the next sample. */
    sceCtrlPeekBufferPositive(&s_pad, 1);

    memcpy(p->keys_prev, p->keys_now, sizeof(p->keys_now));

    pump_pointer(p);

    return s_running;
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

    unsigned int b = s_pad.Buttons;

    state->connected  = true;
    state->dpad_up    = (b & PSP_CTRL_UP)    != 0;
    state->dpad_down  = (b & PSP_CTRL_DOWN)  != 0;
    state->dpad_left  = (b & PSP_CTRL_LEFT)  != 0;
    state->dpad_right = (b & PSP_CTRL_RIGHT) != 0;

    state->left_x = stick_axis(s_pad.Lx);
    state->left_y = (int16_t)-stick_axis(s_pad.Ly);
    /* No right stick: E1's quick swings and the graphics toggle stay unbound. */

    state->btn_south = (b & PSP_CTRL_CROSS)    != 0;
    state->btn_east  = (b & PSP_CTRL_CIRCLE)   != 0;
    state->btn_west  = (b & PSP_CTRL_SQUARE)   != 0;
    state->btn_north = (b & PSP_CTRL_TRIANGLE) != 0;
    state->btn_start = (b & PSP_CTRL_START)    != 0;

    bool l = (b & PSP_CTRL_LTRIGGER) != 0;
    bool r = (b & PSP_CTRL_RTRIGGER) != 0;
    bool select = (b & PSP_CTRL_SELECT) != 0;

    /* One shoulder button per side, but the engine wants LB/RB and LT/RT.
     * Select held with a shoulder promotes it to the second row (and does not
     * toggle the HUD); tapped alone it toggles the HUD. */
    if (select && (l || r)) {
        state->btn_lt = l;
        state->btn_rt = r;
    } else {
        state->btn_lb = l;
        state->btn_rb = r;
        state->btn_select = select;
    }
}

/* ── Timing ─────────────────────────────────────────────────── */

uint32_t platform_ticks(platform_t *p) {
    uint64_t now = sceKernelGetSystemTimeWide();
    uint64_t start = p ? p->start_us : 0;
    return (uint32_t)((now - start) / 1000u);
}

void platform_delay(uint32_t ms) {
    if (ms)
        sceKernelDelayThread(ms * 1000u);
}

/* ── Audio ──────────────────────────────────────────────────────
 * One 44100 Hz stereo channel (the only rate sceAudioOutput offers); the 16
 * voices are point-resampled and summed by a dedicated thread.
 */

#define PSP_VOICES      16
#define PSP_AUDIO_RATE  44100
#define PSP_AUDIO_FRAMES 1024          /* per output block; multiple of 64 */

typedef struct {
    const uint8_t *data;
    uint32_t       len;
    uint32_t       idx;      /* whole sample index into data */
    uint32_t       frac;     /* fractional position, 0..0xFFFF */
    uint32_t       step;     /* 16.16 increment: rate / PSP_AUDIO_RATE */
    int            vol;      /* 0..127 */
    int            pan;      /* -128..127 */
    bool           loop;
    volatile bool  active;
} psp_voice_t;

static psp_voice_t s_voices[PSP_VOICES];
static SceUID s_voice_sema = -1;
static SceUID s_audio_thread = -1;
static int    s_audio_channel = -1;
static volatile bool s_audio_ready;
static volatile bool s_audio_quit;
static int s_sfx_vol = 255;            /* 0..255 master */

static short __attribute__((aligned(64))) s_out[PSP_AUDIO_FRAMES * 2];

/* Mix at full precision and clip once. */
static int s_accum[PSP_AUDIO_FRAMES * 2];

static void voices_lock(void) {
    if (s_voice_sema >= 0)
        sceKernelWaitSema(s_voice_sema, 1, NULL);
}

static void voices_unlock(void) {
    if (s_voice_sema >= 0)
        sceKernelSignalSema(s_voice_sema, 1);
}

/* Held under the voice lock, hence the small block: 23 ms of audio, well
 * under a millisecond of work. */
static void mix_block(void) {
    memset(s_accum, 0, sizeof(s_accum));

    voices_lock();
    for (int v = 0; v < PSP_VOICES; v++) {
        psp_voice_t *vo = &s_voices[v];
        if (!vo->active)
            continue;

        const uint8_t *data = vo->data;
        uint32_t idx = vo->idx, frac = vo->frac, step = vo->step, len = vo->len;

        /* 8.8 gain: shift, not divide. */
        int scale = (vo->vol * s_sfx_vol * 256) / (127 * 255);

        /* Attenuate the far side only, so centred sounds are not quieter. */
        int scale_l = vo->pan > 0 ? scale * (127 - vo->pan) / 127 : scale;
        int scale_r = vo->pan < 0 ? scale * (128 + vo->pan) / 128 : scale;

        for (int i = 0; i < PSP_AUDIO_FRAMES; i++) {
            if (idx >= len) {
                if (!vo->loop) { vo->active = false; break; }
                idx = 0;
                frac = 0;
            }

            int s = ((int)data[idx] - 128) << 8;
            s_accum[i * 2 + 0] += (s * scale_l) >> 8;
            s_accum[i * 2 + 1] += (s * scale_r) >> 8;

            /* Index and fraction kept apart: a 16.16 uint32_t caps a sample at
             * 65535 bytes, and speech lines exceed that. */
            frac += step;
            idx += frac >> 16;
            frac &= 0xFFFFu;
        }

        vo->idx = idx;
        vo->frac = frac;
    }
    voices_unlock();

    for (int i = 0; i < PSP_AUDIO_FRAMES * 2; i++) {
        int s = s_accum[i];
        if (s < -32768) s = -32768;
        if (s >  32767) s =  32767;
        s_out[i] = (short)s;
    }
}

static int audio_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    while (!s_audio_quit) {
        mix_block();
        sceAudioOutputPannedBlocking(s_audio_channel,
                                     PSP_AUDIO_VOLUME_MAX,
                                     PSP_AUDIO_VOLUME_MAX,
                                     s_out);
    }
    return 0;
}

void platform_audio_init(void) {
    if (s_audio_ready)
        return;

    memset((void *)s_voices, 0, sizeof(s_voices));

    s_audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
                                        PSP_AUDIO_FRAMES,
                                        PSP_AUDIO_FORMAT_STEREO);
    if (s_audio_channel < 0)
        return;

    s_voice_sema = sceKernelCreateSema("ecs_voices", 0, 1, 1, NULL);
    if (s_voice_sema < 0) {
        sceAudioChRelease(s_audio_channel);
        s_audio_channel = -1;
        return;
    }

    s_audio_quit = false;
    /* Above the main thread (0x11) so a long frame cannot starve the mixer. */
    s_audio_thread = sceKernelCreateThread("ecs_audio", audio_thread,
                                           0x12, 16 * 1024, 0, NULL);
    if (s_audio_thread < 0) {
        sceKernelDeleteSema(s_voice_sema);
        s_voice_sema = -1;
        sceAudioChRelease(s_audio_channel);
        s_audio_channel = -1;
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
    for (int v = 0; v < PSP_VOICES; v++) {
        if (!s_voices[v].active) { slot = v; break; }
    }
    if (slot < 0)
        slot = 0;                       /* steal slot 0, as documented */

    s_voices[slot].active = false;      /* stop before rewriting */
    s_voices[slot].data = (const uint8_t *)data;
    s_voices[slot].len = (uint32_t)length;
    s_voices[slot].idx = 0;
    s_voices[slot].frac = 0;
    s_voices[slot].step = (uint32_t)(((uint64_t)rate << 16) / PSP_AUDIO_RATE);
    s_voices[slot].vol = volume < 0 ? 0 : (volume > 127 ? 127 : volume);
    s_voices[slot].pan = pan;
    s_voices[slot].loop = loop;
    s_voices[slot].active = true;

    voices_unlock();
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= PSP_VOICES)
        return;
    s_voices[slot].active = false;
}

void platform_audio_stop_all(void) {
    for (int v = 0; v < PSP_VOICES; v++)
        s_voices[v].active = false;
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready)
        return;
    s_audio_ready = false;

    platform_audio_stop_all();
    s_audio_quit = true;

    if (s_audio_thread >= 0) {
    /* Returns after at most one 23 ms block. */
        sceKernelWaitThreadEnd(s_audio_thread, NULL);
        sceKernelDeleteThread(s_audio_thread);
        s_audio_thread = -1;
    }
    if (s_audio_channel >= 0) {
        sceAudioChRelease(s_audio_channel);
        s_audio_channel = -1;
    }
    if (s_voice_sema >= 0) {
        sceKernelDeleteSema(s_voice_sema);
        s_voice_sema = -1;
    }
}

/* The PSP has no General MIDI synth (sceMidi is only a UART), so tunes are
 * silent, as on DOS. Effects and speech use the mixer above. */
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

/* Where the data was found, so saves go beside it. Empty: the working
 * directory, when the EBOOT sits with the data. */
static char s_data_root[256];

void platform_early_init(void) {
    /* The working directory is the EBOOT's folder. The rest covers data in a
     * subfolder, or ef0: on a Go. */
    static const char *const candidates[] = {
        "",
        "data",
        "ms0:/PSP/GAME/ECSTATICA",
        "ms0:/PSP/GAME/ECSTATICA/data",
        "ef0:/PSP/GAME/ECSTATICA",
        "ef0:/PSP/GAME/ECSTATICA/data",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (!file_dir_has_database(candidates[i]))
            continue;
        if (candidates[i][0]) {
            file_set_data_root(candidates[i]);
            snprintf(s_data_root, sizeof(s_data_root), "%s", candidates[i]);
            file_flush_path_cache();
        }
        return;
    }

    /* No console to print to; the engine reports its own failure. */
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

/* HOME. Raised on a kernel thread, so it only asks the loop to stop. */
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    s_running = false;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("ecs_exit", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

/* A constructor, since main() is shared code. */
static __attribute__((constructor)) void setup_callbacks(void) {
    SceUID thid = sceKernelCreateThread("ecs_cb", callback_thread,
                                        0x11, 4 * 1024, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
}

void platform_shutdown(platform_t *p) {
    (void)p;
    platform_audio_shutdown();
    sceGuTerm();
    sceKernelExitGame();
}

#endif /* __PSP__ */
