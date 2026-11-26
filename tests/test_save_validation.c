/* Save-file validation tests: valid round-trip plus malformed payloads. */
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "save.h"

#define SAVE_PATH "tests/save-validation.bin"
#include "../data/balance.h"
#define VERSION SAVE_VERSION

static void raw_write(const unsigned char *payload, size_t length,
                      uint32_t declared_size, uint32_t version,
                      int trailing_byte, int corrupt_magic)
{
    unsigned char magic[8] = { 'T', 'O', 'L', 'S', 'V', '0', '0', '1' };
    if (corrupt_magic) magic[0] = 'X';
    FILE *file = fopen(SAVE_PATH, "wb");
    assert(file != NULL);
    assert(fwrite(magic, 1, 8, file) == 8);
    assert(fwrite(&version, sizeof version, 1, file) == 1);
    assert(fwrite(&declared_size, sizeof declared_size, 1, file) == 1);
    if (length > 0) assert(fwrite(payload, 1, length, file) == length);
    if (trailing_byte) assert(fputc(0, file) != EOF);
    assert(fclose(file) == 0);
}

static void expect_rejected(const unsigned char *payload, size_t length,
                            uint32_t version, int trailing_byte,
                            int corrupt_magic, const GameState *seed)
{
    GameState destination, before;
    memcpy(&destination, seed, sizeof destination);
    destination.run.atp = 7777.0;      /* marker: a failed read must keep it */
    memcpy(&before, &destination, sizeof before);
    raw_write(payload, length, (uint32_t)sizeof(GameState), version,
              trailing_byte, corrupt_magic);
    assert(save_read(SAVE_PATH, &destination)
           == (version == VERSION ? SAVE_INVALID_DATA : SAVE_BAD_VERSION));
    assert(memcmp(&destination, &before, sizeof before) == 0);
}

static void test_payload_ranges(const GameState *good)
{
    GameState bad;
#define REJECT(field, value) do { \
    memcpy(&bad, good, sizeof bad); \
    bad.field = (value); \
    expect_rejected((const unsigned char *)&bad, sizeof bad, VERSION, 0, 0, good); \
} while (0)
    REJECT(save_version, 99);
    REJECT(phase, (GamePhase)99);
    REJECT(run.atp, INFINITY);
    REJECT(run.biomass, -1.0);
    REJECT(run.elapsed_time, NAN);
    REJECT(run.era, -1);
    REJECT(run.knowledge, NAN);
    REJECT(run.crisis_min_population, -1.0);
    REJECT(run.capacity_levels, -1);
    REJECT(run.reserve_levels, 6);
    REJECT(run.tech_levels[0], -1);
    REJECT(run.active_species_count, 0);
    REJECT(run.active_species_count, MAX_ACTIVE_SPECIES + 1);
    REJECT(run.active_species[0].species_id, MAX_SPECIES);
    REJECT(run.active_species[0].population, 0.0);
    REJECT(run.active_species[0].population, INFINITY);
    REJECT(run.active_species[0].is_main_lineage, false);
    REJECT(run.run_upgrade_levels[0], -1);
    REJECT(run.rng_state, 0u);
    REJECT(run.current_environment_id, MAX_ENVIRONMENTS);
    REJECT(run.next_environment_id, -2);
    REJECT(run.environment_time_left, -1.0);
    REJECT(run.event_interval_left, NAN);
    REJECT(run.crisis_start_population, INFINITY);
    REJECT(run.highest_biomass_milestone, 11);
    REJECT(run.reward_claimed, true);
    REJECT(run.mutation_offer.choice_count, MAX_MUTATION_CHOICES + 1);
    REJECT(legacy.evolution_points, -1);
    REJECT(legacy.levels[0], 9);
    REJECT(legacy.best_biomass, NAN);
    REJECT(legacy.best_era, -1);
#undef REJECT

    bad = *good;
    bad.run.mutation_offer.active = true;
    bad.run.mutation_offer.choice_count = 1;
    bad.run.mutation_offer.species_slot = MAX_ACTIVE_SPECIES;
    expect_rejected((const unsigned char *)&bad, sizeof bad, VERSION, 0, 0, good);
    bad.run.mutation_offer.species_slot = 0;
    bad.run.mutation_offer.mutation_ids[0] = MAX_MUTATIONS;
    expect_rejected((const unsigned char *)&bad, sizeof bad, VERSION, 0, 0, good);
    bad.run.mutation_offer.mutation_ids[0] = 1;
    bad.run.mutation_offer.dna_costs[0] = INFINITY;
    expect_rejected((const unsigned char *)&bad, sizeof bad, VERSION, 0, 0, good);
}

