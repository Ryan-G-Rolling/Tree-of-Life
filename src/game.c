#include "game.h"
#include "game_internal.h"

#include <math.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "../data/balance.h"
#include "economy.h"
#include "environment.h"
#include "evolution.h"
#include "legacy.h"

/* Rendering time is transient; only completed 0.1 s steps enter the save. */
static double step_remainder = 0.0;

void game_reset_timing(void)
{
    step_remainder = 0.0;
}

static ActionResult result(ActionCode code, double spent, int affected)
{
    ActionResult value = { code, spent, affected };
    return value;
}

static void reset_run(GameState *game, unsigned int seed)
{
    int era = game->run.era;
    memset(&game->run, 0, sizeof game->run);
    game->run.era = era < INT_MAX ? era + 1 : INT_MAX;
    game->run.active_species_count = 1;
    game->run.active_species[0].species_id = 0;
    game->run.active_species[0].is_main_lineage = true;
    game->run.active_species[0].population = STARTING_POPULATION;
    game->run.discovered_species[0] = true;
    game->run.current_environment_id = 0;
    game->run.next_environment_id = -1;
    game->run.event_interval_left = FIRST_EVENT_DELAY;
    game->run.rng_state = seed ? seed : 1u;
    legacy_apply_new_run_bonuses(game);
    if (era > 0) game->run.dna = LEGACY_STARTING_DNA;
    game->run.max_biomass = game->run.biomass;
    environment_init_run(game);
    game->legacy.codex_species[0] = true;
    game->phase = PHASE_RUNNING;
    game_reset_timing();
}

void game_init(GameState *game)
{
    if (!game) return;
    memset(game, 0, sizeof *game);
    game->save_version = SAVE_VERSION;
    reset_run(game, (unsigned int)time(NULL));
}

void game_start_new_run(GameState *game)
{
    if (!game) return;
    reset_run(game, game->run.rng_state);
}

static bool fixed_step(GameState *game)
{
    SpeciesRates rates[MAX_ACTIVE_SPECIES];
    GameState next = *game;
    double atp = 0.0, biomass = 0.0, dna = 0.0, knowledge = 0.0;
    int count = game->run.active_species_count;
    if (count < 1 || count > MAX_ACTIVE_SPECIES) return false;

    /* Every B calculation sees the same step-start state. */
    for (int i = 0; i < count; ++i)
        rates[i] = evolution_calculate_species_rates(game, i);

    for (int i = 0; i < count; ++i) {
        if (!isfinite(rates[i].atp_per_second) || rates[i].atp_per_second < 0.0
            || !isfinite(rates[i].biomass_per_second) || rates[i].biomass_per_second < 0.0
            || !isfinite(rates[i].dna_per_second) || rates[i].dna_per_second < 0.0
            || !isfinite(rates[i].knowledge_per_second) || rates[i].knowledge_per_second < 0.0
            || !isfinite(rates[i].population_delta_per_second)
            || !isfinite(rates[i].final_population_cap)
            || rates[i].final_population_cap < MIN_ACTIVE_POPULATION) return false;
        double population = game->run.active_species[i].population
                          + rates[i].population_delta_per_second * FIXED_STEP_SECONDS;
        double cap = rates[i].final_population_cap;
        if (!isfinite(population)) return false;
        if (population < MIN_ACTIVE_POPULATION) population = MIN_ACTIVE_POPULATION;
        if (population > cap) population = cap;
        next.run.active_species[i].population = population;
        atp += rates[i].atp_per_second * FIXED_STEP_SECONDS;
        biomass += rates[i].biomass_per_second * FIXED_STEP_SECONDS;
        dna += rates[i].dna_per_second * FIXED_STEP_SECONDS;
        knowledge += rates[i].knowledge_per_second * FIXED_STEP_SECONDS;
    }
    if (!isfinite(atp) || !isfinite(biomass) || !isfinite(dna)) return false;
    next.run.atp = fmax(0.0, next.run.atp + atp);
    next.run.biomass = fmax(0.0, next.run.biomass + biomass);
    next.run.dna = fmax(0.0, next.run.dna + dna);
    next.run.knowledge += knowledge;
    if (!isfinite(next.run.knowledge)) return false;
    next.run.elapsed_time += FIXED_STEP_SECONDS;
    if (!isfinite(next.run.atp) || !isfinite(next.run.biomass)
        || !isfinite(next.run.dna) || !isfinite(next.run.elapsed_time)) return false;
    if (next.run.biomass > next.run.max_biomass)
        next.run.max_biomass = next.run.biomass;

    /* B receives the provisional state after simultaneous resource/population writes. */
    EnvironmentStep environment = environment_calculate_step(&next, FIXED_STEP_SECONDS);
    if (environment.current_environment_id < 0
        || environment.current_environment_id >= MAX_ENVIRONMENTS
        || environment.next_environment_id < -1
        || environment.next_environment_id >= MAX_ENVIRONMENTS
        || !isfinite(environment.environment_time_left) || environment.environment_time_left < 0.0
        || !isfinite(environment.event_interval_left) || environment.event_interval_left < 0.0
        || (environment.event_ended && (environment.ended_environment_id <= 0
            || environment.ended_environment_id >= MAX_ENVIRONMENTS))) return false;
    if (!isfinite(environment.instant_population_loss_ratio)
        || environment.instant_population_loss_ratio < 0.0
        || environment.instant_population_loss_ratio > 1.0) return false;
    environment_apply_step(&next, &environment);
    *game = next;
    return true;
}

