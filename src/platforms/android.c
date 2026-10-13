/**
 * platforms/android.c
 *
 * Android backend (NDK NativeActivity), built by platforms/android/build.sh.
 *
 *   Entry     android_main, on the native_app_glue thread, which then runs the
 *             engine's main() (compiled as ecs_main). The engine's blocking
 *             loops are fine there; platform_pump_events drains the looper.
 *   Video     The window buffer is sized to the screen's aspect at no more
 *             than 960 lines and the compositor scales it up. screen.c
 *             expands the frame into it and the on-screen pad is drawn on top.
 *   Input     Touch (on-screen pad and pointer), keyboards, gamepads, and the
 *             Back button as Escape.
 *   Audio     AAudio callback pulling mixer.c.
 *
 * The launcher (platforms/android/java) imports the game folders into the
 * app's external files directory and starts this activity with the game to
 * run, "e1" or "e2", as the intent extra "game".
 */

#ifdef __ANDROID__

#include "../platform.h"
#include "../asm_f.h"
#include "../types.h"
#include "mixer.h"
#include "screen.h"
#include "touch_controls.h"

#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int ecs_main(int argc, char *argv[]);

#define PKEY_TABLE_SIZE 256
/* Window buffers taller than this cost fill time without looking better: the
 * game is at most 480 lines. */
#define MAX_BUFFER_H 960

struct platform_t {
    int render_w, render_h;
    int scale_mode;

    bool keys_now[PKEY_TABLE_SIZE];
    bool keys_prev[PKEY_TABLE_SIZE];
    bool keys_latch[PKEY_TABLE_SIZE];

    int mouse_x, mouse_y;
    int mouse_buttons;
    bool real_mouse;        /* a mouse moved last, not a touch */

    uint64_t start_ns;
};

static platform_t g_plat;
static struct android_app *s_app;
static ANativeWindow *s_window;
static int  s_buf_w, s_buf_h;           /* window buffer geometry */
static int  s_win_w, s_win_h;           /* window in pixels, for input */
static bool s_resumed, s_focused;
static char s_files_dir[512];

static platform_gamepad_state_t s_pad;
static bool s_pad_seen;
/* Some pads report the triggers as axes, some as L2/R2 keys, some as both. */
static bool s_key_lt, s_key_rt, s_axis_lt, s_axis_rt;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ── Window ──────────────────────────────────────────────── */

static void window_setup(ANativeWindow *win) {
    s_window = win;
    if (!win) return;
    s_win_w = ANativeWindow_getWidth(win);
    s_win_h = ANativeWindow_getHeight(win);
    int div = (s_win_h + MAX_BUFFER_H - 1) / MAX_BUFFER_H;
    if (div < 1) div = 1;
    s_buf_w = s_win_w / div;
    s_buf_h = s_win_h / div;
    ANativeWindow_setBuffersGeometry(win, s_buf_w, s_buf_h, WINDOW_FORMAT_RGBX_8888);
    touch_layout(s_buf_w, s_buf_h);
}

/* Window pixels to buffer pixels. */
static void to_buffer(float x, float y, int *bx, int *by) {
    *bx = s_win_w > 0 ? (int)(x * s_buf_w / s_win_w) : 0;
    *by = s_win_h > 0 ? (int)(y * s_buf_h / s_win_h) : 0;
}

/* ── Audio ───────────────────────────────────────────────── */

static AAudioStream *s_stream;

static aaudio_data_callback_result_t audio_cb(AAudioStream *stream, void *user,
                                              void *data, int32_t frames) {
    (void)stream;
    (void)user;
    mixer_render((int16_t *)data, frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void audio_pause(bool pause) {
    if (!s_stream) return;
    if (pause) AAudioStream_requestPause(s_stream);
    else AAudioStream_requestStart(s_stream);
}

void platform_audio_init(void) {
    if (s_stream) return;
    AAudioStreamBuilder *b;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, audio_cb, NULL);
    AAudioStream *st = NULL;
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &st);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !st) {
        DBG_LOG(1, "[AUDIO] AAudio open failed: %s\n", AAudio_convertResultToText(r));
        return;
    }
    mixer_init(AAudioStream_getSampleRate(st), s_files_dir);
    s_stream = st;
    if (s_resumed) AAudioStream_requestStart(st);
}

void platform_audio_shutdown(void) {
    if (!s_stream) return;
    AAudioStream_requestStop(s_stream);
    AAudioStream_close(s_stream);
    s_stream = NULL;
    mixer_shutdown();
}

/* ── Lifecycle ───────────────────────────────────────────── */

