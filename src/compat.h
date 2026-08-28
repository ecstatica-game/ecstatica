#ifndef COMPAT_H
#define COMPAT_H

/* M_PI — not guaranteed by C99; MSVC needs _USE_MATH_DEFINES */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Case-insensitive compare. POSIX spells it strcasecmp in <strings.h>; every
 * other toolchain here has the same function under a different name. Engine
 * code includes compat.h and calls the POSIX spelling — no <strings.h> in the
 * translation units themselves, because Open Watcom has no such header. */
#if defined(__WATCOMC__)
#include <string.h>
#define strcasecmp  stricmp
#define strncasecmp strnicmp
#elif !defined(_WIN32)
#include <strings.h>
#endif

/* Immediate termination, no atexit handlers and no stdio flush — used from the
 * crash handler, where running more code is exactly what must not happen.
 * POSIX and Watcom both have _exit; MSVC spells it _exit too, via <stdlib.h>. */
#include <stdlib.h>
#if defined(_WIN32) || defined(__WATCOMC__)
#define ecs_exit_now(code) _exit(code)
#else
#include <unistd.h>
#define ecs_exit_now(code) _exit(code)
#endif

/* DOS (Open Watcom): no <dirent.h>, but <direct.h> carries the whole POSIX
 * directory interface — struct dirent with d_name, DIR, opendir/readdir/
 * closedir — so no shim is needed, only the different header name. mkdir
 * takes one argument here, as on Win32. */
#if defined(__WATCOMC__) && !defined(_WIN32)
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* POSIX string case-compare on Windows */
#ifdef _WIN32
#include <string.h>
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

/* mkdir — POSIX takes (path, mode); Win32 _mkdir takes only (path) */
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)

/* Minimal dirent shim using Win32 FindFirstFile/FindNextFile */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct dirent {
    char d_name[MAX_PATH];
};

typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    struct dirent entry;
    int first;
} DIR;

static inline DIR *opendir(const char *path) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    DIR *d = (DIR *)calloc(1, sizeof(DIR));
    if (!d) return NULL;
    d->handle = FindFirstFileA(pattern, &d->data);
    if (d->handle == INVALID_HANDLE_VALUE) { free(d); errno = ENOENT; return NULL; }
    d->first = 1;
    return d;
}

static inline struct dirent *readdir(DIR *d) {
    if (!d) return NULL;
    if (d->first) { d->first = 0; }
    else if (!FindNextFileA(d->handle, &d->data)) return NULL;
    strncpy(d->entry.d_name, d->data.cFileName, MAX_PATH - 1);
    d->entry.d_name[MAX_PATH - 1] = '\0';
    return &d->entry;
}

static inline int closedir(DIR *d) {
    if (!d) return -1;
    FindClose(d->handle);
    free(d);
    return 0;
}
#endif /* _WIN32 */

#endif /* COMPAT_H */
