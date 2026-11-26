#ifndef LEGACY_H
#define LEGACY_H

/*
 * Member B · EP 结算与永久强化
 * 实现见 src/prestige.c、src/legacy.c
 */

#include "game_types.h"

/* ---------------- EP 结算 ---------------- */
int prestige_biomass_milestone_level(double max_biomass);
/* 输出 E、M、S、C 原始数量，供 End of Era 页面显示；指针可为 NULL */
void prestige_run_stats(const GameState *game, int *stages, int *mutations, int *species,
                        int *crises);
/* 各项字段为该项贡献的 EP，五项之和等于 total_points。
 * B 级里程碑由 B 从 run.max_biomass 派生，因此 A 必须维护 max_biomass
 * （花费 Biomass 不得降低它）。 */
EraReward prestige_preview_reward(const GameState *game);

/* A 在 ACTION_CONFIRM_END_ERA 中调用：检查 reward_claimed 后一次性入账并进入 Legacy */
ActionCode prestige_commit_end_era(GameState *game);

/* ---------------- 永久强化 ---------------- */
int legacy_upgrade_cost(int current_level);          /* 目标等级 = current_level + 1 */
ActionCode legacy_validate_upgrade(const GameState *game, LegacyBranch branch);
ActionCode legacy_apply_upgrade(GameState *game, LegacyBranch branch);

double legacy_atp_multiplier(const LegacyState *legacy);
double legacy_biomass_multiplier(const LegacyState *legacy);
double legacy_dna_multiplier(const LegacyState *legacy);
double legacy_cap_multiplier(const LegacyState *legacy);
/* 第四轮：Lv.5–8 的派生效果 */
double legacy_production_multiplier(const LegacyState *legacy);
double legacy_click_multiplier(const LegacyState *legacy);
double legacy_key_guarantee_ratio(const LegacyState *legacy);
double legacy_crisis_damage_reduction(const LegacyState *legacy);
double legacy_positive_duration_scale(const LegacyState *legacy);
double legacy_shield_damage_reduction(const LegacyState *legacy);
double legacy_synergy_multiplier(const LegacyState *legacy);
double legacy_mutation_discount(const LegacyState *legacy);
int legacy_rare_mutation_weight(const LegacyState *legacy);
bool legacy_synergy_enabled(const LegacyState *legacy);
double legacy_speciation_discount(const LegacyState *legacy);
int legacy_active_species_slots(const LegacyState *legacy);
double legacy_environment_duration_scale(const LegacyState *legacy);
double legacy_negative_multiplier_scale(const LegacyState *legacy);
double legacy_positive_multiplier_scale(const LegacyState *legacy);
bool legacy_preview_enabled(const LegacyState *legacy);
bool legacy_shield_available(const LegacyState *legacy);

/* A 在重置 RunState 之后调用，应用 Metabolism Lv.3 的新轮奖励 */
void legacy_apply_new_run_bonuses(GameState *game);

#endif
