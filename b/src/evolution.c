/*
 * Member B · evolution.c
 * 突变池、候选生成、突变应用、物种分化与物种产率
 *
 * 数据来源：02-数据与计算规则 §5、§6、§8
 * 本文件不包含 raylib.h；不直接推进模拟，只计算并返回结果。
 */

#include "evolution.h"
#include "environment.h"
#include "legacy.h"
#include "balance.h"

#include <math.h>
#include <string.h>

/* 字段顺序：name, description, dna, atp%, biomass%, dna%, cap%, growth%,
 *           traits[HEAT, COLD, LOW_LIGHT, OXYGEN, TOXIN], rarity,
 *           min_phase, max_phase, requires_mutation, requires_branch, requires_level */
static const MutationDef k_mutations[MUTATION_COUNT] = {
    /* 0 Cell Membrane */
    { "Cell Membrane", "A tougher membrane keeps biomass in and toxins out.",
      12.0, 0.0, 0.10, 0.0, 0.0, 0.0, { 0, 0, 0, 0, 1 },
      0, 0, -1, -1, -1, -1 },
    /* 1 Photosynthesis（第十轮：取消阶段上限，阶段 2 之后补课价格 ×2） */
    { "Photosynthesis", "Turns light into ATP. Weak in heat, strong in dim water.",
      18.0, 0.25, 0.0, 0.0, 0.0, 0.0, { 0, 0, -1, 0, 0 },
      0, 0, -1, -1, -1, -1 },
    /* 2 Chemosynthesis（第十轮：同上） */
    { "Chemosynthesis", "Extracts biomass from chemical gradients.",
      18.0, 0.0, 0.20, 0.0, 0.0, 0.0, { 0, 0, 2, 0, 0 },
      0, 0, -1, -1, -1, -1 },
    /* 3 Cell Wall */
    { "Cell Wall", "A rigid wall raises the population ceiling.",
      22.0, 0.0, 0.0, 0.0, 0.20, 0.0, { 0, 1, 0, 0, 1 },
      0, 0, -1, -1, -1, -1 },
    /* 4 Heat Resistance */
    { "Heat Resistance", "Heat-shock proteins keep growth steady when warm.",
      28.0, 0.0, 0.0, 0.0, 0.0, 0.05, { 2, 0, 0, 0, 0 },
      0, 0, -1, -1, -1, -1 },
    /* 5 Cold Resistance */
    { "Cold Resistance", "Antifreeze compounds protect the cell in the cold.",
      28.0, 0.0, 0.05, 0.0, 0.0, 0.0, { 0, 2, 0, 0, 0 },
      0, 0, -1, -1, -1, -1 },
    /* 6 Nucleus */
    { "Nucleus", "Compartmentalised DNA boosts replication.",
      45.0, 0.0, 0.0, 0.30, 0.0, 0.0, { 0, 0, 0, 1, 0 },
      0, 1, -1, -1, -1, -1 },
    /* 7 Mitochondria */
    { "Mitochondria", "Aerobic respiration multiplies ATP output.",
      55.0, 0.30, 0.0, 0.0, 0.0, 0.0, { 0, 0, 0, 2, 0 },
      0, 1, -1, MUTATION_NUCLEUS, -1, -1 },
    /* 8 Multicellularity */
    { "Multicellularity", "Cell differentiation supports a much larger body.",
      90.0, 0.0, 0.20, 0.0, 0.40, 0.0, { 0, 1, 0, 0, 0 },
      0, 2, -1, -1, -1, -1 },
    /* 9 Toxin Resistance */
    { "Toxin Resistance", "Pumps toxins out before they damage the genome.",
      50.0, 0.0, 0.0, 0.05, 0.0, 0.0, { 0, 0, 0, 0, 2 },
      1, 0, -1, -1, -1, -1 },
    /* 10 Efficient Respiration（第三轮：取消永久等级前置，单纪元内也可发现） */
    { "Efficient Respiration", "Advanced metabolism squeezes more ATP per molecule.",
      80.0, 0.35, 0.0, 0.0, 0.0, 0.0, { 0, 0, 0, 1, 0 },
      1, 0, -1, -1, -1, -1 },
    /* 11 Adaptive Hypermutation（第三轮：同上，避免内容发现被永久等级卡死） */
    { "Adaptive Hypermutation", "Stress-tolerant replication reshapes the genome.",
      100.0, 0.0, 0.0, 0.25, 0.0, 0.0, { 1, 1, 0, 0, 0 },
      2, 0, -1, -1, -1, -1 },
    /* 12 Cell Differentiation */
    { "Cell Differentiation", "Tissues take on specialised roles and raise the ceiling.",
      110.0, 0.0, 0.15, 0.0, 0.30, 0.06, { 0, 0, 0, 0, 0 },
      0, 3, -1, MUTATION_MULTICELLULARITY, -1, -1 },
    /* 13 Neural Signaling */
    { "Neural Signaling", "A signalling network coordinates the whole colony.",
      130.0, 0.20, 0.0, 0.15, 0.0, 0.04, { 0, 0, 0, 1, 0 },
      1, 4, -1, MUTATION_NUCLEUS, -1, -1 },
    /* 14 Symbiotic Nitrogen Fixation */
    { "Symbiotic Nitrogen Fixation", "Partner microbes unlock a new nutrient cycle.",
      95.0, 0.0, 0.25, 0.0, 0.0, 0.0, { 0, 0, 1, 0, 0 },
      0, 3, -1, MUTATION_CHEMOSYNTHESIS, -1, -1 },
    /* 15 Extremozyme Cocktail */
    { "Extremozyme Cocktail", "A blend of enzymes that works across temperature extremes.",
      90.0, 0.0, 0.0, 0.0, 0.0, 0.05, { 2, 2, 0, 0, 0 },
      0, 3, -1, MUTATION_HEAT_RESISTANCE, -1, -1 },
    /* 16 Quorum Sensing */
    { "Quorum Sensing", "Cells act together once the population crosses a threshold.",
      105.0, 0.0, 0.0, 0.0, 0.25, 0.08, { 0, 0, 0, 1, 0 },
      0, 3, -1, MUTATION_MULTICELLULARITY, -1, -1 },
    /* 17 Ecosystem Engineering */
    { "Ecosystem Engineering", "The colony reshapes its environment to its own benefit.",
      150.0, 0.0, 0.30, 0.0, 0.0, 0.0, { 0, 0, 0, 0, 2 },
      1, 4, -1, MUTATION_TOXIN_RESISTANCE, -1, -1 },
    /* 18 Decomposer Enzymes（第四轮：真菌网络前置） */
    { "Decomposer Enzymes", "Breaks down detritus into usable biomass.",
      85.0, 0.0, 0.25, 0.10, 0.0, 0.0, { 0, 0, 0, 0, 1 },
      0, 2, -1, MUTATION_MULTICELLULARITY, -1, -1 },
    /* 19 Photosymbiosis（第四轮：珊瑚共生体前置） */
    { "Photosymbiosis", "Algal partners trade sugar for shelter.",
      95.0, 0.20, 0.15, 0.0, 0.10, 0.0, { 0, 0, 1, 0, 0 },
      1, 2, -1, MUTATION_PHOTOSYNTHESIS, -1, -1 },
    /* 20 Chitin Wall */
    { "Chitin Wall", "A flexible armour raises the population ceiling.",
      70.0, 0.0, 0.15, 0.0, 0.30, 0.0, { 0, 1, 0, 0, 1 },
      0, 1, -1, -1, -1, -1 },
    /* 21 Nitrogen Sense */
    { "Nitrogen Sense", "Senses fixed nitrogen and replicates faster.",
      75.0, 0.0, 0.0, 0.25, 0.0, 0.04, { 0, 0, 0, 0, 1 },
      0, 2, -1, MUTATION_CHEMOSYNTHESIS, -1, -1 },
    /* 22 Mycorrhizal Symbiosis（第六轮：菌根网络前置） */
    { "Mycorrhizal Symbiosis", "Roots and fungi trade nutrients across the colony.",
      120.0, 0.0, 0.30, 0.15, 0.10, 0.05, { 0, 1, 0, 0, 1 },
      1, 4, -1, MUTATION_SYMBIOTIC_NITROGEN_FIXATION, -1, -1 },
    /* 23 Reef Architecture（第六轮：礁城前置） */
    { "Reef Architecture", "Layered mineral scaffolds host a dense reef city.",
      110.0, 0.15, 0.20, 0.0, 0.25, 0.05, { 0, 0, 1, 0, 0 },
      0, 3, -1, MUTATION_PHOTOSYMBIOSIS, -1, -1 },
    /* 24 Abstract Thought（第七轮：智慧生命前置） */
    { "Abstract Thought", "A mind that can plan ahead and transmit ideas.",
      180.0, 0.20, 0.0, 0.20, 0.20, 0.06, { 0, 0, 0, 2, 0 },
      2, 5, -1, MUTATION_NEURAL_SIGNALING, -1, -1 },
    /* 25 Planetary Industry（第七轮：技术圈前置） */
    { "Planetary Industry", "Industry reshapes the whole biosphere.",
      220.0, 0.25, 0.30, 0.0, 0.30, 0.06, { 0, 0, 0, 0, 2 },
      2, 6, -1, MUTATION_ABSTRACT_THOUGHT, -1, -1 },
    /* 26 Signal Literacy */
    { "Signal Literacy", "Signals are read, stored and recombined.",
      160.0, 0.0, 0.0, 0.30, 0.0, 0.05, { 0, 0, 0, 2, 0 },
      1, 5, -1, MUTATION_QUORUM_SENSING, -1, -1 },
    /* 27 Language（第八轮：文明前置） */
    { "Language", "Shared symbols let a whole planet plan together.",
      260.0, 0.15, 0.0, 0.20, 0.20, 0.05, { 0, 0, 0, 2, 1 },
      2, 7, -1, MUTATION_ABSTRACT_THOUGHT, -1, -1 },
    /* 28 Rocketry（第八轮：太空时代前置） */
    { "Rocketry", "Heavy-lift industry reaches orbit.",
      420.0, 0.25, 0.20, 0.0, 0.30, 0.05, { 2, 0, 0, 2, 0 },
      2, 8, -1, MUTATION_LANGUAGE, -1, -1 },
    /* 29 Recursive Self-Improvement（第八轮：奇点前置） */
    { "Recursive Self-Improvement", "Intelligence improves the machinery that improves it.",
      700.0, 0.35, 0.0, 0.35, 0.40, 0.08, { 2, 2, 2, 2, 2 },
      2, 9, -1, MUTATION_ROCKETRY, -1, -1 },
    /* 30 Urbanization */
    { "Urbanization", "Dense cities raise biomass and the population ceiling.",
      200.0, 0.0, 0.25, 0.0, 0.40, 0.0, { 0, 0, 0, 0, 1 },
      1, 7, -1, MUTATION_LANGUAGE, -1, -1 },
    /* 31 Orbital Habitats */
    { "Orbital Habitats", "Habitats in orbit multiply the carrying capacity.",
      320.0, 0.0, 0.25, 0.0, 0.50, 0.0, { 0, 0, 0, 1, 0 },
      1, 8, -1, MUTATION_ROCKETRY, -1, -1 },
    /* 32 Machine Intelligence */
    { "Machine Intelligence", "Machines extend the mind into every process.",
      500.0, 0.40, 0.0, 0.30, 0.25, 0.06, { 0, 0, 0, 2, 0 },
      2, 8, -1, MUTATION_ABSTRACT_THOUGHT, -1, -1 },
    /* 33 Omega Point（第十二轮：阶段 11 前置） */
    { "Omega Point", "The process folds every layer of reality into one act of will.",
      200.0, 0.50, 0.0, 0.40, 0.50, 0.08, { 1, 1, 1, 1, 1 },
      2, 10, -1, MUTATION_RECURSIVE_SELF_IMPROVEMENT, -1, -1 },
};

