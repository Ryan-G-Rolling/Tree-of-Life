/*
 * mock_game.c  ——  假的 Member A 实现（临时）
 *
 * 目的只有一个：让界面能真的跑起来、数值会动、按钮有反馈。
 * 公式按 02 文档抄，但这不是权威实现，集成时整体删除。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mock_game.h"
#include "mock_tables.h"

#define FIXED_STEP 0.1
#define BASE_CLICK_ATP 1.0
#define STARTING_POP 10.0
#define MIN_POP 1.0
#define FIRST_EVENT_DELAY 90.0
#define EVENT_INTERVAL 75.0
#define SAVE_MAGIC "TOLSV001"

/* ---------------- 随机数（xorshift32，02 §11） ---------------- */

static unsigned int next_random(unsigned int *state)
{
    unsigned int v = *state;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    *state = v ? v : 0x9E3779B9u;
    return *state;
}

/* ---------------- 派生计算 ---------------- */

double mock_run_upgrade_cost(int type, int level)
{
    switch (type) {
    case UPGRADE_METABOLISM:   return floor(20.0 * pow(1.65, level));
    case UPGRADE_REPRODUCTION: return floor(35.0 * pow(1.70, level));
    default:                   return floor(25.0 * pow(1.75, level));
    }
}

static bool synergy_on(const GameState *game)
{
    if (game->legacy.levels[LEGACY_DIVERSIFICATION] < 3) return false;
    if (game->run.active_species_count < 2) return false;
    int a = MOCK_SPECIES[game->run.active_species[0].species_id].stage;
    (void)a;
    return strcmp(MOCK_SPECIES[game->run.active_species[0].species_id].ecology,
                  MOCK_SPECIES[game->run.active_species[1].species_id].ecology) != 0;
}

void mock_calc_rates(const GameState *game, int slot, SpeciesRates *out)
{
    memset(out, 0, sizeof(*out));
    if (slot >= game->run.active_species_count) {
        out->environment_multiplier = 1.0;
        return;
    }

    const SpeciesState *sp = &game->run.active_species[slot];
    const MockSpecies *s = &MOCK_SPECIES[sp->species_id];

    double pscale = sp->population / 10.0;
    double env = mock_species_fitness(game, slot, NULL, NULL);
    double run_metab = 1.0 + 0.25 * game->run.run_upgrade_levels[UPGRADE_METABOLISM];
    double run_repro = 1.0 + 0.20 * game->run.run_upgrade_levels[UPGRADE_REPRODUCTION];
    double run_genome = 1.0 + 0.25 * game->run.run_upgrade_levels[UPGRADE_GENOME];

    int ml = game->legacy.levels[LEGACY_METABOLISM];
    double perm_atp = (ml >= 1 ? 1.08 : 1.0) * (ml >= 4 ? 1.12 : 1.0);
    double perm_bio = (ml >= 2 ? 1.08 : 1.0) * (ml >= 4 ? 1.12 : 1.0);
    double perm_dna = (ml >= 4 ? 1.12 : 1.0);

    double synergy = synergy_on(game) ? 1.10 : 1.0;
    out->synergy_active = synergy > 1.0;

    out->atp_per_second = s->atp * pscale * run_metab * mock_mutation_multiplier(sp, 0) * env * perm_atp * synergy;
    out->biomass_per_second = s->biomass * pscale * run_repro * mock_mutation_multiplier(sp, 1) * env * perm_bio * synergy;
    out->dna_per_second = s->dna * pscale * run_genome * mock_mutation_multiplier(sp, 2) * env * perm_dna * synergy;

    out->final_population_cap = s->cap * mock_mutation_multiplier(sp, 3)
                              * (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 1 ? 1.10 : 1.0);

    double cap = out->final_population_cap;
    double positive = 0.0;
    if (sp->population < cap) {
        positive = sp->population * s->growth * run_repro * mock_mutation_multiplier(sp, 4)
                 * (1.0 - sp->population / cap);
    }
    double damage = MOCK_ENVIRONMENTS[game->run.current_environment_id].damage_per_second;
    double loss = sp->population * damage * (2.0 - env);
    if (loss < 0.0) loss = 0.0;
    out->population_delta_per_second = positive - loss;
    out->environment_multiplier = env;
}

