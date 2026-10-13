/**
 * platforms/mixer.h
 *
 * Software audio for backends whose OS gives them only a PCM stream (web,
 * Android, iOS). mixer.c implements platform.h's PCM voice, MIDI and volume
 * calls; the backend opens the device in platform_audio_init and pulls
 * frames with mixer_render from its audio callback.
 */

#ifndef PLATFORMS_MIXER_H
#define PLATFORMS_MIXER_H

#include <stdint.h>

/* Call before the device starts pulling. `soundfont_dir` is searched for a
 * General MIDI .sf2 after $ECSTATICA_SOUNDFONT and the data folder; NULL to
 * skip it. Without a soundfont, music is silent. */
void mixer_init(int out_rate, const char *soundfont_dir);

/* Interleaved 16-bit stereo. Safe to call from any one audio thread. */
void mixer_render(int16_t *out, int frames);

/* After the device has stopped pulling. */
void mixer_shutdown(void);

#endif /* PLATFORMS_MIXER_H */
