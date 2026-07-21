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

/* ── Key codes (matching common scancodes) ── */
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
};

/* ── mouse button flags ── */
enum {
    PMOUSE_LEFT   = (1 << 0),
    PMOUSE_RIGHT  = (1 << 1),
    PMOUSE_MIDDLE = (1 << 2),
};

/* ── Platform state (opaque internals held by platform implementation) ── */
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

/**
 * Set master SFX volume (0.0 = silent, 1.0 = full).
 */
void platform_set_sfx_volume(float vol);

/**
 * Set master music volume (0.0 = silent, 1.0 = full).
 */
void platform_set_music_volume(float vol);

#endif /* PLATFORM_H */
