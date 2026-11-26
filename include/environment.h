#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

/*
 * Member B · 环境事件、适应度与危机
 * 实现见 src/environment.c
 *
 * 每固定步：
 *   1. A 先用 evolution_calculate_species_rates 汇总产率并写回资源与种群；
 *   2. A 调用 environment_calculate_step 计算环境计时与事件切换；
 *   3. A 调用 environment_apply_step 一次性写回并应用即时损失与保护标记。
 */

#include "game_types.h"

const char *environment_name(int environment_id);
const char *environment_description(int environment_id);
bool environment_is_crisis(int environment_id);
double environment_base_duration(int environment_id);
int environment_damage_trait(int environment_id);      /* 返回 EnvironmentTrait 或 -1 */
int environment_difficulty(int environment_id);
double environment_damage_per_second(int environment_id);

/* A 在 game_init / game_start_new_run 之后调用，设置首次事件计时并预抽一个事件 */
void environment_init_run(GameState *game);

EnvironmentStep environment_calculate_step(const GameState *game, double dt);

/* 写回计时、事件切换、危机统计、即时种群损失与 Lv.5 保护标记 */
void environment_apply_step(GameState *game, const EnvironmentStep *step);

/* 指定物种槽在当前事件中的适应度倍率（已包含 Adaptation Lv.1 / Lv.4） */
double environment_multiplier_for_slot(const GameState *game, int species_slot);

/* Adaptation Lv.3：当前是否允许显示下一事件预览 */
bool environment_preview_available(const GameState *game);

#endif