/* 突变共振：同一谱系同时拥有两个突变时解锁的构筑加成 */
static const MutationSynergyDef k_synergies[] = {
    { "Algal Mat", "Photosynthesis + Cell Wall: a sticky mat that traps light.",
      MUTATION_PHOTOSYNTHESIS, MUTATION_CELL_WALL,
      0.0, SYNERGY_ALGAL_MAT_BIOMASS, 0.0, 0.0, 0.0, { 0, 0, 1, 0, 0 } },
    { "Vent Mat", "Chemosynthesis + Heat Resistance: life on hot rock.",
      MUTATION_CHEMOSYNTHESIS, MUTATION_HEAT_RESISTANCE,
      0.0, 0.0, 0.0, 0.0, SYNERGY_VENT_MAT_GROWTH, { 1, 0, 0, 0, 0 } },
    { "Eukaryotic Engine", "Nucleus + Mitochondria: compartmentalised power.",
      MUTATION_NUCLEUS, MUTATION_MITOCHONDRIA,
      SYNERGY_EUKARYOTIC_ENGINE_ATP, 0.0, 0.0, 0.0, 0.0, { 0, 0, 0, 0, 0 } },
    { "Detox Tissue", "Multicellularity + Toxin Resistance: tissue that filters poison.",
      MUTATION_MULTICELLULARITY, MUTATION_TOXIN_RESISTANCE,
      0.0, 0.0, 0.0, SYNERGY_DETOX_TISSUE_CAP, 0.0, { 0, 0, 0, 0, 2 } },
    { "Thermostable Enzymes", "Heat + Cold Resistance: enzymes that work anywhere.",
      MUTATION_HEAT_RESISTANCE, MUTATION_COLD_RESISTANCE,
      0.0, 0.0, 0.0, 0.0, 0.0, { 1, 1, 0, 0, 0 } },
    { "Decomposer Loop", "Decomposer Enzymes + Nitrogen Sense: detritus feeds the colony.",
      MUTATION_DECOMPOSER_ENZYMES, MUTATION_NITROGEN_SENSE,
      0.0, SYNERGY_DECOMPOSER_BIOMASS, SYNERGY_DECOMPOSER_DNA, 0.0, 0.0, { 0, 0, 0, 0, 1 } },
    { "Coral Holobiont", "Photosymbiosis + Chitin Wall: a reef in miniature.",
      MUTATION_PHOTOSYMBIOSIS, MUTATION_CHITIN_WALL,
      SYNERGY_CORAL_ATP, 0.0, 0.0, SYNERGY_CORAL_CAP, 0.0, { 0, 1, 0, 0, 0 } },
    { "Forest Floor", "Mycorrhizal Symbiosis + Decomposer Enzymes: a living soil.",
      MUTATION_MYCORRHIZAL_SYMBIOSIS, MUTATION_DECOMPOSER_ENZYMES,
      0.0, SYNERGY_FOREST_BIOMASS, SYNERGY_FOREST_DNA, 0.0, 0.0, { 0, 0, 0, 0, 1 } },
    { "Reef Engine", "Reef Architecture + Photosymbiosis: a self-building reef.",
      MUTATION_REEF_ARCHITECTURE, MUTATION_PHOTOSYMBIOSIS,
      SYNERGY_REEF_ATP, 0.0, 0.0, SYNERGY_REEF_CAP, 0.0, { 0, 0, 1, 0, 0 } },
    { "Noosphere", "Abstract Thought + Signal Literacy: a shared planetary mind.",
      MUTATION_ABSTRACT_THOUGHT, MUTATION_SIGNAL_LITERACY,
      SYNERGY_NOOSPHERE_ATP, 0.0, SYNERGY_NOOSPHERE_DNA, 0.0, 0.0, { 0, 0, 0, 1, 0 } },
    { "Global Network", "Language + Urbanization: a planet-wide division of labour.",
      MUTATION_LANGUAGE, MUTATION_URBANIZATION,
      0.0, SYNERGY_GLOBAL_NETWORK_BIOMASS, SYNERGY_GLOBAL_NETWORK_DNA, 0.0, 0.0,
      { 0, 0, 0, 0, 1 } },
    { "Dyson Swarm", "Rocketry + Orbital Habitats: industry wraps the star.",
      MUTATION_ROCKETRY, MUTATION_ORBITAL_HABITATS,
      SYNERGY_DYSON_ATP, 0.0, 0.0, SYNERGY_DYSON_CAP, 0.0, { 0, 0, 0, 1, 0 } },
};

