#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdbool.h>

#define MAX_ACTIVE_SPECIES 2
#define MAX_SPECIES 32
#define MAX_MUTATIONS 64
#define MAX_ENVIRONMENTS 16
#define MAX_MUTATION_CHOICES 3

typedef enum { PHASE_RUNNING, PHASE_END_ERA_PREVIEW, PHASE_LEGACY } GamePhase;
typedef enum { UPGRADE_METABOLISM, UPGRADE_REPRODUCTION, UPGRADE_GENOME } RunUpgradeType;
typedef enum { LEGACY_METABOLISM, LEGACY_MUTATION, LEGACY_ADAPTATION, LEGACY_DIVERSIFICATION } LegacyBranch;
typedef enum {
    ACTION_CLICK_ATP, ACTION_BUY_RUN_UPGRADE, ACTION_GENERATE_MUTATION_OFFER,
    ACTION_CHOOSE_MUTATION, ACTION_SPECIATE, ACTION_OPEN_END_ERA,
    ACTION_CANCEL_END_ERA, ACTION_CONFIRM_END_ERA, ACTION_BUY_LEGACY,
    ACTION_START_NEW_RUN
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
    double atp, biomass, dna, elapsed_time;
    int era;
    int run_upgrade_levels[3];
    int active_species_count;
    SpeciesState active_species[MAX_ACTIVE_SPECIES];
    MutationOfferState mutation_offer;
    int current_environment_id, next_environment_id;
    double environment_time_left, event_interval_left, crisis_start_population;
    bool adaptation_shield_used, adaptation_shield_active;
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
    int best_era, best_evolution_points;
    double best_biomass;
} LegacyState;

typedef struct { int save_version; GamePhase phase; RunState run; LegacyState legacy; } GameState;
typedef struct { ActionType type; int target_species_slot, target_id, option; } GameAction;
typedef enum {
    ACTION_OK, ACTION_INVALID_PHASE, ACTION_INVALID_TARGET,
    ACTION_NOT_ENOUGH_ATP, ACTION_NOT_ENOUGH_BIOMASS, ACTION_NOT_ENOUGH_DNA,
    ACTION_NOT_ENOUGH_EP, ACTION_REQUIREMENT_NOT_MET, ACTION_LEVEL_MAXED,
    ACTION_ALREADY_CLAIMED
} ActionCode;
typedef struct { ActionCode code; double amount_spent; int affected_id; } ActionResult;
typedef struct {
    int era_points, mutation_points, species_points, crisis_points;
    int biomass_points, total_points;
} EraReward;
typedef struct {
    double atp_per_second, biomass_per_second, dna_per_second;
    double population_delta_per_second, environment_multiplier, final_population_cap;
    bool synergy_active;
} SpeciesRates;
typedef struct {
    int current_environment_id, next_environment_id;
    double environment_time_left, event_interval_left;
    bool event_started, event_ended, crisis_survived;
    int ended_environment_id;
} EnvironmentStep;

#endif
