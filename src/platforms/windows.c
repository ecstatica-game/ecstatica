#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "platform.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "xinput.lib")

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

    LARGE_INTEGER start_ticks;
    LARGE_INTEGER freq;

    HWND hwnd;
    BITMAPINFO bmi;
};

static platform_t *s_platform = NULL;

static int win32_scancode(LPARAM lParam, WPARAM wParam) {
    int scancode = (lParam >> 16) & 0xFF;
    bool extended = (lParam >> 24) & 1;

    if (extended) {
        switch (scancode) {
            case 0x48: return 0xC8;  /* up */
            case 0x50: return 0xD0;  /* down */
            case 0x4B: return 0xCB;  /* left */
            case 0x4D: return 0xCD;  /* right */
            default:   return scancode | 0x80;
        }
    }
    return scancode;
}

static void update_mouse_pos(platform_t *p, LPARAM lParam) {
    int raw_x = (short)LOWORD(lParam);
    int raw_y = (short)HIWORD(lParam);
    int win_w = p->fb_width * p->scale;
    int win_h = p->fb_height * p->scale;
    int rw = p->render_width;
    int rh = p->render_height;

    p->mouse_x = raw_x * rw / win_w;
    p->mouse_y = raw_y * rh / win_h;

    if (p->mouse_x < 0) p->mouse_x = 0;
    if (p->mouse_y < 0) p->mouse_y = 0;
    if (p->mouse_x >= rw) p->mouse_x = rw - 1;
    if (p->mouse_y >= rh) p->mouse_y = rh - 1;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    platform_t *p = s_platform;
    if (!p) return DefWindowProcA(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CLOSE:
            p->quit_requested = true;
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int sc = win32_scancode(lParam, wParam);
            if (sc >= 0 && sc < MAX_KEYS) p->key_state[sc] = true;
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int sc = win32_scancode(lParam, wParam);
            if (sc >= 0 && sc < MAX_KEYS) p->key_state[sc] = false;
            return 0;
        }

        case WM_MOUSEMOVE:
            update_mouse_pos(p, lParam);
            return 0;

        case WM_LBUTTONDOWN:
            update_mouse_pos(p, lParam);
            p->mouse_buttons |= PMOUSE_LEFT;
            SetCapture(hwnd);
            return 0;

        case WM_LBUTTONUP:
            update_mouse_pos(p, lParam);
            p->mouse_buttons &= ~PMOUSE_LEFT;
            if (!(p->mouse_buttons & (PMOUSE_RIGHT | PMOUSE_MIDDLE)))
                ReleaseCapture();
            return 0;

        case WM_RBUTTONDOWN:
            update_mouse_pos(p, lParam);
            p->mouse_buttons |= PMOUSE_RIGHT;
            SetCapture(hwnd);
            return 0;

        case WM_RBUTTONUP:
            update_mouse_pos(p, lParam);
            p->mouse_buttons &= ~PMOUSE_RIGHT;
            if (!(p->mouse_buttons & (PMOUSE_LEFT | PMOUSE_MIDDLE)))
                ReleaseCapture();
            return 0;

        case WM_DESTROY:
            p->quit_requested = true;
            return 0;

        default:
            break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    platform_t *p = (platform_t *)calloc(1, sizeof(platform_t));
    if (!p) return NULL;

    p->fb_width      = fb_width;
    p->fb_height     = fb_height;
    p->render_width  = fb_width;
    p->render_height = fb_height;
    p->scale         = scale;
    p->rgba_buffer   = (uint32_t *)calloc(fb_width * fb_height, sizeof(uint32_t));

    QueryPerformanceFrequency(&p->freq);
    QueryPerformanceCounter(&p->start_ticks);

    s_platform = p;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance      = GetModuleHandleA(NULL);
    wc.lpszClassName  = "EcstaticaWnd";
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int win_w = fb_width * scale;
    int win_h = fb_height * scale;

    RECT rc = {0, 0, win_w, win_h};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);

    p->hwnd = CreateWindowExA(
        0, "EcstaticaWnd", title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, GetModuleHandleA(NULL), NULL
    );

    memset(&p->bmi, 0, sizeof(p->bmi));
    p->bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    p->bmi.bmiHeader.biWidth       = fb_width;
    p->bmi.bmiHeader.biHeight      = -fb_height;  /* top-down */
    p->bmi.bmiHeader.biPlanes      = 1;
    p->bmi.bmiHeader.biBitCount    = 32;
    p->bmi.bmiHeader.biCompression = BI_RGB;

    ShowWindow(p->hwnd, SW_SHOW);
    UpdateWindow(p->hwnd);

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

    HDC hdc = GetDC(p->hwnd);
    StretchDIBits(
        hdc,
        0, 0, p->fb_width * p->scale, p->fb_height * p->scale,
        0, 0, p->fb_width, p->fb_height,
        p->rgba_buffer, &p->bmi,
        DIB_RGB_COLORS, SRCCOPY
    );
    ReleaseDC(p->hwnd, hdc);
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    if (!p || !framebuffer) return;

    memcpy(p->rgba_buffer, framebuffer, p->fb_width * p->fb_height * 4);

    HDC hdc = GetDC(p->hwnd);
    StretchDIBits(
        hdc,
        0, 0, p->fb_width * p->scale, p->fb_height * p->scale,
        0, 0, p->fb_width, p->fb_height,
        p->rgba_buffer, &p->bmi,
        DIB_RGB_COLORS, SRCCOPY
    );
    ReleaseDC(p->hwnd, hdc);
}