#define SYNERGY_COUNT ((int)(sizeof(k_synergies) / sizeof(k_synergies[0])))

/* 第十轮：早期专属突变的“晚期补课”加价表。
 * 这些突变取消 max_phase 后仍可购买，但阶段达到 from_phase 起价格 ×multiplier。 */
typedef struct {
    int mutation_id;
    int from_phase;
    double multiplier;
} LateMutationPenalty;

static const LateMutationPenalty k_late_penalties[] = {
    { MUTATION_PHOTOSYNTHESIS, 2, LATE_MUTATION_COST_MULTIPLIER },
    { MUTATION_CHEMOSYNTHESIS, 2, LATE_MUTATION_COST_MULTIPLIER },
};

#define LATE_PENALTY_COUNT ((int)(sizeof(k_late_penalties) / sizeof(k_late_penalties[0])))

static double late_mutation_multiplier(int mutation_id, int phase)
{
    for (int i = 0; i < LATE_PENALTY_COUNT; ++i) {
        if (k_late_penalties[i].mutation_id == mutation_id &&
            phase >= k_late_penalties[i].from_phase) {
            return k_late_penalties[i].multiplier;
        }
    }
    return 1.0;
}

static bool valid_slot(const GameState *game, int slot)
{
    return slot >= 0 && slot < MAX_ACTIVE_SPECIES && slot < game->run.active_species_count;
}

static const SpeciesState *slot_species(const GameState *game, int slot)
{
    return valid_slot(game, slot) ? &game->run.active_species[slot] : 0;
}

const MutationDef *mutation_get(int mutation_id)
{
    if (mutation_id < 0 || mutation_id >= MUTATION_COUNT) {
        return 0;
    }
    return &k_mutations[mutation_id];
}

int evolution_synergy_count(void)
{
    return SYNERGY_COUNT;
}

const MutationSynergyDef *evolution_synergy_get(int index)
{
    return index >= 0 && index < SYNERGY_COUNT ? &k_synergies[index] : 0;
}

bool evolution_synergy_active(const GameState *game, int species_slot, int synergy_index)
{
    const SpeciesState *species = slot_species(game, species_slot);
    const MutationSynergyDef *synergy = evolution_synergy_get(synergy_index);
    if (!species || !synergy) {
        return false;
    }
    return species->mutations[synergy->mutation_a] && species->mutations[synergy->mutation_b];
}

int evolution_active_synergy_count(const GameState *game, int species_slot)
{
    int count = 0;
    for (int i = 0; i < SYNERGY_COUNT; ++i) {
        if (evolution_synergy_active(game, species_slot, i)) {
            ++count;
        }
    }
    return count;
}

