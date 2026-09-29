/* Frame-phase profiler, see prof.h. Writes prof.log in the working (data)
 * directory: a summary every PROF_WINDOW frames and a line per spike. */

#ifdef ECS_PROFILE

#include "prof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROF_WINDOW 100
#define PROF_SPIKE_US 66000u

static const char *const s_prof_names[PROF_COUNT] = {
    "wait", "logic", "prep", "stuck", "draw", "show", "blit", "load",
    "view", "raw"
};
static uint32_t s_prof_start[PROF_COUNT];
static uint32_t s_prof_acc[PROF_COUNT];
static uint32_t s_prof_win_acc[PROF_COUNT];
static uint32_t s_prof_frames_us[PROF_WINDOW];
static int s_prof_n;
static uint32_t s_prof_last;
static uint32_t s_io_reads, s_io_read_us, s_io_seeks, s_io_bytes;
static uint32_t s_io_win_reads, s_io_win_us, s_io_win_seeks, s_io_win_bytes;
static FILE *s_prof_log;

void prof_io_read(uint32_t us, int32_t bytes) {
    s_io_read_us += us;
    s_io_reads++;
    if (bytes > 0) s_io_bytes += (uint32_t)bytes;
}

void prof_io_seek(void) {
    s_io_seeks++;
}

void prof_begin(int phase) { s_prof_start[phase] = prof_clock_us(); }
void prof_end(int phase) { s_prof_acc[phase] += prof_clock_us() - s_prof_start[phase]; }

static int prof_cmp(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

void prof_frame(void) {
    uint32_t now = prof_clock_us();
    if (!s_prof_log) {
        s_prof_log = fopen("prof.log", "w");
        s_prof_last = now;
        memset(s_prof_acc, 0, sizeof(s_prof_acc));
        return;
    }
    uint32_t frame = now - s_prof_last;
    s_prof_last = now;

    if (frame >= PROF_SPIKE_US) {
        fprintf(s_prof_log, "spike %6.1f ms:", frame / 1000.0);
        for (int i = 0; i < PROF_COUNT; i++)
            fprintf(s_prof_log, " %s=%.1f", s_prof_names[i], s_prof_acc[i] / 1000.0);
        fprintf(s_prof_log, " io=%lu/%.1fms\n", (unsigned long)s_io_reads, s_io_read_us / 1000.0);
    }
    for (int i = 0; i < PROF_COUNT; i++) {
        s_prof_win_acc[i] += s_prof_acc[i];
        s_prof_acc[i] = 0;
    }
    s_io_win_reads += s_io_reads; s_io_win_us += s_io_read_us;
    s_io_win_seeks += s_io_seeks; s_io_win_bytes += s_io_bytes;
    s_io_reads = s_io_read_us = s_io_seeks = s_io_bytes = 0;

    s_prof_frames_us[s_prof_n++] = frame;
    if (s_prof_n < PROF_WINDOW)
        return;

    uint32_t sum = 0;
    for (int i = 0; i < PROF_WINDOW; i++) sum += s_prof_frames_us[i];
    qsort(s_prof_frames_us, PROF_WINDOW, sizeof(uint32_t), prof_cmp);
    fprintf(s_prof_log, "window t=%.1fs med=%.1f p90=%.1f max=%.1f mean=%.1f |",
            now / 1e6, s_prof_frames_us[PROF_WINDOW / 2] / 1000.0,
            s_prof_frames_us[PROF_WINDOW * 9 / 10] / 1000.0,
            s_prof_frames_us[PROF_WINDOW - 1] / 1000.0, sum / 1000.0 / PROF_WINDOW);
    for (int i = 0; i < PROF_COUNT; i++) {
        fprintf(s_prof_log, " %s=%.2f", s_prof_names[i], s_prof_win_acc[i] / 1000.0 / PROF_WINDOW);
        s_prof_win_acc[i] = 0;
    }
    fprintf(s_prof_log, " | io reads=%lu seeks=%lu kb=%lu ms=%.1f\n",
            (unsigned long)s_io_win_reads, (unsigned long)s_io_win_seeks,
            (unsigned long)(s_io_win_bytes / 1024), s_io_win_us / 1000.0);
    s_io_win_reads = s_io_win_us = s_io_win_seeks = s_io_win_bytes = 0;
    fflush(s_prof_log);
    s_prof_n = 0;
}

#endif /* ECS_PROFILE */
