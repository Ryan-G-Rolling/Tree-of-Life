#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "game_types.h"

EnvironmentStep environment_calculate_step(const GameState *game, double dt);

#endif
