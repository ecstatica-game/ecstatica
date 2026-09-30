/**
 * platform.h
 *
 * Platform abstraction: window, framebuffer blit, input, timing, audio,
 * saves and the optional hardware-rendering context. Every backend in
 * platforms/ implements all of it.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

enum {
    PKEY_ESCAPE = 0x01,
    PKEY_RETURN = 0x1C,
    PKEY_SPACE  = 0x39,
    PKEY_UP     = 0xC8,
    PKEY_DOWN   = 0xD0,
    PKEY_LEFT   = 0xCB,
    PKEY_RIGHT  = 0xCD,
    PKEY_F1     = 0x3B,
    PKEY_F2     = 0x3C,
    PKEY_F3     = 0x3D,
    PKEY_F4     = 0x3E,
    PKEY_F5     = 0x3F,
    PKEY_F6     = 0x40,
    PKEY_F7     = 0x41,
    PKEY_F8     = 0x42,
    PKEY_F9     = 0x43,
    PKEY_F10    = 0x44,
    PKEY_F11    = 0x57,
    PKEY_F12    = 0x58,
    PKEY_A      = 0x1E,
    PKEY_D      = 0x20,
    PKEY_S      = 0x1F,
    PKEY_W      = 0x11,
    PKEY_Q      = 0x10,
    PKEY_E      = 0x12,
    PKEY_G      = 0x22,
    PKEY_I      = 0x17,
    PKEY_P      = 0x19,
    PKEY_C      = 0x2E,
    PKEY_M      = 0x32,
    PKEY_Z      = 0x2C,
    PKEY_LCTRL  = 0x1D,
    PKEY_LALT   = 0x38,
    PKEY_LSHIFT = 0x2A,
    PKEY_RSHIFT = 0x36,
    PKEY_NUM1   = 0x4F,
    PKEY_NUM2   = 0x50,
    PKEY_NUM3   = 0x51,
    PKEY_NUM4   = 0x4B,
    PKEY_NUM5   = 0x4C,
    PKEY_NUM6   = 0x4D,
    PKEY_NUM7   = 0x47,
    PKEY_NUM8   = 0x48,
    PKEY_NUM9   = 0x49,
    PKEY_1      = 0x02,
    PKEY_2      = 0x03,
    PKEY_3      = 0x04,
    PKEY_LCMD   = 0x5B,  /* macOS Command key */
    /* Viewer keys. Values are set-1 scancodes, as for the keys above. */
    PKEY_B        = 0x30,
    PKEY_F        = 0x21,
    PKEY_H        = 0x23,
    PKEY_L        = 0x26,
    PKEY_N        = 0x31,
    PKEY_O        = 0x18,
    PKEY_R        = 0x13,
    PKEY_T        = 0x14,
    PKEY_V        = 0x2F,
    PKEY_X        = 0x2D,
    PKEY_TAB      = 0x0F,
    PKEY_MINUS    = 0x0C,
    PKEY_EQUALS   = 0x0D,
    PKEY_LBRACKET = 0x1A,
    PKEY_RBRACKET = 0x1B,
    PKEY_COMMA    = 0x33,
    PKEY_PERIOD   = 0x34,
    PKEY_HOME     = 0xC7,
    PKEY_PGUP     = 0xC9,
    PKEY_END      = 0xCF,
    PKEY_PGDN     = 0xD1,
};

enum {
    PMOUSE_LEFT   = (1 << 0),
    PMOUSE_RIGHT  = (1 << 1),
    PMOUSE_MIDDLE = (1 << 2),
};

typedef struct platform_t platform_t;

/* Returns NULL on failure. fb_width/fb_height are the game's native size. */
platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale);

/* palette is 256 RGB triplets (768 bytes). */
void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette);

/* The game's render size, which platform_blit scales to the fb size. */
void platform_set_render_size(platform_t *p, int w, int h);

/* False when the backend cannot present 640x480 — a DOS VGA card with no
 * VESA 2.0 linear framebuffer. */
bool platform_hires_supported(platform_t *p);

/* How the 4:3 image fits a wider fixed panel. Only PSP and Vita act on it. */
enum {
    SCALE_PILLARBOX = 0,   /* full height, 4:3-correct width, bars either side */
    SCALE_CROP      = 1,   /* fills the panel; source cropped top/bottom */
    SCALE_STRETCH   = 2    /* fills the panel; aspect ratio not preserved */
};

