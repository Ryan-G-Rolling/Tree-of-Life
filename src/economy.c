/*
 * economy.c  ——  Member A · 单轮升级价格与点击收益
 *
 * 集成说明：本文件不再自带一套平衡常量。所有数值来自 data/balance.h（B 维护
 * 的唯一平衡源），所有永久强化乘数来自 B 的 legacy_* 函数，里程碑等级来自 B 的
 * prestige_biomass_milestone_level。A 只负责“把 B 的规则串成动作”。
 */
#include "economy.h"

#include <math.h>

#include "../data/balance.h"
#include "legacy.h"

double economy_upgrade_cost(RunUpgradeType upgrade, int current_level)
{
    double base, growth;
    if (current_level < 0) return INFINITY;
    if (current_level >= RUN_UPGRADE_MAX_LEVEL) return INFINITY;
    switch (upgrade) {
    case UPGRADE_METABOLISM:
        base = RUN_UPGRADE_METABOLISM_BASE_COST;
        growth = RUN_UPGRADE_METABOLISM_COST_GROWTH;
        break;
    case UPGRADE_REPRODUCTION:
        base = RUN_UPGRADE_REPRODUCTION_BASE_COST;
        growth = RUN_UPGRADE_REPRODUCTION_COST_GROWTH;
        break;
    case UPGRADE_GENOME:
        base = RUN_UPGRADE_GENOME_BASE_COST;
        growth = RUN_UPGRADE_GENOME_COST_GROWTH;
        break;
    default:
        return INFINITY;
    }
    return floor(base * pow(growth, (double)current_level));
}

double economy_run_multiplier(RunUpgradeType upgrade, int level)
{
    if (level < 0) return 0.0;
    switch (upgrade) {
    case UPGRADE_METABOLISM:   return 1.0 + RUN_METABOLISM_BONUS_PER_LEVEL * (double)level;
    case UPGRADE_REPRODUCTION: return 1.0 + RUN_REPRODUCTION_BONUS_PER_LEVEL * (double)level;
    case UPGRADE_GENOME:       return 1.0 + RUN_GENOME_BONUS_PER_LEVEL * (double)level;
    default:                   return 0.0;
    }
}

double economy_click_atp(const GameState *game)
{
    if (!game) return 0.0;
    /* 点击 = 基础值 × 单轮代谢等级 × 永久点击倍率 × 永久全产率倍率。
       后两项直接取 B 的 legacy_click_multiplier / legacy_atp_multiplier，
       避免 A 再把 1.08 / 1.12 抄一遍。 */
    return BASE_CLICK_ATP
         * economy_run_multiplier(UPGRADE_METABOLISM,
                                  game->run.run_upgrade_levels[UPGRADE_METABOLISM])
         * legacy_click_multiplier(&game->legacy)
         * legacy_atp_multiplier(&game->legacy);
}

int economy_biomass_milestone(double max_biomass)
{
    /* 里程碑口径只有 B 一份实现：run.max_biomass 派生，不缓存等级字段。 */
    return prestige_biomass_milestone_level(max_biomass);
}
