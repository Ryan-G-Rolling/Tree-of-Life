/*
 * Member B · environment.c
 * 环境事件、适应度倍率、自然灾害与危机统计
 *
 * 数据来源：02-数据与计算规则 §7、§8.6
 * 本文件不包含 raylib.h。环境结果的写回统一走 environment_apply_step，
 * 避免 A 和 B 各自实现一遍危机判定与即时损失。
 */

#include "environment.h"
#include "evolution.h"
#include "legacy.h"
#include "balance.h"

#include <math.h>
#include <string.h>

#define TRAIT_NONE (-1)
#define TRAIT_HIGHEST (-2)

typedef struct {
    const char *name;
    const char *description;
    int trait;                 /* TRAIT_NONE、TRAIT_HIGHEST 或 EnvironmentTrait */
    int difficulty;
    double duration;           /* 0 表示常态，持续时间为事件间隔 */
    double damage_per_second;
    int min_stage;
    bool instant_loss;
    bool crisis;
} EnvironmentDef;

static const EnvironmentDef k_environments[ENV_COUNT] = {
    { "常态海洋", "稳定的水体，没有任何选择压力。",
      TRAIT_NONE, 0, 0.0, 0.0, 0, false, false },
    { "全球变暖", "温度上升，耐热物种维持产出。",
      TRAIT_HEAT, 2, 45.0, 0.013, 0, false, true },
    { "全球变冷", "温度下降，耐寒物种维持产出。",
      TRAIT_COLD, 2, 45.0, 0.0135, 0, false, true },
    { "微光期", "光线衰减，光合谱系产出下降。",
      TRAIT_LOW_LIGHT, 2, 50.0, 0.013, 0, false, true },
    { "氧气上升", "氧气涌入水体，有氧谱系受益。",
      TRAIT_OXYGEN, 1, 45.0, 0.015, 0, false, true },
    { "毒素爆发", "毒素扩散，耐毒物种存活。",
      TRAIT_TOXIN, 2, 40.0, 0.0145, 0, false, true },
    { "陨石撞击", "天空燃烧，所有谱系同时损失种群。",
      TRAIT_HIGHEST, 3, 30.0, 0.0245, SPECIATION_METEOR_MIN_STAGE, true, true },
    /* 第四轮：正面 / 中性随机事件，damage = 0 且不计入危机统计 */
    { "养分上涌", "富养洋流在一段时间内滋养所有谱系。",
      TRAIT_NONE, 0, 40.0, 0.0, 0, false, false },
    { "热泉涌动", "热泉奖励已适应温暖的物种。",
      TRAIT_HEAT, 0, 35.0, 0.0, 0, false, false },
    { "孢子云", "漂流的基因库偏向耐毒谱系。",
      TRAIT_TOXIN, 0, 35.0, 0.0, 0, false, false },
    /* 第六轮：按特征维度区分的正面事件 */
    { "氧气脉冲", "氧气骤增，有氧谱系受益。",
      TRAIT_OXYGEN, 0, 40.0, 0.0, 0, false, false },
    { "寒流", "寒潮奖励适应低温的物种。",
      TRAIT_COLD, 0, 40.0, 0.0, 0, false, false },
    { "长夏", "持续高温重赏耐热专精。",
      TRAIT_HEAT, 0, 50.0, 0.0, 0, false, false },
    /* 第七轮：阶段 5 之后的工业污染危机 */
    { "工业雾霾", "膨胀的技术圈用颗粒物窒息水体。",
      TRAIT_TOXIN, 3, 45.0, 0.026, 5, false, true },
};

static bool valid_environment(int environment_id)
{
    return environment_id >= 0 && environment_id < ENV_COUNT;
}

const char *environment_name(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].name : "Unknown";
}

const char *environment_description(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].description : "";
}

bool environment_is_crisis(int environment_id)
{
    return valid_environment(environment_id) && k_environments[environment_id].crisis;
}

double environment_base_duration(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].duration : 0.0;
}

int environment_damage_trait(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].trait : TRAIT_NONE;
}

int environment_difficulty(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].difficulty : 0;
}

double environment_damage_per_second(int environment_id)
{
    return valid_environment(environment_id) ? k_environments[environment_id].damage_per_second
                                             : 0.0;
}

static int species_event_score(const GameState *game, int species_slot, int environment_id)
{
    int trait = environment_damage_trait(environment_id);
    if (trait == TRAIT_NONE) {
        return 0;
    }
    if (trait == TRAIT_HIGHEST) {
        int best = 0;
        for (int t = 0; t < TRAIT_COUNT; ++t) {
            int value = evolution_slot_trait_total(game, species_slot, (EnvironmentTrait)t);
            if (value > best) {
                best = value;
            }
        }
        return best;
    }
    return evolution_slot_trait_total(game, species_slot, (EnvironmentTrait)trait);
}

static double raw_multiplier(int score, int difficulty)
{
    if (score >= difficulty + 1) {
        return 1.25;
    }
    if (score == difficulty) {
        return 1.10;
    }
    if (score == difficulty - 1) {
        return 0.85;
    }
    return 0.60;
}

