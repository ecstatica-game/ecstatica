/**
 * Ecstatica Platform Abstraction Layer
 *
 * Provides a cross-platform interface for window management,
 * framebuffer blitting, input handling, and timing.
 *
 * Each platform (macOS, Linux, Windows) implements these functions
 * using native APIs.
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
    /* Added for the model/animation viewer (viewer.c). Values stay set-1
     * scancodes, which is what the Win32 backend derives straight from
     * lParam — only the table-driven backends need new entries. */
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

/**
 * Create a platform window and initialize graphics.
 *
 * @param title       Window title
 * @param fb_width    Framebuffer width  (game native resolution)
 * @param fb_height   Framebuffer height (game native resolution)
 * @param scale       Integer scale factor for the window
 * @return            Opaque platform handle, or NULL on failure
 */
platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale);

/**
 * Blit an 8-bit paletted framebuffer to the window.
 *
 * @param p           Platform handle
 * @param framebuffer Pointer to fb_width*fb_height bytes (indexed color)
 * @param palette     256-entry RGB palette (768 bytes: R0 G0 B0 R1 G1 B1 …)
 */
void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette);

/**
 * Set the actual game rendering resolution (may differ from fb/window size).
 * platform_blit will scale from render size to fb size.
 */
void platform_set_render_size(platform_t *p, int w, int h);

/**
 * False when the backend cannot present 640x480, so the engine must stay in
 * its 320x200 mode whatever the game data offers. Only real hardware says no:
 * a period VGA card with no VESA 2.0 linear framebuffer has nothing above mode
 * 13h that this port can drive.
 */
bool platform_hires_supported(platform_t *p);

/**
 * Blit a 32-bit RGBA framebuffer to the window.
 *
 * @param p           Platform handle
 * @param framebuffer Pointer to fb_width*fb_height*4 bytes (RGBA)
 */
void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer);

/**
 * Process pending window/input events.
 * Must be called once per frame from the main thread.
 *
 * @param p           Platform handle
 * @return            false if the user requested quit, true otherwise
 */
bool platform_pump_events(platform_t *p);

/**
 * Check if a key is currently held down.
 *
 * @param p           Platform handle
 * @param keycode     PKEY_xxx constant
 */
bool platform_key_down(platform_t *p, int keycode);

/**
 * Check if a key was pressed this frame (edge-triggered).
 *
 * @param p           Platform handle
 * @param keycode     PKEY_xxx constant
 */
bool platform_key_pressed(platform_t *p, int keycode);

/**
 * Consume a latched key-down. Returns true once per physical press and clears
 * the latch.
 *
 * Unlike platform_key_pressed(), which compares this frame's level against
 * last frame's, this survives a tap whose press and release land in the same
 * event pump — the case that makes quick presses vanish during present_delay,
 * where pumps are 50ms apart. Auto-repeat does not re-latch: the flag is only
 * set on a genuine false->true transition.
 *
 * @param p           Platform handle
 * @param keycode     PKEY_xxx constant
 */
bool platform_key_hit(platform_t *p, int keycode);

/**
 * Get current mouse state.
 *
 * @param p           Platform handle
 * @param out_x       Receives X in framebuffer coords
 * @param out_y       Receives Y in framebuffer coords
 * @return            Bitmask of PMOUSE_xxx flags
 */
int platform_mouse_state(platform_t *p, int *out_x, int *out_y);

/**
 * Get milliseconds since platform_init().
 */
uint32_t platform_ticks(platform_t *p);

/**
 * Sleep for approximately `ms` milliseconds.
 */
void platform_delay(uint32_t ms);

/**
 * Destroy the platform window and free resources.
 */
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

/**
 * Poll the first connected gamepad. Fills `state` with current values.
 * If no gamepad is connected, state->connected is false and all fields zero.
 */
void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state);

/**
 * Set the window title.
 */
void platform_set_title(platform_t *p, const char *title);

/* ── Audio ────────────────────────────────────────────────────
 * Simple 8-bit-PCM voice mixer at 22050 Hz mono (matches Ecstatica
 * WAV format). Up to 16 concurrent voices; oldest recycled on overflow.
 */

/**
 * Initialize audio output. Called once from setup.
 */
void platform_audio_init(void);

/**
 * Queue an 8-bit unsigned PCM sample for playback.
 *
 * @param data     Pointer to sample bytes (unsigned 8-bit PCM, mono).
 *                 Must remain valid while playing — engine keeps sound
 *                 buffers alive across the whole game session.
 * @param length   Number of sample bytes.
 * @param rate     Sample rate in Hz (typically 22050).
 * @param volume   0..127 linear volume.
 * @param pan      -128 (left) .. 0 (center) .. 127 (right).
 * @param loop     If true, sample loops continuously until stopped.
 * @return         Voice slot index (0..15), or -1 on failure.
 */
