#include "economy.h"

#include <math.h>

#include "../data/balance.h"

double economy_upgrade_cost(RunUpgradeType upgrade, int current_level)
{
    double base, growth;
    if (current_level < 0) return INFINITY;
    switch (upgrade) {
    case UPGRADE_METABOLISM:   base = METABOLISM_COST_BASE; growth = METABOLISM_COST_GROWTH; break;
    case UPGRADE_REPRODUCTION: base = REPRODUCTION_COST_BASE; growth = REPRODUCTION_COST_GROWTH; break;
    case UPGRADE_GENOME:       base = GENOME_COST_BASE; growth = GENOME_COST_GROWTH; break;
    default: return INFINITY;
    }
    return floor(base * pow(growth, current_level));
}

double economy_run_multiplier(RunUpgradeType upgrade, int level)
{
    if (level < 0) return 0.0;
    switch (upgrade) {
    case UPGRADE_METABOLISM:   return 1.0 + METABOLISM_PER_LEVEL * level;
    case UPGRADE_REPRODUCTION: return 1.0 + REPRODUCTION_PER_LEVEL * level;
    case UPGRADE_GENOME:       return 1.0 + GENOME_PER_LEVEL * level;
    default: return 0.0;
    }
}

double economy_click_atp(const GameState *game)
{
    if (!game) return 0.0;
    double permanent = 1.0;
    if (game->legacy.levels[LEGACY_METABOLISM] >= 1) permanent *= LEGACY_ATP_LEVEL_1_MULTIPLIER;
    if (game->legacy.levels[LEGACY_METABOLISM] >= 4) permanent *= LEGACY_ATP_LEVEL_4_MULTIPLIER;
    if (game->legacy.levels[LEGACY_METABOLISM] >= 6) permanent *= LEGACY_CLICK_MULTIPLIER_LV6;
    return BASE_CLICK_ATP
        * economy_run_multiplier(UPGRADE_METABOLISM,
                                 game->run.run_upgrade_levels[UPGRADE_METABOLISM])
        * permanent;
}

int economy_biomass_milestone(double max_biomass)
{
    static const double thresholds[] = BIOMASS_MILESTONES;
    if (!isfinite(max_biomass) || max_biomass < 0.0) return 0;
    int level = 0;
    for (int i = 0; i < BIOMASS_MILESTONE_COUNT; ++i) {
        if (max_biomass < thresholds[i]) break;
        level = i + 1;
    }
    return level;
}