static void on_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        window_setup(app->window);
        break;
    case APP_CMD_TERM_WINDOW:
        s_window = NULL;
        touch_cancel_all();
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        if (app->window) window_setup(app->window);
        break;
    case APP_CMD_GAINED_FOCUS:
        s_focused = true;
        break;
    case APP_CMD_LOST_FOCUS:
        s_focused = false;
        touch_cancel_all();
        memset(g_plat.keys_now, 0, sizeof(g_plat.keys_now));
        break;
    case APP_CMD_RESUME:
        s_resumed = true;
        audio_pause(false);
        break;
    case APP_CMD_PAUSE:
        s_resumed = false;
        audio_pause(true);
        break;
    default:
        break;
    }
}

/* ── Input ───────────────────────────────────────────────── */

static int keycode_to_pkey(int32_t k) {
    switch (k) {
    case AKEYCODE_ESCAPE: case AKEYCODE_BACK: return PKEY_ESCAPE;
    case AKEYCODE_ENTER: case AKEYCODE_NUMPAD_ENTER: return PKEY_RETURN;
    case AKEYCODE_SPACE: return PKEY_SPACE;
    case AKEYCODE_DPAD_UP: return PKEY_UP;
    case AKEYCODE_DPAD_DOWN: return PKEY_DOWN;
    case AKEYCODE_DPAD_LEFT: return PKEY_LEFT;
    case AKEYCODE_DPAD_RIGHT: return PKEY_RIGHT;
    case AKEYCODE_F1: return PKEY_F1;   case AKEYCODE_F2: return PKEY_F2;
    case AKEYCODE_F3: return PKEY_F3;   case AKEYCODE_F4: return PKEY_F4;
    case AKEYCODE_F5: return PKEY_F5;   case AKEYCODE_F6: return PKEY_F6;
    case AKEYCODE_F7: return PKEY_F7;   case AKEYCODE_F8: return PKEY_F8;
    case AKEYCODE_F9: return PKEY_F9;   case AKEYCODE_F10: return PKEY_F10;
    case AKEYCODE_F11: return PKEY_F11; case AKEYCODE_F12: return PKEY_F12;
    case AKEYCODE_A: return PKEY_A; case AKEYCODE_B: return PKEY_B;
    case AKEYCODE_C: return PKEY_C; case AKEYCODE_D: return PKEY_D;
    case AKEYCODE_E: return PKEY_E; case AKEYCODE_F: return PKEY_F;
    case AKEYCODE_G: return PKEY_G; case AKEYCODE_H: return PKEY_H;
    case AKEYCODE_I: return PKEY_I; case AKEYCODE_L: return PKEY_L;
    case AKEYCODE_M: return PKEY_M; case AKEYCODE_N: return PKEY_N;
    case AKEYCODE_O: return PKEY_O; case AKEYCODE_P: return PKEY_P;
    case AKEYCODE_Q: return PKEY_Q; case AKEYCODE_R: return PKEY_R;
    case AKEYCODE_S: return PKEY_S; case AKEYCODE_T: return PKEY_T;
    case AKEYCODE_V: return PKEY_V; case AKEYCODE_W: return PKEY_W;
    case AKEYCODE_X: return PKEY_X; case AKEYCODE_Z: return PKEY_Z;
    case AKEYCODE_CTRL_LEFT: case AKEYCODE_CTRL_RIGHT: return PKEY_LCTRL;
    case AKEYCODE_ALT_LEFT: case AKEYCODE_ALT_RIGHT: return PKEY_LALT;
    case AKEYCODE_SHIFT_LEFT: return PKEY_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT: return PKEY_RSHIFT;
    case AKEYCODE_NUMPAD_1: return PKEY_NUM1; case AKEYCODE_NUMPAD_2: return PKEY_NUM2;
    case AKEYCODE_NUMPAD_3: return PKEY_NUM3; case AKEYCODE_NUMPAD_4: return PKEY_NUM4;
    case AKEYCODE_NUMPAD_5: return PKEY_NUM5; case AKEYCODE_NUMPAD_6: return PKEY_NUM6;
    case AKEYCODE_NUMPAD_7: return PKEY_NUM7; case AKEYCODE_NUMPAD_8: return PKEY_NUM8;
    case AKEYCODE_NUMPAD_9: return PKEY_NUM9;
    case AKEYCODE_1: return PKEY_1; case AKEYCODE_2: return PKEY_2; case AKEYCODE_3: return PKEY_3;
    case AKEYCODE_TAB: return PKEY_TAB;
    case AKEYCODE_MINUS: return PKEY_MINUS;
    case AKEYCODE_EQUALS: return PKEY_EQUALS;
    case AKEYCODE_LEFT_BRACKET: return PKEY_LBRACKET;
    case AKEYCODE_RIGHT_BRACKET: return PKEY_RBRACKET;
    case AKEYCODE_COMMA: return PKEY_COMMA;
    case AKEYCODE_PERIOD: return PKEY_PERIOD;
    case AKEYCODE_MOVE_HOME: return PKEY_HOME;
    case AKEYCODE_PAGE_UP: return PKEY_PGUP;
    case AKEYCODE_MOVE_END: return PKEY_END;
    case AKEYCODE_PAGE_DOWN: return PKEY_PGDN;
    default: return -1;
    }
}

