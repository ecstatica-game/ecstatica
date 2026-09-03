/**
 * platforms/psp.c
 *
 * PlayStation Portable backend, built against the PSPSDK toolchain.
 *
 *   Video     sceGu, 480x272 16-bit display. The engine's 8-bit indexed frame
 *             is handed to the GPU as a GU_PSM_T8 texture with a 256-entry
 *             CLUT, so palette expansion and scaling both happen in hardware —
 *             the 333 MHz MIPS core has no cycles to spare for either.
 *   Input     sceCtrl. There is no keyboard, so the PKEY table is always empty
 *             and everything reaches the engine through the gamepad path in
 *             win.c; the analog stick doubles as the pointer for requesters.
 *   Timing    sceKernelGetSystemTimeWide (microseconds since boot).
 *   Audio     One hardware channel at 44100 Hz stereo, fed by a mixer thread
 *             that sums the engine's 16 voices in software. Music is silent —
 *             the PSP has no OS synth, see platform_midi_play.
 *
 * Built by psp/Makefile, which compiles this tree directly rather than through
 * CMake, the same way dos/ and pocket/ do. The sce* calls are confined to this
 * file; the engine reaches the PSP only through platform.h.
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
/* Everything but a megabyte. The archives, the part and event pools and the
 * decoded backgrounds all live in the heap, and on a PSP-1000 there are only
 * ~20 MB of user memory to give them. */
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

/* ── Video ──────────────────────────────────────────────────── */

/* Display list. 64 KB is far more than a handful of textured sprites needs,
 * but it costs nothing that is not already spoken for. */
static unsigned int __attribute__((aligned(16))) s_gu_list[64 * 1024 / 4];

/* CLUT, 16-byte aligned as the GE requires, in the PSP's 0xAABBGGRR order. */
static unsigned int __attribute__((aligned(16))) s_clut[256];

/* The GE reads textures straight out of main memory, so the source has to be
 * 16-byte aligned. malloc returns 16-byte-aligned blocks here and the engine's
 * framebuffer is one, so this normally stays NULL — it is allocated on the
 * first frame that arrives misaligned, rather than costing 300 KB of .bss on a
 * console that has 20 MB in total. */
static uint8_t *s_stage;
static size_t   s_stage_size;

typedef struct {
    unsigned short u, v;
    short x, y, z;
} blit_vertex_t;

static void gu_init(void) {
    /* Two 16-bit draw buffers and a depth buffer the engine never uses; the
     * GE still wants somewhere to point at. 3 * 512 * 272 * 2 = 835 KB of the
     * 2 MB of VRAM. */
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

/* view_cmap is a 256-entry VGA 6-bit RGB table, so every component is 0..63
 * and needs the same << 2 every other backend applies. Rebuilding and
 * re-uploading it costs 1 KB of writes plus a cache flush, so only do it when
 * the palette actually changed — outside fades, it rarely does. */
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
    /* The GE scales whatever the engine renders into the 480x272 panel, so
     * the display half is never the constraint — the data half decides. */
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

    /* The GE addresses a T8 texture in 16-byte units, so the row stride has to
     * be a multiple of 16 pixels. Both games render 320 or 640 wide; anything
     * else is a build that changed the render size and needs looking at, not a
     * frame to draw wrong. */
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

    /* The GE has no coherency with the CPU's data cache: whatever the engine
     * just rasterised is still sitting in it. */
    sceKernelDcacheWritebackRange(src, frame_bytes);

    sceGuStart(GU_DIRECT, s_gu_list);

    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(32, s_clut);          /* 32 blocks of 8 entries = 256 */
    sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    /* Filtering happens after the CLUT lookup, on colour, which is what makes
     * this usable at all: filtering the indices themselves would average index
     * 10 with index 200 into an unrelated colour and show as confetti. If a
     * future firmware ever does the latter, drop to GU_NEAREST here. */
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexScale(1.0f, 1.0f);          /* uv given in texels, not normalised */
    sceGuTexOffset(0.0f, 0.0f);

    /* Textures are capped at 512x512, and both games render wider than that at
     * 640. Walk the frame in 64-pixel columns, moving the texture base along
     * the row instead of the u coordinate — the standard PSP framebuffer blit.
     * A 64-pixel step keeps the base 16-byte aligned. */
    const int slice = 64;
    for (int sx = 0; sx < sw; sx += slice) {
        int sw_slice = (sw - sx) < slice ? (sw - sx) : slice;

        blit_vertex_t *v = (blit_vertex_t *)sceGuGetMemory(2 * sizeof(blit_vertex_t));
        v[0].u = 0;
        v[0].v = 0;
        v[0].x = (short)(sx * SCREEN_W / sw);
        v[0].y = 0;
        v[0].z = 0;
        v[1].u = (unsigned short)sw_slice;
        v[1].v = (unsigned short)sh;
        v[1].x = (short)((sx + sw_slice) * SCREEN_W / sw);
        v[1].y = SCREEN_H;
        v[1].z = 0;

        sceGuTexImage(0, 512, 512, sw, src + sx);
        sceGuDrawArray(GU_SPRITES,
                       GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                       2, 0, v);
    }

    sceGuFinish();
    sceGuSync(0, 0);
    /* sceGuSwapBuffers latches at the next vblank on its own, so there is no
     * tearing and no reason to block here — the engine paces itself and every
     * millisecond spent waiting is one the renderer does not get. */
    sceGuSwapBuffers();
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    /* Only the debug overlay takes this path, and the PSP build does not
     * enable it. */
    (void)p;
    (void)framebuffer;
}

