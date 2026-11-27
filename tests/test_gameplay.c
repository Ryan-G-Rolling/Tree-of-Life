/* Regressions through the production controller, including save continuations. */
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "buffs.h"
#include "economy.h"
#include "evolution.h"
#include "environment.h"
#include "save.h"
#include "balance.h"

static ActionResult act(GameState *game, ActionType type)
{
    return game_apply_action(game, (GameAction){ .type = type });
}

static void seeded(GameState *game)
{
    game_init(game);
    game->run.rng_state = 42u;
    environment_init_run(game);
    buffs_init_run(game);
}

static void test_spawn_and_expiration(void)
{
    GameState game;
    seeded(&game);
    assert(game.run.buff_spawn_timer >= BUFF_SPAWN_MIN);
    assert(game.run.buff_spawn_timer <= BUFF_SPAWN_MAX);
    game_update(&game, BUFF_SPAWN_MAX + 1.0);
    assert(game.run.buff_visible && game.run.buff_clock > BUFF_SPAWN_MAX);
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_OK);
    assert(!game.run.buff_visible && game.run.buff_collected == 1);
    GameState before = game;
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_INVALID_TARGET);
    assert(memcmp(&before, &game, sizeof game) == 0);

    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_CLICK;
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_OK);
    double base = economy_click_atp(&game) / BUFF_CLICK_MULT;
    double atp = game.run.atp;
    assert(act(&game, ACTION_CLICK_ATP).code == ACTION_OK);
    assert(fabs(game.run.atp - atp - base * BUFF_CLICK_MULT) < 1e-8);
    assert(act(&game, ACTION_OPEN_END_ERA).code == ACTION_OK);
    before = game;
    game_update(&game, 100.0);
    assert(memcmp(&before, &game, sizeof game) == 0);
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_INVALID_PHASE);
    assert(act(&game, ACTION_BUY_INSIGHT).code == ACTION_INVALID_PHASE);
    assert(act(&game, ACTION_CANCEL_END_ERA).code == ACTION_OK);
    game_update(&game, BUFF_CLICK_DURATION + 1.0);
    assert(game.run.buff_active_time_left == 0.0);
    assert(fabs(economy_click_atp(&game) - base) < 1e-8);

    assert(act(&game, ACTION_OPEN_END_ERA).code == ACTION_OK);
    assert(act(&game, ACTION_CONFIRM_END_ERA).code == ACTION_OK);
    assert(act(&game, ACTION_START_NEW_RUN).code == ACTION_OK);
    assert(!game.run.buff_visible && game.run.buff_collected == 0);
    assert(game.run.buff_active_time_left == 0.0 && game.run.buff_clock == 0.0);
    assert(game.run.buff_spawn_timer >= BUFF_SPAWN_MIN);
}

static void test_rewards_insight_and_save(void)
{
    GameState game, restored;
    seeded(&game);
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_INSTANT;
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_OK);
    assert(game.run.biomass > 0.0 && game.run.max_biomass >= game.run.biomass);
    assert(save_write("build/gameplay.sav", &game) == SAVE_OK);
    assert(save_read("build/gameplay.sav", &restored) == SAVE_OK);
    assert(memcmp(&game, &restored, sizeof game) == 0);

    GameState before = game;
    assert(act(&game, ACTION_BUY_INSIGHT).code == ACTION_NOT_ENOUGH_KNOWLEDGE);
    assert(memcmp(&before, &game, sizeof game) == 0);
    game.run.knowledge = evolution_insight_cost(&game) + 10.0;
    double cost = evolution_insight_cost(&game);
    double rate = evolution_calculate_species_rates(&game, 0).atp_per_second;
    ActionResult bought = act(&game, ACTION_BUY_INSIGHT);
    assert(bought.code == ACTION_OK && bought.amount_spent == cost);
    assert(game.run.insight_levels == 1 && fabs(game.run.knowledge - 10.0) < 1e-8);
    assert(fabs(evolution_calculate_species_rates(&game, 0).atp_per_second
                / rate - (1.0 + INSIGHT_BONUS_PER_LEVEL)) < 1e-8);
    assert(save_write("build/gameplay.sav", &game) == SAVE_OK);
    assert(save_read("build/gameplay.sav", &restored) == SAVE_OK);
    assert(restored.run.insight_levels == 1);

    game.run.insight_levels = INT_MAX;
    before = game;
    assert(act(&game, ACTION_BUY_INSIGHT).code == ACTION_LEVEL_MAXED);
    assert(memcmp(&before, &game, sizeof game) == 0);
    seeded(&game);
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_INSTANT;
    game.run.atp = DBL_MAX;
    game.run.active_species[0].population = DBL_MAX;
    before = game;
    assert(act(&game, ACTION_COLLECT_BUFF).code != ACTION_OK);
    assert(memcmp(&before, &game, sizeof game) == 0);
}

static void test_save_continuation(void)
{
    GameState game, expected, restored;
    seeded(&game);
    game_update(&game, 90.0);
    game.run.buff_visible_type = BUFF_PRODUCTION;
    assert(act(&game, ACTION_COLLECT_BUFF).code == ACTION_OK);
    assert(save_write("build/gameplay.sav", &game) == SAVE_OK);
    game_update(&game, 17.0);
    expected = game;
    assert(save_read("build/gameplay.sav", &restored) == SAVE_OK);
    for (int i = 0; i < 170; ++i) game_update(&restored, 0.1);
    assert(memcmp(&expected, &restored, sizeof expected) == 0);

    seeded(&game);
    game_update(&game, 120.0);
    expected = game;
    seeded(&game);
    for (int i = 0; i < 1200; ++i) game_update(&game, 0.1);
    assert(memcmp(&expected, &game, sizeof expected) == 0);
}

static void test_invalid_save(void)
{
    GameState valid, invalid, output;
    seeded(&valid);
    invalid = valid;
    invalid.run.buff_active_type = 999;
    assert(save_write("build/gameplay.sav", &invalid) == SAVE_INVALID_DATA);
    invalid = valid;
    memset(&invalid.run.buff_visible, 2, sizeof invalid.run.buff_visible);
    assert(save_write("build/gameplay.sav", &invalid) == SAVE_INVALID_DATA);
    assert(save_write("build/gameplay.sav", &valid) == SAVE_OK);
    FILE *file = fopen("build/gameplay.sav", "r+b");
    assert(file);
    size_t offset = 16 + offsetof(GameState, run) + offsetof(RunState, buff_visible);
    assert(fseek(file, (long)offset, SEEK_SET) == 0);
    assert(fputc(2, file) != EOF);
    assert(fclose(file) == 0);
    output = valid;
    assert(save_read("build/gameplay.sav", &output) == SAVE_INVALID_DATA);
    assert(memcmp(&output, &valid, sizeof valid) == 0);
}

int main(void)
{
    test_spawn_and_expiration();
    test_rewards_insight_and_save();
    test_save_continuation();
    test_invalid_save();
    remove("build/gameplay.sav");
    puts("Gameplay: production actions, buff timing, insight, rollback and save continuation passed");
    return 0;
}
