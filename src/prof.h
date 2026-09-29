#ifndef PROF_H
#define PROF_H

/* Frame-phase profiler. Compiled in only with ECS_PROFILE (make -C psp
 * PROFILE=1); every macro is a no-op otherwise. Phases may nest: LOAD runs
 * inside LOGIC, BLIT inside SHOW. */
enum {
    PROF_WAIT, PROF_LOGIC, PROF_PREPARE, PROF_STUCK, PROF_DRAW, PROF_SHOW,
    PROF_BLIT, PROF_LOAD, PROF_VIEW, PROF_RAW, PROF_COUNT
};

#ifdef ECS_PROFILE
void prof_begin(int phase);
void prof_end(int phase);
void prof_frame(void);
#define PROF_BEGIN(p) prof_begin(p)
#define PROF_END(p)   prof_end(p)
#define PROF_FRAME()  prof_frame()
#else
#define PROF_BEGIN(p) ((void)0)
#define PROF_END(p)   ((void)0)
#define PROF_FRAME()  ((void)0)
#endif

#endif
