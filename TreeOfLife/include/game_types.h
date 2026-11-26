#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/*
 * 共享数据结构（01-三人分工与接口约定 §5）
 *
 * 本文件由三人共同评审后使用，任何人都不得私自复制另一份。
 * 相对 §5 草案的补充只有以下两类，含义与所有权不变：
 *   1. 共享编号枚举（02-数据与计算规则 §5.1、§5.2、§6.1、§7.1）
 *   2. EnvironmentStep 中两个只由 B 产生、由 A 写回的临时结果字段
 *      （instant_population_loss_ratio、adaptation_shield_triggered）
 * 这两个字段不进入存档，不影响存档格式与版本号。
 */

#include <stdbool.h>

#define MAX_ACTIVE_SPECIES 3
#define MAX_SPECIES 32
#define MAX_MUTATIONS 64
#define MAX_ENVIRONMENTS 16
#define MAX_MUTATION_CHOICES 4

/* ---- 02 §5.1 物种编号（Member B 数据表 / Member C 显示） ---- */
typedef enum {
    SPECIES_PROTOCELL = 0,
    SPECIES_CYANOBACTERIUM,
    SPECIES_CHEMOLITHOTROPH,
    SPECIES_ALGAL_EUKARYOTE,
    SPECIES_PROTOZOAN,
    SPECIES_MULTICELLULAR_COLONY,
    SPECIES_EXTREMOPHILE_COLONY,
    /* 内容扩容：阶段 4 / 4 / 5，使 EP 的阶段项上限翻倍 */
    SPECIES_DIFFERENTIATED_TISSUE,
    SPECIES_SYMBIOTIC_SUPERORGANISM,
    SPECIES_COGNITIVE_BIOSPHERE,
    /* 第四轮：阶段 4 侧支，配合 Diversification Lv.5 的第三槽 */
    SPECIES_FUNGAL_NETWORK,
    SPECIES_CORAL_SYMBIONT,
    /* 第六轮：阶段 5 侧支终点 */
    SPECIES_MYCORRHIZAL_WEB,
    SPECIES_REEF_METROPOLIS,
    /* 第七轮：阶段 6/7 主线终点 */
    SPECIES_SAPIENT_LIFE,
    SPECIES_TECHNOSPHERE,
    /* 第八轮：阶段 8/9/10 长线 */
    SPECIES_CIVILIZATION,
    SPECIES_SPACE_AGE,
    SPECIES_SINGULARITY,
    /* 第十二轮：第 11 阶，奇点之后 */
    SPECIES_TRANSCENDENCE,
    SPECIES_COUNT
} SpeciesId;

/* ---- 02 §5.2 生态标签 ---- */
typedef enum {
    ECO_PRIMITIVE = 0,
    ECO_PHOTOSYNTHETIC,
    ECO_CHEMOSYNTHETIC,
    ECO_HETEROTROPHIC,
    ECO_MULTICELLULAR,
    ECO_EXTREMOPHILE
} EcologyTag;

/* ---- 02 §6.1 突变编号 ---- */
typedef enum {
    MUTATION_CELL_MEMBRANE = 0,
    MUTATION_PHOTOSYNTHESIS,
    MUTATION_CHEMOSYNTHESIS,
    MUTATION_CELL_WALL,
    MUTATION_HEAT_RESISTANCE,
    MUTATION_COLD_RESISTANCE,
    MUTATION_NUCLEUS,
    MUTATION_MITOCHONDRIA,
    MUTATION_MULTICELLULARITY,
    MUTATION_TOXIN_RESISTANCE,
    MUTATION_EFFICIENT_RESPIRATION,
    MUTATION_ADAPTIVE_HYPERMUTATION,
    /* 内容扩容：把突变池扩到 18，使 EP 的突变项上限翻倍 */
    MUTATION_CELL_DIFFERENTIATION,
    MUTATION_NEURAL_SIGNALING,
    MUTATION_SYMBIOTIC_NITROGEN_FIXATION,
    MUTATION_EXTREMOZYME_COCKTAIL,
    MUTATION_QUORUM_SENSING,
    MUTATION_ECOSYSTEM_ENGINEERING,
    /* 第四轮：新路线的关键突变与构筑件 */
    MUTATION_DECOMPOSER_ENZYMES,
    MUTATION_PHOTOSYMBIOSIS,
    MUTATION_CHITIN_WALL,
    MUTATION_NITROGEN_SENSE,
    /* 第六轮：两条侧支的关键突变 */
    MUTATION_MYCORRHIZAL_SYMBIOSIS,
    MUTATION_REEF_ARCHITECTURE,
    /* 第七轮：阶段 6/7 的关键突变 */
    MUTATION_ABSTRACT_THOUGHT,
    MUTATION_PLANETARY_INDUSTRY,
    MUTATION_SIGNAL_LITERACY,
    /* 第八轮：阶段 8/9/10 的关键突变与构筑件 */
    MUTATION_LANGUAGE,
    MUTATION_ROCKETRY,
    MUTATION_RECURSIVE_SELF_IMPROVEMENT,
    MUTATION_URBANIZATION,
    MUTATION_ORBITAL_HABITATS,
    MUTATION_MACHINE_INTELLIGENCE,
    /* 第十二轮：第 11 阶的前置突变 */
    MUTATION_OMEGA_POINT,
    MUTATION_COUNT
} MutationId;

/* ---- 02 §7.1 环境特征维度 ---- */
typedef enum {
    TRAIT_HEAT = 0,
    TRAIT_COLD,
    TRAIT_LOW_LIGHT,
    TRAIT_OXYGEN,
    TRAIT_TOXIN,
    TRAIT_COUNT
} EnvironmentTrait;

