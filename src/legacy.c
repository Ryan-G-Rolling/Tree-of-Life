/*
 * Member B · legacy.c
 * 四条永久强化路线的成本、效果与一次性加点
 *
 * 数据来源：02-数据与计算规则 §10
 */

#include "legacy.h"
#include "balance.h"

#include <math.h>

int legacy_upgrade_cost(int current_level)
{
    if (current_level < 0 || current_level >= LEGACY_MAX_LEVEL) {
        return 0;
    }
    return current_level + 1;
}

ActionCode legacy_validate_upgrade(const GameState *game, LegacyBranch branch)
{
    if (game->phase != PHASE_LEGACY) {
        return ACTION_INVALID_PHASE;
    }
    if (branch < 0 || branch > LEGACY_DIVERSIFICATION) {
        return ACTION_INVALID_TARGET;
    }
    int level = game->legacy.levels[branch];
    if (level >= LEGACY_MAX_LEVEL) {
        return ACTION_LEVEL_MAXED;
    }
    if (game->legacy.evolution_points < legacy_upgrade_cost(level)) {
        return ACTION_NOT_ENOUGH_EP;
    }
    return ACTION_OK;
}

ActionCode legacy_apply_upgrade(GameState *game, LegacyBranch branch)
{
    ActionCode code = legacy_validate_upgrade(game, branch);
    if (code != ACTION_OK) {
        return code;
    }
    game->legacy.evolution_points -= legacy_upgrade_cost(game->legacy.levels[branch]);
    ++game->legacy.levels[branch];
    return ACTION_OK;
}

/* 图鉴里程碑：每 3 个突变图鉴 +1% 全局产率（上限 6%），
 * 每 2 个物种图鉴 +1% Biomass（上限 3%） */
static double codex_mutation_bonus(const LegacyState *legacy)
{
    int count = 0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (legacy->codex_mutations[id]) {
            ++count;
        }
    }
    double bonus = (double)(count / CODEX_MUTATIONS_PER_BONUS) * CODEX_MUTATION_BONUS;
    return bonus > CODEX_MUTATION_BONUS_CAP ? CODEX_MUTATION_BONUS_CAP : bonus;
}

static double codex_species_bonus(const LegacyState *legacy)
{
    int count = 0;
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        if (legacy->codex_species[id]) {
            ++count;
        }
    }
    double bonus = (double)(count / CODEX_SPECIES_PER_BONUS) * CODEX_SPECIES_BONUS;
    return bonus > CODEX_SPECIES_BONUS_CAP ? CODEX_SPECIES_BONUS_CAP : bonus;
}

double legacy_atp_multiplier(const LegacyState *legacy)
{
    double multiplier = 1.0;
    if (legacy->levels[LEGACY_METABOLISM] >= 1) {
        multiplier *= LEGACY_ATP_MULTIPLIER_LV1;
    }
    if (legacy->levels[LEGACY_METABOLISM] >= 4) {
        multiplier *= LEGACY_ALL_MULTIPLIER_LV4;
    }
    return multiplier * (1.0 + codex_mutation_bonus(legacy));
}

double legacy_biomass_multiplier(const LegacyState *legacy)
{
    double multiplier = 1.0;
    if (legacy->levels[LEGACY_METABOLISM] >= 2) {
        multiplier *= LEGACY_BIOMASS_MULTIPLIER_LV2;
    }
    if (legacy->levels[LEGACY_METABOLISM] >= 4) {
        multiplier *= LEGACY_ALL_MULTIPLIER_LV4;
    }
    if (legacy->levels[LEGACY_METABOLISM] >= 5) {
        multiplier *= LEGACY_BIOMASS_MULTIPLIER_LV5;
    }
    return multiplier * (1.0 + codex_mutation_bonus(legacy) + codex_species_bonus(legacy));
}

double legacy_dna_multiplier(const LegacyState *legacy)
{
    double multiplier = 1.0;
    if (legacy->levels[LEGACY_METABOLISM] >= 4) {
        multiplier *= LEGACY_ALL_MULTIPLIER_LV4;
    }
    return multiplier * (1.0 + codex_mutation_bonus(legacy));
}

double legacy_cap_multiplier(const LegacyState *legacy)
{
    double multiplier =
        legacy->levels[LEGACY_DIVERSIFICATION] >= 1 ? LEGACY_CAP_MULTIPLIER_LV1 : 1.0;
    if (legacy->levels[LEGACY_DIVERSIFICATION] >= 6) {
        multiplier *= LEGACY_CAP_MULTIPLIER_LV6;
    }
    return multiplier;
}

