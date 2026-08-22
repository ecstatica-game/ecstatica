#ifndef VIEWER_H
#define VIEWER_H

#include "types.h"

enum {
    VIEWER_OFF    = 0,
    VIEWER_MODELS = 1,   /* --viewer */
    VIEWER_SCENES = 2,   /* --scenes */
};

/* Set by main() from the command line. setup() branches on it once the
 * archives are open, in place of start_game_medium/make_thing. Non-zero
 * also picks which pane the tool opens on; V switches at runtime. */
extern int viewer_mode;

/* Model / animation browser. Runs its own loop and never returns. */
void viewer_main(void);

#endif /* VIEWER_H */
