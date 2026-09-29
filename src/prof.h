#ifndef PROF_H
#define PROF_H

#include <stdint.h>

/* Frame-phase profiler. Compiled in only with ECS_PROFILE (make PROFILE=1 in
 * psp/ or vita/); every macro is a no-op otherwise. Phases may nest: LOAD
 * runs inside LOGIC, BLIT inside SHOW. */
enum {
    PROF_WAIT, PROF_LOGIC, PROF_PREPARE, PROF_STUCK, PROF_DRAW, PROF_SHOW,
    PROF_BLIT, PROF_LOAD, PROF_VIEW, PROF_RAW, PROF_COUNT
};

#ifdef ECS_PROFILE
void prof_begin(int phase);
void prof_end(int phase);
void prof_frame(void);
/* Supplied by the platform: a free-running microsecond clock. */
uint32_t prof_clock_us(void);
/* Optional, for platforms that can see their own file reads. */
void prof_io_read(uint32_t us, int32_t bytes);
void prof_io_seek(void);
#define PROF_BEGIN(p) prof_begin(p)
#define PROF_END(p)   prof_end(p)
#define PROF_FRAME()  prof_frame()
#else
#define PROF_BEGIN(p) ((void)0)
#define PROF_END(p)   ((void)0)
#define PROF_FRAME()  ((void)0)
#endif

#endif
