#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <linux/joystick.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "platform.h"

#define MAX_KEYS 256

struct platform_t {
    int fb_width;
    int fb_height;
    int render_width;
    int render_height;
    int scale;
    uint32_t *rgba_buffer;
    bool quit_requested;

    bool key_state[MAX_KEYS];
    bool key_prev[MAX_KEYS];
    int  mouse_x;
    int  mouse_y;
    int  mouse_buttons;

    struct timespec start_time;

    Display *display;
    Window   window;
    GC       gc;
    XImage  *ximage;
    Atom     wm_delete;
    int      screen;

    int      js_fd;
    int16_t  js_axes[8];
    uint8_t  js_buttons[16];
};

static int x11_keysym_to_pkey(KeySym ks) {
    switch (ks) {
        case XK_Escape:    return PKEY_ESCAPE;
        case XK_Return:    return PKEY_RETURN;
        case XK_space:     return PKEY_SPACE;
        case XK_Up:        return PKEY_UP;
        case XK_Down:      return PKEY_DOWN;
        case XK_Left:      return PKEY_LEFT;
        case XK_Right:     return PKEY_RIGHT;
        case XK_F1:        return PKEY_F1;
        case XK_F2:        return PKEY_F2;
        case XK_F3:        return PKEY_F3;
        case XK_F4:        return PKEY_F4;
        case XK_F5:        return PKEY_F5;
        case XK_F6:        return PKEY_F6;
        case XK_F7:        return PKEY_F7;
        case XK_F8:        return PKEY_F8;
        case XK_F9:        return PKEY_F9;
        case XK_F10:       return PKEY_F10;
        case XK_F11:       return PKEY_F11;
        case XK_F12:       return PKEY_F12;
        case XK_a: case XK_A: return PKEY_A;
        case XK_d: case XK_D: return PKEY_D;
        case XK_s: case XK_S: return PKEY_S;
        case XK_w: case XK_W: return PKEY_W;
        case XK_q: case XK_Q: return PKEY_Q;
        case XK_e: case XK_E: return PKEY_E;
        case XK_i: case XK_I: return PKEY_I;
        case XK_p: case XK_P: return PKEY_P;
        case XK_c: case XK_C: return PKEY_C;
        case XK_m: case XK_M: return PKEY_M;
        case XK_z: case XK_Z: return PKEY_Z;
        case XK_Control_L: case XK_Control_R: return PKEY_LCTRL;
        case XK_Alt_L:     case XK_Alt_R:     return PKEY_LALT;
        case XK_Shift_L:   return PKEY_LSHIFT;
        case XK_Shift_R:   return PKEY_RSHIFT;
        case XK_KP_1: case XK_KP_End:       return PKEY_NUM1;
        case XK_KP_2: case XK_KP_Down:      return PKEY_NUM2;
        case XK_KP_3: case XK_KP_Page_Down: return PKEY_NUM3;
        case XK_KP_4: case XK_KP_Left:      return PKEY_NUM4;
        case XK_KP_5: case XK_KP_Begin:     return PKEY_NUM5;
        case XK_KP_6: case XK_KP_Right:     return PKEY_NUM6;
        case XK_KP_7: case XK_KP_Home:      return PKEY_NUM7;
        case XK_KP_8: case XK_KP_Up:        return PKEY_NUM8;
        case XK_KP_9: case XK_KP_Page_Up:   return PKEY_NUM9;
        default: return -1;
    }
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    platform_t *p = (platform_t *)calloc(1, sizeof(platform_t));
    if (!p) return NULL;

    p->fb_width      = fb_width;
    p->fb_height     = fb_height;
    p->render_width  = fb_width;
    p->render_height = fb_height;
    p->scale         = scale;

    clock_gettime(CLOCK_MONOTONIC, &p->start_time);

    p->rgba_buffer = (uint32_t *)calloc(fb_width * fb_height, sizeof(uint32_t));
    if (!p->rgba_buffer) { free(p); return NULL; }

    p->display = XOpenDisplay(NULL);
    if (!p->display) { free(p->rgba_buffer); free(p); return NULL; }

    p->screen = DefaultScreen(p->display);
    int win_w = fb_width * scale;
    int win_h = fb_height * scale;

    p->window = XCreateSimpleWindow(
        p->display,
        RootWindow(p->display, p->screen),
        100, 100, win_w, win_h,
        0,
        BlackPixel(p->display, p->screen),
        BlackPixel(p->display, p->screen)
    );

    XStoreName(p->display, p->window, title);

    XSelectInput(p->display, p->window,
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
        StructureNotifyMask);

    p->wm_delete = XInternAtom(p->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(p->display, p->window, &p->wm_delete, 1);

    /* Prevent window resize */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize | PMaxSize;
        hints->min_width  = win_w;
        hints->min_height = win_h;
        hints->max_width  = win_w;
        hints->max_height = win_h;
        XSetWMNormalHints(p->display, p->window, hints);
        XFree(hints);
    }

    p->gc = XCreateGC(p->display, p->window, 0, NULL);

    Visual *visual = DefaultVisual(p->display, p->screen);
    int depth = DefaultDepth(p->display, p->screen);

    p->ximage = XCreateImage(
        p->display, visual, depth, ZPixmap, 0,
        (char *)p->rgba_buffer,
        fb_width, fb_height,
        32, fb_width * 4
    );
    if (!p->ximage) {
        XFreeGC(p->display, p->gc);
        XDestroyWindow(p->display, p->window);
        XCloseDisplay(p->display);
        free(p->rgba_buffer);
        free(p);
        return NULL;
    }

    /* XDestroyImage would free the data pointer; we manage it ourselves */

    XMapWindow(p->display, p->window);
    XFlush(p->display);

    /* Disable X11 key auto-repeat detection */
    XkbSetDetectableAutoRepeat(p->display, True, NULL);

    /* Try to open first joystick device */
    p->js_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);

    return p;
}