/* Gamepad buttons; false if the key is not one. */
static bool pad_button(int32_t k, bool down) {
    switch (k) {
    case AKEYCODE_BUTTON_A: s_pad.btn_south = down; break;
    case AKEYCODE_BUTTON_B: s_pad.btn_east = down; break;
    case AKEYCODE_BUTTON_X: s_pad.btn_west = down; break;
    case AKEYCODE_BUTTON_Y: s_pad.btn_north = down; break;
    case AKEYCODE_BUTTON_L1: s_pad.btn_lb = down; break;
    case AKEYCODE_BUTTON_R1: s_pad.btn_rb = down; break;
    case AKEYCODE_BUTTON_L2: s_key_lt = down; break;
    case AKEYCODE_BUTTON_R2: s_key_rt = down; break;
    case AKEYCODE_BUTTON_START: s_pad.btn_start = down; break;
    case AKEYCODE_BUTTON_SELECT: s_pad.btn_select = down; break;
    case AKEYCODE_BUTTON_THUMBL: s_pad.btn_lstick = down; break;
    case AKEYCODE_BUTTON_THUMBR: s_pad.btn_rstick = down; break;
    case AKEYCODE_DPAD_UP: s_pad.dpad_up = down; break;
    case AKEYCODE_DPAD_DOWN: s_pad.dpad_down = down; break;
    case AKEYCODE_DPAD_LEFT: s_pad.dpad_left = down; break;
    case AKEYCODE_DPAD_RIGHT: s_pad.dpad_right = down; break;
    default: return false;
    }
    return true;
}