/* ---- 02 §7.2 环境事件编号 ---- */
typedef enum {
    ENV_NORMAL_OCEAN = 0,
    ENV_GLOBAL_WARMING,
    ENV_GLOBAL_COOLING,
    ENV_LOW_LIGHT_PERIOD,
    ENV_OXYGEN_RISE,
    ENV_TOXIC_BLOOM,
    ENV_METEOR_IMPACT,
    /* 第四轮：正面 / 中性随机事件，不计入危机统计 */
    ENV_NUTRIENT_UPWELLING,
    ENV_SOLAR_FLARE,
    ENV_SPORE_CLOUD,
    /* 第六轮：更多正面 / 中性随机事件 */
    ENV_OXYGEN_PULSE,
    ENV_COLD_CURRENT,
    ENV_LONG_SUMMER,
    /* 第七轮：阶段 5 之后的工业污染危机 */
    ENV_INDUSTRIAL_SMOG,
    ENV_COUNT
} EnvironmentId;

typedef enum {
    PHASE_RUNNING,
    PHASE_END_ERA_PREVIEW,
    PHASE_LEGACY
} GamePhase;

typedef enum {
    UPGRADE_METABOLISM,
    UPGRADE_REPRODUCTION,
    UPGRADE_GENOME
} RunUpgradeType;

typedef enum {
    LEGACY_METABOLISM,
    LEGACY_MUTATION,
    LEGACY_ADAPTATION,
    LEGACY_DIVERSIFICATION
} LegacyBranch;

typedef enum {
    ACTION_CLICK_ATP,
    ACTION_BUY_RUN_UPGRADE,
    ACTION_GENERATE_MUTATION_OFFER,
    ACTION_CHOOSE_MUTATION,
    ACTION_SPECIATE,
    ACTION_OPEN_END_ERA,
    ACTION_CANCEL_END_ERA,
    ACTION_CONFIRM_END_ERA,
    ACTION_BUY_LEGACY,
    ACTION_START_NEW_RUN,
    ACTION_BUY_SUB_UPGRADE
} ActionType;

typedef struct {
    int species_id;
    bool is_main_lineage;
    double population;
    bool mutations[MAX_MUTATIONS];
} SpeciesState;

typedef struct {
    bool active;
    int species_slot;
    int choice_count;
    int mutation_ids[MAX_MUTATION_CHOICES];
    double dna_costs[MAX_MUTATION_CHOICES];
} MutationOfferState;

typedef struct {
    double atp;
    double biomass;
    double dna;
    double knowledge; /* 第九轮：第 4 资源“理念”，阶段 >= 6 的物种产出 */
    double elapsed_time;

    int era;
    int run_upgrade_levels[3];
    int active_species_count;
    SpeciesState active_species[MAX_ACTIVE_SPECIES];

    MutationOfferState mutation_offer;

    int current_environment_id;
    int next_environment_id;
    double environment_time_left;
    double event_interval_left;
    double crisis_start_population;
    double crisis_min_population; /* 第十一轮：事件期间主谱系最低人口（危机判定口径） */
    bool adaptation_shield_used;
    bool adaptation_shield_active;

    unsigned int rng_state;

    bool discovered_species[MAX_SPECIES];
    bool discovered_mutations[MAX_MUTATIONS];
    bool survived_crises[MAX_ENVIRONMENTS];

    double max_biomass;
    int capacity_levels;
    int reserve_levels;
    int tech_levels[MAX_SPECIES]; /* 第九轮：每个物种的子升级（科技分层）已购层数 */
    bool reward_claimed;
} RunState;

typedef struct {
    int evolution_points;
    int levels[4];

    bool codex_species[MAX_SPECIES];
    bool codex_mutations[MAX_MUTATIONS];

    int best_era;
    int best_evolution_points;
    double best_biomass;
} LegacyState;

typedef struct {
    int save_version;
    GamePhase phase;
    RunState run;
    LegacyState legacy;
} GameState;

typedef struct {
    ActionType type;
    int target_species_slot;
    int target_id;
    int option;
} GameAction;

typedef enum {
    ACTION_OK,
    ACTION_INVALID_PHASE,
    ACTION_INVALID_TARGET,
    ACTION_NOT_ENOUGH_ATP,
    ACTION_NOT_ENOUGH_BIOMASS,
    ACTION_NOT_ENOUGH_DNA,
    ACTION_NOT_ENOUGH_EP,
    ACTION_NOT_ENOUGH_KNOWLEDGE,
    ACTION_REQUIREMENT_NOT_MET,
    ACTION_LEVEL_MAXED,
    ACTION_ALREADY_CLAIMED
} ActionCode;

typedef struct {
    ActionCode code;
    double amount_spent;
    int affected_id;
} ActionResult;

typedef struct {
    int era_points;
    int mutation_points;
    int species_points;
    int crisis_points;
    int biomass_points;
    int total_points;
} EraReward;

typedef struct {
    double atp_per_second;
    double biomass_per_second;
    double dna_per_second;
    double knowledge_per_second; /* 第九轮：阶段 >= 6 物种的理念产率，否则为 0 */
    double population_delta_per_second;
    double environment_multiplier;
    double final_population_cap;
    bool synergy_active;
} SpeciesRates;

typedef struct {
    int current_environment_id;
    int next_environment_id;
    double environment_time_left;
    double event_interval_left;
    bool event_started;
    bool event_ended;
    bool crisis_survived;
    int ended_environment_id;

    /* B 计算、A 在写回时应用的结果（不进入存档） */
    double instant_population_loss_ratio;  /* Meteor Impact 事件开始时的即时种群损失比例 */
    double crisis_lowest_population;       /* 第十二轮：事件期间主谱系最低人口（用于日志/验收） */
    bool adaptation_shield_triggered;      /* Adaptation Lv.5 本步触发保护 */
} EnvironmentStep;

#endif