/* ---------------- 初始化 ---------------- */

static void reset_run(GameState *game, bool keep_era)
{
    int era = keep_era ? game->run.era : 0;
    unsigned int seed = game->run.rng_state;

    memset(&game->run, 0, sizeof(game->run));
    game->run.rng_state = seed ? seed : (unsigned int)time(NULL);
    game->run.era = era;
    game->run.active_species_count = 1;
    game->run.active_species[0].species_id = SPECIES_PROTOCELL;
    game->run.active_species[0].is_main_lineage = true;
    game->run.active_species[0].population = STARTING_POP;

    game->run.current_environment_id = 0;
    game->run.next_environment_id = 1 + (int)(next_random(&game->run.rng_state) % 5u);
    game->run.environment_time_left = FIRST_EVENT_DELAY;
    game->run.event_interval_left = FIRST_EVENT_DELAY;
    game->run.discovered_species[SPECIES_PROTOCELL] = true;

    if (game->legacy.levels[LEGACY_METABOLISM] >= 3) {
        game->run.atp = 50.0;
        game->run.run_upgrade_levels[UPGRADE_METABOLISM] = 1;
    }

    game->legacy.codex_species[SPECIES_PROTOCELL] = true;
}

void game_init(GameState *game)
{
    memset(game, 0, sizeof(*game));
    game->save_version = 1;
    game->phase = PHASE_RUNNING;
    game->run.rng_state = (unsigned int)time(NULL) ^ 0xA5A5A5u;
    reset_run(game, false);
}

/* ---------------- 突变候选 ---------------- */

static void generate_offer(GameState *game, int slot)
{
    MutationOfferState *offer = &game->run.mutation_offer;
    if (offer->active) return;

    int pool[MUTATION_COUNT];
    int n = 0;
    for (int i = 0; i < MUTATION_COUNT; ++i) {
        if (mock_mutation_available(game, slot, i)) pool[n++] = i;
    }

    int want = game->legacy.levels[LEGACY_MUTATION] >= 3 ? 3 : 2;
    int rare_weight = game->legacy.levels[LEGACY_MUTATION] >= 4 ? 40 : 20;

    offer->choice_count = 0;
    offer->species_slot = slot;
    offer->active = true;

    /* 关键突变保底：生物量接近分化门槛时，保证出现缺口突变 */
    int main_id = game->run.active_species[slot].species_id;
    if (game->run.biomass >= 48.0) {
        for (int c = 0; c < SPECIES_COUNT; ++c) {
            if (!mock_speciation_parent_ok(c, main_id)) continue;
            /* 简化：只要该物种的关键突变还没拿到，优先塞进候选 */
            int key = -1;
            if (c == SPECIES_CYANOBACTERIUM && !game->run.active_species[slot].mutations[MUTATION_PHOTOSYNTHESIS]) key = MUTATION_PHOTOSYNTHESIS;
            if (c == SPECIES_CHEMOLITHOTROPH && !game->run.active_species[slot].mutations[MUTATION_CHEMOSYNTHESIS]) key = MUTATION_CHEMOSYNTHESIS;
            if (key >= 0) {
                bool ok = false;
                for (int i = 0; i < n; ++i) if (pool[i] == key) ok = true;
                if (ok) {
                    offer->mutation_ids[offer->choice_count++] = key;
                    offer->dna_costs[0] = mock_mutation_cost(game, key);
                    break;
                }
            }
        }
    }

    while (offer->choice_count < want) {
        int total = 0;
        for (int i = 0; i < n; ++i) {
            bool picked = false;
            for (int k = 0; k < offer->choice_count; ++k)
                if (offer->mutation_ids[k] == pool[i]) picked = true;
            if (picked) continue;
            total += MOCK_MUTATIONS[pool[i]].rare ? rare_weight : 100;
        }
        if (total <= 0) break;

        int roll = (int)(next_random(&game->run.rng_state) % (unsigned int)total);
        int chosen = -1;
        for (int i = 0; i < n; ++i) {
            bool picked = false;
            for (int k = 0; k < offer->choice_count; ++k)
                if (offer->mutation_ids[k] == pool[i]) picked = true;
            if (picked) continue;
            roll -= MOCK_MUTATIONS[pool[i]].rare ? rare_weight : 100;
            if (roll < 0) { chosen = pool[i]; break; }
        }
        if (chosen < 0) break;

        offer->mutation_ids[offer->choice_count] = chosen;
        offer->dna_costs[offer->choice_count] = mock_mutation_cost(game, chosen);
        offer->choice_count++;
    }

    if (offer->choice_count == 0) offer->active = false;
}