static bool from_gamepad(int32_t source) {
    return (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD ||
           (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
}

static int16_t axis(float v) {
    int i = (int)(v * 32767.0f);
    return (int16_t)(i > 32767 ? 32767 : (i < -32767 ? -32767 : i));
}

static int32_t on_key(const AInputEvent *e) {
    int32_t k = AKeyEvent_getKeyCode(e);
    int32_t action = AKeyEvent_getAction(e);
    if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP)
        return 0;
    bool down = action == AKEY_EVENT_ACTION_DOWN;

    /* Volume keys stay the system's. */
    if (k == AKEYCODE_VOLUME_UP || k == AKEYCODE_VOLUME_DOWN || k == AKEYCODE_VOLUME_MUTE)
        return 0;

    if (from_gamepad(AInputEvent_getSource(e)) && pad_button(k, down)) {
        s_pad_seen = true;
        touch_hide();
        return 1;
    }
    int key = keycode_to_pkey(k);
    if (key < 0) return 0;
    if (down && AKeyEvent_getRepeatCount(e) == 0) g_plat.keys_latch[key] = true;
    g_plat.keys_now[key] = down;
    if (k != AKEYCODE_BACK) touch_hide();
    return 1;
}

static int32_t on_motion(const AInputEvent *e) {
    int32_t source = AInputEvent_getSource(e);
    int32_t action = AMotionEvent_getAction(e);
    int32_t masked = action & AMOTION_EVENT_ACTION_MASK;

    if ((source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK) {
        s_pad_seen = true;
        s_pad.left_x  = axis(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_X, 0));
        s_pad.left_y  = (int16_t)-axis(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Y, 0));
        s_pad.right_x = axis(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_Z, 0));
        s_pad.right_y = (int16_t)-axis(AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_RZ, 0));
        float hx = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_X, 0);
        float hy = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_HAT_Y, 0);
        s_pad.dpad_left = hx < -0.5f;
        s_pad.dpad_right = hx > 0.5f;
        s_pad.dpad_up = hy < -0.5f;
        s_pad.dpad_down = hy > 0.5f;
        float lt = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_LTRIGGER, 0);
        float rt = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_RTRIGGER, 0);
        float brake = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_BRAKE, 0);
        float gas = AMotionEvent_getAxisValue(e, AMOTION_EVENT_AXIS_GAS, 0);
        s_axis_lt = (lt > brake ? lt : brake) > 0.3f;
        s_axis_rt = (rt > gas ? rt : gas) > 0.3f;
        touch_hide();
        return 1;
    }

    if ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE) {
        int bx, by;
        to_buffer(AMotionEvent_getX(e, 0), AMotionEvent_getY(e, 0), &bx, &by);
        screen_rect_t pic = screen_picture_rect(s_buf_w, s_buf_h, g_plat.scale_mode);
        int rx, ry;
        screen_to_render(&pic, g_plat.render_w, g_plat.render_h, bx, by, &rx, &ry);
        g_plat.mouse_x = rx;
        g_plat.mouse_y = ry;
        int32_t b = AMotionEvent_getButtonState(e);
        g_plat.mouse_buttons = ((b & AMOTION_EVENT_BUTTON_PRIMARY) ? PMOUSE_LEFT : 0) |
                               ((b & AMOTION_EVENT_BUTTON_SECONDARY) ? PMOUSE_RIGHT : 0) |
                               ((b & AMOTION_EVENT_BUTTON_TERTIARY) ? PMOUSE_MIDDLE : 0);
        g_plat.real_mouse = true;
        return 1;
    }

    if ((source & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN) {
        g_plat.real_mouse = false;
        size_t n = AMotionEvent_getPointerCount(e);
        if (masked == AMOTION_EVENT_ACTION_MOVE) {
            for (size_t i = 0; i < n; i++) {
                int bx, by;
                to_buffer(AMotionEvent_getX(e, i), AMotionEvent_getY(e, i), &bx, &by);
                touch_event(AMotionEvent_getPointerId(e, i), TOUCH_MOVE, bx, by);
            }
        } else if (masked == AMOTION_EVENT_ACTION_CANCEL) {
            touch_cancel_all();
        } else {
            size_t i = (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
            int phase = (masked == AMOTION_EVENT_ACTION_DOWN ||
                         masked == AMOTION_EVENT_ACTION_POINTER_DOWN) ? TOUCH_DOWN : TOUCH_UP;
            if (i < n) {
                int bx, by;
                to_buffer(AMotionEvent_getX(e, i), AMotionEvent_getY(e, i), &bx, &by);
                touch_event(AMotionEvent_getPointerId(e, i), phase, bx, by);
            }
        }
        return 1;
    }
    return 0;
}

static int32_t on_input(struct android_app *app, AInputEvent *e) {
    (void)app;
    switch (AInputEvent_getType(e)) {
    case AINPUT_EVENT_TYPE_KEY: return on_key(e);
    case AINPUT_EVENT_TYPE_MOTION: return on_motion(e);
    default: return 0;
    }
}

/* Drains the looper; blocks while the app is in the background, so a game in
 * the background is paused rather than running blind. */
static bool drain_events(void) {
    for (;;) {
        bool active = s_resumed && s_window;
        int events;
        struct android_poll_source *src = NULL;
        int r = ALooper_pollOnce(active ? 0 : 250, NULL, &events, (void **)&src);
        if (src) src->process(s_app, src);
        if (s_app->destroyRequested) return false;
        if (r == ALOOPER_POLL_TIMEOUT && (s_resumed && s_window)) return true;
        if (r == ALOOPER_POLL_ERROR) return true;
    }
}

/* ── platform.h ──────────────────────────────────────────── */

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)title;
    (void)scale;
    platform_t *p = &g_plat;
    p->render_w = fb_width;
    p->render_h = fb_height;
    p->mouse_x = fb_width / 2;
    p->mouse_y = fb_height / 2;
    p->start_ns = now_ns();
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

bool platform_scale_mode_supported(platform_t *p) {
    (void)p;
    return true;
}

void platform_set_scale_mode(platform_t *p, int mode) {
    if (p && mode >= SCALE_PILLARBOX && mode <= SCALE_STRETCH) p->scale_mode = mode;
}

int platform_crop_inset_y(platform_t *p, int render_h) {
    return p ? screen_crop_inset_y(s_buf_w, s_buf_h, render_h, p->scale_mode) : 0;
}

