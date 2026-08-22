#ifndef VIEWER_H
#define VIEWER_H

#include "types.h"

/* Set by main() when the binary is started with --viewer. setup() branches
 * on it after the archives are open, in place of start_game_medium/make_thing. */
extern int viewer_mode;

/* Model / animation browser. Runs its own loop and never returns. */
void viewer_main(void);

#endif /* VIEWER_H */
