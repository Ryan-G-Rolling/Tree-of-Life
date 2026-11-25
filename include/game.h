#ifndef GAME_H
#define GAME_H

#include "game_types.h"

void game_init(GameState *game);
void game_update(GameState *game, double dt);
ActionResult game_apply_action(GameState *game, GameAction action);
void game_start_new_run(GameState *game);

#endif