void evolution_synergy_bonuses(const GameState *game, int species_slot, double *atp_multiplier,
                               double *biomass_multiplier, double *dna_multiplier,
                               double *cap_multiplier, double *growth_multiplier,
                               int *trait_bonus)
{
    double atp = 1.0;
    double biomass = 1.0;
    double dna = 1.0;
    double cap = 1.0;
    double growth = 1.0;
    int traits[TRAIT_COUNT] = { 0, 0, 0, 0, 0 };

    for (int i = 0; i < SYNERGY_COUNT; ++i) {
        if (!evolution_synergy_active(game, species_slot, i)) {
            continue;
        }
        const MutationSynergyDef *synergy = &k_synergies[i];
        atp *= 1.0 + synergy->atp_bonus;
        biomass *= 1.0 + synergy->biomass_bonus;
        dna *= 1.0 + synergy->dna_bonus;
        cap *= 1.0 + synergy->cap_bonus;
        growth *= 1.0 + synergy->growth_bonus;
        for (int t = 0; t < TRAIT_COUNT; ++t) {
            traits[t] += synergy->trait_bonus[t];
        }
    }

    if (atp_multiplier) {
        *atp_multiplier = atp;
    }
    if (biomass_multiplier) {
        *biomass_multiplier = biomass;
    }
    if (dna_multiplier) {
        *dna_multiplier = dna;
    }
    if (cap_multiplier) {
        *cap_multiplier = cap;
    }
    if (growth_multiplier) {
        *growth_multiplier = growth;
    }
    if (trait_bonus) {
        for (int t = 0; t < TRAIT_COUNT; ++t) {
            trait_bonus[t] = traits[t];
        }
    }
}

int evolution_max_speciation_slots(const GameState *game)
{
    return legacy_active_species_slots(&game->legacy);
}

int evolution_main_slot(const GameState *game)
{
    for (int i = 0; i < game->run.active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
        if (game->run.active_species[i].is_main_lineage) {
            return i;
        }
    }
    return game->run.active_species_count > 0 ? 0 : -1;
}

int evolution_main_stage(const GameState *game)
{
    int slot = evolution_main_slot(game);
    const SpeciesState *species = slot_species(game, slot);
    return species ? species_phase(species->species_id) : 0;
}

double evolution_speciation_cost(const GameState *game, int target_species_id)
{
    double base = species_base_speciation_cost(target_species_id);
    double discount = legacy_speciation_discount(&game->legacy);
    return floor(base * (1.0 - discount));
}

/* ---------------- 突变约束 ---------------- */

static bool species_owns(const SpeciesState *species, int mutation_id)
{
    return mutation_id >= 0 && mutation_id < MUTATION_COUNT && species->mutations[mutation_id];
}

static bool mutation_legacy_requirement_met(const GameState *game, const MutationDef *def)
{
    if (def->requires_branch < 0) {
        return true;
    }
    if (def->requires_branch >= 4) {
        return false;
    }
    return game->legacy.levels[def->requires_branch] >= def->requires_level;
}

bool evolution_mutation_available(const GameState *game, int species_slot, int mutation_id)
{
    const SpeciesState *species = slot_species(game, species_slot);
    const MutationDef *def = mutation_get(mutation_id);
    if (!species || !def) {
        return false;
    }
    if (species_owns(species, mutation_id)) {
        return false;
    }
    int phase = species_phase(species->species_id);
    if (phase < def->min_phase) {
        return false;
    }
    if (def->max_phase >= 0 && phase > def->max_phase) {
        return false;
    }
    if (def->requires_mutation >= 0 && !species_owns(species, def->requires_mutation)) {
        return false;
    }
    return mutation_legacy_requirement_met(game, def);
}

/* 第十一轮：被任一物种的分化前置引用的突变视为“必需”。
 * 只有必需突变进入 1.34 成本链，可选突变独立定价，避免买一个可选就把整条链拖贵。 */
static bool mutation_is_required(int mutation_id)
{
    int required[4];
    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        int count = species_required_mutations(species_id, required, 4);
        for (int i = 0; i < count; ++i) {
            if (required[i] == mutation_id) {
                return true;
            }
        }
    }
    return false;
}

static int required_mutation_count(const SpeciesState *species)
{
    int count = 0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (species->mutations[id] && mutation_is_required(id)) {
            ++count;
        }
    }
    return count;
}

double evolution_mutation_cost_for_slot(const GameState *game, int species_slot, int mutation_id)
{
    const MutationDef *def = mutation_get(mutation_id);
    if (!def) {
        return 0.0;
    }
    double discount = legacy_mutation_discount(&game->legacy);
    double base_cost = def->base_dna_cost;
    if (mutation_id >= MUTATION_CELL_DIFFERENTIATION) {
        base_cost *= EXPANSION_MUTATION_COST_SCALE;
    }
    int phase = 0;
    double growth = 1.0;
    double first_discount = 1.0;
    if (valid_slot(game, species_slot)) {
        const SpeciesState *species = &game->run.active_species[species_slot];
        phase = species_phase(species->species_id);
        if (mutation_is_required(mutation_id)) {
            growth = pow(MUTATION_COST_GROWTH, required_mutation_count(species));
        }
        /* 第十二轮：尚未拥有任何突变时，首个突变 6 折 */
        bool owns_any = false;
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            if (species->mutations[id]) {
                owns_any = true;
                break;
            }
        }
        if (!owns_any) {
            first_discount = 1.0 - FIRST_MUTATION_DISCOUNT;
        }
    }
    /* 第十轮：基价随阶段缩放（早便宜、晚贵），并叠加晚期补课加价 */
    double phase_scale = MUTATION_PHASE_COST_BASE + MUTATION_PHASE_COST_SLOPE * (double)phase;
    return ceil(base_cost * phase_scale * late_mutation_multiplier(mutation_id, phase) *
                (1.0 - discount) * growth * first_discount);
}

double evolution_mutation_cost(const GameState *game, int mutation_id)
{
    return evolution_mutation_cost_for_slot(game, 0, mutation_id);
}

int evolution_mutation_choice_count(const GameState *game)
{
    int level = game->legacy.levels[LEGACY_MUTATION];
    if (level >= 6) {
        return MUTATION_CHOICES_LV6;
    }
    return level >= 3 ? MUTATION_CHOICES_BONUS : MUTATION_CHOICES_BASE;
}

static int mutation_weight(const GameState *game, const MutationDef *def)
{
    if (def->rarity == 1) {
        return legacy_rare_mutation_weight(&game->legacy);
    }
    if (def->rarity == 2) {
        return MUTATION_WEIGHT_RARE;
    }
    return MUTATION_WEIGHT_COMMON;
}

