#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

/* Sub-step rendering time is transient and must not leak across a load. */
void game_reset_timing(void);

#endif