int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop);

/**
 * Stop a specific voice by slot index.
 */
void platform_audio_stop_voice(int slot);

/**
 * Stop all currently-playing voices.
 */
void platform_audio_stop_all(void);

/**
 * Tear down audio output. Called on shutdown.
 */
void platform_audio_shutdown(void);

/* ── MIDI music ─────────────────────────────────────────────
 * Standard-MIDI (SMF format 0/1) playback via the OS software synth
 * (macOS DLSMusicDevice / GM). Engine converts native tune banks
 * (Sound Images GEN2) to SMF and hands the blob here.
 */

/**
 * Start playing an SMF byte blob. Copies the data; caller may free.
 * Any previously-playing tune is stopped first.
 * Returns 0 on success, -1 on failure.
 */
int platform_midi_play(const void *smf_data, int length, bool loop);

/**
 * Stop any currently-playing MIDI tune.
 */
void platform_midi_stop(void);

/* Volumes are 0..255, matching the engine's own scale, so no floating point
 * crosses this interface. Backends that mix in float convert at the boundary.
 * The DOS target has no FPU to assume. */

/**
 * Set master SFX volume (0 = silent, 255 = full).
 */
void platform_set_sfx_volume(int vol);

/**
 * Set master music volume (0 = silent, 255 = full).
 */
void platform_set_music_volume(int vol);

/* ── Platform capabilities ────────────────────────────────────
 * Things the engine needs that differ per platform but must not be spelled
 * as #ifdefs in shared code. Every backend implements all of them; most are
 * one-liners on a desktop.
 */

/**
 * One-time setup that must complete before any engine code opens a data file.
 *
 * Called from main() ahead of win_main_game(), because detect_game_version()
 * reads CODE/ECSTATIC.FAN. Desktop backends have nothing to do — the game runs
 * with the data directory as its working directory. The openfpgaOS backend
 * mounts the game-data image and points the file layer at the mount.
 */
void platform_early_init(void);

/**
 * Number of save slots this platform can store.
 *
 * Desktop platforms write files into a directory and have no real limit;
 * openfpgaOS exposes a fixed set of pre-declared nonvolatile slots.
 */
int platform_save_slot_count(void);

/**
 * Build the path for save slot `slot`.
 *
 * @param game_version  GAME_VERSION_E1 / _E2 — platforms with a fixed slot
 *                      set use it to keep the two games' saves apart.
 */
void platform_save_path(char *buf, int bufsz, int slot, int game_version);

/**
 * Create whatever a save needs to exist first — a directory, typically.
 * Called before writing a save. A no-op where slots are pre-declared.
 */
void platform_save_prepare(void);

/* ── Hardware rendering ───────────────────────────────────────
 * A 3D context alongside the software framebuffer, for the enhanced graphics
 * renderer (render.h). Every backend implements these; the ones with no
 * hardware path return false from platform_gfx_create and stub the rest, the
 * same way platform_hires_supported reports a capability rather than forcing
 * an #ifdef into shared code.
 */

/**
 * Create a rendering context on the game window.
 *
 * Desktop backends make an OpenGL 3.3 core context. Note that macOS has no 3.3
 * profile constant — 4.1 core is requested there because it is the smallest
 * profile that provides GLSL 330.
 *
 * @return false if the context could not be had, which sends the engine back
 *         to the software renderer for the session.
 */
bool platform_gfx_create(platform_t *p);

/** Make the context current on the calling thread. */
void platform_gfx_make_current(platform_t *p);

/**
 * Hand the window's surface to the hardware renderer, or give it back.
 *
 * Distinct from platform_gfx_create because the context is created once at
 * startup just to find out whether the machine can do OpenGL 3.3 at all, which
 * says nothing about whether the player wants it. While inactive the backend
 * must leave the surface alone so the software blit keeps working — conflating
 * the two leaves the window blank in software mode.
 */
void platform_gfx_set_active(platform_t *p, bool active);

/** Present the back buffer. */
void platform_gfx_swap(platform_t *p);

/** Destroy the context and any window resources it needed. */
void platform_gfx_destroy(platform_t *p);

/**
 * Size of the drawable in pixels, which is not the window size on a Retina
 * display and not the framebuffer size at any time.
 */
void platform_gfx_drawable_size(platform_t *p, int *w, int *h);

/**
 * Resolve a GL entry point by name. NULL on backends with no GL.
 */
void *platform_gl_proc(const char *name);

#endif /* PLATFORM_H */