/* 02 §6.4 第 3 条：Biomass 达到下一可用物种分化成本 60% 时保底一个关键突变 */
static int find_key_mutation(const GameState *game, const SpeciesState *species,
                             const int *pool, int pool_n)
{
    int children[SPECIES_COUNT];
    int child_count = species_children(species->species_id, children, SPECIES_COUNT);
    if (child_count == 0) {
        return -1;
    }
    /* 第十二轮（D2）：不再等生物量门槛——只要下一阶段还缺必需突变，
     * 就强制占一个候选位，避免“候选池里有 7 个却只开 2 格”造成的关键突变失约。 */
    (void)game;
    for (int c = 0; c < child_count; ++c) {
        int required[4];
        int required_count = species_required_mutations(children[c], required, 4);
        for (int r = 0; r < required_count; ++r) {
            if (species_owns(species, required[r])) {
                continue;
            }
            for (int p = 0; p < pool_n; ++p) {
                if (pool[p] == required[r]) {
                    return required[r];
                }
            }
        }
    }
    return -1;
}

const char *evolution_offer_block_reason(const GameState *game, int species_slot)
{
    if (game->phase != PHASE_RUNNING) {
        return "Evolution is paused.";
    }
    if (!valid_slot(game, species_slot)) {
        return "No such species slot.";
    }
    if (game->run.mutation_offer.active) {
        if (game->run.mutation_offer.species_slot == species_slot) {
            return 0;
        }
        return "Another species already has a pending mutation offer.";
    }
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (evolution_mutation_available(game, species_slot, id)) {
            return 0;
        }
    }
    return "No mutation is available for this species yet.";
}

MutationOfferState evolution_generate_mutation_offer_rng(const GameState *game, int species_slot,
                                                         unsigned int *rng_state)
{
    MutationOfferState offer;
    memset(&offer, 0, sizeof(offer));
    offer.species_slot = species_slot;

    if (!valid_slot(game, species_slot)) {
        return offer;
    }
    if (game->run.mutation_offer.active && game->run.mutation_offer.species_slot == species_slot) {
        return game->run.mutation_offer;
    }

    const SpeciesState *species = &game->run.active_species[species_slot];
    int pool[MUTATION_COUNT];
    int pool_n = 0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (evolution_mutation_available(game, species_slot, id)) {
            pool[pool_n++] = id;
        }
    }
    if (pool_n == 0) {
        return offer;
    }

    int wanted = evolution_mutation_choice_count(game);
    if (wanted > pool_n) {
        wanted = pool_n;
    }

    bool used[MUTATION_COUNT];
    memset(used, 0, sizeof(used));
    int chosen[MAX_MUTATION_CHOICES];
    int chosen_n = 0;

    int forced = find_key_mutation(game, species, pool, pool_n);
    if (forced >= 0) {
        chosen[chosen_n++] = forced;
        used[forced] = true;
    }

    while (chosen_n < wanted) {
        int total = 0;
        for (int i = 0; i < pool_n; ++i) {
            if (!used[pool[i]]) {
                total += mutation_weight(game, mutation_get(pool[i]));
            }
        }
        if (total <= 0) {
            break;
        }
        unsigned int roll = tof_rng_next(rng_state) % (unsigned int)total;
        int accumulated = 0;
        int picked = -1;
        for (int i = 0; i < pool_n; ++i) {
            int id = pool[i];
            if (used[id]) {
                continue;
            }
            accumulated += mutation_weight(game, mutation_get(id));
            if ((int)roll < accumulated) {
                picked = id;
                break;
            }
        }
        if (picked < 0) {
            break;
        }
        chosen[chosen_n++] = picked;
        used[picked] = true;
    }

    if (chosen_n == 0) {
        return offer;
    }

    /* 第十二轮（D1）：保证候选里至少有一张“当下买得起”的牌（含 15 秒 DNA 收入）。
     * 若全都买不起，用池中最便宜的未选项替换当前最贵的非保底项。 */
    double dna_rate = evolution_calculate_species_rates(game, species_slot).dna_per_second;
    double budget = game->run.dna + 15.0 * dna_rate;
    bool affordable = false;
    for (int i = 0; i < chosen_n; ++i) {
        if (evolution_mutation_cost_for_slot(game, species_slot, chosen[i]) <= budget) {
            affordable = true;
            break;
        }
    }
    if (!affordable && chosen_n > 0) {
        int cheapest = -1;
        double cheapest_cost = 0.0;
        for (int i = 0; i < pool_n; ++i) {
            if (used[pool[i]]) {
                continue;
            }
            double cost = evolution_mutation_cost_for_slot(game, species_slot, pool[i]);
            if (cheapest < 0 || cost < cheapest_cost) {
                cheapest = pool[i];
                cheapest_cost = cost;
            }
        }
        int replace = -1;
        double highest_cost = -1.0;
        for (int i = 0; i < chosen_n; ++i) {
            if (forced >= 0 && chosen[i] == forced) {
                continue;
            }
            double cost = evolution_mutation_cost_for_slot(game, species_slot, chosen[i]);
            if (cost > highest_cost) {
                highest_cost = cost;
                replace = i;
            }
        }
        if (cheapest >= 0 && replace >= 0) {
            chosen[replace] = cheapest;
        }
    }

    offer.active = true;
    offer.choice_count = chosen_n;
    for (int i = 0; i < chosen_n; ++i) {
        double cost = evolution_mutation_cost_for_slot(game, species_slot, chosen[i]);
        /* 第十轮：被保底的关键突变打 40% 折扣，保证“知道缺什么就买得起” */
        if (forced >= 0 && chosen[i] == forced) {
            cost = ceil(cost * (1.0 - KEY_MUTATION_DISCOUNT));
        }
        offer.mutation_ids[i] = chosen[i];
        offer.dna_costs[i] = cost;
    }
    return offer;
}

MutationOfferState evolution_generate_mutation_offer(const GameState *game, int species_slot)
{
    unsigned int rng = game->run.rng_state;
    return evolution_generate_mutation_offer_rng(game, species_slot, &rng);
}

ActionCode evolution_open_mutation_offer(GameState *game, int species_slot)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    if (!valid_slot(game, species_slot)) {
        return ACTION_INVALID_TARGET;
    }
    if (game->run.mutation_offer.active) {
        if (game->run.mutation_offer.species_slot == species_slot) {
            return ACTION_OK;
        }
        /* 02 §6.4 第 7 条：候选只能被选择、分化或新轮清除，不能跨槽刷新 */
        return ACTION_REQUIREMENT_NOT_MET;
    }
    unsigned int rng = game->run.rng_state;
    MutationOfferState offer = evolution_generate_mutation_offer_rng(game, species_slot, &rng);
    if (!offer.active) {
        return ACTION_REQUIREMENT_NOT_MET;
    }
    game->run.rng_state = rng;
    game->run.mutation_offer = offer;
    return ACTION_OK;
}

