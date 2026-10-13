/**
 * platforms/web.c
 *
 * Browser backend (Emscripten), built by platforms/web/CMakeLists.txt.
 *
 *   Threads   -sPROXY_TO_PTHREAD: main() runs on a worker, so the engine's
 *             blocking loops (menus, dialogs, present_delay) work unchanged.
 *             The browser's main thread only presents frames, plays audio and
 *             receives input. threads.c covers the prefetch and raster threads.
 *   Files     WASMFS with the Origin Private File System mounted at /opfs. The
 *             page copies the user's game folder there once
 *             (/opfs/ecstatica/e1 or e2); saves land beside it and persist.
 *   Video     The game thread expands each frame into one of three RGBA
 *             buffers; web_present, called from requestAnimationFrame on the
 *             main thread, puts the newest onto the canvas.
 *   Input     html5.h callbacks run on the main thread and write the shared key
 *             and mouse state; the Gamepad API is sampled in web_present. The
 *             page's on-screen pad (touch screens) calls web_touch_pad.
 *   Audio     A ScriptProcessorNode on the main thread pulls mixer.c.
 */

#ifdef __EMSCRIPTEN__

#include "../platform.h"
#include "../asm_f.h"
#include "../types.h"
#include "mixer.h"
#include "screen.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/wasmfs.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_RENDER_W 640
#define MAX_RENDER_H 480
#define PKEY_TABLE_SIZE 256

struct platform_t {
    int render_w, render_h;

    volatile bool keys_now[PKEY_TABLE_SIZE];
    bool          keys_prev[PKEY_TABLE_SIZE];
    volatile bool keys_latch[PKEY_TABLE_SIZE];

    volatile int mouse_x, mouse_y;
    volatile int mouse_buttons;

    double start_ms;
};

static platform_t g_plat;

/* ── Frames ──────────────────────────────────────────────────
 * Triple-buffered: the game thread writes a buffer that is neither the newest
 * nor the one the main thread is copying. */

static uint32_t s_frames[3][MAX_RENDER_W * MAX_RENDER_H];
static int s_frame_w[3], s_frame_h[3];
static int s_latest = -1, s_reading = -1;
static unsigned s_seq, s_shown_seq;
static pthread_mutex_t s_frame_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Gamepads, written on the main thread ── */

static platform_gamepad_state_t s_pad;
static platform_gamepad_state_t s_touch_pad;
static pthread_mutex_t s_pad_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── Keyboard ─────────────────────────────────────────────── */

static int code_to_pkey(const char *code) {
    static const struct { const char *code; int key; } map[] = {
        { "Escape", PKEY_ESCAPE }, { "Enter", PKEY_RETURN }, { "NumpadEnter", PKEY_RETURN },
        { "Space", PKEY_SPACE },
        { "ArrowUp", PKEY_UP }, { "ArrowDown", PKEY_DOWN },
        { "ArrowLeft", PKEY_LEFT }, { "ArrowRight", PKEY_RIGHT },
        { "F1", PKEY_F1 }, { "F2", PKEY_F2 }, { "F3", PKEY_F3 }, { "F4", PKEY_F4 },
        { "F5", PKEY_F5 }, { "F6", PKEY_F6 }, { "F7", PKEY_F7 }, { "F8", PKEY_F8 },
        { "F9", PKEY_F9 }, { "F10", PKEY_F10 },
        { "KeyA", PKEY_A }, { "KeyB", PKEY_B }, { "KeyC", PKEY_C }, { "KeyD", PKEY_D },
        { "KeyE", PKEY_E }, { "KeyF", PKEY_F }, { "KeyG", PKEY_G }, { "KeyH", PKEY_H },
        { "KeyI", PKEY_I }, { "KeyL", PKEY_L }, { "KeyM", PKEY_M }, { "KeyN", PKEY_N },
        { "KeyO", PKEY_O }, { "KeyP", PKEY_P }, { "KeyQ", PKEY_Q }, { "KeyR", PKEY_R },
        { "KeyS", PKEY_S }, { "KeyT", PKEY_T }, { "KeyV", PKEY_V }, { "KeyW", PKEY_W },
        { "KeyX", PKEY_X }, { "KeyZ", PKEY_Z },
        { "ControlLeft", PKEY_LCTRL }, { "ControlRight", PKEY_LCTRL },
        { "AltLeft", PKEY_LALT }, { "AltRight", PKEY_LALT },
        { "ShiftLeft", PKEY_LSHIFT }, { "ShiftRight", PKEY_RSHIFT },
        { "MetaLeft", PKEY_LCMD },
        { "Numpad1", PKEY_NUM1 }, { "Numpad2", PKEY_NUM2 }, { "Numpad3", PKEY_NUM3 },
        { "Numpad4", PKEY_NUM4 }, { "Numpad5", PKEY_NUM5 }, { "Numpad6", PKEY_NUM6 },
        { "Numpad7", PKEY_NUM7 }, { "Numpad8", PKEY_NUM8 }, { "Numpad9", PKEY_NUM9 },
        { "Digit1", PKEY_1 }, { "Digit2", PKEY_2 }, { "Digit3", PKEY_3 },
        { "Tab", PKEY_TAB }, { "Minus", PKEY_MINUS }, { "Equal", PKEY_EQUALS },
        { "BracketLeft", PKEY_LBRACKET }, { "BracketRight", PKEY_RBRACKET },
        { "Comma", PKEY_COMMA }, { "Period", PKEY_PERIOD },
        { "Home", PKEY_HOME }, { "PageUp", PKEY_PGUP },
        { "End", PKEY_END }, { "PageDown", PKEY_PGDN },
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strcmp(code, map[i].code) == 0) return map[i].key;
    return -1;
}

