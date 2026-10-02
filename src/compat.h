#ifndef COMPAT_H
#define COMPAT_H

#include <stdio.h>

/* M_PI — not guaranteed by C99; MSVC needs _USE_MATH_DEFINES */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Best-effort fread where the caller deliberately doesn't check how much
 * came back (truncated/garbage data is handled downstream, or the read is
 * from a file already known to be the right size). A plain (void) cast on
 * the call does NOT silence Ubuntu/Debian glibc's warn_unused_result here —
 * the cast has to land on a named result instead. */
static inline void fread_ignore(void *ptr, size_t size, size_t n, FILE *f) {
    size_t rc = fread(ptr, size, n, f);
    (void)rc;
}

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

/* Open Watcom, on both its targets here (-bt=dos and -bt=nt): no <dirent.h>,
 * but <direct.h> carries the whole POSIX directory interface — struct dirent
 * with d_name, DIR, opendir/readdir/closedir — so no shim is needed, only the
 * different header name. mkdir takes one argument, as on Win32.
 *
 * This has to come before the _WIN32 block and exclude it: Watcom defines
 * _WIN32 when targeting NT, and the shim below would then redeclare the
 * directory interface that direct.h has already provided. */
#if defined(__WATCOMC__)
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* POSIX string case-compare on Windows */
#if defined(_WIN32) && !defined(__WATCOMC__)
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