static void test_full_state_roundtrip(void)
{
    GameState game, loaded;
    game_init(&game);
    game.run.rng_state = 12345u;
    game.run.active_species_count = 2;
    game.run.active_species[1].species_id = 2;
    game.run.active_species[1].population = 15.0;
    game.run.active_species[1].mutations[3] = true;
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 2;
    game.legacy.codex_mutations[3] = true;
    game.run.mutation_offer.active = true;
    game.run.mutation_offer.choice_count = 2;
    game.run.mutation_offer.mutation_ids[0] = 1;
    game.run.mutation_offer.mutation_ids[1] = 2;
    game.run.mutation_offer.dna_costs[0] = 18.0;
    game.run.mutation_offer.dna_costs[1] = 18.0;
    for (int phase = PHASE_RUNNING; phase <= PHASE_LEGACY; ++phase) {
        game.phase = (GamePhase)phase;
        game.run.reward_claimed = phase == PHASE_LEGACY;
        assert(save_write(SAVE_PATH, &game) == SAVE_OK);
        assert(save_read(SAVE_PATH, &loaded) == SAVE_OK);
        assert(memcmp(&game, &loaded, sizeof game) == 0);
    }
}

int main(void)
{
    GameState good;
    game_init(&good);
    good.run.atp = 42.0;

    /* Valid round-trip. */
    assert(save_write(SAVE_PATH, &good) == SAVE_OK);
    GameState loaded;
    memset(&loaded, 0, sizeof loaded);
    assert(save_read(SAVE_PATH, &loaded) == SAVE_OK);
    assert(memcmp(&loaded, &good, sizeof good) == 0);

    /* Header written by save_write must be canonical. */
    FILE *file = fopen(SAVE_PATH, "rb");
    assert(file != NULL);
    unsigned char magic[8];
    uint32_t version = 0, size = 0;
    assert(fread(magic, 1, 8, file) == 8);
    assert(memcmp(magic, "TOLSV001", 8) == 0);
    assert(fread(&version, sizeof version, 1, file) == 1 && version == VERSION);
    assert(fread(&size, sizeof size, 1, file) == 1
           && size == (uint32_t)sizeof(GameState));
    assert(fclose(file) == 0);

    const unsigned char *bytes = (const unsigned char *)&good;
    const size_t full = sizeof good;
    test_payload_ranges(&good);
    test_full_state_roundtrip();

    /* Bad magic, truncated payload, trailing bytes. */
    expect_rejected(bytes, full, VERSION, 0, 1, &good);
    expect_rejected(bytes, full, VERSION + 1, 0, 0, &good);
    expect_rejected(bytes, full / 2, VERSION, 0, 0, &good);
    expect_rejected(bytes, full, VERSION, 1, 0, &good);
    raw_write(bytes, full, (uint32_t)full + 1, VERSION, 0, 0);
    loaded = good;
    assert(save_read(SAVE_PATH, &loaded) == SAVE_BAD_VERSION);
    assert(memcmp(&loaded, &good, full) == 0);

    /* Invalid resource payloads. */
    GameState negative;
    memcpy(&negative, &good, full);
    negative.run.atp = -1.0;
    expect_rejected((const unsigned char *)&negative, full, VERSION, 0, 0, &good);

    GameState not_a_number;
    memcpy(&not_a_number, &good, full);
    not_a_number.run.dna = NAN;
    expect_rejected((const unsigned char *)&not_a_number, full, VERSION, 0, 0, &good);

    /* Noncanonical bool representation at reward_claimed. */
    const size_t run = offsetof(GameState, run);
    const size_t legacy = offsetof(GameState, legacy);
    const size_t flags[] = {
        run + offsetof(RunState, reward_claimed),
        run + offsetof(RunState, adaptation_shield_used),
        run + offsetof(RunState, adaptation_shield_active),
        run + offsetof(RunState, mutation_offer) + offsetof(MutationOfferState, active),
        run + offsetof(RunState, active_species) + offsetof(SpeciesState, is_main_lineage),
        run + offsetof(RunState, active_species) + sizeof(SpeciesState)
            + offsetof(SpeciesState, mutations),
        run + offsetof(RunState, discovered_species),
        run + offsetof(RunState, discovered_mutations),
        run + offsetof(RunState, survived_crises),
        legacy + offsetof(LegacyState, codex_species),
        legacy + offsetof(LegacyState, codex_mutations)
    };
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; ++i) {
        GameState bad_bool;
        memcpy(&bad_bool, &good, full);
        ((unsigned char *)&bad_bool)[flags[i]] = 2;
        expect_rejected((const unsigned char *)&bad_bool, full, VERSION, 0, 0, &good);
    }

    /* Failed save_write must preserve the existing good save. */
    assert(save_write(SAVE_PATH, &good) == SAVE_OK);
    GameState rejected;
    memcpy(&rejected, &good, full);
    rejected.run.atp = -5.0;
    assert(save_write(SAVE_PATH, &rejected) == SAVE_INVALID_DATA);
    GameState preserved;
    memset(&preserved, 0, sizeof preserved);
    assert(save_read(SAVE_PATH, &preserved) == SAVE_OK);
    assert(memcmp(&preserved, &good, sizeof good) == 0);

    assert(remove(SAVE_PATH) == 0);
    puts("save validation tests passed");
    return 0;
}