/* ---------------- 外部接口 ---------------- */

void game_update(GameState *game, double dt)
{
    if (game->phase != PHASE_RUNNING) return;
    if (dt <= 0.0) return;

    SpeciesRates rates[MAX_ACTIVE_SPECIES];
    double sum_atp = 0.0, sum_bio = 0.0, sum_dna = 0.0;
    double new_pop[MAX_ACTIVE_SPECIES];

    for (int i = 0; i < game->run.active_species_count; ++i) {
        mock_calc_rates(game, i, &rates[i]);
        sum_atp += rates[i].atp_per_second;
        sum_bio += rates[i].biomass_per_second;
        sum_dna += rates[i].dna_per_second;
        new_pop[i] = game->run.active_species[i].population + rates[i].population_delta_per_second * dt;
    }

    /* 一次性写回，避免数组顺序影响结果（02 §2） */
    for (int i = 0; i < game->run.active_species_count; ++i) {
        double cap = rates[i].final_population_cap;
        if (new_pop[i] < MIN_POP) new_pop[i] = MIN_POP;
        if (new_pop[i] > cap) new_pop[i] = cap;
        game->run.active_species[i].population = new_pop[i];
    }

    game->run.atp += sum_atp * dt;
    game->run.biomass += sum_bio * dt;
    game->run.dna += sum_dna * dt;
    game->run.elapsed_time += dt;

    if (game->run.biomass > game->run.max_biomass) game->run.max_biomass = game->run.biomass;
    while (game->run.highest_biomass_milestone + 1 < MOCK_MILESTONE_COUNT
           && game->run.max_biomass >= MOCK_MILESTONES[game->run.highest_biomass_milestone + 1]) {
        game->run.highest_biomass_milestone++;
    }

    /* 环境计时 */
    game->run.environment_time_left -= dt;
    if (game->run.environment_time_left <= 0.0) {
        if (game->run.current_environment_id == 0) {
            int next = game->run.next_environment_id;
            if (next <= 0 || next > 6) next = 1;
            game->run.current_environment_id = next;
            double dur = MOCK_ENVIRONMENTS[next].duration;
            if (game->legacy.levels[LEGACY_ADAPTATION] >= 2) dur *= 0.85;
            game->run.environment_time_left = dur;
            game->run.crisis_start_population = game->run.active_species[0].population;

            if (next == 6) {
                game->run.active_species[0].population *= 0.65;
                if (game->run.active_species[0].population < MIN_POP)
                    game->run.active_species[0].population = MIN_POP;
            }

            /* 预排下一个事件（不与当前相同） */
            unsigned int pick = 1u + next_random(&game->run.rng_state) % 6u;
            if ((int)pick == next) pick = (pick % 6u) + 1u;
            game->run.next_environment_id = (int)pick;
        } else {
            int ended = game->run.current_environment_id;
            double need = game->run.crisis_start_population * 0.25;
            if (need < 1.0) need = 1.0;
            if (game->run.active_species[0].population >= need) {
                game->run.survived_crises[ended] = true;
            }
            game->run.adaptation_shield_active = false;
            game->run.current_environment_id = 0;
            game->run.environment_time_left = EVENT_INTERVAL;
        }
    }
}

