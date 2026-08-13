#ifndef MUSIC_H
#define MUSIC_H

#include "types.h"

#pragma pack(push, 1)
typedef struct wave_s {
    int16_t field_0;
    int16_t field_2;
    int32_t field_4;
    int32_t field_8;
    int16_t field_C;
    int16_t field_E;
    int16_t field_10;
} wave_t;
#pragma pack(pop)

#pragma pack(push, 1)
struct sound_s {
    int16_t sound_name_index;
    struct sound_s *next;
    uint16_t use_flag;
    char *audio_ptr;
    int32_t sound_length;
    int16_t sample_rate;
    int32_t _time;
    int16_t volume;
    char header[32];
    void *DSBuffer;
};
#pragma pack(pop)

extern int16_t sound_driver;
extern bool sound_is_on;
extern bool music_on;
extern bool sound_fx_on;
extern bool subtitles_on;
extern bool tune_playing;
extern char *sound_storage;
extern int32_t top_of_sound_data;
extern int32_t top_of_texture_data;
extern char *tune_buffer;
extern int32_t tune_buffer_length;
extern int16_t current_tune;
extern int32_t tune_position;
extern int16_t num_ambients;
extern int ambiant_name[20];
extern int16_t ambiant_vol[20];
extern int16_t ambiant_freq[20];
extern int16_t ambiant_rand[20];
extern int32_t ambiant_last[20];
extern int32_t tune_offset[10][96];
extern int16_t need_draw_graphics;
extern int16_t need_clear_graphics;
extern sound_t sound_heap_arr[SOUND_POOL_SIZE];

void remove_sound_driver_win95(void);
void release_sound_buffer_win95(sound_t *sound);
void play_sound_win95(sound_t *sound, int volume);
void allocate_ds_buffer(sound_t *sound, FILE *f);
void my_new_direct_sound_buffer(void *dsbuf, sound_t *sound, FILE *f);
void init_sound_driver_win95(void);
void start_tune(void);
void start_tune_win95(void);
void stop_tune_win95(void);
void play_tune(int tune_index);
void stop_tune(void);
void fade_tune(int target_volume, int speed);
void wave_open_file(const char *filename);
void wave_load_file(const char *filename);

#endif /* MUSIC_H */
