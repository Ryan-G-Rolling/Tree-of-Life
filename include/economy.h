#ifndef ECONOMY_H
#define ECONOMY_H

#include "game_types.h"

double economy_upgrade_cost(RunUpgradeType upgrade, int current_level);
double economy_run_multiplier(RunUpgradeType upgrade, int level);
double economy_click_atp(const GameState *game);
int economy_biomass_milestone(double max_biomass);

#endif
