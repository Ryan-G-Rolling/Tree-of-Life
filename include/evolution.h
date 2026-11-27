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
    double base_knowledge_cost;            /* v10：地板价；实际价格 = 50 s 知识产率与此值取大 */
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
/* v9 P0-18：显式主线后继（原始细胞 → … → 宇宙织体），-1 表示主线到此为止。
 * 规则层与演示策略共用同一张表，避免“最小 ID 子物种”把保底引向侧支。 */
int species_main_child(int species_id);
/* 返回子代总数，最多写入 max_ids 项；遍历完整列表时用 SPECIES_COUNT 容量。 */
int species_children(int parent_id, int *out_ids, int max_ids);
int species_min_child_cost(int parent_id);
bool species_has_parent(int target_species_id, int parent_id);
int species_required_mutations(int target_species_id, int *out_ids, int max_ids);
bool species_requires_legacy(int target_species_id, int *branch, int *level);
int evolution_max_speciation_slots(const GameState *game);
int evolution_main_slot(const GameState *game);
int evolution_main_stage(const GameState *game);
/* v8 P0-17：本纪元的阶段深度上限（纪元 1/2/3 = 8/10/13） */
int evolution_era_stage_cap(const GameState *game);
double evolution_speciation_cost(const GameState *game, int target_species_id);
/* v10 P1-11：主谱系阶段 ≥6 时的“2 选 1”支线候选——同阶段、非主线的全部物种。
 * 返回候选数，最多写入 max_ids 项；不是主线物种或没有同阶段候选时返回 0。 */
int species_branch_candidates(int parent_id, int *out_ids, int max_ids);
/* v14 W5：一次分化给“3 张方向卡 + 1 张主链保底”（主链后继永远第一张）。
 * 槽位不是主谱系、或未到 BRANCH_SPECIATION_MIN_PHASE 时，退回全部直接子代。 */
int evolution_speciation_offers(const GameState *game, int species_slot, int *out_ids,
                                int max_ids);
/* v14 E9/E10：是否主线节点。支线门票 ×1.25、基石与共振乘算减半。 */
bool species_is_mainline(int species_id);

/* ---------------- 突变 ---------------- */
MutationOfferState evolution_generate_mutation_offer(const GameState *game, int species_slot);
/* 同上，但会把抽取过程中消耗的随机数写回 *rng_state */
MutationOfferState evolution_generate_mutation_offer_rng(
    const GameState *game,
    int species_slot,
    unsigned int *rng_state);

/* 打开候选：已存在同槽候选时原样返回，不重新抽取，也不消耗随机数 */
ActionCode evolution_open_mutation_offer(GameState *game, int species_slot);
/* v14 E3：DNA 换一批候选。价格见 balance.h 的 MUTATION_REFRESH_*，每批 ≤3 次。 */
double evolution_mutation_refresh_cost(const GameState *game);
ActionCode evolution_refresh_mutation_offer(GameState *game, int species_slot);
/* v14 E3 补充：放弃当前候选（不计购买）。三个槽位共用一个候选位，
 * 候选里没有能推进分化的卡时，调用方需要释放它给其他槽位。 */
void evolution_dismiss_mutation_offer(GameState *game);

int evolution_mutation_choice_count(const GameState *game);
double evolution_mutation_cost(const GameState *game, int mutation_id);
/* 第十五轮：阶段 8 起的高阶突变额外消耗 ATP；低阶返回 0 */
double evolution_mutation_atp_cost(const GameState *game, int mutation_id);
/* 该突变是否被任一物种的分化前置引用（进入必需成本链） */
bool evolution_mutation_is_required(int mutation_id);
/* v14 共享基因组：额外分支与主谱系同源，主谱系已获得的突变视为支线也已获得。
 * 支线因此不再为分化重复购买前置（否则每多一个支线物种就多 1–2 张突变，
 * “突变张数”与“单局物种数”两条门槛互相打架）。 */
bool evolution_slot_owns_mutation(const GameState *game, int species_slot, int mutation_id);
/* v14：物种离主链的层数（主线 0）。支线用它封顶，保证方向卡每局 4–7 次。 */
int species_branch_depth(int species_id);
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

/* ---------------- v10 P0-22：时间本位定价锚 ---------------- */
/* 主谱系“上一 tick 产率”：剔除环境倍率与限时增益倍率（两者都是瞬时状态，
 * 不该让价格跟着抖），保留种群 / 突变 / 永久 / 洞见等持久乘数。
 * 四张价格表（升级 / 容量 / 储备 / 门票）与点击收益共用这一个锚点。 */
void evolution_price_rates(const GameState *game, double *atp_rate, double *biomass_rate,
                           double *knowledge_rate);

/* 第七轮：容量扩张与应急储备（一次性提交，只由 A 调用） */
/* v10 P0-22：容量改时间本位双币——ATP 部分 45 s 产率、生物量部分 45 s 产率 */
double evolution_capacity_cost(const GameState *game);
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
/* v10 P0-22：子升级改时间本位——50 s 主谱系知识产率，base_knowledge_cost 为地板 */
double evolution_sub_upgrade_cost(const GameState *game, const SubUpgradeDef *def);
ActionCode evolution_apply_sub_upgrade(GameState *game, int species_slot);
/* 全部在场物种的理念产率之和，供 UI 与 A 写回 */
double evolution_knowledge_per_second(const GameState *game);

/* 第十六轮：洞见——无上限知识升级，每级 +2% 全产率 */
double evolution_insight_cost(const GameState *game);
int evolution_insight_level(const GameState *game);
double evolution_insight_multiplier(const GameState *game);
ActionCode evolution_apply_insight(GameState *game);

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

