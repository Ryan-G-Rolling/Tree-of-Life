#ifndef ECONOMY_H
#define ECONOMY_H

#include "game_types.h"

/* v10 P0-22：价格 = N × 主谱系产率（时间本位），需要 game 提供产率锚 */
double economy_upgrade_cost(const GameState *game, RunUpgradeType upgrade, int current_level);
double economy_run_multiplier(RunUpgradeType upgrade, int level);
double economy_click_atp(const GameState *game);
int economy_biomass_milestone(double max_biomass);

#endif
