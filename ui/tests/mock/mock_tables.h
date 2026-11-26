/*
 * mock_tables.h  ——  临时平衡数据（仅 Member C 并行开发用）
 *
 * 数据的唯一权威来源是 02-数据与计算规则。这里只是把它抄成表，
 * 让界面在没有 A/B 代码时也能显示真实文字与数字。
 *
 * 集成时：整个 tests/mock 目录从编译中移除，只剩 ui_adapter.c 的转发实现。
 */
#ifndef MOCK_TABLES_H
#define MOCK_TABLES_H

#include "game_types.h"

typedef struct {
    const char *name;
    const char *ecology;
    int stage;
    double atp;      /* 种群为 10 时的每秒产量 */
    double biomass;
    double dna;
    double growth;
    double cap;
    double speci_cost;
    int traits[TRAIT_COUNT];
} MockSpecies;

typedef struct {
    const char *name;
    const char *desc;
    double cost;
    double atp_pct;
    double biomass_pct;
    double dna_pct;
    double cap_pct;
    double growth_pct;
    int traits[TRAIT_COUNT];
    bool rare;
    int req_stage;        /* -1 表示不限 */
    int req_stage_max;
    int req_mutation;     /* -1 表示无前置 */
    int req_legacy_branch;
    int req_legacy_level;
} MockMutation;

typedef struct {
    const char *name;
    const char *desc;
    int trait;            /* -1 表示 Meteor 取最高特征 */
    int difficulty;
    double duration;
    double damage_per_second;
} MockEnvironment;

extern const MockSpecies MOCK_SPECIES[SPECIES_COUNT];
extern const MockMutation MOCK_MUTATIONS[MUTATION_COUNT];
extern const MockEnvironment MOCK_ENVIRONMENTS[7];

#define MOCK_MILESTONE_COUNT 6
extern const double MOCK_MILESTONES[MOCK_MILESTONE_COUNT];

/* 派生数据 */
int mock_speciation_parent_ok(int child, int parent);
double mock_mutation_multiplier(const SpeciesState *sp, int which); /* 0=atp 1=biomass 2=dna 3=cap 4=growth */
double mock_species_fitness(const GameState *game, int species_slot, int *out_score, int *out_diff);
bool mock_mutation_available(const GameState *game, int species_slot, int mutation_id);
double mock_mutation_cost(const GameState *game, int mutation_id);

/* 供 mock_adapter 转发用（相当于 A/B 的只读查询实现） */
double mock_run_upgrade_cost(int type, int level);
void mock_calc_rates(const GameState *game, int slot, SpeciesRates *out);
ActionCode mock_validate_speciation(const GameState *game, int child);

#endif /* MOCK_TABLES_H */