bool platform_pump_events(platform_t *p) {
    if (!p) return false;

    memcpy(p->key_prev, p->key_state, sizeof(p->key_prev));

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
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
    static LARGE_INTEGER s_start = {0};
    static LARGE_INTEGER s_freq = {0};
    LARGE_INTEGER now;
    LARGE_INTEGER *ref_start;
    LARGE_INTEGER *freq;

    if (p) {
        ref_start = &p->start_ticks;
        freq = &p->freq;
    } else {
        if (!s_freq.QuadPart) {
            QueryPerformanceFrequency(&s_freq);
            QueryPerformanceCounter(&s_start);
        }
        ref_start = &s_start;
        freq = &s_freq;
    }

    QueryPerformanceCounter(&now);
    return (uint32_t)((now.QuadPart - ref_start->QuadPart) * 1000 / freq->QuadPart);
}

void platform_delay(uint32_t ms) {
    Sleep(ms);
}

void platform_shutdown(platform_t *p) {
    if (!p) return;
    if (p->hwnd) {
        DestroyWindow(p->hwnd);
        p->hwnd = NULL;
    }
    if (p->rgba_buffer) {
        free(p->rgba_buffer);
        p->rgba_buffer = NULL;
    }
    if (s_platform == p) s_platform = NULL;
    free(p);
}

void platform_set_title(platform_t *p, const char *title) {
    if (!p || !title) return;
    SetWindowTextA(p->hwnd, title);
}

