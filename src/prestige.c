/*
 * Member B · prestige.c
 * EP 里程碑、纪元结算明细与一次性入账
 *
 * 数据来源：02-数据与计算规则 §9
 * 预览与正式结算必须调用同一个函数 prestige_preview_reward。
 */

#include "legacy.h"
#include "evolution.h"
#include "balance.h"

/* 内容扩容：保留原有 5 档门槛，再增加 5 档，上限从 50,000 翻倍到 100,000 */
static const double k_biomass_milestones[11] = {
    0.0, 100.0, 500.0, 2000.0, 10000.0, 50000.0,
    250000.0, 1000000.0, 2500000.0, 5000000.0, 10000000.0
};

int prestige_biomass_milestone_level(double max_biomass)
{
    int level = 0;
    for (int i = 1; i < 11; ++i) {
        if (max_biomass >= k_biomass_milestones[i]) {
            level = i;
        }
    }
    return level;
}

int prestige_milestone_count(void) { return 11; }
double prestige_milestone_threshold(int level)
{
    if (level < 0) level = 0;
    if (level > 10) level = 10;
    return k_biomass_milestones[level];
}

void prestige_run_stats(const GameState *game, int *stages, int *mutations, int *species,
                        int *crises)
{
    int stage_count = evolution_main_stage(game);
    int mutation_count = 0;
    int species_count = 0;
    int crisis_count = 0;

    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (game->run.discovered_mutations[id]) {
            ++mutation_count;
        }
    }
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        if (game->run.discovered_species[id]) {
            ++species_count;
        }
    }
    for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id) {
        if (game->run.survived_crises[id]) {
            ++crisis_count;
        }
    }

    if (stages) {
        *stages = stage_count;
    }
    if (mutations) {
        *mutations = mutation_count;
    }
    if (species) {
        *species = species_count;
    }
    if (crises) {
        *crises = crisis_count;
    }
}

EraReward prestige_preview_reward(const GameState *game)
{
    EraReward reward;
    int stages = 0;
    int mutations = 0;
    int species = 0;
    int crises = 0;
    prestige_run_stats(game, &stages, &mutations, &species, &crises);

    int crisis_points = crises > EP_CRISIS_CAP ? EP_CRISIS_CAP : crises;
    crisis_points *= EP_POINTS_PER_CRISIS;
    reward.era_points = EP_PER_STAGE * stages;
    reward.mutation_points = mutations / EP_MUTATIONS_PER_POINT;
    if (reward.mutation_points > EP_MUTATION_CAP) {
        reward.mutation_points = EP_MUTATION_CAP;
    }
    reward.species_points = species / EP_SPECIES_PER_POINT;
    reward.crisis_points = crisis_points;
    reward.biomass_points = prestige_biomass_milestone_level(game->run.max_biomass);
    reward.total_points = reward.era_points + reward.mutation_points + reward.species_points +
                          reward.crisis_points + reward.biomass_points;
    return reward;
}

ActionCode prestige_commit_end_era(GameState *game)
{
    if (game->run.reward_claimed) {
        return ACTION_ALREADY_CLAIMED;
    }
    if (game->phase != PHASE_RUNNING && game->phase != PHASE_END_ERA_PREVIEW) {
        return ACTION_INVALID_PHASE;
    }

    EraReward reward = prestige_preview_reward(game);
    int stages = 0;
    prestige_run_stats(game, &stages, 0, 0, 0);

    game->legacy.evolution_points += reward.total_points;
    if (stages > game->legacy.best_era) {
        game->legacy.best_era = stages;
    }
    if (reward.total_points > game->legacy.best_evolution_points) {
        game->legacy.best_evolution_points = reward.total_points;
    }
    if (game->run.max_biomass > game->legacy.best_biomass) {
        game->legacy.best_biomass = game->run.max_biomass;
    }

    game->run.reward_claimed = true;
    game->phase = PHASE_LEGACY;
    return ACTION_OK;
}