static bool has_mutation(const GameState *game, int slot, int id)
{
    return game->run.active_species[slot].mutations[id];
}

ActionCode mock_validate_speciation(const GameState *game, int child)
{
    if (game->run.active_species_count <= 0) return ACTION_INVALID_TARGET;
    int parent = game->run.active_species[0].species_id;
    if (!mock_speciation_parent_ok(child, parent)) return ACTION_REQUIREMENT_NOT_MET;

    switch (child) {
    case SPECIES_CYANOBACTERIUM:
        if (!has_mutation(game, 0, MUTATION_PHOTOSYNTHESIS)) return ACTION_REQUIREMENT_NOT_MET;
        break;
    case SPECIES_CHEMOLITHOTROPH:
        if (!has_mutation(game, 0, MUTATION_CHEMOSYNTHESIS)) return ACTION_REQUIREMENT_NOT_MET;
        break;
    case SPECIES_ALGAL_EUKARYOTE:
        if (!has_mutation(game, 0, MUTATION_NUCLEUS) || !has_mutation(game, 0, MUTATION_PHOTOSYNTHESIS))
            return ACTION_REQUIREMENT_NOT_MET;
        break;
    case SPECIES_PROTOZOAN:
        if (!has_mutation(game, 0, MUTATION_NUCLEUS) || !has_mutation(game, 0, MUTATION_MITOCHONDRIA))
            return ACTION_REQUIREMENT_NOT_MET;
        break;
    case SPECIES_MULTICELLULAR_COLONY:
        if (!has_mutation(game, 0, MUTATION_MULTICELLULARITY)) return ACTION_REQUIREMENT_NOT_MET;
        break;
    case SPECIES_EXTREMOPHILE_COLONY:
        if (!has_mutation(game, 0, MUTATION_HEAT_RESISTANCE) || !has_mutation(game, 0, MUTATION_TOXIN_RESISTANCE))
            return ACTION_REQUIREMENT_NOT_MET;
        if (game->legacy.levels[LEGACY_DIVERSIFICATION] < 5) return ACTION_REQUIREMENT_NOT_MET;
        break;
    default:
        return ACTION_INVALID_TARGET;
    }

    double cost = MOCK_SPECIES[child].speci_cost;
    if (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 4) cost *= 0.85;
    if (game->run.biomass < cost) return ACTION_NOT_ENOUGH_BIOMASS;
    return ACTION_OK;
}