/* Gamepad — XInput */

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state) {
    memset(state, 0, sizeof(*state));
    (void)p;

    XINPUT_STATE xs;
    if (XInputGetState(0, &xs) != ERROR_SUCCESS) return;

    state->connected = true;
    WORD b = xs.Gamepad.wButtons;

    state->dpad_up    = (b & XINPUT_GAMEPAD_DPAD_UP) != 0;
    state->dpad_down  = (b & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    state->dpad_left  = (b & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    state->dpad_right = (b & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;

    state->left_x  = xs.Gamepad.sThumbLX;
    state->left_y  = xs.Gamepad.sThumbLY;
    state->right_x = xs.Gamepad.sThumbRX;
    state->right_y = xs.Gamepad.sThumbRY;

    state->btn_south  = (b & XINPUT_GAMEPAD_A) != 0;
    state->btn_east   = (b & XINPUT_GAMEPAD_B) != 0;
    state->btn_west   = (b & XINPUT_GAMEPAD_X) != 0;
    state->btn_north  = (b & XINPUT_GAMEPAD_Y) != 0;
    state->btn_lb     = (b & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    state->btn_rb     = (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    state->btn_lt     = xs.Gamepad.bLeftTrigger > 76;
    state->btn_rt     = xs.Gamepad.bRightTrigger > 76;
    state->btn_start  = (b & XINPUT_GAMEPAD_START) != 0;
    state->btn_select = (b & XINPUT_GAMEPAD_BACK) != 0;
    state->btn_lstick = (b & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
    state->btn_rstick = (b & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
}

/* Audio backend -- waveOut + software mixer */

#define AUDIO_MAX_VOICES 16
#define AUDIO_OUT_RATE   44100
#define AUDIO_OUT_CH     2
#define AUDIO_BUF_FRAMES 2048
#define AUDIO_NUM_BUFS   4

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
static CRITICAL_SECTION s_audio_cs;
static HWAVEOUT s_wave_out = NULL;
static WAVEHDR s_wave_hdrs[AUDIO_NUM_BUFS];
static int16_t *s_wave_bufs[AUDIO_NUM_BUFS];
static bool s_audio_ready = false;
static float s_master_sfx_volume = 0.9f;
static float s_master_music_volume = 0.8f;

static void audio_fill_buffer(int16_t *buf, int frames) {
    float *mix = (float *)calloc(frames * AUDIO_OUT_CH, sizeof(float));

    EnterCriticalSection(&s_audio_cs);
    for (int v = 0; v < AUDIO_MAX_VOICES; ++v) {
        audio_voice_t *voice = &s_voices[v];
        if (!voice->active) continue;

        double step = (double)voice->rate / (double)AUDIO_OUT_RATE;
        float db    = (float)(voice->volume - 127) * 0.25f;
        float vol   = powf(10.0f, db / 20.0f) * s_master_sfx_volume;
        float pan_norm = (float)voice->pan / 127.0f;
        float l_gain = vol * (pan_norm > 0 ? (1.0f - pan_norm) : 1.0f);
        float r_gain = vol * (pan_norm < 0 ? (1.0f + pan_norm) : 1.0f);

        for (int f = 0; f < frames; ++f) {
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
    LeaveCriticalSection(&s_audio_cs);

    for (int i = 0; i < frames * AUDIO_OUT_CH; ++i) {
        float v = mix[i];
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        buf[i] = (int16_t)(v * 32767.0f);
    }

    free(mix);
}

static void CALLBACK wave_out_proc(HWAVEOUT hwo, UINT uMsg,
                                   DWORD_PTR dwInstance,
                                   DWORD_PTR dwParam1,
                                   DWORD_PTR dwParam2) {
    (void)hwo; (void)dwInstance; (void)dwParam2;
    if (uMsg != WOM_DONE) return;

    WAVEHDR *hdr = (WAVEHDR *)dwParam1;
    audio_fill_buffer((int16_t *)hdr->lpData, AUDIO_BUF_FRAMES);
    waveOutWrite(s_wave_out, hdr, sizeof(WAVEHDR));
}

void platform_audio_init(void) {
    if (s_audio_ready) return;

    InitializeCriticalSection(&s_audio_cs);

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels        = AUDIO_OUT_CH;
    wfx.nSamplesPerSec   = AUDIO_OUT_RATE;
    wfx.wBitsPerSample   = 16;
    wfx.nBlockAlign      = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec  = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&s_wave_out, WAVE_MAPPER, &wfx,
                    (DWORD_PTR)wave_out_proc, 0,
                    CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        return;
    }

    int buf_bytes = AUDIO_BUF_FRAMES * AUDIO_OUT_CH * sizeof(int16_t);
    for (int i = 0; i < AUDIO_NUM_BUFS; ++i) {
        s_wave_bufs[i] = (int16_t *)calloc(1, buf_bytes);
        memset(&s_wave_hdrs[i], 0, sizeof(WAVEHDR));
        s_wave_hdrs[i].lpData         = (LPSTR)s_wave_bufs[i];
        s_wave_hdrs[i].dwBufferLength = buf_bytes;
        waveOutPrepareHeader(s_wave_out, &s_wave_hdrs[i], sizeof(WAVEHDR));
        audio_fill_buffer(s_wave_bufs[i], AUDIO_BUF_FRAMES);
        waveOutWrite(s_wave_out, &s_wave_hdrs[i], sizeof(WAVEHDR));
    }

    s_audio_ready = true;
}

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop) {
    if (!s_audio_ready || !data || length <= 0) return -1;
    if (rate <= 0) rate = 22050;

    EnterCriticalSection(&s_audio_cs);
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
    LeaveCriticalSection(&s_audio_cs);
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= AUDIO_MAX_VOICES) return;
    EnterCriticalSection(&s_audio_cs);
    s_voices[slot].active = false;
    LeaveCriticalSection(&s_audio_cs);
}

void platform_audio_stop_all(void) {
    EnterCriticalSection(&s_audio_cs);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) s_voices[i].active = false;
    LeaveCriticalSection(&s_audio_cs);
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready) return;

    waveOutReset(s_wave_out);
    for (int i = 0; i < AUDIO_NUM_BUFS; ++i) {
        waveOutUnprepareHeader(s_wave_out, &s_wave_hdrs[i], sizeof(WAVEHDR));
        free(s_wave_bufs[i]);
        s_wave_bufs[i] = NULL;
    }
    waveOutClose(s_wave_out);
    s_wave_out = NULL;
    DeleteCriticalSection(&s_audio_cs);
    s_audio_ready = false;
}

/* ================================================================
 *  MIDI playback -- mciSendString for SMF files
 * ================================================================ */

static bool s_midi_active = false;
static char s_midi_temp_path[MAX_PATH];
static bool s_midi_loop = false;

int platform_midi_play(const void *smf_data, int length, bool loop) {
    if (!smf_data || length <= 0) return -1;

    platform_midi_stop();

    /* Write SMF data to a temp file -- mciSendString needs a file path */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    GetTempFileNameA(temp_dir, "ecs", 0, s_midi_temp_path);

    /* Replace extension with .mid so MCI recognizes the format */
    size_t len = strlen(s_midi_temp_path);
    if (len > 4) {
        s_midi_temp_path[len - 4] = '\0';
        strcat(s_midi_temp_path, ".mid");
    }

    HANDLE hf = CreateFileA(s_midi_temp_path, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (hf == INVALID_HANDLE_VALUE) return -1;

    DWORD written;
    WriteFile(hf, smf_data, length, &written, NULL);
    CloseHandle(hf);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open \"%s\" type sequencer alias ecsmidi", s_midi_temp_path);
    if (mciSendStringA(cmd, NULL, 0, NULL) != 0) {
        DeleteFileA(s_midi_temp_path);
        return -1;
    }

    mciSendStringA("play ecsmidi from 0", NULL, 0, NULL);
    s_midi_active = true;
    s_midi_loop = loop;
    return 0;
}

void platform_midi_stop(void) {
    if (!s_midi_active) return;
    mciSendStringA("stop ecsmidi", NULL, 0, NULL);
    mciSendStringA("close ecsmidi", NULL, 0, NULL);
    if (s_midi_temp_path[0]) {
        DeleteFileA(s_midi_temp_path);
        s_midi_temp_path[0] = '\0';
    }
    s_midi_active = false;
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

#endif /* _WIN32 */
