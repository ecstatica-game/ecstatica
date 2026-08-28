/**
 * main.c
 *
 * Entry point.  Initialises the platform layer then hands
 * control to the game's own WinMain-equivalent loop.
 */

#include "win.h"
#include "platform.h"
#include "tools/viewer.h"
#include "compat.h"
#include <signal.h>
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

static void crash_handler(int sig) {
    fprintf(stderr, "\n=== CRASH sig=%d ===\n", sig);
#ifdef HAVE_BACKTRACE
    void *bt[32];
    int n = backtrace(bt, 32);
    backtrace_symbols_fd(bt, n, 2);
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

    /* Fails harmlessly (leaving the log NULL) where the root is read-only. */
    debug_log_file = fopen("ecstatica_debug.log", "w");
    if (debug_log_file) {
        setvbuf(debug_log_file, NULL, _IONBF, 0);
        fprintf(debug_log_file, "MAIN: debug_log_file=%p debug_verbose=%d\n", (void*)debug_log_file, debug_verbose);
        fflush(debug_log_file);
    }

    /* The original game went through WinMain → do_init → win_main_game.
     * win_main_game (win.c) now sets up the platform layer and
     * drives the game loop. */
    win_main_game();

    return 0;
}
