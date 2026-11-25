#ifndef EVOLUTION_H
#define EVOLUTION_H

#include "game_types.h"

/* Documented Member B contract; implementation is owned by B. */
MutationOfferState evolution_generate_mutation_offer(const GameState *game, int species_slot);
ActionCode evolution_validate_speciation(const GameState *game, int species_slot, int target_species_id);
SpeciesRates evolution_calculate_species_rates(const GameState *game, int species_slot);
EraReward prestige_preview_reward(const GameState *game);

#endif