static EM_BOOL on_key(int type, const EmscriptenKeyboardEvent *e, void *user) {
    platform_t *p = (platform_t *)user;
    int key = code_to_pkey(e->code);
    if (key < 0) return EM_FALSE;       /* F11, F12 and the rest stay the browser's */
    bool down = type == EMSCRIPTEN_EVENT_KEYDOWN;
    if (down && !e->repeat) p->keys_latch[key] = true;
    p->keys_now[key] = down;
    return EM_TRUE;
}

/* Any key the page does not get (focus lost) must not stay held. */
static EM_BOOL on_blur(int type, const EmscriptenFocusEvent *e, void *user) {
    (void)type;
    (void)e;
    platform_t *p = (platform_t *)user;
    for (int i = 0; i < PKEY_TABLE_SIZE; i++) p->keys_now[i] = false;
    p->mouse_buttons = 0;
    return EM_FALSE;
}

/* ── Pointer ──────────────────────────────────────────────── */

static void pointer_at(platform_t *p, int css_x, int css_y) {
    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw <= 0 || ch <= 0) return;
    int x = (int)(css_x * p->render_w / cw);
    int y = (int)(css_y * p->render_h / ch);
    p->mouse_x = x < 0 ? 0 : (x >= p->render_w ? p->render_w - 1 : x);
    p->mouse_y = y < 0 ? 0 : (y >= p->render_h ? p->render_h - 1 : y);
}

static EM_BOOL on_mouse(int type, const EmscriptenMouseEvent *e, void *user) {
    (void)type;
    platform_t *p = (platform_t *)user;
    pointer_at(p, e->targetX, e->targetY);
    int b = 0;
    if (e->buttons & 1) b |= PMOUSE_LEFT;
    if (e->buttons & 2) b |= PMOUSE_RIGHT;
    if (e->buttons & 4) b |= PMOUSE_MIDDLE;
    p->mouse_buttons = b;
    return EM_TRUE;
}

static EM_BOOL on_touch(int type, const EmscriptenTouchEvent *e, void *user) {
    platform_t *p = (platform_t *)user;
    if (e->numTouches > 0)
        pointer_at(p, e->touches[0].targetX, e->touches[0].targetY);
    p->mouse_buttons = (type == EMSCRIPTEN_EVENT_TOUCHSTART ||
                        type == EMSCRIPTEN_EVENT_TOUCHMOVE) ? PMOUSE_LEFT : 0;
    return EM_TRUE;
}

/* ── Main-thread entry points, called by the page ──────────── */

static int16_t axis(double v) {
    int i = (int)(v * 32767.0);
    return (int16_t)(i > 32767 ? 32767 : (i < -32767 ? -32767 : i));
}

