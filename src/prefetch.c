/**
 * prefetch.c
 *
 * Background reads of the views around the hero. Not in the original, which
 * loaded a camera's .RAW only at the cut and stalled the frame for it; on a
 * Memory Stick or SD card that read is most of the stall.
 *
 * The game thread picks the cameras (map tables are not shared) and queues
 * their paths; one background thread only opens and reads files. Only whole
 * files cross over, and load_raw still unpacks on the game thread.
 */

#include "prefetch.h"
#include "asm_f.h"
#include "game.h"
#include "map.h"
#include "platform.h"
#include "topo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A HIRES view is up to ~575 KB. Three slots (1.7 MB) is all a PSP can
 * spare; with three, a Vita still missed most cuts where several cameras
 * meet, so a multi-core machine keeps eight (4.6 MB). */
#define PREFETCH_SLOTS  8
#define PREFETCH_SLOTS_SMALL 3
/* Cells (512 units) around the hero whose cameras are candidates. */
#define PREFETCH_RADIUS 2

enum { SLOT_EMPTY, SLOT_QUEUED, SLOT_READING, SLOT_READY, SLOT_FAILED };

typedef struct {
    char     path[24];
    int      enhanced;
    int      state;
    uint8_t *data;
    size_t   len;
    uint32_t wanted;    /* stamp of the last pass that asked for it */
    int      rank;      /* place in that pass, nearest first */
} slot_t;

static slot_t slots[PREFETCH_SLOTS];
static int slot_count = PREFETCH_SLOTS_SMALL;
static platform_sem_t *slot_lock;
static platform_sem_t *work_ready;
static int running;     /* 0 not tried, 1 running, -1 no threads */
static uint32_t pass_stamp;
static int last_cell = -1, last_svga = -1, last_camera = -1;

static uint8_t *read_whole(const char *path, size_t *len) {
    FILE *f = fopen_ci(path, "rb");
    if (!f) return NULL;

    uint8_t *buf = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long n = ftell(f);
        if (n > 0 && fseek(f, 0, SEEK_SET) == 0) {
            buf = (uint8_t *)malloc((size_t)n);
            if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
                free(buf);
                buf = NULL;
            }
            if (buf) *len = (size_t)n;
        }
    }
    fclose(f);
    return buf;
}

/* A slot in SLOT_READING is never evicted or taken, so `job` stays valid
 * while the lock is released for the read. */
static void reader_thread(void *arg) {
    (void)arg;
    for (;;) {
        platform_sem_wait(work_ready);

        /* Nearest first: a read takes ~100 ms on a Vita card, so the queue
         * is still draining when the hero reaches the next cell. */
        platform_sem_wait(slot_lock);
        slot_t *job = NULL;
        char path[sizeof(slots[0].path)];
        for (int i = 0; i < slot_count; i++)
            if (slots[i].state == SLOT_QUEUED && (!job || slots[i].rank < job->rank))
                job = &slots[i];
        if (job) {
            job->state = SLOT_READING;
            memcpy(path, job->path, sizeof(path));
        }
        platform_sem_post(slot_lock);
        if (!job) continue;     /* taken or replaced before it was reached */

        size_t len = 0;
        uint8_t *data = read_whole(path, &len);

        platform_sem_wait(slot_lock);
        job->data = data;
        job->len = len;
        job->state = data ? SLOT_READY : SLOT_FAILED;
        platform_sem_post(slot_lock);
    }
}

static void start(void) {
    running = -1;
    if (platform_cpu_count() > 1)
        slot_count = PREFETCH_SLOTS;
    slot_lock = platform_sem_create(1);
    work_ready = platform_sem_create(0);
    if (!slot_lock || !work_ready)
        return;
    file_enable_threads();
    if (platform_thread_start("ecs_prefetch", reader_thread, NULL,
                              PLATFORM_THREAD_BACKGROUND, -1))
        running = 1;
    DBG_LOG(1, "[PREFETCH] %s\n", running == 1 ? "reader thread running" : "off");
}

static slot_t *find_slot(const char *path, int enhanced) {
    for (int i = 0; i < slot_count; i++)
        if (slots[i].state != SLOT_EMPTY && slots[i].enhanced == enhanced &&
            strcmp(slots[i].path, path) == 0)
            return &slots[i];
    return NULL;
}