void platform_set_render_size(platform_t *p, int w, int h) {
    if (!p) return;
    p->render_width  = w;
    p->render_height = h;
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || !palette) return;

    int rw = p->render_width;
    int rh = p->render_height;
    int fw = p->fb_width;
    int fh = p->fb_height;

    if (rw == fw && rh == fh) {
        int total = fw * fh;
        for (int i = 0; i < total; i++) {
            uint8_t idx = framebuffer[i];
            uint8_t r = (palette[idx * 3 + 0] & 0x3F) << 2;
            uint8_t g = (palette[idx * 3 + 1] & 0x3F) << 2;
            uint8_t b = (palette[idx * 3 + 2] & 0x3F) << 2;
            p->rgba_buffer[i] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    } else {
        for (int y = 0; y < fh; y++) {
            int sy = y * rh / fh;
            for (int x = 0; x < fw; x++) {
                int sx = x * rw / fw;
                uint8_t idx = framebuffer[sy * rw + sx];
                uint8_t r = (palette[idx * 3 + 0] & 0x3F) << 2;
                uint8_t g = (palette[idx * 3 + 1] & 0x3F) << 2;
                uint8_t b = (palette[idx * 3 + 2] & 0x3F) << 2;
                p->rgba_buffer[y * fw + x] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }

    if (p->scale == 1) {
        XPutImage(p->display, p->window, p->gc, p->ximage,
                  0, 0, 0, 0, fw, fh);
    } else {
        /* Scale by drawing to a pixmap, then copying scaled.
           XPutImage does not scale, so we do nearest-neighbor in software. */
        int win_w = fw * p->scale;
        int win_h = fh * p->scale;
        uint32_t *scaled = (uint32_t *)malloc(win_w * win_h * 4);
        if (scaled) {
            for (int y = 0; y < win_h; y++) {
                int sy = y / p->scale;
                for (int x = 0; x < win_w; x++) {
                    int sx = x / p->scale;
                    scaled[y * win_w + x] = p->rgba_buffer[sy * fw + sx];
                }
            }
            Visual *visual = DefaultVisual(p->display, p->screen);
            int depth = DefaultDepth(p->display, p->screen);
            XImage *scaled_img = XCreateImage(
                p->display, visual, depth, ZPixmap, 0,
                (char *)scaled, win_w, win_h, 32, win_w * 4
            );
            if (scaled_img) {
                XPutImage(p->display, p->window, p->gc, scaled_img,
                          0, 0, 0, 0, win_w, win_h);
                scaled_img->data = NULL;
                XDestroyImage(scaled_img);
            }
            free(scaled);
        }
    }

    XFlush(p->display);
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    if (!p || !framebuffer) return;

    memcpy(p->rgba_buffer, framebuffer, p->fb_width * p->fb_height * 4);

    if (p->scale == 1) {
        XPutImage(p->display, p->window, p->gc, p->ximage,
                  0, 0, 0, 0, p->fb_width, p->fb_height);
    } else {
        int fw = p->fb_width;
        int fh = p->fb_height;
        int win_w = fw * p->scale;
        int win_h = fh * p->scale;
        uint32_t *scaled = (uint32_t *)malloc(win_w * win_h * 4);
        if (scaled) {
            for (int y = 0; y < win_h; y++) {
                int sy = y / p->scale;
                for (int x = 0; x < win_w; x++) {
                    int sx = x / p->scale;
                    scaled[y * win_w + x] = p->rgba_buffer[sy * fw + sx];
                }
            }
            Visual *visual = DefaultVisual(p->display, p->screen);
            int depth = DefaultDepth(p->display, p->screen);
            XImage *scaled_img = XCreateImage(
                p->display, visual, depth, ZPixmap, 0,
                (char *)scaled, win_w, win_h, 32, win_w * 4
            );
            if (scaled_img) {
                XPutImage(p->display, p->window, p->gc, scaled_img,
                          0, 0, 0, 0, win_w, win_h);
                scaled_img->data = NULL;
                XDestroyImage(scaled_img);
            }
            free(scaled);
        }
    }

    XFlush(p->display);
}

bool platform_pump_events(platform_t *p) {
    if (!p) return false;

    memcpy(p->key_prev, p->key_state, sizeof(p->key_prev));

    while (XPending(p->display)) {
        XEvent ev;
        XNextEvent(p->display, &ev);

        switch (ev.type) {
            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                int k = x11_keysym_to_pkey(ks);
                if (k >= 0 && k < MAX_KEYS) p->key_state[k] = true;
                break;
            }
            case KeyRelease: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                int k = x11_keysym_to_pkey(ks);
                if (k >= 0 && k < MAX_KEYS) p->key_state[k] = false;
                break;
            }
            case ButtonPress: {
                int rw = p->render_width;
                int rh = p->render_height;
                p->mouse_x = ev.xbutton.x * rw / (p->fb_width * p->scale);
                p->mouse_y = ev.xbutton.y * rh / (p->fb_height * p->scale);
                if (p->mouse_x < 0) p->mouse_x = 0;
                if (p->mouse_y < 0) p->mouse_y = 0;
                if (p->mouse_x >= rw) p->mouse_x = rw - 1;
                if (p->mouse_y >= rh) p->mouse_y = rh - 1;
                if (ev.xbutton.button == Button1) p->mouse_buttons |= PMOUSE_LEFT;
                if (ev.xbutton.button == Button3) p->mouse_buttons |= PMOUSE_RIGHT;
                if (ev.xbutton.button == Button2) p->mouse_buttons |= PMOUSE_MIDDLE;
                break;
            }
            case ButtonRelease: {
                int rw = p->render_width;
                int rh = p->render_height;
                p->mouse_x = ev.xbutton.x * rw / (p->fb_width * p->scale);
                p->mouse_y = ev.xbutton.y * rh / (p->fb_height * p->scale);
                if (p->mouse_x < 0) p->mouse_x = 0;
                if (p->mouse_y < 0) p->mouse_y = 0;
                if (p->mouse_x >= rw) p->mouse_x = rw - 1;
                if (p->mouse_y >= rh) p->mouse_y = rh - 1;
                if (ev.xbutton.button == Button1) p->mouse_buttons &= ~PMOUSE_LEFT;
                if (ev.xbutton.button == Button3) p->mouse_buttons &= ~PMOUSE_RIGHT;
                if (ev.xbutton.button == Button2) p->mouse_buttons &= ~PMOUSE_MIDDLE;
                break;
            }
            case MotionNotify: {
                int rw = p->render_width;
                int rh = p->render_height;
                p->mouse_x = ev.xmotion.x * rw / (p->fb_width * p->scale);
                p->mouse_y = ev.xmotion.y * rh / (p->fb_height * p->scale);
                if (p->mouse_x < 0) p->mouse_x = 0;
                if (p->mouse_y < 0) p->mouse_y = 0;
                if (p->mouse_x >= rw) p->mouse_x = rw - 1;
                if (p->mouse_y >= rh) p->mouse_y = rh - 1;
                break;
            }
            case ClientMessage: {
                if ((Atom)ev.xclient.data.l[0] == p->wm_delete)
                    p->quit_requested = true;
                break;
            }
            case Expose: {
                if (p->ximage) {
                    if (p->scale == 1) {
                        XPutImage(p->display, p->window, p->gc, p->ximage,
                                  0, 0, 0, 0, p->fb_width, p->fb_height);
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    return !p->quit_requested;
}

bool platform_key_down(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= MAX_KEYS) return false;
    return p->key_state[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= MAX_KEYS) return false;
    return p->key_state[keycode] && !p->key_prev[keycode];
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y) {
    if (!p) return 0;
    if (out_x) *out_x = p->mouse_x;
    if (out_y) *out_y = p->mouse_y;
    return p->mouse_buttons;
}

uint32_t platform_ticks(platform_t *p) {
    static struct timespec s_start = {0, 0};
    struct timespec *ref;

    if (p) {
        ref = &p->start_time;
    } else {
        if (s_start.tv_sec == 0 && s_start.tv_nsec == 0)
            clock_gettime(CLOCK_MONOTONIC, &s_start);
        ref = &s_start;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    /* Signed arithmetic throughout: tv_nsec difference is negative whenever
     * now.tv_nsec < ref->tv_nsec, and an unsigned cast would wrap it. */
    int64_t elapsed_ms = (int64_t)(now.tv_sec - ref->tv_sec) * 1000
                       + ((int64_t)now.tv_nsec - (int64_t)ref->tv_nsec) / 1000000;
    if (elapsed_ms < 0) elapsed_ms = 0;
    return (uint32_t)elapsed_ms;
}

void platform_delay(uint32_t ms) {
    struct timespec ts, rem;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    /* Resume the remaining interval if a signal cuts the sleep short. */
    while (nanosleep(&ts, &rem) == -1 && errno == EINTR)
        ts = rem;
}

void platform_shutdown(platform_t *p) {
    if (!p) return;

    if (p->ximage) {
        /* Prevent XDestroyImage from freeing our buffer */
        p->ximage->data = NULL;
        XDestroyImage(p->ximage);
        p->ximage = NULL;
    }
    if (p->gc) {
        XFreeGC(p->display, p->gc);
    }
    if (p->window) {
        XDestroyWindow(p->display, p->window);
    }
    if (p->display) {
        XCloseDisplay(p->display);
    }
    if (p->js_fd >= 0) close(p->js_fd);
    if (p->rgba_buffer) {
        free(p->rgba_buffer);
        p->rgba_buffer = NULL;
    }
    free(p);
}

void platform_set_title(platform_t *p, const char *title) {
    if (!p || !title) return;
    XStoreName(p->display, p->window, title);
    XFlush(p->display);
}

/* Gamepad — Linux joystick API (/dev/input/js0) */

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state) {
    memset(state, 0, sizeof(*state));
    if (!p) return;

    /* Try to reopen if not connected */
    if (p->js_fd < 0) {
        p->js_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
        if (p->js_fd < 0) return;
        memset(p->js_axes, 0, sizeof(p->js_axes));
        memset(p->js_buttons, 0, sizeof(p->js_buttons));
    }

    /* Drain pending events */
    struct js_event ev;
    while (read(p->js_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        ev.type &= ~JS_EVENT_INIT;
        if (ev.type == JS_EVENT_BUTTON && ev.number < 16)
            p->js_buttons[ev.number] = ev.value;
        else if (ev.type == JS_EVENT_AXIS && ev.number < 8)
            p->js_axes[ev.number] = ev.value;
    }
    if (errno == ENODEV) {
        close(p->js_fd);
        p->js_fd = -1;
        return;
    }

    state->connected = true;

    /* Standard gamepad axis mapping (SDL convention):
     * 0=LX, 1=LY, 2=LT, 3=RX, 4=RY, 5=RT, 6=DX, 7=DY */
    state->left_x  = p->js_axes[0];
    state->left_y  = (int16_t)(-p->js_axes[1]);
    state->right_x = p->js_axes[3];
    state->right_y = (int16_t)(-p->js_axes[4]);

    state->dpad_left  = p->js_axes[6] < -16000;
    state->dpad_right = p->js_axes[6] > 16000;
    state->dpad_up    = p->js_axes[7] < -16000;
    state->dpad_down  = p->js_axes[7] > 16000;

    state->btn_lt = p->js_axes[2] > 0;
    state->btn_rt = p->js_axes[5] > 0;

    /* Standard button mapping (Xbox layout):
     * 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 6=Select, 7=Start,
     * 8=Logo, 9=LStick, 10=RStick */
    state->btn_south  = p->js_buttons[0];
    state->btn_east   = p->js_buttons[1];
    state->btn_west   = p->js_buttons[2];
    state->btn_north  = p->js_buttons[3];
    state->btn_lb     = p->js_buttons[4];
    state->btn_rb     = p->js_buttons[5];
    state->btn_select = p->js_buttons[6];
    state->btn_start  = p->js_buttons[7];
    state->btn_lstick = p->js_buttons[9];
    state->btn_rstick = p->js_buttons[10];
}

/* Audio backend -- ALSA + software mixer in a pthread */

#define AUDIO_MAX_VOICES 16
#define AUDIO_OUT_RATE   44100
#define AUDIO_OUT_CH     2
#define AUDIO_PERIOD     1024

typedef struct {
    const uint8_t *data;
    int   length;
    int   rate;
    int   volume;
    int   pan;
    double pos;
    bool  active;
    bool  loop;
} audio_voice_t;

static audio_voice_t s_voices[AUDIO_MAX_VOICES];
static pthread_mutex_t s_audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static snd_pcm_t *s_pcm = NULL;
static pthread_t s_audio_thread;
static volatile bool s_audio_running = false;
static float s_master_sfx_volume = 0.9f;
static float s_master_music_volume = 0.8f;

static void *audio_thread_func(void *arg) {
    (void)arg;

    int16_t buf[AUDIO_PERIOD * AUDIO_OUT_CH];

    while (s_audio_running) {
        memset(buf, 0, sizeof(buf));

        float mix[AUDIO_PERIOD * AUDIO_OUT_CH];
        memset(mix, 0, sizeof(mix));

        pthread_mutex_lock(&s_audio_mutex);
        for (int v = 0; v < AUDIO_MAX_VOICES; ++v) {
            audio_voice_t *voice = &s_voices[v];
            if (!voice->active) continue;

            double step = (double)voice->rate / (double)AUDIO_OUT_RATE;
            float db    = (float)(voice->volume - 127) * 0.25f;
            float vol   = powf(10.0f, db / 20.0f) * s_master_sfx_volume;
            float pan_norm = (float)voice->pan / 127.0f;
            float l_gain = vol * (pan_norm > 0 ? (1.0f - pan_norm) : 1.0f);
            float r_gain = vol * (pan_norm < 0 ? (1.0f + pan_norm) : 1.0f);

            for (int f = 0; f < AUDIO_PERIOD; ++f) {
                int idx = (int)voice->pos;
                if (idx >= voice->length) {
                    if (voice->loop) {
                        voice->pos = 0.0;
                        idx = 0;
                    } else {
                        voice->active = false;
                        break;
                    }
                }
                float s = ((float)voice->data[idx] - 128.0f) / 128.0f;
                mix[f * AUDIO_OUT_CH + 0] += s * l_gain;
                mix[f * AUDIO_OUT_CH + 1] += s * r_gain;
                voice->pos += step;
            }
        }
        pthread_mutex_unlock(&s_audio_mutex);

        for (int i = 0; i < AUDIO_PERIOD * AUDIO_OUT_CH; ++i) {
            float s = mix[i];
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;
            buf[i] = (int16_t)(s * 32767.0f);
        }

        snd_pcm_sframes_t written = snd_pcm_writei(s_pcm, buf, AUDIO_PERIOD);
        if (written < 0) {
            snd_pcm_recover(s_pcm, (int)written, 1);
        }
    }

    return NULL;
}

void platform_audio_init(void) {
    if (s_pcm) return;

    int err = snd_pcm_open(&s_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) return;

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(s_pcm, params);
    snd_pcm_hw_params_set_access(s_pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(s_pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(s_pcm, params, AUDIO_OUT_CH);

    unsigned int rate = AUDIO_OUT_RATE;
    snd_pcm_hw_params_set_rate_near(s_pcm, params, &rate, NULL);

    snd_pcm_uframes_t period = AUDIO_PERIOD;
    snd_pcm_hw_params_set_period_size_near(s_pcm, params, &period, NULL);

    snd_pcm_uframes_t buffer_size = AUDIO_PERIOD * 4;
    snd_pcm_hw_params_set_buffer_size_near(s_pcm, params, &buffer_size);

    err = snd_pcm_hw_params(s_pcm, params);
    if (err < 0) {
        snd_pcm_close(s_pcm);
        s_pcm = NULL;
        return;
    }

    snd_pcm_prepare(s_pcm);

    s_audio_running = true;
    pthread_create(&s_audio_thread, NULL, audio_thread_func, NULL);
}

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop) {
    if (!s_pcm || !data || length <= 0) return -1;
    if (rate <= 0) rate = 22050;

    pthread_mutex_lock(&s_audio_mutex);
    int slot = -1;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!s_voices[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0;
    s_voices[slot].data   = (const uint8_t *)data;
    s_voices[slot].length = length;
    s_voices[slot].rate   = rate;
    s_voices[slot].volume = volume;
    s_voices[slot].pan    = pan;
    s_voices[slot].pos    = 0.0;
    s_voices[slot].loop   = loop;
    s_voices[slot].active = true;
    pthread_mutex_unlock(&s_audio_mutex);
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= AUDIO_MAX_VOICES) return;
    pthread_mutex_lock(&s_audio_mutex);
    s_voices[slot].active = false;
    pthread_mutex_unlock(&s_audio_mutex);
}

void platform_audio_stop_all(void) {
    pthread_mutex_lock(&s_audio_mutex);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) s_voices[i].active = false;
    pthread_mutex_unlock(&s_audio_mutex);
}

void platform_audio_shutdown(void) {
    if (!s_pcm) return;

    s_audio_running = false;
    pthread_join(s_audio_thread, NULL);

    snd_pcm_drop(s_pcm);
    snd_pcm_close(s_pcm);
    s_pcm = NULL;
}

/* MIDI: requires FluidSynth or external synth */
int platform_midi_play(const void *smf_data, int length, bool loop) {
    (void)smf_data; (void)length; (void)loop;
    return -1;
}

void platform_midi_stop(void) {
}

void platform_set_sfx_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_master_sfx_volume = vol;
}

void platform_set_music_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_master_music_volume = vol;
}

#endif /* __linux__ */