ActionResult game_apply_action(GameState *game, GameAction action)
{
    ActionResult r;
    memset(&r, 0, sizeof(r));
    r.code = ACTION_OK;
    r.affected_id = action.target_id;

    switch (action.type) {
    case ACTION_CLICK_ATP: {
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        double click = BASE_CLICK_ATP
                     * (1.0 + 0.25 * game->run.run_upgrade_levels[UPGRADE_METABOLISM])
                     * (game->legacy.levels[LEGACY_METABOLISM] >= 1 ? 1.08 : 1.0)
                     * (game->legacy.levels[LEGACY_METABOLISM] >= 4 ? 1.12 : 1.0);
        game->run.atp += click;
        r.amount_spent = click;
        break;
    }
    case ACTION_BUY_RUN_UPGRADE: {
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        int type = action.target_id;
        if (type < 0 || type > 2) { r.code = ACTION_INVALID_TARGET; break; }
        double cost = mock_run_upgrade_cost(type, game->run.run_upgrade_levels[type]);
        if (type == UPGRADE_GENOME) {
            if (game->run.biomass < cost) { r.code = ACTION_NOT_ENOUGH_BIOMASS; break; }
            game->run.biomass -= cost;
        } else {
            if (game->run.atp < cost) { r.code = ACTION_NOT_ENOUGH_ATP; break; }
            game->run.atp -= cost;
        }
        game->run.run_upgrade_levels[type]++;
        r.amount_spent = cost;
        break;
    }
    case ACTION_GENERATE_MUTATION_OFFER: {
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        int slot = action.target_species_slot;
        if (slot < 0 || slot >= game->run.active_species_count) slot = 0;
        generate_offer(game, slot);
        if (!game->run.mutation_offer.active) { r.code = ACTION_REQUIREMENT_NOT_MET; break; }
        r.affected_id = game->run.mutation_offer.mutation_ids[0];
        break;
    }
    case ACTION_CHOOSE_MUTATION: {
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        if (!game->run.mutation_offer.active) { r.code = ACTION_INVALID_TARGET; break; }
        int idx = -1;
        for (int i = 0; i < game->run.mutation_offer.choice_count; ++i)
            if (game->run.mutation_offer.mutation_ids[i] == action.target_id) idx = i;
        if (idx < 0) { r.code = ACTION_INVALID_TARGET; break; }
        double cost = game->run.mutation_offer.dna_costs[idx];
        if (game->run.dna < cost) { r.code = ACTION_NOT_ENOUGH_DNA; break; }

        int slot = game->run.mutation_offer.species_slot;
        game->run.dna -= cost;
        game->run.active_species[slot].mutations[action.target_id] = true;
        game->run.discovered_mutations[action.target_id] = true;
        game->legacy.codex_mutations[action.target_id] = true;
        memset(&game->run.mutation_offer, 0, sizeof(game->run.mutation_offer));
        r.amount_spent = cost;
        r.affected_id = action.target_id;
        break;
    }
    case ACTION_SPECIATE: {
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        int child = action.target_id;
        r.code = mock_validate_speciation(game, child);
        if (r.code != ACTION_OK) break;

        double cost = MOCK_SPECIES[child].speci_cost;
        if (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 4) cost *= 0.85;
        game->run.biomass -= cost;
        r.amount_spent = cost;

        SpeciesState parent = game->run.active_species[0];
        double parent_pop = parent.population;

        SpeciesState offspring = parent;
        offspring.species_id = child;
        offspring.is_main_lineage = true;

        bool keep = (action.option == 1) && (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 2);
        if (keep) {
            offspring.population = parent_pop * 0.60;
            if (offspring.population < 10.0) offspring.population = 10.0;
            parent.is_main_lineage = false;
            parent.population = parent_pop * 0.40;
            game->run.active_species[0] = offspring;
            game->run.active_species[1] = parent;
            game->run.active_species_count = 2;
        } else {
            offspring.population = parent_pop * 0.70;
            if (offspring.population < 10.0) offspring.population = 10.0;
            game->run.active_species[0] = offspring;
            if (game->run.active_species_count > 1) game->run.active_species_count = 2;
        }

        game->run.discovered_species[child] = true;
        game->legacy.codex_species[child] = true;
        memset(&game->run.mutation_offer, 0, sizeof(game->run.mutation_offer));
        r.affected_id = child;
        break;
    }
    case ACTION_OPEN_END_ERA:
        if (game->phase != PHASE_RUNNING) { r.code = ACTION_INVALID_PHASE; break; }
        game->phase = PHASE_END_ERA_PREVIEW;
        break;
    case ACTION_CANCEL_END_ERA:
        if (game->phase != PHASE_END_ERA_PREVIEW) { r.code = ACTION_INVALID_PHASE; break; }
        game->phase = PHASE_RUNNING;
        break;
    case ACTION_CONFIRM_END_ERA: {
        if (game->phase != PHASE_END_ERA_PREVIEW) { r.code = ACTION_INVALID_PHASE; break; }
        if (game->run.reward_claimed) { r.code = ACTION_ALREADY_CLAIMED; break; }
        int e = 0, m = 0, s = 0, c = 0, b = game->run.highest_biomass_milestone;
        e = MOCK_SPECIES[game->run.active_species[0].species_id].stage;
        for (int i = 0; i < MUTATION_COUNT; ++i) if (game->run.discovered_mutations[i]) m++;
        for (int i = 1; i < SPECIES_COUNT; ++i) if (game->run.discovered_species[i]) s++;
        for (int i = 1; i < 7; ++i) if (game->run.survived_crises[i]) c++;
        if (c > 3) c = 3;
        int total = 2 * e + m / 3 + s / 2 + c + b;
        game->legacy.evolution_points += total;
        game->run.reward_claimed = true;
        game->phase = PHASE_LEGACY;
        if (game->run.era > game->legacy.best_era) game->legacy.best_era = game->run.era;
        if (total > game->legacy.best_evolution_points) game->legacy.best_evolution_points = total;
        if (game->run.max_biomass > game->legacy.best_biomass) game->legacy.best_biomass = game->run.max_biomass;
        r.amount_spent = total;
        break;
    }
    case ACTION_BUY_LEGACY: {
        if (game->phase != PHASE_LEGACY) { r.code = ACTION_INVALID_PHASE; break; }
        int branch = action.target_id;
        if (branch < 0 || branch > 3) { r.code = ACTION_INVALID_TARGET; break; }
        int level = game->legacy.levels[branch];
        if (level >= 5) { r.code = ACTION_LEVEL_MAXED; break; }
        int cost = level + 1;
        if (game->legacy.evolution_points < cost) { r.code = ACTION_NOT_ENOUGH_EP; break; }
        game->legacy.evolution_points -= cost;
        game->legacy.levels[branch]++;
        r.amount_spent = cost;
        break;
    }
    case ACTION_START_NEW_RUN:
        if (game->phase != PHASE_LEGACY) { r.code = ACTION_INVALID_PHASE; break; }
        game_start_new_run(game);
        break;
    default:
        r.code = ACTION_INVALID_TARGET;
        break;
    }

    return r;
}

