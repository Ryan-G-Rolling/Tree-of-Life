#ifndef EVOLUTION_H
#define EVOLUTION_H

/*
 * Member B · 物种、进化树、突变与物种产率
 * 实现见 src/species.c、src/evolution.c
 *
 * 本模块不包含 raylib.h，只读取 GameState 并返回计算结果。
 * 其中 evolution_apply_mutation / evolution_apply_speciation 是供 A 的
 * game_apply_action 在检查通过后调用的一次性提交辅助函数，保证规则只有
 * 一份实现；除 A 之外任何人不得调用。
 */

#include "game_types.h"

typedef struct {
    const char *name;
    const char *description;
    int phase;                             /* 0 原始生命 … 3 多细胞 */
    EcologyTag ecology;
    double base_atp;                       /* 种群为 10 时的每秒产率 */
    double base_biomass;
    double base_dna;
    double base_knowledge;                 /* 第九轮：理念产率，阶段 >= 6 的物种非 0 */
    double base_growth;                    /* 每秒基础种群增长率 */
    double base_cap;
    double base_speciation_cost;           /* Biomass */
    int traits[TRAIT_COUNT];
} SpeciesDef;

/* 物种基石：每个物种自带的被动，提供差异化的成长路线 */
typedef struct {
    const char *name;
    const char *description;
    double atp_bonus;
    double biomass_bonus;
    double dna_bonus;
    double cap_bonus;
    double growth_bonus;
    double low_light_bonus;   /* 低光事件中的额外产率乘数，0 表示无 */
    double toxin_bonus;       /* 毒素事件中的额外产率乘数，0 表示无 */
    double crisis_damage_reduction;
} SpeciesKeystone;

/* 突变共振：同一物种同时拥有两个突变时解锁的额外加成 */
typedef struct {
    const char *name;
    const char *description;
    int mutation_a;
    int mutation_b;
    double atp_bonus;
    double biomass_bonus;
    double dna_bonus;
    double cap_bonus;
    double growth_bonus;
    int trait_bonus[TRAIT_COUNT];
} MutationSynergyDef;

/* 第九轮：子升级（科技分层），Cookie Clicker 式命名分层 + 细胞进化式理念消费。
 * 每个高阶物种有按 tier 顺序解锁的命名升级，乘算该物种的全部产率。 */
typedef struct {
    const char *name;
    const char *description;
    int species_id;
    int tier;                              /* 0 起，同一物种必须按 tier 顺序购买 */
    double multiplier;                     /* 该物种全部产率（含理念）的乘数 */
    double base_knowledge_cost;            /* tier 0 的理念成本，逐层 ×SUB_UPGRADE_COST_GROWTH */
} SubUpgradeDef;

typedef struct {
    const char *name;
    const char *description;
    double base_dna_cost;
    double atp_bonus;                      /* 加法百分比，例如 0.25 */
    double biomass_bonus;
    double dna_bonus;
    double cap_bonus;
    double growth_bonus;
    int trait_bonus[TRAIT_COUNT];
    int rarity;                            /* 0 普通，1 稀有，2 特殊 */
    int min_phase;
    int max_phase;                         /* -1 表示无上限 */
    int requires_mutation;                 /* -1 表示无前置突变 */
    int requires_branch;                   /* LegacyBranch 或 -1 */
    int requires_level;
} MutationDef;

/* ---------------- 数据访问（Member C 可只读使用） ---------------- */
const SpeciesDef *species_get(int species_id);
const MutationDef *mutation_get(int mutation_id);
const char *species_name(int species_id);
const char *species_description(int species_id);
int species_phase(int species_id);
EcologyTag species_ecology(int species_id);
int species_base_trait(int species_id, EnvironmentTrait trait);
double species_base_speciation_cost(int species_id);

/* 物种基石（Member C 可只读用于图鉴与提示） */
const SpeciesKeystone *species_keystone(int species_id);

/* 突变共振 */
int evolution_synergy_count(void);
/* 第十三轮：少数突变直接加成理念产出（不在 MutationDef 中，保持公开结构不变） */
double evolution_mutation_knowledge_bonus(int mutation_id);
const MutationSynergyDef *evolution_synergy_get(int index);
bool evolution_synergy_active(const GameState *game, int species_slot, int synergy_index);
int evolution_active_synergy_count(const GameState *game, int species_slot);
/* 只统计共振带来的加成（倍率为 1.0 + 加成），trait_bonus 可为 NULL */
void evolution_synergy_bonuses(const GameState *game, int species_slot, double *atp_multiplier,
                               double *biomass_multiplier, double *dna_multiplier,
                               double *cap_multiplier, double *growth_multiplier,
                               int *trait_bonus);