double environment_multiplier_for_slot(const GameState *game, int species_slot)
{
    int environment_id = game->run.current_environment_id;
    if (!valid_environment(environment_id) || environment_id == ENV_NORMAL_OCEAN) {
        return 1.0;
    }
    double multiplier = raw_multiplier(species_event_score(game, species_slot, environment_id),
                                       environment_difficulty(environment_id));
    if (multiplier < 1.0) {
        multiplier = 1.0 - (1.0 - multiplier) * legacy_negative_multiplier_scale(&game->legacy);
        /* 第十一轮：危机期间再压低劣势方产率，让“事件中还靠复利上涨”不再无代价。
         * 只作用于被压制的一方（<1.0），适应良好的物种不受额外惩罚。 */
        if (environment_damage_per_second(environment_id) > 0.0) {
            multiplier *= CRISIS_EVENT_PRODUCTION_SCALE;
        }
    } else if (multiplier > 1.0) {
        multiplier = 1.0 + (multiplier - 1.0) * legacy_positive_multiplier_scale(&game->legacy);
    }
    return multiplier;
}

bool environment_preview_available(const GameState *game)
{
    return legacy_preview_enabled(&game->legacy) &&
           game->run.current_environment_id == ENV_NORMAL_OCEAN &&
           valid_environment(game->run.next_environment_id) &&
           game->run.event_interval_left <= ADAPTATION_PREVIEW_SECONDS;
}

static double main_lineage_population(const GameState *game)
{
    int slot = evolution_main_slot(game);
    return slot >= 0 ? game->run.active_species[slot].population : 0.0;
}

/* 单个随机数抽取，排除上一个事件与阶段未解锁的事件。
 * 第三轮：危机轮转——优先抽取本纪元尚未通过的危机类型，
 * 避免同一类型反复出现导致危机种类 C 长期停在 5。 */
static int pick_next_environment(const GameState *game, int exclude_id, int pending_survived_id,
                                 unsigned int *rng_state)
{
    int candidates[ENV_COUNT];
    int count = 0;
    int stage = evolution_main_stage(game);
    for (int id = 1; id < ENV_COUNT; ++id) {
        if (id == exclude_id) {
            continue;
        }
        if (k_environments[id].min_stage > stage) {
            continue;
        }
        candidates[count++] = id;
    }
    if (count == 0) {
        return ENV_NORMAL_OCEAN;
    }
    int boons[ENV_COUNT];
    int boon_count = 0;
    int unpassed[ENV_COUNT];
    int unpassed_count = 0;
    for (int i = 0; i < count; ++i) {
        int id = candidates[i];
        if (!environment_is_crisis(id)) {
            boons[boon_count++] = id;
            continue;
        }
        if (game->run.survived_crises[id] || id == pending_survived_id) {
            continue;
        }
        unpassed[unpassed_count++] = id;
    }
    /* 约每 4 次事件插入一次正面事件；其余仍优先补齐未通过的危机类型，
     * 保证危机种类 C 与 EP 不被稀释。 */
    unsigned int roll = tof_rng_next(rng_state);
    if (boon_count > 0 && (roll % 4u) == 0u) {
        return boons[roll % (unsigned int)boon_count];
    }
    if (unpassed_count > 0) {
        return unpassed[roll % (unsigned int)unpassed_count];
    }
    return candidates[roll % (unsigned int)count];
}

void environment_init_run(GameState *game)
{
    RunState *run = &game->run;
    run->current_environment_id = ENV_NORMAL_OCEAN;
    run->environment_time_left = 0.0;
    run->event_interval_left = FIRST_EVENT_DELAY;
    run->crisis_start_population = 0.0;
    run->crisis_min_population = 0.0;
    run->adaptation_shield_active = false;
    run->adaptation_shield_used = false;
    run->next_environment_id = pick_next_environment(game, -1, -1, &run->rng_state);
}