void game_start_new_run(GameState *game)
{
    int era = game->run.era;
    reset_run(game, true);
    game->run.era = era + 1;
    game->phase = PHASE_RUNNING;
}

/* ---------------- 存档（临时实现，格式同 02 §12） ---------------- */

SaveResult save_write(const char *path, const GameState *game)
{
    FILE *f = fopen(path, "wb");
    if (!f) return SAVE_FILE_ERROR;
    unsigned int version = 1;
    unsigned int size = (unsigned int)sizeof(GameState);
    fwrite(SAVE_MAGIC, 1, 8, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&size, sizeof(size), 1, f);
    fwrite(game, sizeof(GameState), 1, f);
    fclose(f);
    return SAVE_OK;
}

SaveResult save_read(const char *path, GameState *out_game)
{
    FILE *f = fopen(path, "rb");
    if (!f) return SAVE_FILE_ERROR;

    char magic[8] = { 0 };
    unsigned int version = 0, size = 0;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, SAVE_MAGIC, 8) != 0) { fclose(f); return SAVE_INVALID_DATA; }
    if (fread(&version, sizeof(version), 1, f) != 1) { fclose(f); return SAVE_INVALID_DATA; }
    if (fread(&size, sizeof(size), 1, f) != 1) { fclose(f); return SAVE_INVALID_DATA; }
    if (version != 1) { fclose(f); return SAVE_BAD_VERSION; }
    if (size != sizeof(GameState)) { fclose(f); return SAVE_INVALID_DATA; }

    GameState tmp;
    if (fread(&tmp, sizeof(GameState), 1, f) != 1) { fclose(f); return SAVE_INVALID_DATA; }
    fclose(f);

    if (tmp.phase < PHASE_RUNNING || tmp.phase > PHASE_LEGACY) return SAVE_INVALID_DATA;
    if (tmp.run.active_species_count < 0 || tmp.run.active_species_count > MAX_ACTIVE_SPECIES) return SAVE_INVALID_DATA;
    if (tmp.run.atp < 0.0 || tmp.run.biomass < 0.0 || tmp.run.dna < 0.0) return SAVE_INVALID_DATA;
    if (tmp.legacy.evolution_points < 0) return SAVE_INVALID_DATA;
    for (int i = 0; i < 4; ++i) if (tmp.legacy.levels[i] < 0 || tmp.legacy.levels[i] > 5) return SAVE_INVALID_DATA;

    *out_game = tmp;
    return SAVE_OK;
}

