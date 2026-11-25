#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "economy.h"
#include "rules_stub.h"

static void close_to(double actual, double expected)
{
    assert(fabs(actual - expected) < 1e-7);
}

static GameAction action(ActionType type, int id)
{
    GameAction value = {type, 0, id, 0};
    return value;
}

static void test_step_order_and_chunking(void)
{
    GameState game;
    rules_stub_reset();
    game_init(&game);
    game.run.rng_state = 123u;
    game.run.active_species_count = 2;
    game.run.active_species[1].species_id = 1;
    game.run.active_species[1].population = 10.0;
    game_update(&game, 0.1);
    assert(rules_stub.rate_calls == 2);
    close_to(rules_stub.observed_atp[0], 0.0);
    close_to(rules_stub.observed_atp[1], 0.0);
    close_to(rules_stub.observed_population[1], 10.0);
    close_to(rules_stub.environment_atp, 0.3);
    close_to(rules_stub.environment_population, 10.2);

    game_init(&game);
    game.run.rng_state = 123u;
    game_update(&game, 10.0);
    GameState expected = game;
    game_init(&game);
    game.run.rng_state = 123u;
    for (int i = 0; i < 400; ++i) game_update(&game, 0.025);
    assert(memcmp(&game, &expected, sizeof game) == 0);

    GameState before = game;
    game_update(&game, NAN);
    game_update(&game, INFINITY);
    game_update(&game, -1.0);
    game_update(&game, 0.0);
    assert(memcmp(&game, &before, sizeof game) == 0);
}

static void test_invalid_rules_are_atomic(void)
{
    GameState game;
    rules_stub_reset();
    game_init(&game);
    SpeciesRates rate = {1.0, 0.5, 0.2, 2.0, 1.0, 80.0, false};
    rules_stub.override_rates = true;
    rules_stub.rates[0] = rate;
    GameState before = game;
    rules_stub.rates[0].atp_per_second = NAN;
    game_update(&game, 0.1);
    assert(memcmp(&game, &before, sizeof game) == 0);
    rules_stub.rates[0] = rate;
    rules_stub.rates[0].final_population_cap = INFINITY;
    game_update(&game, 0.1);
    assert(memcmp(&game, &before, sizeof game) == 0);
    rules_stub.rates[0] = rate;
    rules_stub.rates[0].dna_per_second = -1.0;
    game_update(&game, 0.1);
    assert(memcmp(&game, &before, sizeof game) == 0);
    rules_stub.rates[0] = rate;
    rules_stub.override_environment = true;
    rules_stub.environment.current_environment_id = MAX_ENVIRONMENTS;
    game_update(&game, 0.1);
    assert(memcmp(&game, &before, sizeof game) == 0);
    rules_stub.override_environment = false;
    game_update(&game, 0.05);
    assert(memcmp(&game, &before, sizeof game) == 0);
    game_update(&game, 0.05);
    close_to(game.run.elapsed_time, 0.1);

    game.run.atp = DBL_MAX;
    rules_stub.rates[0].atp_per_second = DBL_MAX;
    before = game;
    game_update(&game, 0.1);
    assert(memcmp(&game, &before, sizeof game) == 0);
}

static void test_population_and_environment(void)
{
    GameState game;
    rules_stub_reset();
    game_init(&game);
    rules_stub.override_rates = true;
    rules_stub.rates[0].final_population_cap = 80.0;
    rules_stub.rates[0].population_delta_per_second = -1000.0;
    game_update(&game, 0.1);
    close_to(game.run.active_species[0].population, 1.0);
    rules_stub.rates[0].population_delta_per_second = 1000.0;
    game_update(&game, 0.1);
    close_to(game.run.active_species[0].population, 80.0);
    rules_stub.override_environment = true;
    rules_stub.environment.current_environment_id = 1;
    rules_stub.environment.next_environment_id = -1;
    rules_stub.environment.event_started = true;
    rules_stub.environment.environment_time_left = 45.0;
    game_update(&game, 0.1);
    close_to(game.run.crisis_start_population, 80.0);
    rules_stub.environment.event_started = false;
    rules_stub.environment.event_ended = true;
    rules_stub.environment.ended_environment_id = 1;
    rules_stub.environment.crisis_survived = true;
    rules_stub.environment.current_environment_id = 0;
    game.run.adaptation_shield_active = true;
    game.run.adaptation_shield_used = true;
    game_update(&game, 0.1);
    assert(game.run.survived_crises[1]);
    assert(!game.run.adaptation_shield_active);
    assert(game.run.adaptation_shield_used);
}

