/*
 * game_types.h  ——  M0 共享类型（提案稿，待三人确认后冻结）
 *
 * 来源：01-三人分工与接口约定 §5（结构体原样保留）
 *       02-数据与计算规则 §5.1 / §6.1 / §7.1（编号枚举）
 *
 * 规则提醒：
 *   - 本文件禁止包含 raylib.h。
 *   - 三个人不得各自复制一份修改，改动必须同步 01 文档。
 *   - Member C 只读取本文件中的结构体与枚举，用于界面显示与状态索引；
 *     不在此文件中增加 UI 专用字段（UI 自己的状态放 src/ui.c 的 static UIState）。
 */
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdbool.h>

#define MAX_ACTIVE_SPECIES 2
#define MAX_SPECIES 32
#define MAX_MUTATIONS 64
#define MAX_ENVIRONMENTS 16
#define MAX_MUTATION_CHOICES 3

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
    ACTION_START_NEW_RUN
} ActionType;

typedef enum {
    ACTION_OK,
    ACTION_INVALID_PHASE,
    ACTION_INVALID_TARGET,
    ACTION_NOT_ENOUGH_ATP,
    ACTION_NOT_ENOUGH_BIOMASS,
    ACTION_NOT_ENOUGH_DNA,
    ACTION_NOT_ENOUGH_EP,
    ACTION_REQUIREMENT_NOT_MET,
    ACTION_LEVEL_MAXED,
    ACTION_ALREADY_CLAIMED
} ActionCode;

/* ---- 编号枚举（02 §5.1 / §5.2 / §6.1 / §7.1）---- */

typedef enum {
    SPECIES_PROTOCELL = 0,
    SPECIES_CYANOBACTERIUM,
    SPECIES_CHEMOLITHOTROPH,
    SPECIES_ALGAL_EUKARYOTE,
    SPECIES_PROTOZOAN,
    SPECIES_MULTICELLULAR_COLONY,
    SPECIES_EXTREMOPHILE_COLONY,
    SPECIES_COUNT
} SpeciesId;

typedef enum {
    ECO_PRIMITIVE = 0,
    ECO_PHOTOSYNTHETIC,
    ECO_CHEMOSYNTHETIC,
    ECO_HETEROTROPHIC,
    ECO_MULTICELLULAR,
    ECO_EXTREMOPHILE
} EcologyTag;

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
    MUTATION_COUNT
} MutationId;

typedef enum {
    TRAIT_HEAT = 0,
    TRAIT_COLD,
    TRAIT_LOW_LIGHT,
    TRAIT_OXYGEN,
    TRAIT_TOXIN,
    TRAIT_COUNT
} EnvironmentTrait;

/* ---- 状态结构体（01 §5 原样）---- */

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
    bool adaptation_shield_used;
    bool adaptation_shield_active;

    unsigned int rng_state;

    bool discovered_species[MAX_SPECIES];
    bool discovered_mutations[MAX_MUTATIONS];
    bool survived_crises[MAX_ENVIRONMENTS];

    double max_biomass;
    int highest_biomass_milestone;
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

    /* 提案 #4（待三人确认）：成就位标记，跨轮保留。位含义由 B 在成就表定义。 */
    unsigned int achievement_flags;
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
} EnvironmentStep;

#endif /* GAME_TYPES_H */