static void sample_gamepad(void) {
    platform_gamepad_state_t st;
    memset(&st, 0, sizeof(st));

    if (emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS) {
        int n = emscripten_get_num_gamepads();
        for (int i = 0; i < n; i++) {
            EmscriptenGamepadEvent g;
            if (emscripten_get_gamepad_status(i, &g) != EMSCRIPTEN_RESULT_SUCCESS || !g.connected)
                continue;
            /* The "standard" mapping: buttons 0-3 face, 4-7 shoulders and
             * triggers, 8/9 select/start, 10/11 stick clicks, 12-15 d-pad. */
            const EM_BOOL *b = g.digitalButton;
            int nb = g.numButtons;
            #define BTN(k) ((k) < nb && b[k])
            st.connected  = true;
            st.btn_south  = BTN(0);
            st.btn_east   = BTN(1);
            st.btn_west   = BTN(2);
            st.btn_north  = BTN(3);
            st.btn_lb     = BTN(4);
            st.btn_rb     = BTN(5);
            st.btn_lt     = BTN(6) || (nb > 6 && g.analogButton[6] > 0.3);
            st.btn_rt     = BTN(7) || (nb > 7 && g.analogButton[7] > 0.3);
            st.btn_select = BTN(8);
            st.btn_start  = BTN(9);
            st.btn_lstick = BTN(10);
            st.btn_rstick = BTN(11);
            st.dpad_up    = BTN(12);
            st.dpad_down  = BTN(13);
            st.dpad_left  = BTN(14);
            st.dpad_right = BTN(15);
            #undef BTN
            if (g.numAxes >= 2) {
                st.left_x = axis(g.axis[0]);
                st.left_y = (int16_t)-axis(g.axis[1]);   /* win.c: Y positive-up */
            }
            if (g.numAxes >= 4) {
                st.right_x = axis(g.axis[2]);
                st.right_y = (int16_t)-axis(g.axis[3]);
            }
            break;
        }
    }

    pthread_mutex_lock(&s_pad_lock);
    s_pad = st;
    pthread_mutex_unlock(&s_pad_lock);
}

EMSCRIPTEN_KEEPALIVE void web_present(void) {
    sample_gamepad();

    pthread_mutex_lock(&s_frame_lock);
    int idx = (s_latest >= 0 && s_seq != s_shown_seq) ? s_latest : -1;
    if (idx >= 0) {
        s_reading = idx;
        s_shown_seq = s_seq;
    }
    pthread_mutex_unlock(&s_frame_lock);
    if (idx < 0) return;

    EM_ASM({
        var c = Module.canvas;
        if (!c) return;
        var w = $1;
        var h = $2;
        if (c.width != w || c.height != h || !Module.ecsImg) {
            c.width = w;
            c.height = h;
            Module.ecsCtx = c.getContext('2d');
            Module.ecsImg = Module.ecsCtx.createImageData(w, h);
        }
        Module.ecsImg.data.set(HEAPU8.subarray($0, $0 + w * h * 4));
        Module.ecsCtx.putImageData(Module.ecsImg, 0, 0);
    }, s_frames[idx], s_frame_w[idx], s_frame_h[idx]);

    pthread_mutex_lock(&s_frame_lock);
    s_reading = -1;
    pthread_mutex_unlock(&s_frame_lock);
}

/* The page's on-screen pad: a bitmask in platform_gamepad_state_t field
 * order (south, east, west, north, lb, rb, lt, rt, start, select, lstick,
 * rstick) and the stick in -32767..32767, Y positive-up. */
EMSCRIPTEN_KEEPALIVE void web_touch_pad(int buttons, int lx, int ly) {
    platform_gamepad_state_t st;
    memset(&st, 0, sizeof(st));
    st.connected  = true;
    st.btn_south  = (buttons >> 0) & 1;
    st.btn_east   = (buttons >> 1) & 1;
    st.btn_west   = (buttons >> 2) & 1;
    st.btn_north  = (buttons >> 3) & 1;
    st.btn_lb     = (buttons >> 4) & 1;
    st.btn_rb     = (buttons >> 5) & 1;
    st.btn_lt     = (buttons >> 6) & 1;
    st.btn_rt     = (buttons >> 7) & 1;
    st.btn_start  = (buttons >> 8) & 1;
    st.btn_select = (buttons >> 9) & 1;
    st.btn_lstick = (buttons >> 10) & 1;
    st.btn_rstick = (buttons >> 11) & 1;
    st.left_x = (int16_t)lx;
    st.left_y = (int16_t)ly;
    pthread_mutex_lock(&s_pad_lock);
    s_touch_pad = st;
    pthread_mutex_unlock(&s_pad_lock);
}