ActionCode evolution_apply_mutation(GameState *game, int species_slot, int mutation_id)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    if (!valid_slot(game, species_slot)) {
        return ACTION_INVALID_TARGET;
    }
    MutationOfferState *offer = &game->run.mutation_offer;
    if (!offer->active || offer->species_slot != species_slot) {
        return ACTION_INVALID_TARGET;
    }
    int choice = -1;
    for (int i = 0; i < offer->choice_count; ++i) {
        if (offer->mutation_ids[i] == mutation_id) {
            choice = i;
            break;
        }
    }
    if (choice < 0) {
        return ACTION_INVALID_TARGET;
    }

    SpeciesState *species = &game->run.active_species[species_slot];
    if (species_owns(species, mutation_id)) {
        return ACTION_INVALID_TARGET;
    }
    double cost = offer->dna_costs[choice];
    if (game->run.dna < cost) {
        return ACTION_NOT_ENOUGH_DNA;
    }

    game->run.dna -= cost;
    species->mutations[mutation_id] = true;
    game->run.discovered_mutations[mutation_id] = true;
    game->legacy.codex_mutations[mutation_id] = true;
    memset(&game->run.mutation_offer, 0, sizeof(game->run.mutation_offer));
    return ACTION_OK;
}

/* ---------------- 物种分化 ---------------- */

ActionCode evolution_validate_speciation(const GameState *game, int species_slot,
                                         int target_species_id)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    if (!valid_slot(game, species_slot) || target_species_id < 0 ||
        target_species_id >= SPECIES_COUNT) {
        return ACTION_INVALID_TARGET;
    }
    const SpeciesState *parent = &game->run.active_species[species_slot];
    if (target_species_id == parent->species_id ||
        !species_has_parent(target_species_id, parent->species_id)) {
        return ACTION_INVALID_TARGET;
    }

    int required[4];
    int required_count = species_required_mutations(target_species_id, required, 4);
    for (int i = 0; i < required_count; ++i) {
        if (!parent->mutations[required[i]]) {
            return ACTION_REQUIREMENT_NOT_MET;
        }
    }
    int branch = -1;
    int level = 0;
    if (species_requires_legacy(target_species_id, &branch, &level)) {
        if (branch < 0 || branch >= 4 || game->legacy.levels[branch] < level) {
            return ACTION_REQUIREMENT_NOT_MET;
        }
    }
    if (game->run.biomass < evolution_speciation_cost(game, target_species_id)) {
        return ACTION_NOT_ENOUGH_BIOMASS;
    }
    return ACTION_OK;
}

ActionCode evolution_apply_speciation(GameState *game, int species_slot, int target_species_id,
                                      int option)
{
    ActionCode code = evolution_validate_speciation(game, species_slot, target_species_id);
    if (code != ACTION_OK) {
        return code;
    }
    int slots = evolution_max_speciation_slots(game);
    if (slots > MAX_ACTIVE_SPECIES) {
        slots = MAX_ACTIVE_SPECIES;
    }
    bool keep_parent = option == 1;

    RunState *run = &game->run;
    SpeciesState parent = run->active_species[species_slot];
    bool source_is_main = parent.is_main_lineage;
    /* 01 §5：option=1 保留亲本只允许从主谱系发起 */
    if (option == 1 && !source_is_main) {
        return ACTION_INVALID_TARGET;
    }

    /* 保留亲本：优先占用空槽，槽满时替换一个非主谱系的额外分支 */
    int keep_index = -1;
    if (keep_parent) {
        for (int i = 0; i < slots; ++i) {
            if (i == species_slot) {
                continue;
            }
            if (i >= run->active_species_count) {
                keep_index = i;
                break;
            }
        }
        if (keep_index < 0) {
            for (int i = slots - 1; i >= 0; --i) {
                if (i != species_slot && !run->active_species[i].is_main_lineage) {
                    keep_index = i;
                    break;
                }
            }
        }
        if (keep_index < 0) {
            return ACTION_REQUIREMENT_NOT_MET;
        }
    }
    double cost = evolution_speciation_cost(game, target_species_id);
    double parent_population = parent.population;

    SpeciesState child;
    memset(&child, 0, sizeof(child));
    child.species_id = target_species_id;
    child.is_main_lineage = source_is_main;
    memcpy(child.mutations, parent.mutations, sizeof(child.mutations));

    if (keep_parent) {
        child.population = fmax(MIN_ACTIVE_POPULATION,
                                parent_population * SPECIATION_CHILD_SHARE_KEEP);
        SpeciesState kept = parent;
        kept.is_main_lineage = false;
        kept.population = fmax(MIN_ACTIVE_POPULATION,
                               parent_population * SPECIATION_PARENT_SHARE_KEEP);
        run->active_species[species_slot] = child;
        run->active_species[keep_index] = kept;
        if (run->active_species_count <= keep_index) {
            run->active_species_count = keep_index + 1;
        }
    } else {
        child.population = fmax(SPECIATION_MIN_CHILD_POPULATION,
                                parent_population * SPECIATION_CHILD_SHARE_SOLO);
        /* 原地替换来源槽：主谱系或额外分支各自推进，互不吞并 */
        run->active_species[species_slot] = child;
        if (run->active_species_count <= species_slot) {
            run->active_species_count = species_slot + 1;
        }
    }

    run->biomass -= cost;
    run->discovered_species[target_species_id] = true;
    game->legacy.codex_species[target_species_id] = true;
    memset(&run->mutation_offer, 0, sizeof(run->mutation_offer));
    return ACTION_OK;
}

/* ---------------- 容量扩张与应急储备 ---------------- */

double evolution_capacity_cost(const GameState *game)
{
    int slot = evolution_main_slot(game);
    if (slot < 0) {
        return 0.0;
    }
    SpeciesRates rates = evolution_calculate_species_rates(game, slot);
    return floor(CAPACITY_COST_PER_CAP * rates.final_population_cap *
                 pow(CAPACITY_COST_GROWTH, (double)game->run.capacity_levels));
}

/* 第十一轮：容量双币拆分。前 CAPACITY_BIOMASS_ONLY_LEVELS 级仍只收 Biomass，
 * 之后按 70/30 由 ATP 与 Biomass 分担，让 ATP 在高阶段有出口。 */
double evolution_capacity_atp_cost(const GameState *game)
{
    if (game->run.capacity_levels < CAPACITY_BIOMASS_ONLY_LEVELS) {
        return 0.0;
    }
    return ceil(evolution_capacity_cost(game) * CAPACITY_ATP_SHARE);
}

double evolution_capacity_biomass_cost(const GameState *game)
{
    double total = evolution_capacity_cost(game);
    if (game->run.capacity_levels < CAPACITY_BIOMASS_ONLY_LEVELS) {
        return total;
    }
    return ceil(total * CAPACITY_BIOMASS_SHARE);
}