double legacy_mutation_discount(const LegacyState *legacy)
{
    int level = legacy->levels[LEGACY_MUTATION];
    if (level >= 7) {
        return LEGACY_MUTATION_DISCOUNT_LV7;
    }
    if (level >= 2) {
        return LEGACY_MUTATION_DISCOUNT_LV2;
    }
    if (level >= 1) {
        return LEGACY_MUTATION_DISCOUNT_LV1;
    }
    return 0.0;
}

int legacy_rare_mutation_weight(const LegacyState *legacy)
{
    if (legacy->levels[LEGACY_MUTATION] >= 8) {
        return LEGACY_RARE_WEIGHT_LV8;
    }
    return legacy->levels[LEGACY_MUTATION] >= 4 ? MUTATION_WEIGHT_RARE_BOOSTED
                                                : MUTATION_WEIGHT_RARE;
}

bool legacy_synergy_enabled(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_DIVERSIFICATION] >= 3;
}

double legacy_speciation_discount(const LegacyState *legacy)
{
    int level = legacy->levels[LEGACY_DIVERSIFICATION];
    double discount = 0.0;
    if (level >= 4) {
        discount += DIVERSITY_SPECIATION_DISCOUNT;
    }
    if (level >= 8) {
        discount += LEGACY_SPECIATION_DISCOUNT_LV8;
    }
    return discount;
}

int legacy_active_species_slots(const LegacyState *legacy)
{
    int level = legacy->levels[LEGACY_DIVERSIFICATION];
    if (level >= 5) {
        return 3;
    }
    return level >= 2 ? 2 : 1;
}

double legacy_environment_duration_scale(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 2 ? ENVIRONMENT_DURATION_SCALE_LV2 : 1.0;
}

double legacy_negative_multiplier_scale(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 1 ? ENVIRONMENT_NEGATIVE_DELTA_SCALE_LV1 : 1.0;
}

double legacy_positive_multiplier_scale(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 4 ? ENVIRONMENT_POSITIVE_BONUS_SCALE_LV4 : 1.0;
}

bool legacy_preview_enabled(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 3;
}

bool legacy_shield_available(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 5;
}

/* ---------------- 第四轮：Lv.5–8 派生效果 ---------------- */

double legacy_production_multiplier(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_METABOLISM] >= 8 ? LEGACY_PRODUCTION_MULTIPLIER_LV8 : 1.0;
}

double legacy_click_multiplier(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_METABOLISM] >= 6 ? LEGACY_CLICK_MULTIPLIER_LV6 : 1.0;
}

double legacy_key_guarantee_ratio(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_MUTATION] >= 5 ? LEGACY_KEY_GUARANTEE_RATIO_LV5
                                                : MUTATION_KEY_GUARANTEE_BIOMASS_RATIO;
}

double legacy_crisis_damage_reduction(const LegacyState *legacy)
{
    if (legacy->levels[LEGACY_ADAPTATION] >= 8) {
        return LEGACY_CRISIS_DAMAGE_LV8;
    }
    if (legacy->levels[LEGACY_ADAPTATION] >= 6) {
        return LEGACY_CRISIS_DAMAGE_LV6;
    }
    return 0.0;
}

double legacy_positive_duration_scale(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 7 ? LEGACY_POSITIVE_DURATION_LV7 : 1.0;
}

double legacy_shield_damage_reduction(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_ADAPTATION] >= 8 ? LEGACY_SHIELD_REDUCTION_LV8
                                                  : CRISIS_SHIELD_DAMAGE_REDUCTION;
}

double legacy_synergy_multiplier(const LegacyState *legacy)
{
    return legacy->levels[LEGACY_DIVERSIFICATION] >= 7 ? LEGACY_SYNERGY_MULTIPLIER_LV7
                                                       : DIVERSITY_SYNERGY_MULTIPLIER;
}

void legacy_apply_new_run_bonuses(GameState *game)
{
    if (game->legacy.levels[LEGACY_METABOLISM] >= 3) {
        if (game->run.atp < LEGACY_NEW_RUN_ATP_LV3) {
            game->run.atp = LEGACY_NEW_RUN_ATP_LV3;
        }
        if (game->run.run_upgrade_levels[UPGRADE_METABOLISM] < 1) {
            game->run.run_upgrade_levels[UPGRADE_METABOLISM] = 1;
        }
    }
    if (game->legacy.levels[LEGACY_METABOLISM] >= 7 &&
        game->run.biomass < LEGACY_NEW_RUN_BIOMASS_LV7) {
        game->run.biomass = LEGACY_NEW_RUN_BIOMASS_LV7;
    }
}