void platform_set_render_size(platform_t *p, int w, int h) {
    if (!p || w <= 0 || h <= 0) return;
    p->render_w = w;
    p->render_h = h;
    if (p->mouse_x >= w) p->mouse_x = w - 1;
    if (p->mouse_y >= h) p->mouse_y = h - 1;
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || !palette || !s_window) return;
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(s_window, &buf, NULL) != 0) return;
    screen_present((uint32_t *)buf.bits, buf.width, buf.height, buf.stride,
                   framebuffer, p->render_w, p->render_h, palette, p->scale_mode);
    touch_draw((uint32_t *)buf.bits, buf.width, buf.height, buf.stride);
    ANativeWindow_unlockAndPost(s_window);
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

bool platform_pump_events(platform_t *p) {
    if (!p) return false;
    memcpy(p->keys_prev, p->keys_now, sizeof(p->keys_now));
    if (!drain_events()) return false;

    if (!p->real_mouse) {
        platform_gamepad_state_t unused;
        memset(&unused, 0, sizeof(unused));
        bool down = false;
        int x = 0, y = 0;
        touch_poll(&unused, &down, &x, &y);
        if (down) {
            screen_rect_t pic = screen_picture_rect(s_buf_w, s_buf_h, p->scale_mode);
            screen_to_render(&pic, p->render_w, p->render_h, x, y, &p->mouse_x, &p->mouse_y);
        }
        p->mouse_buttons = down ? PMOUSE_LEFT : 0;
    }
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
    *state = s_pad;
    state->connected = s_pad_seen;
    state->btn_lt = s_key_lt || s_axis_lt;
    state->btn_rt = s_key_rt || s_axis_rt;
    bool down;
    int x, y;
    touch_poll(state, &down, &x, &y);
}

uint32_t platform_ticks(platform_t *p) {
    uint64_t start = p ? p->start_ns : g_plat.start_ns;
    return (uint32_t)((now_ns() - start) / 1000000ull);
}

void platform_delay(uint32_t ms) {
    if (ms) usleep(ms * 1000u);
}

/* ── Files ───────────────────────────────────────────────── */

/* The intent extra "game", through JNI: NativeActivity has no C accessor. */
static void intent_game(char *out, size_t outlen) {
    snprintf(out, outlen, "e2");
    JavaVM *vm = s_app->activity->vm;
    JNIEnv *env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK || !env) return;

    jobject act = s_app->activity->clazz;
    jclass act_cls = (*env)->GetObjectClass(env, act);
    jmethodID get_intent = (*env)->GetMethodID(env, act_cls, "getIntent", "()Landroid/content/Intent;");
    jobject intent = (*env)->CallObjectMethod(env, act, get_intent);
    if (intent) {
        jclass intent_cls = (*env)->GetObjectClass(env, intent);
        jmethodID get_extra = (*env)->GetMethodID(env, intent_cls, "getStringExtra",
                                                  "(Ljava/lang/String;)Ljava/lang/String;");
        jstring key = (*env)->NewStringUTF(env, "game");
        jstring val = (jstring)(*env)->CallObjectMethod(env, intent, get_extra, key);
        if (val) {
            const char *s = (*env)->GetStringUTFChars(env, val, NULL);
            if (s && (strcmp(s, "e1") == 0 || strcmp(s, "e2") == 0))
                snprintf(out, outlen, "%s", s);
            if (s) (*env)->ReleaseStringUTFChars(env, val, s);
        }
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*vm)->DetachCurrentThread(vm);
}

void platform_early_init(void) {
    char game[8], dir[600];
    intent_game(game, sizeof(game));
    snprintf(dir, sizeof(dir), "%s/%s", s_files_dir, game);
    if (file_dir_has_database(dir) && chdir(dir) == 0) {
        file_flush_path_cache();
        return;
    }
    __android_log_print(ANDROID_LOG_ERROR, "ecstatica", "no game data in %s", dir);
    ANativeActivity_finish(s_app->activity);
    exit(2);
}

/* Flash writes can stall; the log is written off the game thread. */
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

/* ── Entry ───────────────────────────────────────────────── */

void android_main(struct android_app *app) {
    s_app = app;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    snprintf(s_files_dir, sizeof(s_files_dir), "%s",
             app->activity->externalDataPath ? app->activity->externalDataPath
                                             : app->activity->internalDataPath);
    touch_init();

    /* The engine draws from its first frame; wait until there is a window. */
    while (!(s_window && s_resumed)) {
        int events;
        struct android_poll_source *src = NULL;
        ALooper_pollOnce(-1, NULL, &events, (void **)&src);
        if (src) src->process(app, src);
        if (app->destroyRequested) return;
    }

    char *argv[] = { "ecstatica", NULL };
    ecs_main(1, argv);
    ANativeActivity_finish(app->activity);
}

#endif /* __ANDROID__ */