EnvironmentStep environment_calculate_step(const GameState *game, double dt)
{
    EnvironmentStep step;
    memset(&step, 0, sizeof(step));
    step.ended_environment_id = -1;
    step.current_environment_id = game->run.current_environment_id;
    step.next_environment_id = game->run.next_environment_id;
    step.environment_time_left = game->run.environment_time_left;
    step.event_interval_left = game->run.event_interval_left;

    int current = game->run.current_environment_id;
    if (valid_environment(current) && current != ENV_NORMAL_OCEAN) {
        double remaining = game->run.environment_time_left - dt;
        if (remaining <= 0.0) {
            step.event_ended = true;
            step.ended_environment_id = current;
            if (environment_is_crisis(current)) {
                /* 第十二轮：通过线按纪元区分，避免开局就被 0.70 打穿低尾 */
                double pass_ratio = game->run.era <= 1 ? CRISIS_PASS_RATIO_ERA1
                                                       : CRISIS_PASS_POPULATION_RATIO;
                double threshold =
                    fmax(MIN_ACTIVE_POPULATION,
                         game->run.crisis_start_population * pass_ratio);
                /* 第十一轮：判定口径改为“事件期间最低人口”，而不是结束瞬间的人口 */
                double lowest = game->run.crisis_min_population > 0.0
                                    ? game->run.crisis_min_population
                                    : game->run.crisis_start_population;
                double now = main_lineage_population(game);
                if (now < lowest) {
                    lowest = now;
                }
                step.crisis_lowest_population = lowest;
                step.crisis_survived = lowest >= threshold;
            }
            step.current_environment_id = ENV_NORMAL_OCEAN;
            step.environment_time_left = 0.0;
            step.event_interval_left = EVENT_INTERVAL;
            /* 下一事件只在 environment_apply_step 抽取一次，避免算两次、写一次 */
            step.next_environment_id = game->run.next_environment_id;
        } else {
            step.environment_time_left = remaining;
            if (environment_is_crisis(current) && legacy_shield_available(&game->legacy) &&
                !game->run.adaptation_shield_used && game->run.crisis_start_population > 0.0 &&
                main_lineage_population(game) <= game->run.crisis_start_population *
                                                    CRISIS_SHIELD_POPULATION_RATIO) {
                step.adaptation_shield_triggered = true;
            }
        }
        return step;
    }

    double remaining = game->run.event_interval_left - dt;
    bool has_next = valid_environment(game->run.next_environment_id) &&
                    game->run.next_environment_id != ENV_NORMAL_OCEAN;
    if (remaining <= 0.0 && has_next) {
        int started = game->run.next_environment_id;
        step.event_started = true;
        step.current_environment_id = started;
        double duration_scale = EVENT_DURATION_SCALE;
        if (!environment_is_crisis(started)) {
            duration_scale *= legacy_positive_duration_scale(&game->legacy);
        }
        step.environment_time_left = environment_base_duration(started) *
                                     legacy_environment_duration_scale(&game->legacy) *
                                     duration_scale;
        step.event_interval_left = 0.0;
        step.next_environment_id = -1;
        if (k_environments[started].instant_loss) {
            /* 第十一轮：一次性损失随阶段放大（0.35 + 0.05×stage），应急储备按级减免 */
            int stage = evolution_main_stage(game);
            double ratio = METEOR_INSTANT_LOSS_RATIO + INSTANT_LOSS_STAGE_SCALE * (double)stage;
            if (ratio > INSTANT_LOSS_MAX) {
                ratio = INSTANT_LOSS_MAX;
            }
            double reserve_scale = 1.0 - RESERVE_REDUCTION_PER_LEVEL *
                                             (double)game->run.reserve_levels;
            if (reserve_scale < 0.5) {
                reserve_scale = 0.5;
            }
            step.instant_population_loss_ratio = ratio * reserve_scale;
        }
    } else {
        step.event_interval_left = remaining > 0.0 ? remaining : 0.0;
    }
    return step;
}

void environment_apply_step(GameState *game, const EnvironmentStep *step)
{
    RunState *run = &game->run;

    if (step->event_ended) {
        int ended = step->ended_environment_id;
        if (step->crisis_survived && valid_environment(ended) && ended < MAX_ENVIRONMENTS) {
            run->survived_crises[ended] = true;
        }
        if (run->adaptation_shield_active) {
            run->adaptation_shield_active = false;
        }
        run->current_environment_id = ENV_NORMAL_OCEAN;
        run->environment_time_left = 0.0;
        run->event_interval_left = EVENT_INTERVAL;
        run->crisis_min_population = 0.0;
        run->next_environment_id = pick_next_environment(game, ended, -1, &run->rng_state);
    } else if (step->event_started) {
        int started = step->current_environment_id;
        run->crisis_start_population = main_lineage_population(game);
        run->current_environment_id = started;
        run->environment_time_left = step->environment_time_left;
        run->event_interval_left = 0.0;
        run->next_environment_id = -1;
        if (step->instant_population_loss_ratio > 0.0) {
            for (int i = 0; i < run->active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
                double population = run->active_species[i].population *
                                    (1.0 - step->instant_population_loss_ratio);
                if (population < MIN_ACTIVE_POPULATION) {
                    population = MIN_ACTIVE_POPULATION;
                }
                run->active_species[i].population = population;
            }
        }
        /* 最低人口从即时损失后的值开始记录 */
        run->crisis_min_population = main_lineage_population(game);
    } else {
        run->environment_time_left = step->environment_time_left;
        run->event_interval_left = step->event_interval_left;
        /* 第十一轮：事件期间逐帧记录最低人口，作为危机判定依据 */
        if (valid_environment(run->current_environment_id) &&
            run->current_environment_id != ENV_NORMAL_OCEAN) {
            double now = main_lineage_population(game);
            if (run->crisis_min_population <= 0.0 || now < run->crisis_min_population) {
                run->crisis_min_population = now;
            }
        }
    }

    if (step->adaptation_shield_triggered) {
        run->adaptation_shield_active = true;
        run->adaptation_shield_used = true;
    }
}