/* ── platform.h: video ─────────────────────────────────────── */

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)scale;
    platform_t *p = &g_plat;
    memset((void *)p, 0, sizeof(*p));
    p->render_w = fb_width;
    p->render_h = fb_height;
    p->mouse_x = fb_width / 2;
    p->mouse_y = fb_height / 2;
    p->start_ms = emscripten_get_now();

    pthread_t main_thread = EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD;
    emscripten_set_keydown_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, p, 1, on_key, main_thread);
    emscripten_set_keyup_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, p, 1, on_key, main_thread);
    emscripten_set_blur_callback_on_thread(EMSCRIPTEN_EVENT_TARGET_WINDOW, p, 1, on_blur, main_thread);
    emscripten_set_mousedown_callback_on_thread("#canvas", p, 1, on_mouse, main_thread);
    emscripten_set_mouseup_callback_on_thread("#canvas", p, 1, on_mouse, main_thread);
    emscripten_set_mousemove_callback_on_thread("#canvas", p, 1, on_mouse, main_thread);
    emscripten_set_touchstart_callback_on_thread("#canvas", p, 1, on_touch, main_thread);
    emscripten_set_touchmove_callback_on_thread("#canvas", p, 1, on_touch, main_thread);
    emscripten_set_touchend_callback_on_thread("#canvas", p, 1, on_touch, main_thread);
    emscripten_set_touchcancel_callback_on_thread("#canvas", p, 1, on_touch, main_thread);

    platform_set_title(p, title);
    return p;
}

bool platform_hires_supported(platform_t *p) {
    (void)p;
    return true;
}

bool platform_boot_low_res(platform_t *p) {
    (void)p;
    return false;
}

/* The page scales the canvas to 4:3 with CSS. */
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
    if (!p || w <= 0 || h <= 0) return;
    p->render_w = w;
    p->render_h = h;
    if (p->mouse_x >= w) p->mouse_x = w - 1;
    if (p->mouse_y >= h) p->mouse_y = h - 1;
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || !palette) return;
    int w = p->render_w, h = p->render_h;
    if (w <= 0 || h <= 0 || w > MAX_RENDER_W || h > MAX_RENDER_H) return;

    pthread_mutex_lock(&s_frame_lock);
    int idx = 0;
    while (idx == s_latest || idx == s_reading) idx++;
    pthread_mutex_unlock(&s_frame_lock);

    screen_present(s_frames[idx], w, h, w, framebuffer, w, h, palette, SCALE_STRETCH);
    s_frame_w[idx] = w;
    s_frame_h[idx] = h;

    pthread_mutex_lock(&s_frame_lock);
    s_latest = idx;
    s_seq++;
    pthread_mutex_unlock(&s_frame_lock);
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    /* Only the debug overlay uses this, and it is off here. */
    (void)p;
    (void)framebuffer;
}

void platform_set_title(platform_t *p, const char *title) {
    (void)p;
    if (title)
        MAIN_THREAD_EM_ASM({ document.title = UTF8ToString($0); }, title);
}

/* ── platform.h: input ─────────────────────────────────────── */

bool platform_pump_events(platform_t *p) {
    if (!p) return false;
    memcpy(p->keys_prev, (const void *)p->keys_now, sizeof(p->keys_prev));
    return true;
}

bool platform_key_down(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    return p->keys_now[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    return p->keys_now[keycode] && !p->keys_prev[keycode];
}

bool platform_key_hit(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    bool hit = p->keys_latch[keycode];
    p->keys_latch[keycode] = false;
    return hit;
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y) {
    if (!p) return 0;
    if (out_x) *out_x = p->mouse_x;
    if (out_y) *out_y = p->mouse_y;
    return p->mouse_buttons;
}

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state) {
    (void)p;
    pthread_mutex_lock(&s_pad_lock);
    platform_gamepad_state_t pad = s_pad, touch = s_touch_pad;
    pthread_mutex_unlock(&s_pad_lock);

    *state = pad;
    if (!touch.connected) return;
    state->connected = true;
    state->btn_south  |= touch.btn_south;
    state->btn_east   |= touch.btn_east;
    state->btn_west   |= touch.btn_west;
    state->btn_north  |= touch.btn_north;
    state->btn_lb     |= touch.btn_lb;
    state->btn_rb     |= touch.btn_rb;
    state->btn_lt     |= touch.btn_lt;
    state->btn_rt     |= touch.btn_rt;
    state->btn_start  |= touch.btn_start;
    state->btn_select |= touch.btn_select;
    state->btn_lstick |= touch.btn_lstick;
    state->btn_rstick |= touch.btn_rstick;
    if (touch.left_x || touch.left_y) {
        state->left_x = touch.left_x;
        state->left_y = touch.left_y;
    }
}