ActionCode evolution_apply_capacity_upgrade(GameState *game)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    /* 第十轮：CAPACITY_MAX_LEVEL < 0 表示无上限，Biomass 始终有出口 */
    if (CAPACITY_MAX_LEVEL >= 0 && game->run.capacity_levels >= CAPACITY_MAX_LEVEL) {
        return ACTION_LEVEL_MAXED;
    }
    double atp_cost = evolution_capacity_atp_cost(game);
    double biomass_cost = evolution_capacity_biomass_cost(game);
    if (game->run.biomass < biomass_cost) {
        return ACTION_NOT_ENOUGH_BIOMASS;
    }
    if (game->run.atp < atp_cost) {
        return ACTION_NOT_ENOUGH_ATP;
    }
    game->run.biomass -= biomass_cost;
    game->run.atp -= atp_cost;
    ++game->run.capacity_levels;
    return ACTION_OK;
}

double evolution_reserve_cost(const GameState *game)
{
    (void)game;
    return RESERVE_COST_ATP;
}

ActionCode evolution_apply_reserve(GameState *game)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    if (game->run.reserve_levels >= RESERVE_MAX_LEVEL) {
        return ACTION_LEVEL_MAXED;
    }
    if (game->run.atp < RESERVE_COST_ATP) {
        return ACTION_NOT_ENOUGH_ATP;
    }
    game->run.atp -= RESERVE_COST_ATP;
    ++game->run.reserve_levels;
    return ACTION_OK;
}

/* ---------------- 第九轮：理念与子升级 ---------------- */

int evolution_sub_upgrade_level(const GameState *game, int species_slot)
{
    const SpeciesState *species = slot_species(game, species_slot);
    return species ? game->run.tech_levels[species->species_id] : 0;
}

const SubUpgradeDef *evolution_sub_upgrade_next(const GameState *game, int species_slot)
{
    const SpeciesState *species = slot_species(game, species_slot);
    if (!species) {
        return 0;
    }
    int level = game->run.tech_levels[species->species_id];
    for (int i = 0; i < evolution_sub_upgrade_count(); ++i) {
        const SubUpgradeDef *def = evolution_sub_upgrade_get(i);
        if (def->species_id == species->species_id && def->tier == level) {
            return def;
        }
    }
    return 0;
}

double evolution_sub_upgrade_multiplier(const GameState *game, int species_slot)
{
    const SpeciesState *species = slot_species(game, species_slot);
    if (!species) {
        return 1.0;
    }
    int level = game->run.tech_levels[species->species_id];
    double multiplier = 1.0;
    for (int i = 0; i < evolution_sub_upgrade_count(); ++i) {
        const SubUpgradeDef *def = evolution_sub_upgrade_get(i);
        if (def->species_id == species->species_id && def->tier < level) {
            multiplier *= def->multiplier;
        }
    }
    return multiplier;
}

double evolution_sub_upgrade_cost(const SubUpgradeDef *def)
{
    if (!def) {
        return 0.0;
    }
    return def->base_knowledge_cost * pow(SUB_UPGRADE_COST_GROWTH, (double)def->tier);
}

ActionCode evolution_apply_sub_upgrade(GameState *game, int species_slot)
{
    if (game->phase != PHASE_RUNNING) {
        return ACTION_INVALID_PHASE;
    }
    if (!valid_slot(game, species_slot)) {
        return ACTION_INVALID_TARGET;
    }
    const SubUpgradeDef *next = evolution_sub_upgrade_next(game, species_slot);
    if (!next) {
        /* 区分“该物种没有子升级”与“已满层” */
        int species_id = game->run.active_species[species_slot].species_id;
        for (int i = 0; i < evolution_sub_upgrade_count(); ++i) {
            if (evolution_sub_upgrade_get(i)->species_id == species_id) {
                return ACTION_LEVEL_MAXED;
            }
        }
        return ACTION_INVALID_TARGET;
    }
    double cost = evolution_sub_upgrade_cost(next);
    if (game->run.knowledge < cost) {
        return ACTION_NOT_ENOUGH_KNOWLEDGE;
    }
    game->run.knowledge -= cost;
    ++game->run.tech_levels[next->species_id];
    return ACTION_OK;
}

double evolution_knowledge_per_second(const GameState *game)
{
    double total = 0.0;
    for (int i = 0; i < game->run.active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
        total += evolution_calculate_species_rates(game, i).knowledge_per_second;
    }
    return total;
}

/* ---------------- 特征与产率 ---------------- */

int evolution_slot_trait_total(const GameState *game, int species_slot, EnvironmentTrait trait)
{
    const SpeciesState *species = slot_species(game, species_slot);
    if (!species || trait < 0 || trait >= TRAIT_COUNT) {
        return 0;
    }
    int total = species_base_trait(species->species_id, trait);
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (!species->mutations[id]) {
            continue;
        }
        const MutationDef *def = mutation_get(id);
        if (def) {
            total += def->trait_bonus[trait];
        }
    }
    int synergy_traits[TRAIT_COUNT];
    evolution_synergy_bonuses(game, species_slot, 0, 0, 0, 0, 0, synergy_traits);
    total += synergy_traits[trait];
    return total;
}

static bool slot_has_synergy(const GameState *game, int species_slot)
{
    if (!legacy_synergy_enabled(&game->legacy) || game->run.active_species_count < 2) {
        return false;
    }
    if (species_slot < 0 || species_slot >= game->run.active_species_count) {
        return false;
    }
    EcologyTag a = species_ecology(game->run.active_species[species_slot].species_id);
    /* 三槽：只要存在任一异标签伙伴即视为协同，不叠加 */
    for (int i = 0; i < game->run.active_species_count; ++i) {
        if (i == species_slot) {
            continue;
        }
        if (species_ecology(game->run.active_species[i].species_id) != a) {
            return true;
        }
    }
    return false;
}

/* 生态韧性：本纪每存活一种危机，全局产率 +5%，最多 +15% */
static double resilience_multiplier(const GameState *game)
{
    int survived = 0;
    for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id) {
        if (game->run.survived_crises[id]) {
            ++survived;
        }
    }
    double bonus = survived * RESILIENCE_PER_CRISIS;
    if (bonus > RESILIENCE_CAP) {
        bonus = RESILIENCE_CAP;
    }
    return 1.0 + bonus;
}

