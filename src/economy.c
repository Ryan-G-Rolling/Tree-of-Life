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
#include "buffs.h"
#include "evolution.h"

/* v10 P0-22：单轮升级改时间本位——价格 = N × 主谱系产率（进化锚见 evolution_price_rates）。
 * 代谢 30 s ATP / 繁殖 40 s ATP / 基因组 55 s 生物量；BASE 常数是开局地板。
 * 旧的 base × 1.25^L 与产率脱钩：早期 5 s 一级、点火后 60 s 扫 16 级。 */
double economy_upgrade_cost(const GameState *game, RunUpgradeType upgrade, int current_level)
{
    if (current_level < 0) return INFINITY;
    if (RUN_UPGRADE_MAX_LEVEL >= 0 && current_level >= RUN_UPGRADE_MAX_LEVEL) return INFINITY;
    double atp_rate = 0.0, biomass_rate = 0.0, knowledge_rate = 0.0;
    evolution_price_rates(game, &atp_rate, &biomass_rate, &knowledge_rate);
    double base, seconds, rate;
    switch (upgrade) {
    case UPGRADE_METABOLISM:
        base = RUN_UPGRADE_METABOLISM_BASE_COST;
        seconds = UPGRADE_ATP_SECONDS_METABOLISM;
        rate = atp_rate;
        break;
    case UPGRADE_REPRODUCTION:
        base = RUN_UPGRADE_REPRODUCTION_BASE_COST;
        seconds = UPGRADE_ATP_SECONDS_REPRODUCTION;
        rate = atp_rate;
        break;
    case UPGRADE_GENOME:
        base = RUN_UPGRADE_GENOME_BASE_COST;
        seconds = UPGRADE_BIOMASS_SECONDS_GENOME;
        rate = biomass_rate;
        break;
    default:
        return INFINITY;
    }
    double priced = seconds * rate;
    return floor(priced > base ? priced : base);
}

double economy_run_multiplier(RunUpgradeType upgrade, int level)
{
    if (level < 0) return 0.0;
    /* v10 P0-23：线性加成收一次平方根，与 evolution_calculate_species_rates 同一公式 */
    switch (upgrade) {
    case UPGRADE_METABOLISM:   return 1.0 + RUN_METABOLISM_BONUS_PER_LEVEL * sqrt((double)level);
    case UPGRADE_REPRODUCTION: return 1.0 + RUN_REPRODUCTION_BONUS_PER_LEVEL * sqrt((double)level);
    case UPGRADE_GENOME:       return 1.0 + RUN_GENOME_BONUS_PER_LEVEL * sqrt((double)level);
    default:                   return 0.0;
    }
}

double economy_click_atp(const GameState *game)
{
    if (!game) return 0.0;
    /* v10：点击 = 3 秒主谱系产率 × 永久点击倍率 × 限时点击倍率。
     * 产率涨点击同步涨（BASE_CLICK_ATP 常数曾让点击在 200 s 后形同装饰）；
     * 产率锚已含代谢等级与永久产率倍率，不再单独乘一遍。 */
    double atp_rate = 0.0, biomass_rate = 0.0, knowledge_rate = 0.0;
    evolution_price_rates(game, &atp_rate, &biomass_rate, &knowledge_rate);
    return CLICK_INCOME_SECONDS
         * atp_rate
         * legacy_click_multiplier(&game->legacy)
         * buffs_click_multiplier(game);
}

int economy_biomass_milestone(double max_biomass)
{
    /* 里程碑口径只有 B 一份实现：run.max_biomass 派生，不缓存等级字段。 */
    return prestige_biomass_milestone_level(max_biomass);
}

