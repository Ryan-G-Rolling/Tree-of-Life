#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "economy.h"
#include "game.h"
#include "save.h"

static GameAction action(ActionType type, int target_id)
{
    GameAction value = { type, 0, target_id, 0 };
    return value;
}

static void close_to(double actual, double expected)
{
    assert(fabs(actual - expected) < 1e-7);
}

static void test_economy_and_actions(void)
{
    GameState game;
    game_init(&game);
    game.run.rng_state = 123u;
    assert(game.phase == PHASE_RUNNING);
    assert(game.run.active_species_count == 1);
    close_to(game.run.active_species[0].population, 10.0);
    close_to(economy_upgrade_cost(UPGRADE_METABOLISM, 0), 20.0);
    close_to(economy_upgrade_cost(UPGRADE_REPRODUCTION, 0), 35.0);
    close_to(economy_upgrade_cost(UPGRADE_GENOME, 0), 25.0);

    GameState before = game;
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, UPGRADE_METABOLISM)).code
           == ACTION_NOT_ENOUGH_ATP);
    assert(memcmp(&before, &game, sizeof game) == 0);
    for (int i = 0; i < 20; ++i)
        assert(game_apply_action(&game, action(ACTION_CLICK_ATP, 0)).code == ACTION_OK);
    close_to(game.run.atp, 20.0);
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, UPGRADE_METABOLISM)).code
           == ACTION_OK);
    close_to(game.run.atp, 0.0);
    close_to(economy_click_atp(&game), 1.25);
    assert(game_apply_action(&game, action(ACTION_BUY_RUN_UPGRADE, 77)).code
           == ACTION_INVALID_TARGET);
    close_to(game.run.atp, 0.0);

    game_update(&game, 0.05);
    close_to(game.run.elapsed_time, 0.0);
    game_update(&game, 0.95);
    close_to(game.run.elapsed_time, 1.0);
    close_to(game.run.atp, 1.0);
    close_to(game.run.biomass, 0.5);
    close_to(game.run.dna, 0.2);
    close_to(game.run.active_species[0].population, 12.0);
    game_update(&game, -1.0);
    close_to(game.run.elapsed_time, 1.0);

    game.run.active_species_count = 2;
    game.run.active_species[1].species_id = 1;
    game.run.active_species[1].population = 10.0;
    game_update(&game, 1.0);
    close_to(game.run.atp, 4.0);
    close_to(game.run.biomass, 2.0);
    close_to(game.run.dna, 0.8);
    close_to(game.run.active_species[0].population, 14.0);
    close_to(game.run.active_species[1].population, 12.0);
    assert(economy_biomass_milestone(500.0) == 2);
}

static void test_era_and_save(void)
{
    const char *path = "tests/test-save.bin";
    GameState game;
    game_init(&game);
    game.run.rng_state = 777u;
    game.run.era = 1;
    game.run.biomass = 100.0;
    game.run.max_biomass = 100.0;
    game.run.highest_biomass_milestone = 1;
    game.run.discovered_species[1] = true;
    game.legacy.codex_species[1] = true;
    game.run.mutation_offer.active = true;
    game.run.mutation_offer.species_slot = 0;
    game.run.mutation_offer.choice_count = 1;
    game.run.mutation_offer.mutation_ids[0] = 1;
    game.run.mutation_offer.dna_costs[0] = 18.0;
    GameState same_offer = game;
    assert(game_apply_action(&game, action(ACTION_GENERATE_MUTATION_OFFER, 0)).code == ACTION_OK);
    assert(memcmp(&same_offer, &game, sizeof game) == 0);

    assert(game_apply_action(&game, action(ACTION_OPEN_END_ERA, 0)).code == ACTION_OK);
    GameState preview = game;
    game_update(&game, 1.0);
    assert(memcmp(&preview, &game, sizeof game) == 0);
    assert(game_apply_action(&game, action(ACTION_CANCEL_END_ERA, 0)).code == ACTION_OK);
    assert(game_apply_action(&game, action(ACTION_OPEN_END_ERA, 0)).code == ACTION_OK);
    assert(game_apply_action(&game, action(ACTION_CONFIRM_END_ERA, 0)).code == ACTION_OK);
    assert(game.phase == PHASE_LEGACY && game.run.reward_claimed);
    assert(game.legacy.evolution_points == 3);
    GameState claimed = game;
    assert(game_apply_action(&game, action(ACTION_CONFIRM_END_ERA, 0)).code != ACTION_OK);
    assert(memcmp(&claimed, &game, sizeof game) == 0);
    assert(save_write(path, &game) == SAVE_OK);

    GameState loaded;
    game_init(&loaded);
    assert(save_read(path, &loaded) == SAVE_OK);
    assert(loaded.phase == PHASE_LEGACY && loaded.run.reward_claimed);
    assert(loaded.run.rng_state == 777u);
    assert(loaded.run.mutation_offer.mutation_ids[0] == 1);
    assert(game_apply_action(&loaded, action(ACTION_CONFIRM_END_ERA, 0)).code != ACTION_OK);
    assert(game_apply_action(&loaded, action(ACTION_BUY_LEGACY, LEGACY_METABOLISM)).code == ACTION_OK);
    assert(loaded.legacy.evolution_points == 2);
    assert(save_write(path, &loaded) == SAVE_OK); /* Replace existing save safely. */
    assert(game_apply_action(&loaded, action(ACTION_START_NEW_RUN, 0)).code == ACTION_OK);
    close_to(loaded.run.atp, 0.0);
    close_to(loaded.run.biomass, 0.0);
    assert(!loaded.run.mutation_offer.active);
    assert(loaded.legacy.codex_species[1]);
    assert(loaded.legacy.levels[LEGACY_METABOLISM] == 1);
    close_to(economy_click_atp(&loaded), 1.08);

    /* A corrupt header is rejected without touching the existing game. */
    FILE *file = fopen(path, "r+b");
    assert(file);
    assert(fseek(file, 8, SEEK_SET) == 0);
    unsigned int bad_version = 99;
    assert(fwrite(&bad_version, sizeof bad_version, 1, file) == 1);
    assert(fclose(file) == 0);
    GameState intact = loaded;
    assert(save_read(path, &loaded) == SAVE_BAD_VERSION);
    assert(memcmp(&intact, &loaded, sizeof loaded) == 0);
    file = fopen(path, "wb");
    assert(file);
    assert(fwrite("TOL", 1, 3, file) == 3);
    assert(fclose(file) == 0);
    assert(save_read(path, &loaded) == SAVE_INVALID_DATA);
    assert(memcmp(&intact, &loaded, sizeof loaded) == 0);
    assert(remove(path) == 0);
}

static void test_load_discards_partial_frame(void)
{
    const char *path = "tests/test-save.bin";
    GameState game;
    game_init(&game);
    game_update(&game, 0.05);
    assert(save_write(path, &game) == SAVE_OK);
    assert(save_read(path, &game) == SAVE_OK);
    game_update(&game, 0.05);
    close_to(game.run.elapsed_time, 0.0);
    game_update(&game, 0.05);
    close_to(game.run.elapsed_time, 0.1);
    assert(remove(path) == 0);
}

int main(void)
{
    test_economy_and_actions();
    test_era_and_save();
    test_load_discards_partial_frame();
    puts("Member A tests passed");
    return 0;
}
