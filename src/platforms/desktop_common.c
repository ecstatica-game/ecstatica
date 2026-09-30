/**
 * desktop_common.c
 *
 * Platform capabilities macOS, Linux and Windows share: the data directory is
 * the working directory, saves are files under saved/.
 */

#include "../platform.h"
#include "../asm_f.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir_compat(p) _mkdir(p)
#define chdir_compat(p) _chdir(p)
#else
#include <sys/types.h>
#include <unistd.h>
#define mkdir_compat(p) mkdir((p), 0755)
#define chdir_compat(p) chdir(p)
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

/* Directory holding the running executable. Empty string if it can't be
 * determined — every caller treats that as "candidate not available". */
static void executable_dir(char *buf, size_t bufsz) {
    buf[0] = '\0';
#if defined(_WIN32)
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof(path));
    if (n == 0 || n >= sizeof(path)) return;
#elif defined(__APPLE__)
    char path[1024];
    uint32_t n = sizeof(path);
    if (_NSGetExecutablePath(path, &n) != 0) return;
#else
    char path[1024];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) return;
    path[n] = '\0';
#endif
    char *slash = strrchr(path, '/');
#ifdef _WIN32
    char *back = strrchr(path, '\\');
    if (back && (!slash || back > slash)) slash = back;
#endif
    if (!slash || slash == path) return;
    *slash = '\0';
    snprintf(buf, bufsz, "%s", path);
}

/* Directory the AppImage file itself sits in — not the squashfs mount the
 * executable runs from, which holds no game data. */
static void appimage_dir(char *buf, size_t bufsz) {
    buf[0] = '\0';
    const char *img = getenv("APPIMAGE");
    if (!img || !*img) return;
    snprintf(buf, bufsz, "%s", img);
    char *slash = strrchr(buf, '/');
    if (!slash || slash == buf) { buf[0] = '\0'; return; }
    *slash = '\0';
}

/* Assets are read relative to the working directory, but an AppImage, a
 * .desktop entry or a Steam shortcut starts in the launcher's cwd (usually
 * $HOME). So chdir to the first candidate holding the database:
 * ECSTATICA_DATA first, and a cwd that already has it is left alone. */
void platform_early_init(void) {
    struct { const char *what; char dir[1024]; } cand[5];
    int n = 0;

    const char *env = getenv("ECSTATICA_DATA");
    cand[n].what = "$ECSTATICA_DATA";
    snprintf(cand[n].dir, sizeof(cand[n].dir), "%s", env ? env : "");
    n++;

    cand[n].what = "working directory";
    cand[n].dir[0] = '\0';   /* "" means cwd to file_dir_has_database */
    n++;

    cand[n].what = "AppImage folder";
    appimage_dir(cand[n].dir, sizeof(cand[n].dir));
    n++;

    const char *owd = getenv("OWD");   /* set by the AppImage runtime */
    cand[n].what = "launch directory";
    snprintf(cand[n].dir, sizeof(cand[n].dir), "%s", owd ? owd : "");
    n++;

    cand[n].what = "executable folder";
    executable_dir(cand[n].dir, sizeof(cand[n].dir));
    n++;

    for (int i = 0; i < n; i++) {
        /* Only the cwd candidate is empty; others are probed by absolute path. */
        if (i > 0 && !cand[i].dir[0]) continue;
        if (!file_dir_has_database(cand[i].dir)) continue;
        if (cand[i].dir[0]) {
            if (chdir_compat(cand[i].dir) != 0) continue;
            /* Listings cached during the probe are keyed by the old cwd. */
            file_flush_path_cache();
        }
        return;
    }

    fprintf(stderr,
            "ecstatica: no game data found.\n\n"
            "Run the game from the folder holding the game files (the one with\n"
            "CODE/ECSTATIC.FAN in it), or point ECSTATICA_DATA at that folder:\n\n"
            "    ECSTATICA_DATA=/path/to/game /path/to/ecstatica\n\n"
            "Searched:\n");
    for (int i = 0; i < n; i++) {
        const char *shown = cand[i].dir[0] ? cand[i].dir : (i == 1 ? "." : "(unset)");
        fprintf(stderr, "  %-18s %s\n", cand[i].what, shown);
    }
    exit(2);
}

/* Matches the original: eleven slots, files in saved/. */
int platform_save_slot_count(void) {
    return 11;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version) {
    (void)game_version;   /* one directory per install, so no need to split */
    snprintf(buf, bufsz, "saved/%04d.ecs", slot);
}

void platform_save_prepare(void) {
    struct stat st;
    if (stat("saved", &st) != 0)
        mkdir_compat("saved");
}
