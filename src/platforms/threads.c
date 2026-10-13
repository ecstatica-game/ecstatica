/**
 * threads.c
 *
 * platform.h's thread and semaphore API on Win32 and POSIX threads, for every
 * backend that has an OS scheduler underneath: the desktops, the web build
 * (Emscripten pthreads), Android and iOS. Left to the OS scheduler: no
 * pinning, and SetThreadAffinityMask is not in Windows 95's kernel32.
 */

#include "../platform.h"

#include <stdlib.h>

#ifdef _WIN32

#include <windows.h>

struct platform_sem_s {
    HANDLE h;
};

typedef struct {
    void (*fn)(void *);
    void *arg;
} thread_start_t;

static DWORD WINAPI thread_entry(LPVOID param) {
    thread_start_t start = *(thread_start_t *)param;
    free(param);
    start.fn(start.arg);
    return 0;
}

bool platform_thread_start(const char *name, void (*fn)(void *), void *arg,
                           int priority, int core) {
    (void)name;
    (void)core;
    thread_start_t *start = (thread_start_t *)malloc(sizeof(*start));
    if (!start) return false;
    start->fn = fn;
    start->arg = arg;
    DWORD id;
    HANDLE h = CreateThread(NULL, 0, thread_entry, start, 0, &id);
    if (!h) {
        free(start);
        return false;
    }
    if (priority == PLATFORM_THREAD_BACKGROUND)
        SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
    CloseHandle(h);
    return true;
}

int platform_cpu_count(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
}

platform_sem_t *platform_sem_create(int initial) {
    platform_sem_t *s = (platform_sem_t *)malloc(sizeof(*s));
    if (!s) return NULL;
    s->h = CreateSemaphoreA(NULL, initial, 0x7FFFFFFF, NULL);
    if (!s->h) {
        free(s);
        return NULL;
    }
    return s;
}

void platform_sem_wait(platform_sem_t *s) {
    WaitForSingleObject(s->h, INFINITE);
}

void platform_sem_post(platform_sem_t *s) {
    ReleaseSemaphore(s->h, 1, NULL);
}

#else

#include <pthread.h>
#include <unistd.h>

/* macOS has no unnamed POSIX semaphores, so both build one from a condvar. */
struct platform_sem_s {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int count;
};

typedef struct {
    void (*fn)(void *);
    void *arg;
} thread_start_t;

static void *thread_entry(void *param) {
    thread_start_t start = *(thread_start_t *)param;
    free(param);
    start.fn(start.arg);
    return NULL;
}

bool platform_thread_start(const char *name, void (*fn)(void *), void *arg,
                           int priority, int core) {
    (void)name;
    (void)priority;
    (void)core;
    thread_start_t *start = (thread_start_t *)malloc(sizeof(*start));
    if (!start) return false;
    start->fn = fn;
    start->arg = arg;
    pthread_t th;
    if (pthread_create(&th, NULL, thread_entry, start) != 0) {
        free(start);
        return false;
    }
    pthread_detach(th);
    return true;
}

int platform_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
#ifdef __EMSCRIPTEN__
    /* Every thread is a Web Worker from a pool sized at link time
     * (PTHREAD_POOL_SIZE in platforms/web/CMakeLists.txt). */
    if (n > 4) n = 4;
#endif
    return n > 0 ? (int)n : 1;
}

platform_sem_t *platform_sem_create(int initial) {
    platform_sem_t *s = (platform_sem_t *)malloc(sizeof(*s));
    if (!s) return NULL;
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->count = initial;
    return s;
}

void platform_sem_wait(platform_sem_t *s) {
    pthread_mutex_lock(&s->lock);
    while (s->count == 0)
        pthread_cond_wait(&s->cond, &s->lock);
    s->count--;
    pthread_mutex_unlock(&s->lock);
}

void platform_sem_post(platform_sem_t *s) {
    pthread_mutex_lock(&s->lock);
    s->count++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->lock);
}

#endif