void game_update(GameState *game, double dt)
{
    if (!game || game->phase != PHASE_RUNNING || !isfinite(dt) || dt <= 0.0) return;
    if (!isfinite(step_remainder + dt)) return;
    step_remainder += dt;
    /* Tiny tolerance avoids losing a step to binary representation of 0.1. */
    while (step_remainder + 1e-9 >= FIXED_STEP_SECONDS) {
        if (!fixed_step(game)) {
            game_reset_timing();
            return;
        }
        step_remainder -= FIXED_STEP_SECONDS;
    }
}

ActionResult game_apply_action(GameState *game, GameAction action)
{
    if (!game) return result(ACTION_INVALID_TARGET, 0.0, -1);
    GameState next = *game;
    ActionResult answer = result(ACTION_OK, 0.0, action.target_id);

    switch (action.type) {
    case ACTION_CLICK_ATP:
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        next.run.atp += economy_click_atp(game);
        if (!isfinite(next.run.atp)) return result(ACTION_REQUIREMENT_NOT_MET, 0.0, -1);
        break;
    case ACTION_BUY_RUN_UPGRADE: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        if (action.target_id < UPGRADE_METABOLISM || action.target_id > UPGRADE_GENOME)
            return result(ACTION_INVALID_TARGET, 0.0, -1);
        int level = game->run.run_upgrade_levels[action.target_id];
        double cost = economy_upgrade_cost((RunUpgradeType)action.target_id, level);
        double *wallet = action.target_id == UPGRADE_GENOME ? &next.run.biomass : &next.run.atp;
        if (!isfinite(cost) || level >= RUN_UPGRADE_MAX_LEVEL) return result(ACTION_LEVEL_MAXED, 0.0, -1);
        if (*wallet < cost)
            return result(action.target_id == UPGRADE_GENOME ? ACTION_NOT_ENOUGH_BIOMASS
                                                              : ACTION_NOT_ENOUGH_ATP, 0.0, -1);
        *wallet -= cost;
        next.run.run_upgrade_levels[action.target_id]++;
        answer.amount_spent = cost;
        break;
    }
    case ACTION_OPEN_END_ERA:
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        next.phase = PHASE_END_ERA_PREVIEW;
        break;
    case ACTION_CANCEL_END_ERA:
        if (game->phase != PHASE_END_ERA_PREVIEW) return result(ACTION_INVALID_PHASE, 0.0, -1);
        next.phase = PHASE_RUNNING;
        break;
    case ACTION_CONFIRM_END_ERA: {
        if (game->phase != PHASE_END_ERA_PREVIEW) return result(ACTION_INVALID_PHASE, 0.0, -1);
        if (game->run.reward_claimed) return result(ACTION_ALREADY_CLAIMED, 0.0, -1);
        EraReward reward = prestige_preview_reward(game);
        if (reward.total_points < 0 || reward.total_points > INT_MAX - game->legacy.evolution_points)
            return result(ACTION_REQUIREMENT_NOT_MET, 0.0, -1);
        next.run.reward_claimed = true;
        next.phase = PHASE_LEGACY;
        next.legacy.evolution_points += reward.total_points;
        int stage = evolution_main_stage(game);
        if (stage > next.legacy.best_era) next.legacy.best_era = stage;
        if (next.run.max_biomass > next.legacy.best_biomass)
            next.legacy.best_biomass = next.run.max_biomass;
        if (reward.total_points > next.legacy.best_evolution_points)
            next.legacy.best_evolution_points = reward.total_points;
        break;
    }
    case ACTION_BUY_LEGACY: {
        if (game->phase != PHASE_LEGACY) return result(ACTION_INVALID_PHASE, 0.0, -1);
        if (action.target_id < LEGACY_METABOLISM || action.target_id > LEGACY_DIVERSIFICATION)
            return result(ACTION_INVALID_TARGET, 0.0, -1);
        int level = game->legacy.levels[action.target_id];
        if (level >= LEGACY_MAX_LEVEL) return result(ACTION_LEVEL_MAXED, 0.0, -1);
        ActionCode code = legacy_validate_upgrade(game, (LegacyBranch)action.target_id);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        int cost = legacy_upgrade_cost(level);
        if (game->legacy.evolution_points < cost) return result(ACTION_NOT_ENOUGH_EP, 0.0, -1);
        next.legacy.evolution_points -= cost;
        next.legacy.levels[action.target_id]++;
        answer.amount_spent = cost;
        break;
    }
    case ACTION_START_NEW_RUN:
        if (game->phase != PHASE_LEGACY) return result(ACTION_INVALID_PHASE, 0.0, -1);
        game_start_new_run(&next);
        break;
    case ACTION_GENERATE_MUTATION_OFFER:
    case ACTION_CHOOSE_MUTATION:
    case ACTION_SPECIATE:
    case ACTION_BUY_SUB_UPGRADE:
    case ACTION_BUY_CAPACITY:
    case ACTION_BUY_RESERVE: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        if (action.target_species_slot < 0 || action.target_species_slot >= game->run.active_species_count)
            return result(ACTION_INVALID_TARGET, 0.0, -1);
        if (action.type == ACTION_SPECIATE && action.option != 0 && action.option != 1)
            return result(ACTION_INVALID_TARGET, 0.0, -1);
        ActionCode code;
        if (action.type == ACTION_GENERATE_MUTATION_OFFER)
            code = evolution_open_mutation_offer(&next, action.target_species_slot);
        else if (action.type == ACTION_CHOOSE_MUTATION)
            code = evolution_apply_mutation(&next, action.target_species_slot, action.target_id);
        else if (action.type == ACTION_SPECIATE)
            code = evolution_apply_speciation(&next, action.target_species_slot, action.target_id, action.option);
        else if (action.type == ACTION_BUY_SUB_UPGRADE)
            code = evolution_apply_sub_upgrade(&next, action.target_species_slot);
        else if (action.type == ACTION_BUY_CAPACITY)
            code = evolution_apply_capacity_upgrade(&next);
        else code = evolution_apply_reserve(&next);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        answer.amount_spent = (game->run.atp-next.run.atp) + (game->run.biomass-next.run.biomass)
                            + (game->run.dna-next.run.dna) + (game->run.knowledge-next.run.knowledge);
        break;
    }
    default:
        return result(ACTION_INVALID_TARGET, 0.0, -1);
    }
    *game = next;
    return answer;
}