SpeciesRates evolution_calculate_species_rates(const GameState *game, int species_slot)
{
    SpeciesRates rates;
    memset(&rates, 0, sizeof(rates));
    rates.environment_multiplier = 1.0;

    const SpeciesState *species = slot_species(game, species_slot);
    const SpeciesDef *def = species ? species_get(species->species_id) : 0;
    if (!species || !def) {
        return rates;
    }

    double mutation_atp = 0.0;
    double mutation_biomass = 0.0;
    double mutation_dna = 0.0;
    double mutation_cap = 0.0;
    double mutation_growth = 0.0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (!species->mutations[id]) {
            continue;
        }
        const MutationDef *mutation = mutation_get(id);
        if (!mutation) {
            continue;
        }
        mutation_atp += mutation->atp_bonus;
        mutation_biomass += mutation->biomass_bonus;
        mutation_dna += mutation->dna_bonus;
        mutation_cap += mutation->cap_bonus;
        mutation_growth += mutation->growth_bonus;
    }

    const RunState *run = &game->run;
    double run_metabolism = 1.0 + RUN_METABOLISM_BONUS_PER_LEVEL *
                                      run->run_upgrade_levels[UPGRADE_METABOLISM];
    double run_reproduction = 1.0 + RUN_REPRODUCTION_BONUS_PER_LEVEL *
                                        run->run_upgrade_levels[UPGRADE_REPRODUCTION];
    double run_genome = 1.0 + RUN_GENOME_BONUS_PER_LEVEL *
                                  run->run_upgrade_levels[UPGRADE_GENOME];

    double permanent_atp = legacy_atp_multiplier(&game->legacy);
    double permanent_biomass = legacy_biomass_multiplier(&game->legacy);
    double permanent_dna = legacy_dna_multiplier(&game->legacy);

    double diversity = slot_has_synergy(game, species_slot) ? legacy_synergy_multiplier(&game->legacy)
                                                           : 1.0;
    double production = legacy_production_multiplier(&game->legacy);
    double environment = environment_multiplier_for_slot(game, species_slot);
    double population_scale = species->population / 10.0;
    double dna_scale = pow(population_scale, DNA_POPULATION_EXPONENT);

    /* 可玩性扩展：突变共振 + 物种基石 + 生态韧性 */
    double syn_atp = 1.0;
    double syn_biomass = 1.0;
    double syn_dna = 1.0;
    double syn_cap = 1.0;
    double syn_growth = 1.0;
    evolution_synergy_bonuses(game, species_slot, &syn_atp, &syn_biomass, &syn_dna, &syn_cap,
                              &syn_growth, 0);

    const SpeciesKeystone *keystone = species_keystone(species->species_id);
    double key_atp = keystone ? 1.0 + keystone->atp_bonus : 1.0;
    double key_biomass = keystone ? 1.0 + keystone->biomass_bonus : 1.0;
    double key_dna = keystone ? 1.0 + keystone->dna_bonus : 1.0;
    double key_cap = keystone ? 1.0 + keystone->cap_bonus : 1.0;
    double key_growth = keystone ? 1.0 + keystone->growth_bonus : 1.0;
    double key_event = 1.0;
    if (keystone && run->current_environment_id == ENV_LOW_LIGHT_PERIOD) {
        key_event *= 1.0 + keystone->low_light_bonus;
    }
    if (keystone && run->current_environment_id == ENV_TOXIC_BLOOM) {
        key_event *= 1.0 + keystone->toxin_bonus;
    }
    double resilience = resilience_multiplier(game);
    double eco_atp = syn_atp * key_atp * key_event * resilience;
    double eco_biomass = syn_biomass * key_biomass * key_event * resilience;
    double eco_dna = syn_dna * key_dna * key_event * resilience;

    rates.environment_multiplier = environment;
    rates.synergy_active = diversity > 1.0;
    /* 第九轮：子升级乘算该物种全部产率；阶段 >= 6 的物种产出理念 */
    double sub = evolution_sub_upgrade_multiplier(game, species_slot);
    rates.atp_per_second = def->base_atp * population_scale * run_metabolism *
                           (1.0 + mutation_atp) * environment * permanent_atp * diversity *
                           production * eco_atp * sub;
    rates.biomass_per_second = def->base_biomass * population_scale * run_reproduction *
                               (1.0 + mutation_biomass) * environment * permanent_biomass *
                               diversity * production * eco_biomass * sub;
    double knowledge_scale = pow(population_scale, KNOWLEDGE_POPULATION_EXPONENT);
    /* 第十轮：阶段 0–1 的 DNA 产出 ×1.5，缓解开局饥饿 */
    double early_dna = species_phase(species->species_id) <= EARLY_DNA_MAX_PHASE ? EARLY_DNA_BONUS
                                                                               : 1.0;
    rates.dna_per_second = def->base_dna * dna_scale * run_genome *
                           (1.0 + mutation_dna) * environment * permanent_dna * diversity *
                           production * eco_dna * sub * early_dna;
    rates.knowledge_per_second = def->base_knowledge * knowledge_scale * run_genome *
                                 environment * production * diversity * sub;

    double cap = def->base_cap * (1.0 + mutation_cap) * legacy_cap_multiplier(&game->legacy) *
                 syn_cap * key_cap;
    cap *= 1.0 + CAPACITY_GAIN_PER_LEVEL * run->capacity_levels;
    rates.final_population_cap = cap;

    double positive_growth = 0.0;
    if (species->population < cap && cap > 0.0) {
        positive_growth = species->population * def->base_growth * run_reproduction *
                          (1.0 + mutation_growth) * syn_growth * key_growth *
                          (1.0 - species->population / cap);
    }

    double event_loss = 0.0;
    double damage = environment_damage_per_second(run->current_environment_id);
    if (damage > 0.0) {
        int stage = species_phase(species->species_id);
        double stage_scale = 1.0 + CRISIS_DAMAGE_STAGE_SCALE * (double)stage;
        double reserve_scale = 1.0 - RESERVE_REDUCTION_PER_LEVEL * (double)run->reserve_levels;
        if (reserve_scale < 0.5) {
            reserve_scale = 0.5;
        }
        event_loss = species->population * damage * (2.0 - environment) * stage_scale;
        event_loss *= CRISIS_DAMAGE_MULTIPLIER;
        if (keystone) {
            event_loss *= (1.0 - keystone->crisis_damage_reduction);
        }
        event_loss *= (1.0 - legacy_crisis_damage_reduction(&game->legacy));
        event_loss *= reserve_scale;
        if (species->is_main_lineage && run->adaptation_shield_active) {
            event_loss *= (1.0 - legacy_shield_damage_reduction(&game->legacy));
        }
    }

    rates.population_delta_per_second = positive_growth - event_loss;
    return rates;
}