static void test_phase_actions_and_new_run(void)
{
    GameState game;
    rules_stub_reset();
    game_init(&game);
    game_update(&game, 0.05);
    assert(game_apply_action(&game, action(ACTION_OPEN_END_ERA, 0)).code == ACTION_OK);
    GameState before = game;
    for (int i = ACTION_CLICK_ATP; i <= ACTION_SPECIATE; ++i) {
        assert(game_apply_action(&game, action((ActionType)i, 0)).code == ACTION_INVALID_PHASE);
        assert(memcmp(&before, &game, sizeof game) == 0);
    }
    game_update(&game, 100.0);
    assert(memcmp(&before, &game, sizeof game) == 0);
    assert(game_apply_action(&game, action(ACTION_CANCEL_END_ERA, 0)).code == ACTION_OK);
    game_update(&game, 0.05);
    close_to(game.run.elapsed_time, 0.1);
    GameAction invalid = {ACTION_GENERATE_MUTATION_OFFER, -1, 0, 0};
    before = game;
    assert(game_apply_action(&game, invalid).code == ACTION_INVALID_TARGET);
    assert(memcmp(&game, &before, sizeof game) == 0);

    game.legacy.evolution_points = INT_MAX;
    game.run.era = 1;
    assert(game_apply_action(&game, action(ACTION_OPEN_END_ERA, 0)).code == ACTION_OK);
    before = game;
    assert(game_apply_action(&game, action(ACTION_CONFIRM_END_ERA, 0)).code == ACTION_REQUIREMENT_NOT_MET);
    assert(memcmp(&game, &before, sizeof game) == 0);
    game.legacy.evolution_points = 0;
    assert(game_apply_action(&game, action(ACTION_CONFIRM_END_ERA, 0)).code == ACTION_OK);
    game.legacy.levels[LEGACY_METABOLISM] = 4;
    game.legacy.codex_mutations[1] = true;
    LegacyState legacy = game.legacy;
    assert(game_apply_action(&game, action(ACTION_START_NEW_RUN, 0)).code == ACTION_OK);
    assert(memcmp(&legacy, &game.legacy, sizeof legacy) == 0);
    close_to(game.run.atp, 50.0);
    close_to(game.run.biomass, 0.0);
    assert(game.run.run_upgrade_levels[UPGRADE_METABOLISM] == 1);
    assert(!game.run.discovered_mutations[1]);
    close_to(economy_click_atp(&game), 1.25 * 1.08 * 1.12);
}

static void test_economy_boundaries(void)
{
    GameState game;
    rules_stub_reset();
    game_init(&game);
    close_to(economy_upgrade_cost(UPGRADE_METABOLISM, 1), 33.0);
    close_to(economy_upgrade_cost(UPGRADE_REPRODUCTION, 1), 59.0);
    close_to(economy_upgrade_cost(UPGRADE_GENOME, 1), 43.0);
    assert(isinf(economy_upgrade_cost(UPGRADE_GENOME, INT_MAX)));
    assert(isinf(economy_upgrade_cost(UPGRADE_GENOME, -1)));
    assert(economy_biomass_milestone(99.999) == 0);
    assert(economy_biomass_milestone(100.0) == 1);
    assert(economy_biomass_milestone(50000.0) == 5);
    assert(economy_biomass_milestone(NAN) == 0);
    close_to(economy_click_atp(NULL), 0.0);

    game.run.biomass = 25.0;
    game.run.max_biomass = 25.0;
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, UPGRADE_GENOME)).code == ACTION_OK);
    close_to(game.run.biomass, 0.0);
    close_to(game.run.max_biomass, 25.0);
    assert(game.run.run_upgrade_levels[UPGRADE_GENOME] == 1);
    GameState before = game;
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, UPGRADE_GENOME)).code == ACTION_NOT_ENOUGH_BIOMASS);
    assert(memcmp(&game, &before, sizeof game) == 0);
    game.run.run_upgrade_levels[UPGRADE_METABOLISM] = INT_MAX;
    before = game;
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, UPGRADE_METABOLISM)).code == ACTION_LEVEL_MAXED);
    assert(memcmp(&game, &before, sizeof game) == 0);

    game.phase = PHASE_LEGACY;
    game.run.reward_claimed = true;
    game.legacy.evolution_points = 15;
    for (int i = 0; i < 5; ++i) {
        ActionResult result = game_apply_action(&game, action(ACTION_BUY_LEGACY, LEGACY_MUTATION));
        assert(result.code == ACTION_OK);
        close_to(result.amount_spent, i + 1.0);
    }
    assert(game.legacy.evolution_points == 0);
    before = game;
    assert(game_apply_action(&game, action(ACTION_BUY_LEGACY, LEGACY_MUTATION)).code == ACTION_LEVEL_MAXED);
    assert(game_apply_action(&game, action(ACTION_BUY_LEGACY, LEGACY_ADAPTATION)).code == ACTION_NOT_ENOUGH_EP);
    assert(memcmp(&game, &before, sizeof game) == 0);
}

int main(void)
{
    test_step_order_and_chunking();
    test_invalid_rules_are_atomic();
    test_population_and_environment();
    test_phase_actions_and_new_run();
    test_economy_boundaries();
    puts("Game boundary tests passed");
    return 0;
}