/* ---------------- 进化树 ---------------- */
int species_children(int parent_id, int *out_ids, int max_ids);
int species_min_child_cost(int parent_id);
bool species_has_parent(int target_species_id, int parent_id);
int species_required_mutations(int target_species_id, int *out_ids, int max_ids);
bool species_requires_legacy(int target_species_id, int *branch, int *level);
int evolution_max_speciation_slots(const GameState *game);
int evolution_main_slot(const GameState *game);
int evolution_main_stage(const GameState *game);
double evolution_speciation_cost(const GameState *game, int target_species_id);

/* ---------------- 突变 ---------------- */
MutationOfferState evolution_generate_mutation_offer(const GameState *game, int species_slot);
/* 同上，但会把抽取过程中消耗的随机数写回 *rng_state */
MutationOfferState evolution_generate_mutation_offer_rng(
    const GameState *game,
    int species_slot,
    unsigned int *rng_state);

/* 打开候选：已存在同槽候选时原样返回，不重新抽取，也不消耗随机数 */
ActionCode evolution_open_mutation_offer(GameState *game, int species_slot);

int evolution_mutation_choice_count(const GameState *game);
double evolution_mutation_cost(const GameState *game, int mutation_id);
double evolution_mutation_cost_for_slot(const GameState *game, int species_slot,
                                        int mutation_id);
bool evolution_mutation_available(const GameState *game, int species_slot, int mutation_id);
/* 没有可用候选时返回原因文本，可用时返回 NULL */
const char *evolution_offer_block_reason(const GameState *game, int species_slot);

/* ---------------- 一次性提交辅助（只由 A 调用） ---------------- */
ActionCode evolution_apply_mutation(GameState *game, int species_slot, int mutation_id);
ActionCode evolution_apply_speciation(
    GameState *game,
    int species_slot,
    int target_species_id,
    int option);

/* 第七轮：Biomass 容量扩张与 ATP 应急储备（一次性提交，只由 A 调用） */
double evolution_capacity_cost(const GameState *game);
/* 第十一轮：容量改为双币支付；前 3 级 ATP 份额为 0，返回 0 */
double evolution_capacity_atp_cost(const GameState *game);
double evolution_capacity_biomass_cost(const GameState *game);
ActionCode evolution_apply_capacity_upgrade(GameState *game);
double evolution_reserve_cost(const GameState *game);
ActionCode evolution_apply_reserve(GameState *game);

/* 第九轮：理念与子升级（数据只读查询 + 一次性提交，提交只由 A 调用） */
int evolution_sub_upgrade_count(void);
const SubUpgradeDef *evolution_sub_upgrade_get(int index);
/* 该槽物种已购买的子升级层数 */
int evolution_sub_upgrade_level(const GameState *game, int species_slot);
/* 下一层可购买的子升级；物种没有子升级或已满层时返回 NULL */
const SubUpgradeDef *evolution_sub_upgrade_next(const GameState *game, int species_slot);
/* 该槽物种当前已购子升级的总产率乘数（未购任何层时为 1.0） */
double evolution_sub_upgrade_multiplier(const GameState *game, int species_slot);
double evolution_sub_upgrade_cost(const SubUpgradeDef *def);
ActionCode evolution_apply_sub_upgrade(GameState *game, int species_slot);
/* 全部在场物种的理念产率之和，供 UI 与 A 写回 */
double evolution_knowledge_per_second(const GameState *game);

/* ---------------- 规则查询 ---------------- */
/* 只校验主谱系能否分化为目标物种；option=1（保留亲本）还需要
 * evolution_max_speciation_slots(game) >= 2，由 UI 决定是否提供该选项。 */
ActionCode evolution_validate_speciation(
    const GameState *game,
    int species_slot,
    int target_species_id);

SpeciesRates evolution_calculate_species_rates(const GameState *game, int species_slot);

/* 物种当前某项环境特征总值（基础值 + 突变修正），供环境模块与 UI 使用 */
int evolution_slot_trait_total(const GameState *game, int species_slot, EnvironmentTrait trait);

#endif
