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
    "view", "raw", "vtx", "bands", "bwait", "blitwait",
    "rawread", "unpack", "vismap", "pal", "camera", "swap", "hotspot", "rep",
    "actors"
};
static const char *const s_count_names[PROF_C_COUNT] = { "pf_hit", "pf_miss", "dirlist" };
static uint32_t s_counts[PROF_C_COUNT], s_win_counts[PROF_C_COUNT];
static uint32_t s_io_opens, s_io_open_us, s_io_win_opens, s_io_win_open_us;
static uint32_t s_io_writes, s_io_write_us, s_io_win_writes, s_io_win_write_us;
static uint32_t s_op_us[PROF_OP_KEYS];
static uint16_t s_op_touched[64];
static int s_op_ntouched;
static const char *const s_worker_names[PROF_W_COUNT] = {
    "band1", "band2", "band3", "blit0", "blit1"
};
static volatile uint32_t s_worker_us[PROF_W_COUNT];
static uint32_t s_worker_seen[PROF_W_COUNT];
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

void prof_io_open(uint32_t us) {
    s_io_opens++;
    s_io_open_us += us;
}

void prof_count(int counter) {
    s_counts[counter]++;
}

void prof_io_write(uint32_t us) {
    s_io_writes++;
    s_io_write_us += us;
}

void prof_op(int key, uint32_t us) {
    if (key < 0 || key >= PROF_OP_KEYS) return;
    if (!s_op_us[key] && s_op_ntouched < 64)
        s_op_touched[s_op_ntouched++] = (uint16_t)key;
    s_op_us[key] += us;
}

/* The three keys with the most time this frame, then clear them all. */
static void print_ops(void) {
    for (int n = 0; n < 3; n++) {
        int best = -1;
        for (int i = 0; i < s_op_ntouched; i++) {
            int k = s_op_touched[i];
            if (s_op_us[k] && (best < 0 || s_op_us[k] > s_op_us[best]))
                best = k;
        }
        if (best < 0) break;
        fprintf(s_prof_log, " op%d=%.1f", best, s_op_us[best] / 1000.0);
        s_op_us[best] = 0;
    }
}

static void clear_ops(void) {
    for (int i = 0; i < s_op_ntouched; i++)
        s_op_us[s_op_touched[i]] = 0;
    s_op_ntouched = 0;
}

void prof_worker(int slot, uint32_t us) {
    s_worker_us[slot] += us;
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
        fprintf(s_prof_log, " io=%lu/%.1fms open=%lu/%.1fms write=%lu/%.1fms",
                (unsigned long)s_io_reads, s_io_read_us / 1000.0, (unsigned long)s_io_opens,
                s_io_open_us / 1000.0, (unsigned long)s_io_writes, s_io_write_us / 1000.0);
        print_ops();
        for (int i = 0; i < PROF_C_COUNT; i++)
            fprintf(s_prof_log, " %s=%lu", s_count_names[i], (unsigned long)s_counts[i]);
        fputc('\n', s_prof_log);
    }
    for (int i = 0; i < PROF_C_COUNT; i++) {
        s_win_counts[i] += s_counts[i];
        s_counts[i] = 0;
    }
    s_io_win_opens += s_io_opens; s_io_win_open_us += s_io_open_us;
    s_io_opens = s_io_open_us = 0;
    s_io_win_writes += s_io_writes; s_io_win_write_us += s_io_write_us;
    s_io_writes = s_io_write_us = 0;
    clear_ops();
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
    fprintf(s_prof_log, " | busy");
    for (int i = 0; i < PROF_W_COUNT; i++) {
        uint32_t total = s_worker_us[i];
        fprintf(s_prof_log, " %s=%.2f", s_worker_names[i],
                (total - s_worker_seen[i]) / 1000.0 / PROF_WINDOW);
        s_worker_seen[i] = total;
    }
    fprintf(s_prof_log, " | io reads=%lu seeks=%lu kb=%lu ms=%.1f opens=%lu/%.1fms writes=%lu/%.1fms",
            (unsigned long)s_io_win_reads, (unsigned long)s_io_win_seeks,
            (unsigned long)(s_io_win_bytes / 1024), s_io_win_us / 1000.0,
            (unsigned long)s_io_win_opens, s_io_win_open_us / 1000.0,
            (unsigned long)s_io_win_writes, s_io_win_write_us / 1000.0);
    s_io_win_writes = s_io_win_write_us = 0;
    for (int i = 0; i < PROF_C_COUNT; i++) {
        fprintf(s_prof_log, " %s=%lu", s_count_names[i], (unsigned long)s_win_counts[i]);
        s_win_counts[i] = 0;
    }
    fputc('\n', s_prof_log);
    s_io_win_opens = s_io_win_open_us = 0;
    s_io_win_reads = s_io_win_us = s_io_win_seeks = s_io_win_bytes = 0;
    fflush(s_prof_log);
    s_prof_n = 0;
}

#endif /* ECS_PROFILE */