bool platform_scale_mode_supported(platform_t *p);
void platform_set_scale_mode(platform_t *p, int mode);

/* Rows cropped off the top (and bottom) of the render image, in game pixels;
 * non-zero only under SCALE_CROP. Keeps subtitles inside the visible band. */
int platform_crop_inset_y(platform_t *p, int render_h);

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer);

/* Once per frame, main thread. Returns false when the user asked to quit. */
bool platform_pump_events(platform_t *p);

bool platform_key_down(platform_t *p, int keycode);
bool platform_key_pressed(platform_t *p, int keycode);

/* Consumes a latched key-down: true once per physical press. Unlike
 * platform_key_pressed() it survives a tap whose press and release land in
 * the same pump (present_delay pumps 50ms apart). Auto-repeat does not
 * re-latch. */
bool platform_key_hit(platform_t *p, int keycode);

/* Coordinates in framebuffer space; returns PMOUSE_* bits. */
int platform_mouse_state(platform_t *p, int *out_x, int *out_y);

/* Milliseconds since platform_init(). */
uint32_t platform_ticks(platform_t *p);
void platform_delay(uint32_t ms);
void platform_shutdown(platform_t *p);

#define GAMEPAD_STICK_DEADZONE 8000

typedef struct {
    bool connected;
    bool dpad_up, dpad_down, dpad_left, dpad_right;
    int16_t left_x, left_y;
    int16_t right_x, right_y;
    bool btn_south;     /* A / Cross */
    bool btn_east;      /* B / Circle */
    bool btn_west;      /* X / Square */
    bool btn_north;     /* Y / Triangle */
    bool btn_lb, btn_rb;
    bool btn_lt, btn_rt;
    bool btn_start, btn_select;
    bool btn_lstick, btn_rstick;
} platform_gamepad_state_t;

/* First connected gamepad; all zero when none is connected. */
void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state);

void platform_set_title(platform_t *p, const char *title);

/* Audio: 8-bit unsigned mono PCM mixer, 16 voices, oldest recycled. */

void platform_audio_init(void);

/* data must stay valid while playing. volume 0..127, pan -128..127.
 * Returns the voice slot, or -1. */
int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop);

void platform_audio_stop_voice(int slot);
void platform_audio_stop_all(void);
void platform_audio_shutdown(void);

/* MIDI: plays an SMF blob (converted from the game's tune banks) on the OS
 * synth. Copies the data and stops any current tune. Returns 0 or -1. */
int platform_midi_play(const void *smf_data, int length, bool loop);
void platform_midi_stop(void);

/* Volumes are 0..255 so no floating point crosses this interface — the DOS
 * target has no FPU to assume. */
void platform_set_sfx_volume(int vol);
void platform_set_music_volume(int vol);

/* Called from main() before any data file is opened (detect_game_version
 * reads CODE/ECSTATIC.FAN). openfpgaOS mounts the data image here. */
void platform_early_init(void);

/* openfpgaOS has a fixed set of nonvolatile slots; desktops have no limit. */
int platform_save_slot_count(void);

/* game_version keeps the two games' saves apart on fixed-slot platforms. */
void platform_save_path(char *buf, int bufsz, int slot, int game_version);

/* Creates the save directory where one is needed. */
void platform_save_prepare(void);

/* Hardware rendering (render.h). Backends without it return false from
 * platform_gfx_create and stub the rest. */

/* OpenGL 3.3 core on desktops (4.1 core on macOS, the smallest profile with
 * GLSL 330). False sends the engine back to the software renderer. */
bool platform_gfx_create(platform_t *p);

void platform_gfx_make_current(platform_t *p);

/* Separate from platform_gfx_create, which only probes for GL at startup.
 * While inactive the backend must leave the surface to the software blit. */
void platform_gfx_set_active(platform_t *p, bool active);

void platform_gfx_swap(platform_t *p);
void platform_gfx_destroy(platform_t *p);

/* In pixels: not the window size on Retina, never the framebuffer size. */
void platform_gfx_drawable_size(platform_t *p, int *w, int *h);

/* NULL on backends with no GL. */
void *platform_gl_proc(const char *name);

#endif /* PLATFORM_H */
