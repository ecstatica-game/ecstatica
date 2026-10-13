/**
 * main.c
 *
 * Entry point.  Initialises the platform layer then hands
 * control to the game's own WinMain-equivalent loop.
 */

#include "win.h"
#include "platform.h"
#include "tools/viewer.h"
#include "layout.h"
#include "compat.h"
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* HAVE_EXECINFO comes from the build: glibc and macOS have backtrace(),
 * musl does not. A capability, not a platform name. */
#ifdef HAVE_EXECINFO
#include <execinfo.h>
#define HAVE_BACKTRACE 1
#endif

int debug_verbose = 1;  /* 0=quiet, 1=important, 2=verbose */
FILE *debug_log_file = NULL;

#if defined(__WATCOMC__) && defined(__DOS__)
#include <dos.h>
void dbg_log_commit(void) {
    union REGS r;
    if (!debug_log_file) return;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x6800;
    r.w.bx = (unsigned short)fileno(debug_log_file);
    int386(0x21, &r, &r);
}
#endif

/* Background log writer, used where platform_slow_writes(): on a Vita one
 * unbuffered log line could hold the game thread for 70-150 ms. Lines go into
 * the filling half of a double buffer; the writer swaps halves and writes the
 * full one. A line that does not fit is dropped and counted. */
#define LOG_BUF_SIZE (32 * 1024)

static char log_buf[2][LOG_BUF_SIZE];
static int log_fill, log_len;
static unsigned log_dropped;
static platform_sem_t *log_lock, *log_ready;
static bool log_async;
static bool log_writing;    /* the writer has a half out of the lock */

static void log_write_pending(void) {
    platform_sem_wait(log_lock);
    int idx = log_fill, len = log_len;
    unsigned dropped = log_dropped;
    log_fill ^= 1;
    log_len = 0;
    log_dropped = 0;
    platform_sem_post(log_lock);

    if (len) fwrite(log_buf[idx], 1, (size_t)len, debug_log_file);
    if (dropped) fprintf(debug_log_file, "[LOG] %u lines dropped\n", dropped);
    if (len || dropped) fflush(debug_log_file);
}

static void log_writer(void *arg) {
    (void)arg;
    for (;;) {
        platform_sem_wait(log_ready);
        platform_sem_wait(log_lock);
        bool go = log_async;
        if (go) log_writing = true;
        platform_sem_post(log_lock);
        if (!go) continue;      /* stopped: dbg_log_flush writes the rest */

        log_write_pending();

        platform_sem_wait(log_lock);
        log_writing = false;
        platform_sem_post(log_lock);
    }
}

static void dbg_log_start_async(void) {
    log_lock = platform_sem_create(1);
    log_ready = platform_sem_create(0);
    if (!log_lock || !log_ready) return;
    setvbuf(debug_log_file, NULL, _IOFBF, 16 * 1024);
    log_async = platform_thread_start("ecs_log", log_writer, NULL,
                                      PLATFORM_THREAD_BACKGROUND, -1);
    if (!log_async) {
        log_lock = NULL;
        setvbuf(debug_log_file, NULL, _IONBF, 0);
    }
}

void dbg_log_printf(const char *fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;

    if (log_lock) {
        platform_sem_wait(log_lock);
        if (log_async) {
            if (log_len + n <= LOG_BUF_SIZE) {
                memcpy(log_buf[log_fill] + log_len, line, (size_t)n);
                log_len += n;
            } else {
                log_dropped++;
            }
            platform_sem_post(log_lock);
            platform_sem_post(log_ready);
            return;
        }
        platform_sem_post(log_lock);
    }

    fwrite(line, 1, (size_t)n, debug_log_file);
    fflush(debug_log_file);
    dbg_log_commit();
}

/* Stops the writer for good and writes what is queued from this thread: at
 * exit newlib flushes and frees every FILE, which must not meet a write in
 * flight. Later lines are written directly. */
void dbg_log_flush(void) {
    if (!log_lock || !debug_log_file) return;

    platform_sem_wait(log_lock);
    bool was_async = log_async;
    log_async = false;
    platform_sem_post(log_lock);
    if (!was_async) return;

    for (;;) {
        platform_sem_wait(log_lock);
        bool writing = log_writing;
        platform_sem_post(log_lock);
        if (!writing) break;
        platform_delay(1);
    }
    log_write_pending();
    setvbuf(debug_log_file, NULL, _IONBF, 0);
}

static void crash_handler(int sig) {
    fprintf(stderr, "\n=== CRASH sig=%d ===\n", sig);
#ifdef HAVE_BACKTRACE
    void *bt[32];
    int n = backtrace(bt, 32);
    backtrace_symbols_fd(bt, n, 2);
    dbg_log_flush();
    if (debug_log_file) {
        fprintf(debug_log_file, "\n=== CRASH sig=%d ===\n", sig);
        char **syms = backtrace_symbols(bt, n);
        if (syms) {
            for (int i = 0; i < n; i++)
                fprintf(debug_log_file, "  %s\n", syms[i]);
            free(syms);
        }
        fflush(debug_log_file);
    }
#endif
    ecs_exit_now(128 + sig);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--viewer") == 0)
            viewer_mode = VIEWER_MODELS;
        else if (strcmp(argv[i], "--scenes") == 0)
            viewer_mode = VIEWER_SCENES;
        else if (strcmp(argv[i], "--pointcloud") == 0)
            viewer_mode = VIEWER_POINTCLOUD;
        else if (strcmp(argv[i], "--images") == 0)
            viewer_mode = VIEWER_IMAGES;
    }
    signal(SIGSEGV, crash_handler);
    /* SIGBUS is POSIX; neither Win32 nor DOS defines it. */
#ifdef SIGBUS
    signal(SIGBUS, crash_handler);
#endif
    signal(SIGABRT, crash_handler);
    setvbuf(stderr, NULL, _IONBF, 0); /* unbuffered stderr for diagnostics */

    /* Whatever has to exist before the first asset open. On targets with no
     * working directory this is where the data root gets mounted, so it must
     * precede win_main_game() — that probes the archives for the version. */
    platform_early_init();

    /* ECSTATICA_DEBUG=<n> raises the log level without a rebuild; the
     * level-2 traces are the detailed ones. */
    {
        const char *lvl = getenv("ECSTATICA_DEBUG");
        if (lvl && *lvl) debug_verbose = atoi(lvl);
    }

    /* Fails harmlessly (leaving the log NULL) where the root is read-only.
     * DOS gets an 8.3 name — "ecstatica_debug.log" cannot be created on a FAT
     * volume, so on that target the log silently never appeared. */
#ifdef __WATCOMC__
    debug_log_file = fopen("ECSTATIC.LOG", "w");
#else
    debug_log_file = fopen("ecstatica_debug.log", "w");
#endif
    if (debug_log_file) {
        setvbuf(debug_log_file, NULL, _IONBF, 0);
        /* %p + (void*) on the FILE* we're about to write to trips GCC's
         * -Wrestrict (it can't see fprintf only reads the pointer's value
         * here, not through it) — print it as a plain integer instead. */
        fprintf(debug_log_file, "MAIN: debug_log_file=%lx debug_verbose=%d\n",
                (unsigned long)(uintptr_t)debug_log_file, debug_verbose);
        fflush(debug_log_file);
        dbg_log_commit();
        if (platform_slow_writes())
            dbg_log_start_async();
    }

    /* The original ran WinMain → do_init → win_main_game. */
    win_main_game();

    return 0;
}
