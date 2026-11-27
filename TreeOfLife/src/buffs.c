/*
 * Member B · buffs.c
 * 随机增益因子：刷新、收集、限时倍率与一次性资源包。
 *
 * 随机数统一走 run.rng_state（02 §11 的 tof_rng_next），
 * 因此同一随机种子下增益序列完全可复现。
 */

#include "buffs.h"
#include "balance.h"
#include "evolution.h"

static double spawn_interval(RunState *run)
{
    unsigned int roll = tof_rng_next(&run->rng_state);
    double frac = (double)(roll % 1000u) / 1000.0;
    return BUFF_SPAWN_MIN + frac * (BUFF_SPAWN_MAX - BUFF_SPAWN_MIN);
}

/* 100 权重：稀有 10 / 点击 25 / 资源包 25 / 普通产率 40 */
static int roll_buff_type(RunState *run)
{
    unsigned int roll = tof_rng_next(&run->rng_state) % 100u;
    if (roll < (unsigned int)BUFF_WEIGHT_RARE) {
        bool rare_ready = run->buff_clock - run->buff_last_rare_time >= BUFF_RARE_COOLDOWN;
        return rare_ready ? BUFF_PRODUCTION_STRONG : BUFF_PRODUCTION;
    }
    roll -= (unsigned int)BUFF_WEIGHT_RARE;
    if (roll < (unsigned int)BUFF_WEIGHT_CLICK) {
        return BUFF_CLICK;
    }
    roll -= (unsigned int)BUFF_WEIGHT_CLICK;
    if (roll < (unsigned int)BUFF_WEIGHT_INSTANT) {
        return BUFF_INSTANT;
    }
    return BUFF_PRODUCTION;
}

static double active_duration(int buff_type)
{
    switch (buff_type) {
    case BUFF_PRODUCTION:
        return BUFF_PRODUCTION_DURATION;
    case BUFF_PRODUCTION_STRONG:
        return BUFF_STRONG_DURATION;
    case BUFF_CLICK:
        return BUFF_CLICK_DURATION;
    default:
        return 0.0;
    }
}

void buffs_init_run(GameState *game)
{
    RunState *run = &game->run;
    run->buff_visible = false;
    run->buff_visible_type = BUFF_PRODUCTION;
    run->buff_visible_icon = BUFF_ICON_CELL;
    run->buff_active_type = BUFF_PRODUCTION;
    run->buff_active_time_left = 0.0;
    run->buff_clock = 0.0;
    run->buff_last_rare_time = 0.0;
    run->buff_total_active_time = 0.0;
    run->buff_collected = 0;
    run->buff_rare_collected = 0;
    run->buff_spawn_timer = spawn_interval(run);
}

void buffs_update(GameState *game, double dt)
{
    if (!game || game->phase != PHASE_RUNNING || dt <= 0.0) {
        return;
    }
    RunState *run = &game->run;
    run->buff_clock += dt;

    if (run->buff_active_time_left > 0.0) {
        double tick = dt < run->buff_active_time_left ? dt : run->buff_active_time_left;
        run->buff_active_time_left -= tick;
        run->buff_total_active_time += tick;
        if (run->buff_active_time_left <= 0.0) {
            run->buff_active_time_left = 0.0;
        }
    }

    /* 页面上已有因子时暂停刷新，保证最多同时 1 个 */
    if (run->buff_visible) {
        return;
    }
    run->buff_spawn_timer -= dt;
    if (run->buff_spawn_timer > 0.0) {
        return;
    }
    run->buff_visible = true;
    run->buff_visible_type = roll_buff_type(run);
    run->buff_visible_icon =
        (int)(tof_rng_next(&run->rng_state) % (unsigned int)BUFF_ICON_COUNT);
    run->buff_spawn_timer = 0.0;
}

ActionCode buffs_collect(GameState *game)
{
    if (!game) {
        return ACTION_INVALID_TARGET;
    }
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    RunState *run = &game->run;
    if (!run->buff_visible) {
        return ACTION_INVALID_TARGET;
    }

    int type = run->buff_visible_type;
    if (type == BUFF_INSTANT) {
        double atp_rate = 0.0;
        double biomass_rate = 0.0;
        double dna_rate = 0.0;
        double knowledge_rate = 0.0;
        for (int i = 0; i < run->active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
            SpeciesRates rates = evolution_calculate_species_rates(game, i);
            atp_rate += rates.atp_per_second;
            biomass_rate += rates.biomass_per_second;
            dna_rate += rates.dna_per_second;
            knowledge_rate += rates.knowledge_per_second;
        }
        run->atp += atp_rate * BUFF_INSTANT_ATP_SECONDS;
        run->biomass += biomass_rate * BUFF_INSTANT_BIOMASS_SECONDS;
        run->dna += dna_rate * BUFF_INSTANT_DNA_SECONDS;
        run->knowledge += knowledge_rate * BUFF_INSTANT_KNOWLEDGE_SECONDS;
    } else {
        /* 新限时增益覆盖旧增益，避免叠乘失控 */
        run->buff_active_type = type;
        run->buff_active_time_left = active_duration(type);
        if (type == BUFF_PRODUCTION_STRONG) {
            run->buff_last_rare_time = run->buff_clock;
        }
    }

    run->buff_visible = false;
    ++run->buff_collected;
    if (type == BUFF_PRODUCTION_STRONG) {
        ++run->buff_rare_collected;
    }
    run->buff_spawn_timer = spawn_interval(run);
    return ACTION_OK;
}

double buffs_production_multiplier(const GameState *game)
{
    const RunState *run = &game->run;
    if (run->buff_active_time_left <= 0.0) {
        return 1.0;
    }
    if (run->buff_active_type == BUFF_PRODUCTION) {
        return BUFF_PRODUCTION_MULT;
    }
    if (run->buff_active_type == BUFF_PRODUCTION_STRONG) {
        return BUFF_STRONG_MULT;
    }
    return 1.0;
}

double buffs_click_multiplier(const GameState *game)
{
    const RunState *run = &game->run;
    if (run->buff_active_time_left > 0.0 && run->buff_active_type == BUFF_CLICK) {
        return BUFF_CLICK_MULT;
    }
    return 1.0;
}

bool buffs_factor_visible(const GameState *game)
{
    return game->run.buff_visible;
}

const char *buffs_type_name(int buff_type)
{
    switch (buff_type) {
    case BUFF_PRODUCTION:
        return "Resource Bloom";
    case BUFF_PRODUCTION_STRONG:
        return "Hyper Bloom";
    case BUFF_CLICK:
        return "Click Surge";
    case BUFF_INSTANT:
        return "Nutrient Cache";
    default:
        return "None";
    }
}

const char *buffs_icon_name(int icon)
{
    switch (icon) {
    case BUFF_ICON_CELL:
        return "Cell";
    case BUFF_ICON_DNA:
        return "Glowing DNA";
    case BUFF_ICON_VESICLE:
        return "Vesicle";
    case BUFF_ICON_SPORE:
        return "Spore";
    default:
        return "Cell";
    }
}
