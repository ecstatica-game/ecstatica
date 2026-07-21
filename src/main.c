/**
 * main.c
 *
 * Entry point.  Initialises the platform layer then hands
 * control to the game's own WinMain-equivalent loop.
 */

#include "win.h"
#include "platform.h"
#include <signal.h>
#include <stdlib.h>
#ifndef _WIN32
#include <execinfo.h>
#include <unistd.h>
#endif

int debug_verbose = 1;  /* 0=quiet, 1=important, 2=verbose */
FILE *debug_log_file = NULL;

static void crash_handler(int sig) {
    fprintf(stderr, "\n=== CRASH sig=%d ===\n", sig);
#ifndef _WIN32
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
    _exit(128 + sig);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    signal(SIGSEGV, crash_handler);
#ifndef _WIN32
    signal(SIGBUS, crash_handler);
#endif
    signal(SIGABRT, crash_handler);
    setvbuf(stderr, NULL, _IONBF, 0); /* unbuffered stderr for diagnostics */
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
