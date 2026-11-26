#include "save.h"
#include "game_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../data/balance.h"

static const unsigned char magic[8] = { 'T','O','L','S','V','0','0','1' };

static int valid_nonnegative(double value)
{
    return isfinite(value) && value >= 0.0;
}

/* Validate object representations before any bool value is evaluated. */
static int valid_bool(const bool *value)
{
    const bool no = false, yes = true;
    return memcmp(value, &no, sizeof no) == 0
        || memcmp(value, &yes, sizeof yes) == 0;
}

static int valid_flags(const GameState *game)
{
    const RunState *run = &game->run;
    if (!valid_bool(&run->mutation_offer.active)
        || !valid_bool(&run->adaptation_shield_used)
        || !valid_bool(&run->adaptation_shield_active)
        || !valid_bool(&run->reward_claimed)) return 0;
    for (int i = 0; i < MAX_ACTIVE_SPECIES; ++i) {
        if (!valid_bool(&run->active_species[i].is_main_lineage)) return 0;
        for (int j = 0; j < MAX_MUTATIONS; ++j)
            if (!valid_bool(&run->active_species[i].mutations[j])) return 0;
    }
    for (int i = 0; i < MAX_SPECIES; ++i)
        if (!valid_bool(&run->discovered_species[i])
            || !valid_bool(&game->legacy.codex_species[i])) return 0;
    for (int i = 0; i < MAX_MUTATIONS; ++i)
        if (!valid_bool(&run->discovered_mutations[i])
            || !valid_bool(&game->legacy.codex_mutations[i])) return 0;
    for (int i = 0; i < MAX_ENVIRONMENTS; ++i)
        if (!valid_bool(&run->survived_crises[i])) return 0;
    return 1;
}

static int valid_game(const GameState *game)
{
    const RunState *run = &game->run;
    const LegacyState *legacy = &game->legacy;
    int mains = 0;
    if (!valid_flags(game)) return 0;
    if (game->save_version != SAVE_VERSION || game->phase < PHASE_RUNNING
        || game->phase > PHASE_LEGACY) return 0;
    if (!valid_nonnegative(run->atp) || !valid_nonnegative(run->biomass)
        || !valid_nonnegative(run->dna) || !valid_nonnegative(run->knowledge)
        || !valid_nonnegative(run->elapsed_time)
        || !valid_nonnegative(run->max_biomass)
        || !valid_nonnegative(run->environment_time_left)
        || !valid_nonnegative(run->event_interval_left)
        || !valid_nonnegative(run->crisis_start_population)
        || !valid_nonnegative(run->crisis_min_population)
        || run->max_biomass < run->biomass || run->era < 0
        || run->capacity_levels < 0
        || (CAPACITY_MAX_LEVEL >= 0 && run->capacity_levels > CAPACITY_MAX_LEVEL)
        || run->reserve_levels < 0 || run->reserve_levels > RESERVE_MAX_LEVEL
        || run->active_species_count < 1 || run->active_species_count > MAX_ACTIVE_SPECIES
        || run->current_environment_id < 0 || run->current_environment_id >= ENV_COUNT
        || run->next_environment_id < -1 || run->next_environment_id >= ENV_COUNT
        || run->rng_state == 0) return 0;
    for (int i = 0; i < 3; ++i)
        if (run->run_upgrade_levels[i] < 0) return 0;
    for (int i = 0; i < MAX_SPECIES; ++i)
        if (run->tech_levels[i] < 0) return 0;
    for (int i = 0; i < run->active_species_count; ++i) {
        const SpeciesState *species = &run->active_species[i];
        if (species->species_id < 0 || species->species_id >= SPECIES_COUNT
            || !isfinite(species->population) || species->population < MIN_ACTIVE_POPULATION)
            return 0;
        if (species->is_main_lineage) ++mains;
    }
    if (mains != 1 || !run->active_species[0].is_main_lineage) return 0;
    if (run->mutation_offer.choice_count < 0
        || run->mutation_offer.choice_count > MAX_MUTATION_CHOICES) return 0;
    if (run->mutation_offer.active) {
        if (run->mutation_offer.species_slot < 0
            || run->mutation_offer.species_slot >= run->active_species_count
            || run->mutation_offer.choice_count < 1) return 0;
        for (int i = 0; i < run->mutation_offer.choice_count; ++i)
            if (run->mutation_offer.mutation_ids[i] < 0
                || run->mutation_offer.mutation_ids[i] >= MUTATION_COUNT
                || !valid_nonnegative(run->mutation_offer.dna_costs[i])) return 0;
    }
    if ((game->phase == PHASE_LEGACY) != run->reward_claimed) return 0;
    if (legacy->evolution_points < 0 || legacy->best_era < 0
        || legacy->best_evolution_points < 0 || !valid_nonnegative(legacy->best_biomass))
        return 0;
    for (int i = 0; i < 4; ++i)
        if (legacy->levels[i] < 0 || legacy->levels[i] > LEGACY_MAX_LEVEL) return 0;
    return 1;
}

SaveResult save_write(const char *path, const GameState *game)
{
    if (!path || !*path || !game) return SAVE_INVALID_DATA;
    if (!valid_game(game) || sizeof(GameState) > UINT32_MAX) return SAVE_INVALID_DATA;
    size_t length = strlen(path);
    char *temporary = malloc(length + 5);
    if (!temporary) return SAVE_FILE_ERROR;
    memcpy(temporary, path, length);
    memcpy(temporary + length, ".tmp", 5);
    FILE *file = fopen(temporary, "wb");
    if (!file) { free(temporary); return SAVE_FILE_ERROR; }
    uint32_t version = SAVE_VERSION;
    uint32_t payload_size = (uint32_t)sizeof(GameState);
    int success = fwrite(magic, 1, 8, file) == 8
        && fwrite(&version, sizeof version, 1, file) == 1
        && fwrite(&payload_size, sizeof payload_size, 1, file) == 1
        && fwrite(game, sizeof *game, 1, file) == 1
        && fflush(file) == 0;
    if (fclose(file) != 0) success = 0;
    if (!success) {
        remove(temporary);
        free(temporary);
        return SAVE_FILE_ERROR;
    }
#ifdef _WIN32
    success = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    success = rename(temporary, path) == 0;
#endif
    if (!success) remove(temporary);
    free(temporary);
    return success ? SAVE_OK : SAVE_FILE_ERROR;
}

SaveResult save_read(const char *path, GameState *out_game)
{
    if (!path || !*path || !out_game) return SAVE_INVALID_DATA;
    FILE *file = fopen(path, "rb");
    if (!file) return SAVE_FILE_ERROR;
    unsigned char actual_magic[8];
    uint32_t version = 0, payload_size = 0;
    GameState candidate;
    SaveResult status = SAVE_INVALID_DATA;
    if (fread(actual_magic, 1, 8, file) != 8
        || fread(&version, sizeof version, 1, file) != 1
        || fread(&payload_size, sizeof payload_size, 1, file) != 1) goto done;
    if (memcmp(actual_magic, magic, 8) != 0) goto done;
    if (version != SAVE_VERSION || payload_size != sizeof(GameState)) {
        status = SAVE_BAD_VERSION;
        goto done;
    }
    if (fread(&candidate, sizeof candidate, 1, file) != 1 || fgetc(file) != EOF
        || ferror(file)
        || !valid_game(&candidate)) goto done;
    *out_game = candidate;
    game_reset_timing();
    status = SAVE_OK;
done:
    fclose(file);
    return status;
}
