/*
 * game.c  ——  Member A · 控制层（集成版）
 *
 * 职责边界（01 §1）：
 *   - A 负责时钟、固定步、动作分发、原子提交与存档时机。
 *   - 所有规则与扣款由 B 的 *_apply_* 一次性提交函数完成，A 不重复计算成本。
 *   - 任何一步失败都丢弃候选状态（GameState next），不改动调用者的 game。
 *
 * 与单独开发版的差别：
 *   - 突变 / 分化 / 子升级 / 容量 / 储备 / 结算全部接 B 的真实实现，
 *     不再返回 ACTION_REQUIREMENT_NOT_MET。
 *   - 环境走 environment_calculate_step + environment_apply_step，
 *     A 不再手写事件字段。
 *   - 删除了 highest_biomass_milestone 缓存，里程碑统一由 B 从 max_biomass 派生。
 */
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
    memset(&game->run, 0, sizeof game->run);
    game->run.active_species_count = 1;
    game->run.active_species[0].species_id = SPECIES_PROTOCELL;
    game->run.active_species[0].is_main_lineage = true;
    game->run.active_species[0].population = STARTING_POPULATION;
    game->run.discovered_species[SPECIES_PROTOCELL] = true;
    game->run.current_environment_id = ENV_NORMAL_OCEAN;
    game->run.next_environment_id = -1;
    game->run.rng_state = seed ? seed : 1u;

    game->legacy.codex_species[SPECIES_PROTOCELL] = true;
    game->phase = PHASE_RUNNING;

    /* B 负责首次事件计时与预抽下一事件（会消耗 rng_state，故必须在设种子之后）。 */
    environment_init_run(game);
    /* B 负责 Metabolism Lv.3 / Lv.7 的新轮资源与等级奖励。 */
    legacy_apply_new_run_bonuses(game);

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
    if (!isfinite(atp) || !isfinite(biomass) || !isfinite(dna) || !isfinite(knowledge))
        return false;
    next.run.atp = fmax(0.0, next.run.atp + atp);
    next.run.biomass = fmax(0.0, next.run.biomass + biomass);
    next.run.dna = fmax(0.0, next.run.dna + dna);
    next.run.knowledge = fmax(0.0, next.run.knowledge + knowledge);
    next.run.elapsed_time += FIXED_STEP_SECONDS;
    if (!isfinite(next.run.atp) || !isfinite(next.run.biomass)
        || !isfinite(next.run.dna) || !isfinite(next.run.knowledge)
        || !isfinite(next.run.elapsed_time)) return false;
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
        || !isfinite(environment.instant_population_loss_ratio)
        || environment.instant_population_loss_ratio < 0.0
        || environment.instant_population_loss_ratio > 1.0
        || (environment.event_ended && (environment.ended_environment_id <= 0
            || environment.ended_environment_id >= MAX_ENVIRONMENTS))) return false;

    /* 计时、事件切换、危机统计、即时损失、保护标记全部由 B 一次写回。 */
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
        if (!isfinite(cost)) return result(ACTION_LEVEL_MAXED, 0.0, -1);
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
        /* B 负责 reward_claimed 检查、EP 入账、最佳记录与阶段切换。 */
        ActionCode code = prestige_commit_end_era(&next);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        break;
    }

    case ACTION_BUY_LEGACY: {
        if (game->phase != PHASE_LEGACY) return result(ACTION_INVALID_PHASE, 0.0, -1);
        if (action.target_id < LEGACY_METABOLISM || action.target_id > LEGACY_DIVERSIFICATION)
            return result(ACTION_INVALID_TARGET, 0.0, -1);
        int level = game->legacy.levels[action.target_id];
        int cost = legacy_upgrade_cost(level);
        /* B 负责前置校验、EP 扣款与等级递增。 */
        ActionCode code = legacy_apply_upgrade(&next, (LegacyBranch)action.target_id);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        answer.amount_spent = cost;
        break;
    }

    case ACTION_START_NEW_RUN:
        if (game->phase != PHASE_LEGACY) return result(ACTION_INVALID_PHASE, 0.0, -1);
        game_start_new_run(&next);
        break;

    case ACTION_GENERATE_MUTATION_OFFER: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        /* B 负责候选生成、随机数推进与“同槽已有候选则原样返回”。 */
        ActionCode code = evolution_open_mutation_offer(&next, action.target_species_slot);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        break;
    }

    case ACTION_CHOOSE_MUTATION: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        double cost = evolution_mutation_cost_for_slot(game, action.target_species_slot,
                                                       action.target_id);
        /* B 负责候选归属校验、DNA 扣款、突变写入与候选清空。 */
        ActionCode code = evolution_apply_mutation(&next, action.target_species_slot,
                                                   action.target_id);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        answer.amount_spent = cost;
        break;
    }

    case ACTION_SPECIATE: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        double cost = evolution_speciation_cost(game, action.target_id);
        /* B 负责前置校验、槽位分配、种群分摊与 Biomass 扣款。 */
        ActionCode code = evolution_apply_speciation(&next, action.target_species_slot,
                                                     action.target_id, action.option);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        answer.amount_spent = cost;
        break;
    }

    case ACTION_BUY_SUB_UPGRADE: {
        if (game->phase != PHASE_RUNNING) return result(ACTION_INVALID_PHASE, 0.0, -1);
        /* B 负责理念扣款、层数与乘数推进。 */
        ActionCode code = evolution_apply_sub_upgrade(&next, action.target_species_slot);
        if (code != ACTION_OK) return result(code, 0.0, -1);
        break;
    }

    default:
        return result(ACTION_INVALID_TARGET, 0.0, -1);
    }
    *game = next;
    return answer;
}