void platform_set_title(platform_t *p, const char *title) {
    (void)p;
    (void)title;
}

/* ── Input ──────────────────────────────────────────────────── */

/* Analog stick: 0..255 per axis, centred at 128. Scale to the int16 range the
 * gamepad interface uses, and invert Y — win.c reads left_y as positive-up. */
static int16_t stick_axis(unsigned char raw) {
    int v = ((int)raw - 128) * 258;
    if (v >  32767) v =  32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

static SceCtrlData s_pad;

static void pump_pointer(platform_t *p) {
    /* The PSP has one stick and no mouse, so the stick drives both the legs
     * and the pointer. In play the cursor is not drawn and moving it is
     * harmless; in the menus and requesters, which is the only place req.c
     * reads it, nothing is walking. Cross clicks, matching the button that
     * already means "confirm" everywhere else. */
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

    /* Peek, not read: sceCtrlReadBufferPositive blocks until the next sample,
     * which would pace the whole game off the controller. */
    sceCtrlPeekBufferPositive(&s_pad, 1);

    /* No keyboard exists, so the key table stays empty and the latches with
     * it; everything the player can press arrives through the pad. */
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
    /* No second stick. win.c uses the right one for E1's quick swings and the
     * graphics toggle; both stay unbound here. */

    state->btn_south = (b & PSP_CTRL_CROSS)    != 0;
    state->btn_east  = (b & PSP_CTRL_CIRCLE)   != 0;
    state->btn_west  = (b & PSP_CTRL_SQUARE)   != 0;
    state->btn_north = (b & PSP_CTRL_TRIANGLE) != 0;
    state->btn_start = (b & PSP_CTRL_START)    != 0;

    bool l = (b & PSP_CTRL_LTRIGGER) != 0;
    bool r = (b & PSP_CTRL_RTRIGGER) != 0;
    bool select = (b & PSP_CTRL_SELECT) != 0;

    /* The PSP has one shoulder button per side and the engine wants two: LB/RB
     * (jump, attack) and LT/RT (E1's per-hand pick-up, E2's magic). Select is
     * the shift — held with a shoulder it promotes it to the second row, and
     * is itself suppressed so the HUD does not toggle underneath the chord.
     * Tapped alone it still toggles the HUD. */
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
 * One hardware channel at 44100 Hz stereo — the only rate sceAudioOutput
 * offers — with the engine's 16 voices summed in software by a dedicated
 * thread. Voices are 8-bit unsigned mono at whatever rate the WAV carried,
 * usually 22050, so each one is point-resampled as it is mixed.
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

/* Signed accumulator, so voices sum at full precision and clip once at the end
 * rather than being clamped against each other one at a time. */
static int s_accum[PSP_AUDIO_FRAMES * 2];

static void voices_lock(void) {
    if (s_voice_sema >= 0)
        sceKernelWaitSema(s_voice_sema, 1, NULL);
}

static void voices_unlock(void) {
    if (s_voice_sema >= 0)
        sceKernelSignalSema(s_voice_sema, 1);
}

/* Mix one output block. Held under the voice lock, which is why the block is
 * small: platform_audio_play_pcm waits on it, and a 1024-frame block is 23 ms
 * of audio but only a fraction of a millisecond of work. */
static void mix_block(void) {
    memset(s_accum, 0, sizeof(s_accum));

    voices_lock();
    for (int v = 0; v < PSP_VOICES; v++) {
        psp_voice_t *vo = &s_voices[v];
        if (!vo->active)
            continue;

        const uint8_t *data = vo->data;
        uint32_t idx = vo->idx, frac = vo->frac, step = vo->step, len = vo->len;

        /* 8.8 gain, so the inner loop shifts instead of dividing — a divide
         * per sample per channel per voice is not affordable here. */
        int scale = (vo->vol * s_sfx_vol * 256) / (127 * 255);

        /* pan is -128 (left) .. 0 (centre) .. 127 (right). Attenuate the far
         * side only, so a centred sound is not quieter than a panned one. */
        int scale_l = vo->pan > 0 ? scale * (127 - vo->pan) / 127 : scale;
        int scale_r = vo->pan < 0 ? scale * (128 + vo->pan) / 128 : scale;

        for (int i = 0; i < PSP_AUDIO_FRAMES; i++) {
            if (idx >= len) {
                if (!vo->loop) { vo->active = false; break; }
                idx = 0;
                frac = 0;
            }

            /* 8-bit unsigned centred on 128, widened to the output's 16 bits. */
            int s = ((int)data[idx] - 128) << 8;
            s_accum[i * 2 + 0] += (s * scale_l) >> 8;
            s_accum[i * 2 + 1] += (s * scale_r) >> 8;

            /* Index and fraction kept apart on purpose: a single 16.16
             * position in a uint32_t caps the sample at 65535 bytes and wraps
             * past that, which speech lines comfortably exceed. */
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
    /* Above the main thread's priority (0x11 by default) so a long frame in
     * the renderer cannot starve the mixer into an underrun. */
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
        /* The thread is parked inside a blocking output call; it returns after
         * at most one block, 23 ms. */
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

/* music.c converts the native Sound Images tune banks to a Standard MIDI File
 * and hands the blob here, expecting a General MIDI synth on the other side.
 * The PSP has none — sceMidi is not a synth, only a UART to the remote port —
 * so tunes are silent until a software synth and a bank are linked in, the
 * same position the DOS target is in. Sound effects and speech are unaffected;
 * they go through the mixer above. */
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

/* Where the data was found, so saves can be written beside it. Empty means the
 * working directory already holds the game, which is the normal case when the
 * EBOOT sits in the same folder. */
static char s_data_root[256];

void platform_early_init(void) {
    /* PSPSDK sets the working directory to the folder the EBOOT was launched
     * from, so data dropped next to it is found with no search at all. The
     * rest covers a Memory Stick install where the game files were put in a
     * subfolder, or on ef0: on a Go. */
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

    /* Nothing to print to on a PSP — the screen belongs to the GU and there is
     * no console. Let the engine fail its own way, with its own message. */
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
    char dir[256];
    if (s_data_root[0])
        snprintf(dir, sizeof(dir), "%s/saved", s_data_root);
    else
        snprintf(dir, sizeof(dir), "saved");
    sceIoMkdir(dir, 0777);
}

/* ── Shutdown ───────────────────────────────────────────────── */

/* HOME. The kernel raises this on its own thread, so all it may do is ask the
 * game loop to stop; platform_pump_events reports it on the next frame and the
 * engine tears down in the usual order. */
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

/* Registered from platform_init via a constructor rather than from main(),
 * which is shared code and knows nothing about PSP callbacks. */
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
