/* Test doubles for Member B's documented functions. Never ship as game rules. */
#include "environment.h"
#include "evolution.h"
#include "legacy.h"
#include "rules_stub.h"

#include <string.h>

RulesStubControl rules_stub;

void rules_stub_reset(void)
{
    memset(&rules_stub, 0, sizeof rules_stub);
}

MutationOfferState evolution_generate_mutation_offer(const GameState *game, int species_slot)
{
    (void)game; (void)species_slot;
    MutationOfferState offer = {0};
    return offer;
}

ActionCode evolution_validate_speciation(const GameState *game, int species_slot, int target_species_id)
{
    (void)game; (void)species_slot; (void)target_species_id;
    return ACTION_REQUIREMENT_NOT_MET;
}

SpeciesRates evolution_calculate_species_rates(const GameState *game, int species_slot)
{
    ++rules_stub.rate_calls;
    rules_stub.observed_atp[species_slot] = game->run.atp;
    rules_stub.observed_population[species_slot] = game->run.active_species[0].population;
    if (rules_stub.override_rates) return rules_stub.rates[species_slot];
    SpeciesRates rates = {0};
    rates.atp_per_second = species_slot == 0 ? 1.0 : 2.0;
    rates.biomass_per_second = species_slot == 0 ? 0.5 : 1.0;
    rates.dna_per_second = species_slot == 0 ? 0.2 : 0.4;
    rates.population_delta_per_second = 2.0;
    rates.environment_multiplier = 1.0;
    rates.final_population_cap = 80.0;
    return rates;
}

EnvironmentStep environment_calculate_step(const GameState *game, double dt)
{
    (void)dt;
    rules_stub.environment_atp = game->run.atp;
    rules_stub.environment_population = game->run.active_species[0].population;
    if (rules_stub.override_environment) return rules_stub.environment;
    EnvironmentStep step = {0};
    step.current_environment_id = game->run.current_environment_id;
    step.next_environment_id = game->run.next_environment_id;
    step.environment_time_left = game->run.environment_time_left;
    step.event_interval_left = game->run.event_interval_left;
    return step;
}

EraReward prestige_preview_reward(const GameState *game)
{
    if (rules_stub.override_reward) return rules_stub.reward;
    EraReward reward = {0};
    int mutations = 0, species = 0, crises = 0;
    for (int i = 0; i < MAX_MUTATIONS; ++i)
        mutations += game->run.discovered_mutations[i];
    for (int i = 1; i < MAX_SPECIES; ++i)
        species += game->run.discovered_species[i];
    for (int i = 1; i < MAX_ENVIRONMENTS; ++i)
        crises += game->run.survived_crises[i];
    reward.era_points = 2 * game->run.era;
    reward.mutation_points = mutations / 3;
    reward.species_points = species / 2;
    reward.crisis_points = crises < 3 ? crises : 3;
    reward.biomass_points = game->run.highest_biomass_milestone;
    reward.total_points = reward.era_points + reward.mutation_points
        + reward.species_points + reward.crisis_points + reward.biomass_points;
    return reward;
}

ActionCode legacy_validate_upgrade(const GameState *game, LegacyBranch branch)
{
    (void)game; (void)branch;
    return ACTION_OK;
}

void environment_init_run(GameState *g) { (void)g; }
void legacy_apply_new_run_bonuses(GameState *g) {
    if (g->legacy.levels[LEGACY_METABOLISM] >= 3) { g->run.atp=50.; g->run.run_upgrade_levels[0]=1; }
}
int legacy_upgrade_cost(int level) { return level+1; }
void environment_apply_step(GameState *g, const EnvironmentStep *s) {
    g->run.current_environment_id=s->current_environment_id;
    g->run.next_environment_id=s->next_environment_id;
    g->run.environment_time_left=s->environment_time_left;
    g->run.event_interval_left=s->event_interval_left;
    if(s->event_started) g->run.crisis_start_population=g->run.active_species[0].population;
    if(s->event_ended) { if(s->crisis_survived) g->run.survived_crises[s->ended_environment_id]=true; g->run.adaptation_shield_active=false; }
}
ActionCode evolution_open_mutation_offer(GameState *g,int slot) { return g->run.mutation_offer.active && g->run.mutation_offer.species_slot==slot ? ACTION_OK : ACTION_REQUIREMENT_NOT_MET; }
ActionCode evolution_apply_mutation(GameState *g,int slot,int id) { (void)g;(void)slot;(void)id;return ACTION_REQUIREMENT_NOT_MET; }
ActionCode evolution_apply_speciation(GameState *g,int slot,int id,int option) { (void)g;(void)slot;(void)id;(void)option;return ACTION_REQUIREMENT_NOT_MET; }
ActionCode evolution_apply_sub_upgrade(GameState *g,int slot) { (void)g;(void)slot;return ACTION_REQUIREMENT_NOT_MET; }
ActionCode evolution_apply_capacity_upgrade(GameState *g) { (void)g;return ACTION_REQUIREMENT_NOT_MET; }
ActionCode evolution_apply_reserve(GameState *g) { (void)g;return ACTION_REQUIREMENT_NOT_MET; }

int evolution_main_stage(const GameState *g) { return g->run.era; }