/* Caller holds slot_lock. */
static void queue_path(const char *path, int rank) {
    slot_t *s = find_slot(path, enhanced_graphics);
    if (s) {
        s->wanted = pass_stamp;     /* a failed read is not retried */
        s->rank = rank;
        return;
    }

    slot_t *victim = NULL;
    for (int i = 0; i < slot_count && !victim; i++)
        if (slots[i].state == SLOT_EMPTY)
            victim = &slots[i];
    for (int i = 0; i < slot_count; i++) {
        slot_t *c = &slots[i];
        if (victim || c->state == SLOT_READING || c->wanted == pass_stamp)
            continue;
        victim = c;
    }
    if (!victim) return;

    free(victim->data);
    victim->data = NULL;
    victim->len = 0;
    snprintf(victim->path, sizeof(victim->path), "%s", path);
    victim->enhanced = enhanced_graphics;
    victim->wanted = pass_stamp;
    victim->rank = rank;
    victim->state = SLOT_QUEUED;
    platform_sem_post(work_ready);
}

static int add_camera(int16_t *want, int n, int camera) {
    if (camera <= 0 || camera == selected_camera || n >= slot_count)
        return n;
    for (int i = 0; i < n; i++)
        if (want[i] == camera) return n;
    want[n] = (int16_t)camera;
    return n + 1;
}

void prefetch_views_near(const vector_t *pos) {
    if (running == 0) start();
    if (running != 1) return;

    int col = (pos->X >> 9) + 64;
    int row = (pos->Z >> 9) + 64;
    if (col < 0 || col >= 128 || row < 0 || row >= 128) return;

    int cell = row * 128 + col;
    if (cell == last_cell && mode_svga == last_svga && selected_camera == last_camera)
        return;
    last_cell = cell;
    last_svga = mode_svga;
    last_camera = selected_camera;

    /* Nearest rings first, so the slots go to the likeliest cuts. */
    int16_t want[PREFETCH_SLOTS];
    int n = 0;
    for (int r = 0; r <= PREFETCH_RADIUS && n < slot_count; r++) {
        for (int dz = -r; dz <= r; dz++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx != -r && dx != r && dz != -r && dz != r) continue;
                int c = col + dx, w = row + dz;
                if (c < 0 || c >= 128 || w < 0 || w >= 128) continue;

                uint16_t idx = new_map[w][c];
                while (idx != 0xFFFF && idx < 60000) {
                    const map_area_element_t *e = &map_elements[idx];
                    n = add_camera(want, n, e->camera_index);
                    n = add_camera(want, n, e->camera_override);
                    if (e->code_index_p1 & 0x8000) break;
                    idx++;
                }
            }
        }
    }

    DBG_LOG(2, "[PREFETCH] cell %d,%d camera %d: %d candidates%s%d %d %d\n", col, row,
            selected_camera, n, n ? " " : "", n > 0 ? want[0] : 0, n > 1 ? want[1] : 0,
            n > 2 ? want[2] : 0);

    platform_sem_wait(slot_lock);
    pass_stamp++;
    for (int i = 0; i < n; i++) {
        char path[sizeof(slots[0].path)];
        snprintf(path, sizeof(path), mode_svga ? "HIRES/%04d.RAW" : "VIEWS/%04d.RAW",
                 want[i]);
        queue_path(path, i);
    }
    platform_sem_post(slot_lock);
}

void prefetch_shutdown(void) {
    if (running != 1) return;

    platform_sem_wait(slot_lock);
    running = -1;
    for (int i = 0; i < slot_count; i++)
        if (slots[i].state == SLOT_QUEUED)
            slots[i].state = SLOT_EMPTY;
    platform_sem_post(slot_lock);

    for (;;) {
        bool reading = false;
        platform_sem_wait(slot_lock);
        for (int i = 0; i < slot_count; i++)
            if (slots[i].state == SLOT_READING)
                reading = true;
        platform_sem_post(slot_lock);
        if (!reading) break;
        platform_delay(1);
    }
}

const uint8_t *prefetch_get(const char *path, size_t *len) {
    if (running != 1) return NULL;

    for (;;) {
        platform_sem_wait(slot_lock);
        slot_t *s = find_slot(path, enhanced_graphics);
        if (s && s->state == SLOT_READING) {
            platform_sem_post(slot_lock);
            platform_delay(1);
            continue;
        }

        /* Stays cached, so cutting back to this camera is a hit too. Only
         * the game thread evicts, in prefetch_views_near, which is what
         * bounds the pointer's life. */
        const uint8_t *data = NULL;
        if (s && s->state == SLOT_READY) {
            data = s->data;
            *len = s->len;
        } else if (s && s->state == SLOT_QUEUED) {
            s->state = SLOT_EMPTY;  /* the caller reads it now anyway */
        }
        platform_sem_post(slot_lock);

        DBG_LOG(2, "[PREFETCH] %s %s\n", path, data ? "hit" : "miss");
        return data;
    }
}