/* ---------------- 演示状态 ---------------- */

void mock_fill_demo_state(GameState *game)
{
    game_init(game);

    game->run.era = 3;
    game->run.elapsed_time = 431.0;
    game->run.atp = 1840.0;
    game->run.biomass = 742.0;
    game->run.dna = 63.0;
    game->run.max_biomass = 910.0;
    game->run.highest_biomass_milestone = 2;
    game->run.run_upgrade_levels[UPGRADE_METABOLISM] = 3;
    game->run.run_upgrade_levels[UPGRADE_REPRODUCTION] = 2;
    game->run.run_upgrade_levels[UPGRADE_GENOME] = 1;

    game->run.active_species_count = 2;
    game->run.active_species[0].species_id = SPECIES_CYANOBACTERIUM;
    game->run.active_species[0].is_main_lineage = true;
    game->run.active_species[0].population = 118.0;
    game->run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    game->run.active_species[0].mutations[MUTATION_CELL_MEMBRANE] = true;

    game->run.active_species[1].species_id = SPECIES_CHEMOLITHOTROPH;
    game->run.active_species[1].is_main_lineage = false;
    game->run.active_species[1].population = 54.0;
    game->run.active_species[1].mutations[MUTATION_CHEMOSYNTHESIS] = true;

    game->legacy.evolution_points = 6;
    game->legacy.levels[LEGACY_METABOLISM] = 2;
    game->legacy.levels[LEGACY_MUTATION] = 1;
    game->legacy.levels[LEGACY_ADAPTATION] = 3;
    game->legacy.levels[LEGACY_DIVERSIFICATION] = 2;
    game->legacy.best_era = 4;
    game->legacy.best_evolution_points = 19;
    game->legacy.best_biomass = 1420.0;
    game->legacy.achievement_flags = 0x5;

    for (int i = 0; i < SPECIES_COUNT; ++i) if (i <= 2) game->legacy.codex_species[i] = true;
    game->legacy.codex_mutations[MUTATION_PHOTOSYNTHESIS] = true;
    game->legacy.codex_mutations[MUTATION_CHEMOSYNTHESIS] = true;
    game->legacy.codex_mutations[MUTATION_CELL_MEMBRANE] = true;
    game->legacy.codex_mutations[MUTATION_CELL_WALL] = true;
    game->legacy.codex_mutations[MUTATION_HEAT_RESISTANCE] = true;

    game->run.discovered_species[SPECIES_CYANOBACTERIUM] = true;
    game->run.discovered_species[SPECIES_CHEMOLITHOTROPH] = true;
    game->run.discovered_mutations[MUTATION_PHOTOSYNTHESIS] = true;
    game->run.discovered_mutations[MUTATION_CHEMOSYNTHESIS] = true;
    game->run.discovered_mutations[MUTATION_CELL_MEMBRANE] = true;
    game->run.survived_crises[1] = true;

    game->run.current_environment_id = 3;
    game->run.next_environment_id = 2;
    game->run.environment_time_left = 27.0;
    game->run.crisis_start_population = 124.0;

    generate_offer(game, 0);
}
