#ifndef PROF_H
#define PROF_H

#include <stdint.h>

/* Frame-phase profiler. Compiled in only with ECS_PROFILE (make PROFILE=1 in
 * psp/ or vita/); every macro is a no-op otherwise. Phases may nest: LOAD
 * runs inside LOGIC, BLIT inside SHOW; VTX and BANDS inside DRAW, BWAIT
 * inside BANDS, BLITWAIT inside BLIT. */
enum {
    PROF_WAIT, PROF_LOGIC, PROF_PREPARE, PROF_STUCK, PROF_DRAW, PROF_SHOW,
    PROF_BLIT, PROF_LOAD, PROF_VIEW, PROF_RAW,
    PROF_VTX,       /* draw_parts pass 1: vertex and ellipse transforms */
    PROF_BANDS,     /* raster_end: band 0 on the game thread plus the wait */
    PROF_BWAIT,     /* game thread waiting for the other bands */
    PROF_BLITWAIT,  /* platform_blit waiting for the previous frame's blit */
    PROF_RAWREAD,   /* load_raw: open and read, or the prefetched copy */
    PROF_UNPACK,    /* load_raw: unpack the background and depth */
    PROF_VISMAP,    /* load_raw: load_visibility_map */
    PROF_PAL,       /* load_raw: load_palette */
    PROF_CAMERA,    /* prepare_parts: check_camera, load_raw included */
    PROF_SWAP,      /* check_camera: swapping actors out and in at a cut */
    PROF_HOTSPOT,   /* prepare_parts: check_hot_spots */
    PROF_REP,       /* prepare_parts: check_hero_rep and play_ambients */
    PROF_ACTORS,    /* prepare_parts: prepare_an_actor for every actor */
    PROF_COUNT
};

enum {
    PROF_OP_BOOL = 256,
    PROF_OP_SOUND_LOAD = 600,   /* play_sound_ecstatica: check_sound_loaded */
    PROF_OP_SOUND_START,        /* play_sound_ecstatica: start_playing_sample */
    PROF_OP_HERO_REP,           /* check_hero_rep */
    PROF_OP_AMBIENTS,           /* play_ambients */
    PROF_OP_KEYS = 640
};

/* Events counted per frame. */
enum { PROF_C_PF_HIT, PROF_C_PF_MISS, PROF_C_DIRLIST, PROF_C_COUNT };

/* Busy time of the helper threads, reported per frame next to the phases. */
enum {
    PROF_W_BAND1, PROF_W_BAND2, PROF_W_BAND3, PROF_W_BLIT0, PROF_W_BLIT1,
    PROF_W_COUNT
};

#ifdef ECS_PROFILE
void prof_begin(int phase);
void prof_end(int phase);
void prof_frame(void);
/* Any thread; each slot is written by one thread only. */
void prof_worker(int slot, uint32_t us);
void prof_count(int counter);
/* Time spent under a key this frame; the top keys go into the spike line.
 * Keys: a script opcode, PROF_OP_BOOL + a condition token, or PROF_OP_*. */
void prof_op(int key, uint32_t us);
void prof_io_write(uint32_t us);
/* Supplied by the platform: a free-running microsecond clock. */
uint32_t prof_clock_us(void);
/* Optional, for platforms that can see their own file reads. */
void prof_io_read(uint32_t us, int32_t bytes);
void prof_io_seek(void);
void prof_io_open(uint32_t us);
#define PROF_BEGIN(p) prof_begin(p)
#define PROF_END(p)   prof_end(p)
#define PROF_FRAME()  prof_frame()
#define PROF_WORKER(s, us) prof_worker((s), (us))
#define PROF_CLOCK()  prof_clock_us()
#define PROF_COUNT_EVENT(c) prof_count(c)
#define PROF_OP(k, us) prof_op((k), (us))
#else
#define PROF_OP(k, us) ((void)(us))
#define PROF_COUNT_EVENT(c) ((void)0)
#define PROF_WORKER(s, us) ((void)(us))
#define PROF_CLOCK()  0u
#define PROF_BEGIN(p) ((void)0)
#define PROF_END(p)   ((void)0)
#define PROF_FRAME()  ((void)0)
#endif

#endif
