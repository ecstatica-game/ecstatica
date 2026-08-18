/**
 * desktop_common.c
 *
 * The platform capabilities that macOS, Linux and Windows all answer the same
 * way: the game runs with its data directory as the working directory, saves
 * are files under saved/, and the display is whatever the database asks for.
 *
 * Kept out of the three window backends so the answers can't drift apart.
 */

#include "../platform.h"

#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_compat(p) _mkdir(p)
#else
#include <sys/types.h>
#define mkdir_compat(p) mkdir((p), 0755)
#endif

/* Nothing to prepare: argv[0]'s directory already holds the data. */
void platform_early_init(void) {
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