/* ── platform.h: timing ────────────────────────────────────── */

uint32_t platform_ticks(platform_t *p) {
    double start = p ? p->start_ms : g_plat.start_ms;
    return (uint32_t)(emscripten_get_now() - start);
}

void platform_delay(uint32_t ms) {
    if (ms) usleep(ms * 1000u);
}

/* ── platform.h: audio ─────────────────────────────────────── */

#define AUDIO_FRAMES 2048

static int16_t s_audio_buf[AUDIO_FRAMES * 2];
static bool s_audio_ready;

/* Called by the ScriptProcessorNode on the main thread. */
EMSCRIPTEN_KEEPALIVE int16_t *web_audio_render(int frames) {
    if (frames > AUDIO_FRAMES) frames = AUDIO_FRAMES;
    mixer_render(s_audio_buf, frames);
    return s_audio_buf;
}

void platform_audio_init(void) {
    if (s_audio_ready) return;
    /* The page creates Module.audioCtx in the click that starts the game:
     * browsers only let a user gesture start audio. */
    int rate = MAIN_THREAD_EM_ASM_INT({
        return Module.audioCtx ? Module.audioCtx.sampleRate : 0;
    });
    if (rate <= 0) return;

    mixer_init(rate, "/opfs/ecstatica");
    MAIN_THREAD_EM_ASM({
        var ctx = Module.audioCtx;
        var node = ctx.createScriptProcessor($0, 0, 2);
        node.onaudioprocess = function(e) {
            var out = e.outputBuffer;
            var n = out.length;
            var base = Module._web_audio_render(n) >> 1;
            var l = out.getChannelData(0);
            var r = out.getChannelData(1);
            for (var i = 0; i < n; i++) {
                l[i] = HEAP16[base + 2 * i] / 32768;
                r[i] = HEAP16[base + 2 * i + 1] / 32768;
            }
        };
        node.connect(ctx.destination);
        Module.audioNode = node;
        if (ctx.state === 'suspended') ctx.resume();
    }, AUDIO_FRAMES);
    s_audio_ready = true;
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready) return;
    MAIN_THREAD_EM_ASM({
        if (Module.audioNode) { Module.audioNode.disconnect(); Module.audioNode = null; }
    });
    mixer_shutdown();
    s_audio_ready = false;
}

/* ── platform.h: files ─────────────────────────────────────── */

void platform_early_init(void) {
    static char dir[64];
    const char *game = getenv("ECSTATICA_GAME");
    if (!game || (strcmp(game, "e1") != 0 && strcmp(game, "e2") != 0))
        game = "e2";

    backend_t opfs = wasmfs_create_opfs_backend();
    wasmfs_create_directory("/opfs", 0777, opfs);

    snprintf(dir, sizeof(dir), "/opfs/ecstatica/%s", game);
    if (file_dir_has_database(dir) && chdir(dir) == 0) {
        file_flush_path_cache();
        return;
    }

    MAIN_THREAD_EM_ASM({
        if (Module.onNoData) Module.onNoData(UTF8ToString($0));
    }, dir);
    exit(2);
}

/* OPFS writes go through WASMFS's proxy thread; the log is written off the
 * game thread. */
bool platform_slow_writes(void) {
    return true;
}

int platform_save_slot_count(void) {
    return 11;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version) {
    (void)game_version;   /* one folder per game */
    snprintf(buf, bufsz, "saved/%04d.ecs", slot);
}

void platform_save_prepare(void) {
    mkdir("saved", 0777);
}

void platform_shutdown(platform_t *p) {
    (void)p;
    platform_audio_shutdown();
}

#endif /* __EMSCRIPTEN__ */
