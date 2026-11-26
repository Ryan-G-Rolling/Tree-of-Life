/*
 * Member B 自测入口（不依赖 raylib）
 *
 * 覆盖 02-数据与计算规则 中属于 B 的规则：
 * 随机数、物种数据、突变成本与候选、关键突变保底、物种分化、
 * 环境倍率与事件切换、危机统计、Lv.5 保护、EP 结算、永久强化、
 * 理念产出与子升级（第九轮）。
 *
 * 编译：gcc -std=c11 -Wall -Wextra -Iinclude -Idata \
 *          tests/test_b.c src/evolution.c src/species.c \
 *          src/environment.c src/prestige.c src/legacy.c -lm -o build/test_b
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "balance.h"
#include "environment.h"
#include "evolution.h"
#include "game_types.h"
#include "legacy.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(condition)                                                                      \
    do {                                                                                      \
        ++g_checks;                                                                           \
        if (!(condition)) {                                                                   \
            ++g_failures;                                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                        \
        }                                                                                     \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                                     \
    do {                                                                                      \
        double a_ = (actual);                                                                 \
        double e_ = (expected);                                                               \
        ++g_checks;                                                                           \
        if (fabs(a_ - e_) > (eps)) {                                                          \
            ++g_failures;                                                                     \
            printf("FAIL %s:%d  %s=%.6f 期望 %.6f\n", __FILE__, __LINE__, #actual, a_, e_);    \
        }                                                                                     \
    } while (0)

static void reset_game(GameState *game, int species_id)
{
    memset(game, 0, sizeof(*game));
    game->save_version = 1;
    game->phase = PHASE_RUNNING;
    game->run.era = 1;
    game->run.rng_state = 0x12345678u;
    game->run.active_species_count = 1;
    game->run.active_species[0].species_id = species_id;
    game->run.active_species[0].is_main_lineage = true;
    game->run.active_species[0].population = STARTING_POPULATION;
    game->run.discovered_species[species_id] = true;
    game->run.current_environment_id = ENV_NORMAL_OCEAN;
    game->run.next_environment_id = -1;
}

static bool offer_contains(const MutationOfferState *offer, int mutation_id)
{
    for (int i = 0; i < offer->choice_count; ++i) {
        if (offer->mutation_ids[i] == mutation_id) {
            return true;
        }
    }
    return false;
}

static void test_rng(void)
{
    unsigned int state = 1u;
    CHECK(tof_rng_next(&state) == 270369u);
    CHECK(state == 270369u);

    unsigned int a = 7u;
    unsigned int b = 7u;
    for (int i = 0; i < 16; ++i) {
        CHECK(tof_rng_next(&a) == tof_rng_next(&b));
    }
    CHECK(a == b);
}

static void test_species_data(void)
{
    CHECK(SPECIES_COUNT == 26);
    CHECK(species_phase(SPECIES_PROTOCELL) == 0);
    CHECK(species_phase(SPECIES_MULTICELLULAR_COLONY) == 3);
    CHECK(species_phase(SPECIES_DIFFERENTIATED_TISSUE) == 4);
    CHECK(species_phase(SPECIES_COGNITIVE_BIOSPHERE) == 5);
    CHECK(species_ecology(SPECIES_CYANOBACTERIUM) == ECO_PHOTOSYNTHETIC);
    CHECK(species_base_trait(SPECIES_CYANOBACTERIUM, TRAIT_LOW_LIGHT) == -1);
    CHECK(species_base_trait(SPECIES_EXTREMOPHILE_COLONY, TRAIT_TOXIN) == 2);
    CHECK_NEAR(species_base_speciation_cost(SPECIES_ALGAL_EUKARYOTE), 350.0, 0.001);

    int children[SPECIES_COUNT];
    CHECK(species_children(SPECIES_PROTOCELL, children, SPECIES_COUNT) == 2);
    CHECK(species_children(SPECIES_PROTOZOAN, children, SPECIES_COUNT) == 3);
    CHECK(species_children(SPECIES_MULTICELLULAR_COLONY, children, SPECIES_COUNT) == 5);
    CHECK(species_children(SPECIES_DIFFERENTIATED_TISSUE, children, SPECIES_COUNT) == 1);
    CHECK(species_has_parent(SPECIES_EXTREMOPHILE_COLONY, SPECIES_PROTOZOAN));
    CHECK(species_has_parent(SPECIES_COGNITIVE_BIOSPHERE, SPECIES_SYMBIOTIC_SUPERORGANISM));
    CHECK(!species_has_parent(SPECIES_EXTREMOPHILE_COLONY, SPECIES_PROTOCELL));

    int required[4];
    CHECK(species_required_mutations(SPECIES_PROTOZOAN, required, 4) == 2);
    CHECK(required[0] == MUTATION_NUCLEUS);
    CHECK(required[1] == MUTATION_MITOCHONDRIA);
    CHECK(species_required_mutations(SPECIES_COGNITIVE_BIOSPHERE, required, 4) == 2);
    CHECK(required[0] == MUTATION_NEURAL_SIGNALING);
    CHECK(required[1] == MUTATION_QUORUM_SENSING);

    int branch = -1;
    int level = 0;
    CHECK(species_requires_legacy(SPECIES_EXTREMOPHILE_COLONY, &branch, &level));
    CHECK(branch == LEGACY_DIVERSIFICATION);
    CHECK(level == 5);
}

static void test_mutation_rules(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);

    /* 第十二轮：尚未拥有任何突变时首个突变 6 折（18 × 0.35 × 0.6 = 3.78 -> 4） */
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 4.0, 0.001);
    game.legacy.levels[LEGACY_MUTATION] = 1;
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 4.0, 0.001);
    game.legacy.levels[LEGACY_MUTATION] = 2;
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 4.0, 0.001);

    game.legacy.levels[LEGACY_MUTATION] = 0;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_CELL_MEMBRANE));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_PHOTOSYNTHESIS));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_MITOCHONDRIA));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_NUCLEUS));
    /* 第三轮：两个原 Lv.5 突变取消永久等级前置，单纪元内即可发现 */
    CHECK(evolution_mutation_available(&game, 0, MUTATION_EFFICIENT_RESPIRATION));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_ADAPTIVE_HYPERMUTATION));

    /* 第十轮：光合/化能取消阶段上限，阶段 2 之后可“补课”，价格 ×2。
     * 阶段 3、尚无任何突变（首突变 6 折）：
     * 18 × (0.35 + 0.12×3) × 2 × 0.6 = 15.34 -> 16 */
    game.legacy.levels[LEGACY_MUTATION] = 0;
    game.run.active_species[0].species_id = SPECIES_MULTICELLULAR_COLONY;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_PHOTOSYNTHESIS));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_CHEMOSYNTHESIS));
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 16.0, 0.001);
    /* 同阶段下不受补课加价的突变：22 × 0.71 × 0.6 = 9.37 -> 10 */
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_CELL_WALL), 10.0, 0.001);
    CHECK(evolution_mutation_available(&game, 0, MUTATION_MULTICELLULARITY));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_CELL_DIFFERENTIATION));
    game.run.active_species[0].mutations[MUTATION_MULTICELLULARITY] = true;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_CELL_DIFFERENTIATION));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_NEURAL_SIGNALING));

    /* 前置突变 */
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.active_species[0].species_id = SPECIES_ALGAL_EUKARYOTE;
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_MITOCHONDRIA));
    game.run.active_species[0].mutations[MUTATION_NUCLEUS] = true;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_MITOCHONDRIA));

    /* 第四轮：Decomposer Enzymes 需要先有多细胞性 */
    game.run.active_species[0].species_id = SPECIES_ALGAL_EUKARYOTE;
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_DECOMPOSER_ENZYMES));
    game.run.active_species[0].mutations[MUTATION_MULTICELLULARITY] = true;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_DECOMPOSER_ENZYMES));
}

static void test_mutation_offer(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.dna = 500.0;
    game.run.biomass = 0.0;

    MutationOfferState offer = evolution_generate_mutation_offer(&game, 0);
    CHECK(offer.active);
    CHECK(offer.choice_count == 2);
    CHECK(offer.mutation_ids[0] != offer.mutation_ids[1]);
    CHECK(game.run.rng_state == 0x12345678u); /* const 版本不推进随机数 */

    /* 打开后保存，重复打开返回同一组候选且不消耗随机数 */
    ActionCode code = evolution_open_mutation_offer(&game, 0);
    CHECK(code == ACTION_OK);
    unsigned int rng_after_open = game.run.rng_state;
    MutationOfferState stored = game.run.mutation_offer;
    CHECK(evolution_open_mutation_offer(&game, 0) == ACTION_OK);
    CHECK(game.run.rng_state == rng_after_open);
    CHECK(game.run.mutation_offer.choice_count == stored.choice_count);
    for (int i = 0; i < stored.choice_count; ++i) {
        CHECK(game.run.mutation_offer.mutation_ids[i] == stored.mutation_ids[i]);
        CHECK_NEAR(game.run.mutation_offer.dna_costs[i], stored.dna_costs[i], 0.001);
    }

    /* 只能选择候选中的突变 */
    int not_offered = -1;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (!offer_contains(&game.run.mutation_offer, id)) {
            not_offered = id;
            break;
        }
    }
    CHECK(not_offered >= 0);
    CHECK(evolution_apply_mutation(&game, 0, not_offered) == ACTION_INVALID_TARGET);

    /* 跨槽打开不得刷新候选（02 §6.4 第 7 条） */
    GameState two;
    reset_game(&two, SPECIES_PROTOCELL);
    two.run.active_species_count = 2;
    two.run.active_species[1].species_id = SPECIES_CYANOBACTERIUM;
    two.run.dna = 500.0;
    CHECK(evolution_open_mutation_offer(&two, 0) == ACTION_OK);
    MutationOfferState first_offer = two.run.mutation_offer;
    unsigned int rng_before = two.run.rng_state;
    CHECK(evolution_open_mutation_offer(&two, 1) == ACTION_REQUIREMENT_NOT_MET);
    CHECK(two.run.rng_state == rng_before);
    CHECK(two.run.mutation_offer.species_slot == 0);
    CHECK(two.run.mutation_offer.mutation_ids[0] == first_offer.mutation_ids[0]);

    /* DNA 不足时不扣资源 */
    int chosen = game.run.mutation_offer.mutation_ids[0];
    double cost = game.run.mutation_offer.dna_costs[0];
    game.run.dna = cost - 1.0;
    CHECK(evolution_apply_mutation(&game, 0, chosen) == ACTION_NOT_ENOUGH_DNA);
    CHECK(game.run.dna == cost - 1.0);
    CHECK(!game.run.active_species[0].mutations[chosen]);
    CHECK(game.run.mutation_offer.active);

    /* 成功选择后扣费、记录图鉴并清空候选 */
    game.run.dna = cost + 5.0;
    CHECK(evolution_apply_mutation(&game, 0, chosen) == ACTION_OK);
    CHECK_NEAR(game.run.dna, 5.0, 0.001);
    CHECK(game.run.active_species[0].mutations[chosen]);
    CHECK(game.run.discovered_mutations[chosen]);
    CHECK(game.legacy.codex_mutations[chosen]);
    CHECK(!game.run.mutation_offer.active);

    /* Mutation Lv.3 -> 3 个候选 */
    GameState game3;
    reset_game(&game3, SPECIES_PROTOCELL);
    game3.legacy.levels[LEGACY_MUTATION] = 3;
    game3.run.dna = 500.0;
    CHECK(evolution_open_mutation_offer(&game3, 0) == ACTION_OK);
    CHECK(game3.run.mutation_offer.choice_count == 3);

    /* 已经拥有的突变不会再次出现 */
    reset_game(&game3, SPECIES_PROTOCELL);
    game3.legacy.levels[LEGACY_MUTATION] = 3;
    game3.run.active_species[0].mutations[MUTATION_CELL_MEMBRANE] = true;
    game3.run.active_species[0].mutations[MUTATION_CELL_WALL] = true;
    CHECK(evolution_open_mutation_offer(&game3, 0) == ACTION_OK);
    for (int i = 0; i < game3.run.mutation_offer.choice_count; ++i) {
        CHECK(game3.run.mutation_offer.mutation_ids[i] != MUTATION_CELL_MEMBRANE);
        CHECK(game3.run.mutation_offer.mutation_ids[i] != MUTATION_CELL_WALL);
    }
}

static void test_key_mutation_guarantee(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.legacy.levels[LEGACY_MUTATION] = 1; /* 缩小可选池，仍应保底 */
    game.run.biomass = 48.0;                /* 80 × 60% */
    game.run.dna = 500.0;

    CHECK(evolution_open_mutation_offer(&game, 0) == ACTION_OK);
    CHECK(offer_contains(&game.run.mutation_offer, MUTATION_PHOTOSYNTHESIS) ||
          offer_contains(&game.run.mutation_offer, MUTATION_CHEMOSYNTHESIS));

    /* 未达到 60% 时不保底 */
    GameState low;
    reset_game(&low, SPECIES_PROTOCELL);
    low.legacy.levels[LEGACY_MUTATION] = 1;
    low.run.biomass = 10.0;
    low.run.dna = 500.0;
    CHECK(evolution_open_mutation_offer(&low, 0) == ACTION_OK);
    CHECK(low.run.mutation_offer.active);
}

static void test_speciation(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.active_species[0].population = 100.0;
    game.run.biomass = 100.0;

    CHECK(evolution_validate_speciation(&game, 0, SPECIES_CYANOBACTERIUM) ==
          ACTION_REQUIREMENT_NOT_MET);

    game.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_CYANOBACTERIUM) == ACTION_OK);
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) ==
          ACTION_INVALID_TARGET);
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_CHEMOLITHOTROPH) ==
          ACTION_REQUIREMENT_NOT_MET);

    /* 未解锁第二槽时 option=1 不能保留亲本 */
    CHECK(evolution_apply_speciation(&game, 0, SPECIES_CYANOBACTERIUM, 1) ==
          ACTION_REQUIREMENT_NOT_MET);
    CHECK_NEAR(game.run.biomass, 100.0, 0.001);

    CHECK(evolution_apply_speciation(&game, 0, SPECIES_CYANOBACTERIUM, 0) == ACTION_OK);
    CHECK(game.run.active_species_count == 1);
    CHECK(game.run.active_species[0].species_id == SPECIES_CYANOBACTERIUM);
    CHECK(game.run.active_species[0].is_main_lineage);
    CHECK_NEAR(game.run.active_species[0].population, 70.0, 0.001);
    CHECK_NEAR(game.run.biomass, 45.0, 0.001); /* 第十轮：首次分化 80 → 55 */
    CHECK(game.run.discovered_species[SPECIES_CYANOBACTERIUM]);
    CHECK(game.legacy.codex_species[SPECIES_CYANOBACTERIUM]);
    CHECK(game.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS]); /* 继承亲本突变 */
    CHECK(!game.run.mutation_offer.active);

    /* 阶段 2：藻类真核生物需要 Nucleus，并且成本 350 */
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) ==
          ACTION_REQUIREMENT_NOT_MET);
    game.run.active_species[0].mutations[MUTATION_NUCLEUS] = true;
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) ==
          ACTION_NOT_ENOUGH_BIOMASS);
    game.run.biomass = 350.0;
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) == ACTION_OK);

    /* Diversification Lv.4 降低分化成本 15% */
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 4;
    CHECK_NEAR(evolution_speciation_cost(&game, SPECIES_ALGAL_EUKARYOTE), 297.0, 0.001);
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 0;

    /* 保留亲本：后代 60%，亲本 40%，且亲本不再是主谱系 */
    GameState keep;
    reset_game(&keep, SPECIES_PROTOCELL);
    keep.run.active_species[0].population = 100.0;
    keep.run.biomass = 200.0;
    keep.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    keep.legacy.levels[LEGACY_DIVERSIFICATION] = 2;
    CHECK(evolution_max_speciation_slots(&keep) == 2);
    CHECK(evolution_apply_speciation(&keep, 0, SPECIES_CYANOBACTERIUM, 1) == ACTION_OK);
    CHECK(keep.run.active_species_count == 2);
    CHECK(keep.run.active_species[0].species_id == SPECIES_CYANOBACTERIUM);
    CHECK(keep.run.active_species[0].is_main_lineage);
    CHECK_NEAR(keep.run.active_species[0].population, 60.0, 0.001);
    CHECK(keep.run.active_species[1].species_id == SPECIES_PROTOCELL);
    CHECK(!keep.run.active_species[1].is_main_lineage);
    CHECK_NEAR(keep.run.active_species[1].population, 40.0, 0.001);

    /* 第三轮：额外分支可以独立分化，且不吞并主谱系 */
    keep.run.active_species[1].mutations[MUTATION_CHEMOSYNTHESIS] = true;
    CHECK(evolution_validate_speciation(&keep, 1, SPECIES_CHEMOLITHOTROPH) == ACTION_OK);
    CHECK(evolution_apply_speciation(&keep, 1, SPECIES_CHEMOLITHOTROPH, 0) == ACTION_OK);
    CHECK(keep.run.active_species[0].species_id == SPECIES_CYANOBACTERIUM);
    CHECK(keep.run.active_species[0].is_main_lineage);
    CHECK(keep.run.active_species[1].species_id == SPECIES_CHEMOLITHOTROPH);
    CHECK(!keep.run.active_species[1].is_main_lineage);

    /* 01 §5：option=1 只允许从主谱系发起 */
    GameState extra_keep;
    reset_game(&extra_keep, SPECIES_PROTOCELL);
    extra_keep.legacy.levels[LEGACY_DIVERSIFICATION] = 5;
    extra_keep.run.active_species_count = 2;
    extra_keep.run.active_species[0].species_id = SPECIES_ALGAL_EUKARYOTE;
    extra_keep.run.active_species[0].is_main_lineage = true;
    extra_keep.run.active_species[1].species_id = SPECIES_PROTOCELL;
    extra_keep.run.active_species[1].is_main_lineage = false;
    extra_keep.run.active_species[1].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    extra_keep.run.biomass = 1000.0;
    CHECK(evolution_apply_speciation(&extra_keep, 1, SPECIES_CYANOBACTERIUM, 1) ==
          ACTION_INVALID_TARGET);

    /* 极端环境群落需要 Diversification Lv.5 */
    GameState extreme;
    reset_game(&extreme, SPECIES_CHEMOLITHOTROPH);
    extreme.run.active_species[0].mutations[MUTATION_HEAT_RESISTANCE] = true;
    extreme.run.active_species[0].mutations[MUTATION_TOXIN_RESISTANCE] = true;
    extreme.run.biomass = 5000.0;
    CHECK(evolution_validate_speciation(&extreme, 0, SPECIES_EXTREMOPHILE_COLONY) ==
          ACTION_REQUIREMENT_NOT_MET);
    extreme.legacy.levels[LEGACY_DIVERSIFICATION] = 5;
    CHECK(evolution_validate_speciation(&extreme, 0, SPECIES_EXTREMOPHILE_COLONY) == ACTION_OK);
    CHECK_NEAR(evolution_speciation_cost(&extreme, SPECIES_EXTREMOPHILE_COLONY), 850.0, 0.001);

    /* 第四轮：Diversification Lv.5 解锁第三槽 */
    GameState triple;
    reset_game(&triple, SPECIES_PROTOCELL);
    triple.legacy.levels[LEGACY_DIVERSIFICATION] = 5;
    CHECK(evolution_max_speciation_slots(&triple) == 3);

    /* 真菌网络：原生捕食者 + Decomposer Enzymes */
    GameState fungal;
    reset_game(&fungal, SPECIES_PROTOZOAN);
    fungal.run.active_species[0].mutations[MUTATION_DECOMPOSER_ENZYMES] = true;
    fungal.run.biomass = 5000.0;
    CHECK(evolution_validate_speciation(&fungal, 0, SPECIES_FUNGAL_NETWORK) == ACTION_OK);
    CHECK(evolution_validate_speciation(&fungal, 0, SPECIES_CORAL_SYMBIONT) ==
          ACTION_INVALID_TARGET);
}

static void test_rates(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);

    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80, 0.0001);
    CHECK_NEAR(rates.biomass_per_second, 0.25, 0.0001);
    /* 第十轮：阶段 0–1 的 DNA 产出 ×1.5 */
    CHECK_NEAR(rates.dna_per_second, 0.12, 0.0001);
    CHECK_NEAR(rates.final_population_cap, 80.0, 0.0001);
    CHECK_NEAR(rates.environment_multiplier, 1.0, 0.0001);
    CHECK(!rates.synergy_active);

    /* 单轮代谢升级 ×1.25 */
    game.run.run_upgrade_levels[UPGRADE_METABOLISM] = 1;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 1.00, 0.0001);

    /* 永久代谢 Lv.1 ×1.08 */
    game.legacy.levels[LEGACY_METABOLISM] = 1;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 1.08, 0.0001);

    /* 多样性 Lv.1 提高上限 10% */
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 1;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.final_population_cap, 88.0, 0.0001);

    /* 种群到达上限时正向增长为 0 */
    game.run.active_species[0].population = 88.0;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.population_delta_per_second, 0.0, 0.000001);

    /* 协同：不同生态标签 ×1.10，相同标签不协同 */
    GameState synergy;
    reset_game(&synergy, SPECIES_PROTOCELL);
    synergy.legacy.levels[LEGACY_DIVERSIFICATION] = 3;
    synergy.run.active_species_count = 2;
    synergy.run.active_species[1].species_id = SPECIES_CYANOBACTERIUM;
    synergy.run.active_species[1].population = 10.0;
    rates = evolution_calculate_species_rates(&synergy, 0);
    CHECK(rates.synergy_active);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.10, 0.0001);

    synergy.run.active_species[1].species_id = SPECIES_PROTOCELL;
    rates = evolution_calculate_species_rates(&synergy, 0);
    CHECK(!rates.synergy_active);
    CHECK_NEAR(rates.atp_per_second, 0.80, 0.0001);
}

static void test_environment_multiplier(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.current_environment_id = ENV_GLOBAL_WARMING;

    /* 第十一轮：危机期间劣势方产率再 ×0.4（0.60 × 0.4 = 0.24） */
    CHECK_NEAR(environment_multiplier_for_slot(&game, 0), 0.24, 0.0001);

    game.run.active_species[0].mutations[MUTATION_HEAT_RESISTANCE] = true;
    CHECK_NEAR(environment_multiplier_for_slot(&game, 0), 1.10, 0.0001);

    game.legacy.levels[LEGACY_ADAPTATION] = 1;
    CHECK_NEAR(environment_multiplier_for_slot(&game, 0), 1.10, 0.0001); /* 只削负面 */

    game.run.active_species[0].mutations[MUTATION_HEAT_RESISTANCE] = false;
    CHECK_NEAR(environment_multiplier_for_slot(&game, 0), 0.2528, 0.0001);

    game.legacy.levels[LEGACY_ADAPTATION] = 4;
    game.run.active_species[0].mutations[MUTATION_HEAT_RESISTANCE] = true;
    CHECK_NEAR(environment_multiplier_for_slot(&game, 0), 1.115, 0.0001);

    /* 低光事件对不同物种表现不同 */
    GameState compare;
    reset_game(&compare, SPECIES_PROTOCELL);
    compare.run.current_environment_id = ENV_LOW_LIGHT_PERIOD;
    CHECK_NEAR(environment_multiplier_for_slot(&compare, 0), 0.24, 0.0001);
    compare.run.active_species[0].species_id = SPECIES_CHEMOLITHOTROPH;
    CHECK_NEAR(environment_multiplier_for_slot(&compare, 0), 1.10, 0.0001);
    compare.run.active_species[0].species_id = SPECIES_CYANOBACTERIUM;
    CHECK_NEAR(environment_multiplier_for_slot(&compare, 0), 0.24, 0.0001);
}

static void test_environment_steps(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    environment_init_run(&game);
    CHECK_NEAR(game.run.event_interval_left, FIRST_EVENT_DELAY, 0.0001);
    CHECK(game.run.next_environment_id >= 1);
    CHECK(game.run.next_environment_id != ENV_METEOR_IMPACT); /* 阶段 3 前不出现 */

    /* 事件开始 */
    game.run.event_interval_left = 0.05;
    game.run.next_environment_id = ENV_GLOBAL_WARMING;
    EnvironmentStep step = environment_calculate_step(&game, FIXED_STEP_SECONDS);
    CHECK(step.event_started);
    CHECK(step.current_environment_id == ENV_GLOBAL_WARMING);
    CHECK_NEAR(step.environment_time_left, 45.0 * EVENT_DURATION_SCALE, 0.0001);
    environment_apply_step(&game, &step);
    CHECK(game.run.current_environment_id == ENV_GLOBAL_WARMING);
    CHECK_NEAR(game.run.crisis_start_population, 10.0, 0.0001);
    CHECK(game.run.next_environment_id == -1);

    /* 事件结束且主谱系全程保留 70% 以上 -> 算度过一次危机 */
    game.run.active_species[0].population = 8.0;
    game.run.environment_time_left = 0.05;
    step = environment_calculate_step(&game, FIXED_STEP_SECONDS);
    CHECK(step.event_ended);
    CHECK(step.crisis_survived);
    environment_apply_step(&game, &step);
    CHECK(game.run.survived_crises[ENV_GLOBAL_WARMING]);
    CHECK(game.run.current_environment_id == ENV_NORMAL_OCEAN);
    CHECK_NEAR(game.run.event_interval_left, EVENT_INTERVAL, 0.0001);
    CHECK(game.run.next_environment_id >= 1);
    CHECK(game.run.next_environment_id != ENV_GLOBAL_WARMING); /* 不连续出现 */

    /* 主谱系跌破 70% 不算通过 */
    GameState fail_case;
    reset_game(&fail_case, SPECIES_PROTOCELL);
    fail_case.run.current_environment_id = ENV_GLOBAL_COOLING;
    fail_case.run.crisis_start_population = 100.0;
    fail_case.run.active_species[0].population = 5.0;
    fail_case.run.environment_time_left = 0.05;
    step = environment_calculate_step(&fail_case, FIXED_STEP_SECONDS);
    CHECK(step.event_ended);
    CHECK(!step.crisis_survived);
    environment_apply_step(&fail_case, &step);
    CHECK(!fail_case.run.survived_crises[ENV_GLOBAL_COOLING]);

    /* Adaptation Lv.2 缩短危机持续时间 */
    GameState shortened;
    reset_game(&shortened, SPECIES_PROTOCELL);
    shortened.legacy.levels[LEGACY_ADAPTATION] = 2;
    shortened.run.event_interval_left = 0.05;
    shortened.run.next_environment_id = ENV_GLOBAL_COOLING;
    step = environment_calculate_step(&shortened, FIXED_STEP_SECONDS);
    CHECK(step.event_started);
    CHECK_NEAR(step.environment_time_left, 45.0 * 0.85 * EVENT_DURATION_SCALE, 0.0001);

    /* Meteor Impact：阶段 3 解锁，即时损失 0.35 + 0.05×3 = 50% */
    GameState meteor;
    reset_game(&meteor, SPECIES_MULTICELLULAR_COLONY);
    meteor.run.active_species[0].population = 100.0;
    environment_init_run(&meteor);
    meteor.run.event_interval_left = 0.05;
    meteor.run.next_environment_id = ENV_METEOR_IMPACT;
    step = environment_calculate_step(&meteor, FIXED_STEP_SECONDS);
    CHECK(step.event_started);
    CHECK_NEAR(step.instant_population_loss_ratio, 0.50, 0.0001);
    environment_apply_step(&meteor, &step);
    CHECK_NEAR(meteor.run.active_species[0].population, 50.0, 0.0001);
    CHECK_NEAR(meteor.run.crisis_start_population, 100.0, 0.0001);
    CHECK_NEAR(meteor.run.crisis_min_population, 50.0, 0.0001);

    /* 应急储备 Lv.5 把即时损失减半：0.50 × 0.5 = 25% */
    GameState meteor_reserve;
    reset_game(&meteor_reserve, SPECIES_MULTICELLULAR_COLONY);
    meteor_reserve.run.active_species[0].population = 100.0;
    meteor_reserve.run.reserve_levels = 5;
    environment_init_run(&meteor_reserve);
    meteor_reserve.run.event_interval_left = 0.05;
    meteor_reserve.run.next_environment_id = ENV_METEOR_IMPACT;
    step = environment_calculate_step(&meteor_reserve, FIXED_STEP_SECONDS);
    CHECK_NEAR(step.instant_population_loss_ratio, 0.25, 0.0001);

    /* Adaptation Lv.3 事件预览 */
    GameState preview;
    reset_game(&preview, SPECIES_PROTOCELL);
    preview.legacy.levels[LEGACY_ADAPTATION] = 3;
    preview.run.event_interval_left = 10.0;
    preview.run.next_environment_id = ENV_GLOBAL_WARMING;
    CHECK(environment_preview_available(&preview));
    preview.run.event_interval_left = 60.0;
    CHECK(!environment_preview_available(&preview));
    preview.legacy.levels[LEGACY_ADAPTATION] = 2;
    preview.run.event_interval_left = 10.0;
    CHECK(!environment_preview_available(&preview));
}

static void test_adaptation_shield(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.legacy.levels[LEGACY_ADAPTATION] = 5;
    game.run.current_environment_id = ENV_GLOBAL_WARMING;
    game.run.environment_time_left = 10.0;
    game.run.crisis_start_population = 100.0;
    game.run.active_species[0].population = 20.0;

    EnvironmentStep step = environment_calculate_step(&game, FIXED_STEP_SECONDS);
    CHECK(step.adaptation_shield_triggered);
    environment_apply_step(&game, &step);
    CHECK(game.run.adaptation_shield_active);
    CHECK(game.run.adaptation_shield_used);

    double environment = environment_multiplier_for_slot(&game, 0);
    double population = game.run.active_species[0].population;
    double cap = 80.0;
    double damage = environment_damage_per_second(ENV_GLOBAL_WARMING);
    double positive = population * 0.010 * 1.10 * (1.0 - population / cap); /* 原始细胞基石 */
    double raw_loss = population * damage * (2.0 - environment) * CRISIS_DAMAGE_MULTIPLIER;
    double expected = positive - raw_loss * 0.2;

    SpeciesRates shielded = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(shielded.population_delta_per_second, expected, 0.0001);

    GameState unshielded = game;
    unshielded.run.adaptation_shield_active = false;
    SpeciesRates plain = evolution_calculate_species_rates(&unshielded, 0);
    CHECK_NEAR(plain.population_delta_per_second, positive - raw_loss, 0.0001);

    /* 危机结束时清除 active，但不退还本轮已使用的次数 */
    game.run.environment_time_left = 0.05;
    step = environment_calculate_step(&game, FIXED_STEP_SECONDS);
    environment_apply_step(&game, &step);
    CHECK(!game.run.adaptation_shield_active);
    CHECK(game.run.adaptation_shield_used);

    /* 已使用后不会再次触发 */
    game.run.current_environment_id = ENV_GLOBAL_COOLING;
    game.run.environment_time_left = 10.0;
    game.run.crisis_start_population = 100.0;
    game.run.active_species[0].population = 10.0;
    step = environment_calculate_step(&game, FIXED_STEP_SECONDS);
    CHECK(!step.adaptation_shield_triggered);
}

static void test_prestige(void)
{
    GameState game;
    reset_game(&game, SPECIES_MULTICELLULAR_COLONY);
    for (int id = 0; id < SPECIES_COUNT; ++id) {
        game.run.discovered_species[id] = false;
    }
    game.run.discovered_mutations[0] = true;
    game.run.discovered_mutations[1] = true;
    game.run.discovered_mutations[2] = true;
    game.run.discovered_mutations[3] = true;
    game.run.discovered_mutations[4] = true;
    game.run.discovered_species[1] = true;
    game.run.discovered_species[2] = true;
    game.run.discovered_species[3] = true;
    game.run.survived_crises[ENV_GLOBAL_WARMING] = true;
    game.run.survived_crises[ENV_GLOBAL_COOLING] = true;
    game.run.max_biomass = 500.0;

    int stages = 0;
    int mutations = 0;
    int species = 0;
    int crises = 0;
    prestige_run_stats(&game, &stages, &mutations, &species, &crises);
    CHECK(stages == 3);
    CHECK(mutations == 5);
    CHECK(species == 3);
    CHECK(crises == 2);

    EraReward reward = prestige_preview_reward(&game);
    CHECK(reward.era_points == 6);
    CHECK(reward.mutation_points == 1);
    CHECK(reward.species_points == 1);
    CHECK(reward.crisis_points == 2);
    CHECK(reward.biomass_points == 2);
    CHECK(reward.total_points == 12);
    CHECK(reward.total_points == reward.era_points + reward.mutation_points +
                                     reward.species_points + reward.crisis_points +
                                     reward.biomass_points);

    CHECK(prestige_biomass_milestone_level(99.0) == 0);
    CHECK(prestige_biomass_milestone_level(100.0) == 1);
    CHECK(prestige_biomass_milestone_level(49999.0) == 4);
    CHECK(prestige_biomass_milestone_level(50000.0) == 5);
    CHECK(prestige_biomass_milestone_level(99999.0) == 5);
    CHECK(prestige_biomass_milestone_level(250000.0) == 6);
    CHECK(prestige_biomass_milestone_level(1000000.0) == 7);
    CHECK(prestige_biomass_milestone_level(2500000.0) == 8);
    CHECK(prestige_biomass_milestone_level(5000000.0) == 9);
    CHECK(prestige_biomass_milestone_level(10000000.0) == 10);

    /* 第八轮长线后的理论上限：2*10 + 33/3 + 18/2 + 6 + 10 = 56 */
    GameState full;
    reset_game(&full, SPECIES_SINGULARITY);
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        full.run.discovered_mutations[id] = true;
    }
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        full.run.discovered_species[id] = true;
    }
    for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id) {
        full.run.survived_crises[id] = true;
    }
    full.run.max_biomass = 10000000.0;
    EraReward max_reward = prestige_preview_reward(&full);
    CHECK(max_reward.era_points == 20);
    CHECK(max_reward.mutation_points == 14);
    CHECK(max_reward.species_points == 12);
    CHECK(max_reward.crisis_points == 6);
    CHECK(max_reward.biomass_points == 10);
    CHECK(max_reward.total_points == 62);

    /* 第十二轮阶段 11 后的理论上限：2*11 + 34/3 + 19/2 + 6 + 10 = 58 */
    GameState full11;
    reset_game(&full11, SPECIES_TRANSCENDENCE);
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        full11.run.discovered_mutations[id] = true;
    }
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        full11.run.discovered_species[id] = true;
    }
    for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id) {
        full11.run.survived_crises[id] = true;
    }
    full11.run.max_biomass = 10000000.0;
    EraReward max11 = prestige_preview_reward(&full11);
    CHECK(max11.era_points == 22);
    CHECK(max11.mutation_points == 14);
    CHECK(max11.species_points == 12);
    CHECK(max11.crisis_points == 6);
    CHECK(max11.biomass_points == 10);
    CHECK(max11.total_points == 64);

    /* 危机最多计 6 个（上限翻倍） */
    game.run.survived_crises[ENV_LOW_LIGHT_PERIOD] = true;
    game.run.survived_crises[ENV_OXYGEN_RISE] = true;
    game.run.survived_crises[ENV_TOXIC_BLOOM] = true;
    reward = prestige_preview_reward(&game);
    CHECK(reward.crisis_points == 5);
    game.run.survived_crises[ENV_METEOR_IMPACT] = true;
    reward = prestige_preview_reward(&game);
    CHECK(reward.crisis_points == 6);

    /* 预览不改变状态 */
    CHECK(game.legacy.evolution_points == 0);
    CHECK(game.phase == PHASE_RUNNING);
    CHECK(!game.run.reward_claimed);

    /* 确认结束：只入账一次 */
    game.phase = PHASE_END_ERA_PREVIEW;
    CHECK(prestige_commit_end_era(&game) == ACTION_OK);
    CHECK(game.legacy.evolution_points == reward.total_points);
    CHECK(game.phase == PHASE_LEGACY);
    CHECK(game.run.reward_claimed);
    CHECK(game.legacy.best_era == 3);
    CHECK_NEAR(game.legacy.best_biomass, 500.0, 0.001);
    CHECK(prestige_commit_end_era(&game) == ACTION_ALREADY_CLAIMED);
    CHECK(game.legacy.evolution_points == reward.total_points);

    /* 读档后停留在 Legacy 也仍然不能重复领取 */
    GameState reloaded = game;
    reloaded.phase = PHASE_LEGACY;
    CHECK(prestige_commit_end_era(&reloaded) == ACTION_ALREADY_CLAIMED);
}

static void test_legacy(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.phase = PHASE_LEGACY;
    game.legacy.evolution_points = 3;

    CHECK(legacy_upgrade_cost(0) == 1);
    CHECK(legacy_upgrade_cost(1) == 2);
    CHECK(legacy_upgrade_cost(4) == 5);
    CHECK(legacy_upgrade_cost(7) == 8);
    CHECK(legacy_upgrade_cost(8) == 0);

    CHECK(legacy_apply_upgrade(&game, LEGACY_METABOLISM) == ACTION_OK);
    CHECK(game.legacy.levels[LEGACY_METABOLISM] == 1);
    CHECK(game.legacy.evolution_points == 2);
    CHECK(legacy_apply_upgrade(&game, LEGACY_METABOLISM) == ACTION_OK);
    CHECK(game.legacy.evolution_points == 0);
    CHECK(legacy_apply_upgrade(&game, LEGACY_METABOLISM) == ACTION_NOT_ENOUGH_EP);

    CHECK(legacy_validate_upgrade(&game, LEGACY_DIVERSIFICATION) == ACTION_NOT_ENOUGH_EP);
    game.phase = PHASE_RUNNING;
    CHECK(legacy_validate_upgrade(&game, LEGACY_DIVERSIFICATION) == ACTION_INVALID_PHASE);

    game.phase = PHASE_LEGACY;
    game.legacy.evolution_points = 15;
    game.legacy.levels[LEGACY_MUTATION] = 0;
    for (int i = 0; i < 5; ++i) {
        CHECK(legacy_apply_upgrade(&game, LEGACY_MUTATION) == ACTION_OK);
    }
    CHECK(game.legacy.levels[LEGACY_MUTATION] == 5);
    CHECK(game.legacy.evolution_points == 0);
    game.legacy.evolution_points = 100;
    for (int i = 5; i < LEGACY_MAX_LEVEL; ++i) {
        CHECK(legacy_apply_upgrade(&game, LEGACY_MUTATION) == ACTION_OK);
    }
    CHECK(game.legacy.levels[LEGACY_MUTATION] == LEGACY_MAX_LEVEL);
    CHECK(legacy_apply_upgrade(&game, LEGACY_MUTATION) == ACTION_LEVEL_MAXED);

    /* 永久倍率 */
    game.legacy.levels[LEGACY_METABOLISM] = 4;
    CHECK_NEAR(legacy_atp_multiplier(&game.legacy), 1.08 * 1.12, 0.0001);
    CHECK_NEAR(legacy_biomass_multiplier(&game.legacy), 1.08 * 1.12, 0.0001);
    CHECK_NEAR(legacy_dna_multiplier(&game.legacy), 1.12, 0.0001);

    /* 第四轮：Lv.5–8 不再空转 */
    game.legacy.levels[LEGACY_METABOLISM] = 5;
    CHECK_NEAR(legacy_biomass_multiplier(&game.legacy), 1.08 * 1.12 * 1.05, 0.0001);
    game.legacy.levels[LEGACY_METABOLISM] = 6;
    CHECK_NEAR(legacy_click_multiplier(&game.legacy), 1.5, 0.0001);
    game.legacy.levels[LEGACY_METABOLISM] = 8;
    CHECK_NEAR(legacy_production_multiplier(&game.legacy), 1.10, 0.0001);

    game.legacy.levels[LEGACY_MUTATION] = 5;
    CHECK_NEAR(legacy_key_guarantee_ratio(&game.legacy), 0.40, 0.0001);
    CHECK(evolution_mutation_choice_count(&game) == 3);
    game.legacy.levels[LEGACY_MUTATION] = 6;
    CHECK(evolution_mutation_choice_count(&game) == 4);
    game.legacy.levels[LEGACY_MUTATION] = 7;
    CHECK_NEAR(legacy_mutation_discount(&game.legacy), 0.20, 0.0001);
    game.legacy.levels[LEGACY_MUTATION] = 8;
    CHECK(legacy_rare_mutation_weight(&game.legacy) == 60);

    game.legacy.levels[LEGACY_ADAPTATION] = 6;
    CHECK_NEAR(legacy_crisis_damage_reduction(&game.legacy), 0.15, 0.0001);
    game.legacy.levels[LEGACY_ADAPTATION] = 7;
    CHECK_NEAR(legacy_positive_duration_scale(&game.legacy), 1.5, 0.0001);
    game.legacy.levels[LEGACY_ADAPTATION] = 8;
    CHECK_NEAR(legacy_crisis_damage_reduction(&game.legacy), 0.30, 0.0001);
    CHECK_NEAR(legacy_shield_damage_reduction(&game.legacy), 0.95, 0.0001);

    game.legacy.levels[LEGACY_DIVERSIFICATION] = 6;
    CHECK_NEAR(legacy_cap_multiplier(&game.legacy), 1.10 * 1.15, 0.0001);
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 7;
    CHECK_NEAR(legacy_synergy_multiplier(&game.legacy), 1.15, 0.0001);
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 8;
    CHECK_NEAR(legacy_speciation_discount(&game.legacy), 0.25, 0.0001);

    /* 新轮奖励：50 ATP 与免费 1 级代谢 */
    GameState fresh;
    reset_game(&fresh, SPECIES_PROTOCELL);
    fresh.legacy.levels[LEGACY_METABOLISM] = 3;
    fresh.run.atp = STARTING_ATP;
    legacy_apply_new_run_bonuses(&fresh);
    CHECK_NEAR(fresh.run.atp, 50.0, 0.0001);
    CHECK(fresh.run.run_upgrade_levels[UPGRADE_METABOLISM] == 1);
    legacy_apply_new_run_bonuses(&fresh);
    CHECK_NEAR(fresh.run.atp, 50.0, 0.0001); /* 不会重复叠加 */

    /* 未达 Lv.3 不发放 */
    GameState plain;
    reset_game(&plain, SPECIES_PROTOCELL);
    plain.legacy.levels[LEGACY_METABOLISM] = 2;
    legacy_apply_new_run_bonuses(&plain);
    CHECK_NEAR(plain.run.atp, 0.0, 0.0001);
    CHECK(plain.run.run_upgrade_levels[UPGRADE_METABOLISM] == 0);
}

static void test_two_species_flow(void)
{
    /* 多样性 Lv.2/3 的完整一轮：额外分支独立产生资源并参与协同 */
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 3;
    game.run.active_species_count = 2;
    game.run.active_species[0].species_id = SPECIES_CYANOBACTERIUM;
    game.run.active_species[0].population = 20.0;
    game.run.active_species[1].species_id = SPECIES_PROTOCELL;
    game.run.active_species[1].is_main_lineage = false;
    game.run.active_species[1].population = 20.0;
    game.run.discovered_species[SPECIES_CYANOBACTERIUM] = true;

    SpeciesRates main_rates = evolution_calculate_species_rates(&game, 0);
    SpeciesRates branch_rates = evolution_calculate_species_rates(&game, 1);
    CHECK(main_rates.synergy_active);
    CHECK(branch_rates.synergy_active);
    CHECK(main_rates.atp_per_second > 0.0);
    CHECK(branch_rates.atp_per_second > 0.0);
    CHECK_NEAR(main_rates.atp_per_second, 1.30 * 2.0 * 1.10, 0.0001);
    CHECK_NEAR(branch_rates.atp_per_second, 0.80 * 2.0 * 1.10, 0.0001);

    /* 额外分支不推进时代：主谱系仍是阶段 1 */
    CHECK(evolution_main_stage(&game) == 1);
    CHECK(evolution_main_slot(&game) == 0);
}

/* 模拟 A 的固定步数据流：先算全部产率，再一起写回，最后推进环境 */
static void simulate_fixed_step(GameState *game, double dt)
{
    SpeciesRates rates[MAX_ACTIVE_SPECIES];
    double populations[MAX_ACTIVE_SPECIES];
    double atp = 0.0;
    double biomass = 0.0;
    double dna = 0.0;
    double knowledge = 0.0;

    for (int i = 0; i < game->run.active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
        rates[i] = evolution_calculate_species_rates(game, i);
        atp += rates[i].atp_per_second;
        biomass += rates[i].biomass_per_second;
        dna += rates[i].dna_per_second;
        knowledge += rates[i].knowledge_per_second;
        double population = game->run.active_species[i].population +
                            rates[i].population_delta_per_second * dt;
        if (population < MIN_ACTIVE_POPULATION) {
            population = MIN_ACTIVE_POPULATION;
        }
        if (population > rates[i].final_population_cap) {
            population = rates[i].final_population_cap;
        }
        populations[i] = population;
    }
    for (int i = 0; i < game->run.active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
        game->run.active_species[i].population = populations[i];
    }
    game->run.atp += atp * dt;
    game->run.biomass += biomass * dt;
    game->run.dna += dna * dt;
    game->run.knowledge += knowledge * dt;
    if (game->run.biomass > game->run.max_biomass) {
        game->run.max_biomass = game->run.biomass;
    }

    EnvironmentStep step = environment_calculate_step(game, dt);
    environment_apply_step(game, &step);

    int main_slot = evolution_main_slot(game);
    if (!game->run.mutation_offer.active) {
        evolution_open_mutation_offer(game, main_slot);
    }
    if (game->run.mutation_offer.active) {
        int cheapest = -1;
        for (int i = 0; i < game->run.mutation_offer.choice_count; ++i) {
            if (cheapest < 0 || game->run.mutation_offer.dna_costs[i] <
                                    game->run.mutation_offer.dna_costs[cheapest]) {
                cheapest = i;
            }
        }
        if (cheapest >= 0 && game->run.dna >= game->run.mutation_offer.dna_costs[cheapest]) {
            evolution_apply_mutation(game, main_slot,
                                     game->run.mutation_offer.mutation_ids[cheapest]);
        }
    }

    int children[SPECIES_COUNT];
    int child_count = species_children(game->run.active_species[main_slot].species_id, children,
                                       SPECIES_COUNT);
    for (int i = 0; i < child_count; ++i) {
        if (evolution_validate_speciation(game, main_slot, children[i]) == ACTION_OK) {
            evolution_apply_speciation(game, main_slot, children[i], 0);
            break;
        }
    }
}

static void test_long_run_simulation(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    environment_init_run(&game);
    game.run.rng_state = 20261124u;

    const double dt = FIXED_STEP_SECONDS;
    const int steps = 9000; /* 15 分钟 */
    for (int i = 0; i < steps; ++i) {
        simulate_fixed_step(&game, dt);

        CHECK(game.run.active_species_count >= 1);
        CHECK(game.run.active_species[0].is_main_lineage);
        CHECK(isfinite(game.run.atp) && isfinite(game.run.biomass) && isfinite(game.run.dna));
        CHECK(game.run.active_species[0].population >= MIN_ACTIVE_POPULATION);
        if (g_failures > 0) {
            return; /* 首次失败即停止，避免刷屏 */
        }
    }

    CHECK(game.run.max_biomass >= 100.0);
    CHECK(evolution_main_stage(&game) >= 1);

    EraReward first = prestige_preview_reward(&game);
    EraReward second = prestige_preview_reward(&game);
    CHECK(first.total_points == second.total_points);
    CHECK(first.total_points >= 0);
    CHECK(first.total_points == first.era_points + first.mutation_points +
                                   first.species_points + first.crisis_points +
                                   first.biomass_points);

    int crises = 0;
    prestige_run_stats(&game, 0, 0, 0, &crises);
    CHECK(crises >= 1);
    CHECK(crises <= ENV_COUNT - 1);

    CHECK(game.run.environment_time_left <= 50.0 + 0.001);
    CHECK(game.run.event_interval_left <= EVENT_INTERVAL + 0.001);
}

static void test_synergy(void)
{
    /* 第四轮：三槽中只要存在异标签伙伴就触发协同 */
    GameState triple;
    reset_game(&triple, SPECIES_MULTICELLULAR_COLONY);
    triple.legacy.levels[LEGACY_DIVERSIFICATION] = 3;
    triple.run.active_species_count = 3;
    triple.run.active_species[1].species_id = SPECIES_MULTICELLULAR_COLONY;
    triple.run.active_species[2].species_id = SPECIES_CYANOBACTERIUM;
    SpeciesRates triple_rates = evolution_calculate_species_rates(&triple, 1);
    CHECK(triple_rates.synergy_active);

    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    CHECK(evolution_synergy_count() == 15);
    CHECK(evolution_active_synergy_count(&game, 0) == 0);

    game.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    CHECK(evolution_active_synergy_count(&game, 0) == 0);
    game.run.active_species[0].mutations[MUTATION_CELL_WALL] = true;
    CHECK(evolution_synergy_active(&game, 0, 0));
    CHECK(evolution_active_synergy_count(&game, 0) == 1);

    double atp = 0.0;
    double biomass = 0.0;
    double dna = 0.0;
    double cap = 0.0;
    double growth = 0.0;
    int traits[TRAIT_COUNT];
    evolution_synergy_bonuses(&game, 0, &atp, &biomass, &dna, &cap, &growth, traits);
    CHECK_NEAR(atp, 1.0, 0.0001);
    CHECK_NEAR(biomass, 1.10, 0.0001);
    CHECK_NEAR(dna, 1.0, 0.0001);
    CHECK_NEAR(cap, 1.0, 0.0001);
    CHECK_NEAR(growth, 1.0, 0.0001);
    CHECK(traits[TRAIT_LOW_LIGHT] == 1);

    /* 共振加成进入特征总值与产率 */
    /* 光合本身 -1 低光，共振 +1，净值为 0 */
    CHECK(evolution_slot_trait_total(&game, 0, TRAIT_LOW_LIGHT) == 0);
    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.25, 0.0001);
    CHECK_NEAR(rates.biomass_per_second, 0.25 * 1.10, 0.0001);
    CHECK_NEAR(rates.final_population_cap, 80.0 * 1.20, 0.0001);

    /* 四个组合的触发条件 */
    GameState other;
    reset_game(&other, SPECIES_CYANOBACTERIUM);
    other.run.active_species[0].mutations[MUTATION_CHEMOSYNTHESIS] = true;
    other.run.active_species[0].mutations[MUTATION_HEAT_RESISTANCE] = true;
    CHECK(evolution_synergy_active(&other, 0, 1));
    other.run.active_species[0].mutations[MUTATION_NUCLEUS] = true;
    other.run.active_species[0].mutations[MUTATION_MITOCHONDRIA] = true;
    CHECK(evolution_synergy_active(&other, 0, 2));
    other.run.active_species[0].mutations[MUTATION_MULTICELLULARITY] = true;
    other.run.active_species[0].mutations[MUTATION_TOXIN_RESISTANCE] = true;
    CHECK(evolution_synergy_active(&other, 0, 3));
    other.run.active_species[0].mutations[MUTATION_COLD_RESISTANCE] = true;
    CHECK(evolution_synergy_active(&other, 0, 4));
    other.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    other.run.active_species[0].mutations[MUTATION_CELL_WALL] = true;
    CHECK(evolution_synergy_active(&other, 0, 0));
    CHECK(evolution_active_synergy_count(&other, 0) == 5);
}

static void test_keystone(void)
{
    CHECK(species_keystone(SPECIES_PROTOCELL) != 0);
    CHECK(species_keystone(SPECIES_COUNT) == 0);
    CHECK_NEAR(species_keystone(SPECIES_EXTREMOPHILE_COLONY)->crisis_damage_reduction, 0.15,
               0.0001);

    /* 原始细胞：分裂更快 */
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.population_delta_per_second, 10.0 * 0.010 * 1.10 * (1.0 - 10.0 / 80.0),
               0.0001);

    /* 藻类真核：ATP +8% */
    reset_game(&game, SPECIES_ALGAL_EUKARYOTE);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 2.00 * 1.08, 0.0001);

    /* 原生捕食者：DNA +10% */
    reset_game(&game, SPECIES_PROTOZOAN);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.dna_per_second, 0.20 * 1.10, 0.0001);

    /* 多细胞群落：上限 +10%，增长 +5% */
    reset_game(&game, SPECIES_MULTICELLULAR_COLONY);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.final_population_cap, 450.0 * 1.10, 0.0001);

    /* 蓝细菌低光 +15%，化能菌毒素 +15%（危机期劣势产率 ×0.4） */
    reset_game(&game, SPECIES_CYANOBACTERIUM);
    game.run.current_environment_id = ENV_LOW_LIGHT_PERIOD;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 1.30 * 0.24 * 1.15, 0.0001);

    reset_game(&game, SPECIES_CHEMOLITHOTROPH);
    game.run.current_environment_id = ENV_TOXIC_BLOOM;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 1.10 * 0.34 * 1.15, 0.0001);
}

static void test_knowledge_and_sub_upgrades(void)
{
    /* 理念：阶段 < 6 不产出；阶段 >= 6 按 (种群/10)^0.5 产出 */
    GameState game;
    reset_game(&game, SPECIES_COGNITIVE_BIOSPHERE); /* 阶段 5 */
    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.knowledge_per_second, 0.0, 0.000001);

    reset_game(&game, SPECIES_SAPIENT_LIFE); /* 阶段 6，base_knowledge 0.30 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.knowledge_per_second, 0.30, 0.0001); /* 种群 10 -> 规模 1 */

    game.run.active_species[0].population = 1000.0; /* 规模 100 -> sqrt = 10 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.knowledge_per_second, 3.0, 0.0001);
    CHECK_NEAR(evolution_knowledge_per_second(&game), 3.0, 0.0001);

    /* 子升级表：6 个高阶物种各 3 层，成本逐层 ×6 */
    CHECK(evolution_sub_upgrade_count() == 24);
    const SubUpgradeDef *first = evolution_sub_upgrade_get(0);
    CHECK(first != 0);
    CHECK(first->species_id == SPECIES_SAPIENT_LIFE);
    CHECK(first->tier == 0);
    CHECK_NEAR(evolution_sub_upgrade_cost(first), 300.0, 0.001);
    const SubUpgradeDef *second = evolution_sub_upgrade_get(1);
    CHECK_NEAR(evolution_sub_upgrade_cost(second), 1800.0, 0.001);
    const SubUpgradeDef *last = evolution_sub_upgrade_get(14);
    CHECK(last->species_id == SPECIES_SINGULARITY);
    CHECK(last->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(last), 172800.0, 0.001);
    /* 第十二轮：阶段 11 追加 3 层，基础 9600，tier2 = 345600 */
    const SubUpgradeDef *omega = evolution_sub_upgrade_get(17);
    CHECK(omega->species_id == SPECIES_TRANSCENDENCE);
    CHECK(omega->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(omega), 345600.0, 0.001);
    /* 第十三轮：两条侧支各 3 层，最后一条为 Stellar Logistics（2400 × 36） */
    const SubUpgradeDef *stellar = evolution_sub_upgrade_get(23);
    CHECK(stellar->species_id == SPECIES_ORBITAL_SWARM);
    CHECK(stellar->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(stellar), 86400.0, 0.001);
    CHECK(evolution_sub_upgrade_get(24) == 0);

    /* 低阶物种没有子升级 */
    reset_game(&game, SPECIES_MULTICELLULAR_COLONY);
    CHECK(evolution_sub_upgrade_next(&game, 0) == 0);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_INVALID_TARGET);

    /* 理念不足 -> 拒绝且不扣资源 */
    reset_game(&game, SPECIES_SAPIENT_LIFE);
    game.run.knowledge = 299.0;
    CHECK(evolution_sub_upgrade_next(&game, 0) == first);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_NOT_ENOUGH_KNOWLEDGE);
    CHECK_NEAR(game.run.knowledge, 299.0, 0.001);
    CHECK(evolution_sub_upgrade_level(&game, 0) == 0);

    /* 购买后逐层推进，乘数生效 */
    game.run.knowledge = 300.0;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK);
    CHECK_NEAR(game.run.knowledge, 0.0, 0.001);
    CHECK(evolution_sub_upgrade_level(&game, 0) == 1);
    CHECK(evolution_sub_upgrade_next(&game, 0) == second);
    CHECK_NEAR(evolution_sub_upgrade_multiplier(&game, 0), 1.35, 0.0001);

    /* 乘数同时作用于 ATP 与理念（智慧生命基石 ATP +15%） */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 5.20 * 1.15 * 1.35, 0.0001);
    CHECK_NEAR(rates.knowledge_per_second, 0.30 * 1.35, 0.0001);

    /* 乘数同时作用于三种产率（DNA 不享受阶段 0–1 加成，阶段 6 无加成） */
    CHECK_NEAR(rates.dna_per_second, 0.70 * 1.35, 0.0001);

    /* 买满 3 层后拒绝；总乘数 1.35^3 */
    game.run.knowledge = 20000.0;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK); /* tier 1，1800 */
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK); /* tier 2，10800 */
    CHECK(evolution_sub_upgrade_level(&game, 0) == 3);
    CHECK(evolution_sub_upgrade_next(&game, 0) == 0);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_LEVEL_MAXED);
    CHECK_NEAR(evolution_sub_upgrade_multiplier(&game, 0), 1.35 * 1.35 * 1.35, 0.0001);

    /* 非 RUNNING 阶段拒绝 */
    game.phase = PHASE_END_ERA_PREVIEW;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_INVALID_PHASE);
}

static void test_round10_economy(void)
{
    /* 第十轮：容量扩张无上限；第十一轮：4 级起改为 ATP/Biomass 双币 */
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.biomass = 1e12;
    game.run.atp = 1e12;
    game.run.capacity_levels = 60;
    CHECK(evolution_apply_capacity_upgrade(&game) == ACTION_OK);
    CHECK(game.run.capacity_levels == 61);
    game.run.capacity_levels = 200;
    game.run.biomass = 1e30;
    game.run.atp = 1e30;
    CHECK(evolution_apply_capacity_upgrade(&game) == ACTION_OK);
    CHECK(game.run.capacity_levels == 201);

    /* 双币拆分：前 3 级 100% Biomass，第 4 级起 70/30 且总价不变 */
    GameState split;
    reset_game(&split, SPECIES_PROTOCELL);
    split.run.capacity_levels = 2;
    CHECK_NEAR(evolution_capacity_atp_cost(&split), 0.0, 0.001);
    CHECK_NEAR(evolution_capacity_biomass_cost(&split), evolution_capacity_cost(&split), 0.001);
    split.run.capacity_levels = 3;
    double total = evolution_capacity_cost(&split);
    CHECK_NEAR(evolution_capacity_atp_cost(&split), ceil(total * 0.70), 0.001);
    CHECK_NEAR(evolution_capacity_biomass_cost(&split), ceil(total * 0.30), 0.001);

    /* 双币支付：ATP 不足时拒绝且不扣 Biomass */
    GameState poor;
    reset_game(&poor, SPECIES_PROTOCELL);
    poor.run.capacity_levels = 3;
    poor.run.biomass = 1e9;
    poor.run.atp = 0.0;
    CHECK(evolution_apply_capacity_upgrade(&poor) == ACTION_NOT_ENOUGH_ATP);
    CHECK_NEAR(poor.run.biomass, 1e9, 1.0);
    CHECK(poor.run.capacity_levels == 3);

    /* 成本随等级单调增长 */
    reset_game(&game, SPECIES_PROTOCELL);
    double cost0 = evolution_capacity_cost(&game);
    game.run.capacity_levels = 5;
    double cost5 = evolution_capacity_cost(&game);
    CHECK(cost0 > 0.0);
    CHECK(cost5 > cost0);

    /* 保底折扣：生物量达门槛且关键突变进入候选时，其中一张按 6 折计价 */
    int found_key_offer = 0;
    for (int attempt = 0; attempt < 200 && !found_key_offer; ++attempt) {
        reset_game(&game, SPECIES_PROTOCELL);
        game.legacy.levels[LEGACY_MUTATION] = 1; /* 2 个候选，便于出现关键突变 */
        game.run.biomass = 48.0;
        game.run.max_biomass = 48.0;
        game.run.dna = 500.0;
        game.run.rng_state = 0x1234u + (unsigned int)attempt * 7919u;
        if (evolution_open_mutation_offer(&game, 0) != ACTION_OK) {
            continue;
        }
        int key_present = 0;
        int discounted = 0;
        for (int i = 0; i < game.run.mutation_offer.choice_count; ++i) {
            int id = game.run.mutation_offer.mutation_ids[i];
            if (id == MUTATION_PHOTOSYNTHESIS || id == MUTATION_CHEMOSYNTHESIS) {
                key_present = 1;
            }
            double normal = evolution_mutation_cost_for_slot(&game, 0, id);
            double stored = game.run.mutation_offer.dna_costs[i];
            if (stored < normal - 0.001) {
                ++discounted;
                CHECK_NEAR(stored, ceil(normal * 0.6), 0.001);
            } else {
                CHECK_NEAR(stored, normal, 0.001);
            }
        }
        if (key_present) {
            found_key_offer = 1;
            CHECK(discounted == 1);
        }
    }
    CHECK(found_key_offer);

    /* 峰值触发：当前 Biomass 为 0，但本纪峰值达门槛，关键突变仍保底 */
    GameState peak;
    reset_game(&peak, SPECIES_PROTOCELL);
    peak.legacy.levels[LEGACY_MUTATION] = 1;
    peak.run.biomass = 0.0;
    peak.run.max_biomass = 48.0;
    peak.run.dna = 500.0;
    CHECK(evolution_open_mutation_offer(&peak, 0) == ACTION_OK);
    CHECK(offer_contains(&peak.run.mutation_offer, MUTATION_PHOTOSYNTHESIS) ||
          offer_contains(&peak.run.mutation_offer, MUTATION_CHEMOSYNTHESIS));

    /* 早期 DNA 加成只作用于阶段 0–1 */
    reset_game(&game, SPECIES_CYANOBACTERIUM); /* 阶段 1 */
    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.dna_per_second, 0.10 * 1.5, 0.0001);
    reset_game(&game, SPECIES_ALGAL_EUKARYOTE); /* 阶段 2 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.dna_per_second, 0.16, 0.0001);
}

/* 可达性不动点：只依赖数据表，枚举“理论上是否存在一条合法路径”。
 * 能抓出永久锁（前置阶段窗口与依赖突变冲突），比多跑 1000 个种子更直接。 */
static void test_reachability(void)
{
    bool species_ok[SPECIES_COUNT] = { false };
    bool mutation_ok[MUTATION_COUNT] = { false };
    species_ok[SPECIES_PROTOCELL] = true;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int species_id = 1; species_id < SPECIES_COUNT; ++species_id) {
            if (species_ok[species_id]) {
                continue;
            }
            bool parent_ok = false;
            for (int parent = 0; parent < SPECIES_COUNT; ++parent) {
                if (species_ok[parent] && species_has_parent(species_id, parent)) {
                    parent_ok = true;
                    break;
                }
            }
            if (!parent_ok) {
                continue;
            }
            int required[4];
            int count = species_required_mutations(species_id, required, 4);
            bool all_required = true;
            for (int i = 0; i < count; ++i) {
                if (!mutation_ok[required[i]]) {
                    all_required = false;
                    break;
                }
            }
            if (all_required) {
                species_ok[species_id] = true;
                changed = true;
            }
        }
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            if (mutation_ok[id]) {
                continue;
            }
            const MutationDef *def = mutation_get(id);
            if (!def) {
                continue;
            }
            if (def->requires_mutation >= 0 && !mutation_ok[def->requires_mutation]) {
                continue;
            }
            bool phase_ok = false;
            for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
                if (!species_ok[species_id]) {
                    continue;
                }
                int phase = species_phase(species_id);
                if (phase >= def->min_phase && (def->max_phase < 0 || phase <= def->max_phase)) {
                    phase_ok = true;
                    break;
                }
            }
            if (phase_ok) {
                mutation_ok[id] = true;
                changed = true;
            }
        }
    }

    for (int id = 0; id < SPECIES_COUNT; ++id) {
        CHECK(species_ok[id]);
    }
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        CHECK(mutation_ok[id]);
    }
}

static void test_round12_stage11_and_crisis(void)
{
    /* 阶段 11：超越 */
    CHECK(species_phase(SPECIES_TRANSCENDENCE) == 11);
    CHECK(species_has_parent(SPECIES_TRANSCENDENCE, SPECIES_SINGULARITY));
    int required[4];
    CHECK(species_required_mutations(SPECIES_TRANSCENDENCE, required, 4) == 1);
    CHECK(required[0] == MUTATION_OMEGA_POINT);
    CHECK_NEAR(species_base_speciation_cost(SPECIES_TRANSCENDENCE), 4000000.0 * 0.45, 0.001);
    CHECK(species_keystone(SPECIES_TRANSCENDENCE) != 0);

    /* Omega Point：阶段 10 起可用，且需要 Recursive Self-Improvement */
    GameState game;
    reset_game(&game, SPECIES_SPACE_AGE); /* 阶段 9 */
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_OMEGA_POINT));
    game.run.active_species[0].species_id = SPECIES_SINGULARITY;
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_OMEGA_POINT));
    game.run.active_species[0].mutations[MUTATION_RECURSIVE_SELF_IMPROVEMENT] = true;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_OMEGA_POINT));

    /* 首个突变 6 折：拥有任意突变后恢复原价 */
    GameState first;
    reset_game(&first, SPECIES_PROTOCELL);
    double cost0 = evolution_mutation_cost(&first, MUTATION_PHOTOSYNTHESIS);
    first.run.active_species[0].mutations[MUTATION_CELL_MEMBRANE] = true;
    double cost1 = evolution_mutation_cost(&first, MUTATION_PHOTOSYNTHESIS);
    CHECK(cost0 < cost1);

    /* D2：即使生物量为 0，候选也强制包含下一阶段的必需突变 */
    GameState keyed;
    reset_game(&keyed, SPECIES_PROTOCELL);
    keyed.run.dna = 500.0;
    keyed.run.biomass = 0.0;
    keyed.run.max_biomass = 0.0;
    CHECK(evolution_open_mutation_offer(&keyed, 0) == ACTION_OK);
    CHECK(offer_contains(&keyed.run.mutation_offer, MUTATION_PHOTOSYNTHESIS) ||
          offer_contains(&keyed.run.mutation_offer, MUTATION_CHEMOSYNTHESIS));

    /* D1：候选里至少有一张买得起的牌（含 15 s DNA 收入） */
    int affordable_count = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
        GameState cheap;
        reset_game(&cheap, SPECIES_PROTOCELL);
        cheap.run.dna = 3.0;
        cheap.run.rng_state = 0xBEEF0001u + (unsigned int)attempt * 7919u;
        if (evolution_open_mutation_offer(&cheap, 0) != ACTION_OK) {
            continue;
        }
        double rate = evolution_calculate_species_rates(&cheap, 0).dna_per_second;
        double budget = cheap.run.dna + 15.0 * rate;
        bool affordable = false;
        for (int i = 0; i < cheap.run.mutation_offer.choice_count; ++i) {
            if (cheap.run.mutation_offer.dna_costs[i] <= budget) {
                affordable = true;
            }
        }
        CHECK(affordable);
        ++affordable_count;
        if (affordable_count >= 5) {
            break;
        }
    }
    CHECK(affordable_count > 0);

    /* 危机日志的最低价：事件结束后 step 携带真实最低人口 */
    GameState crisis;
    reset_game(&crisis, SPECIES_PROTOCELL);
    crisis.run.current_environment_id = ENV_GLOBAL_WARMING;
    crisis.run.crisis_start_population = 100.0;
    crisis.run.crisis_min_population = 100.0;
    crisis.run.active_species[0].population = 42.0;
    crisis.run.environment_time_left = 0.05;
    EnvironmentStep step = environment_calculate_step(&crisis, FIXED_STEP_SECONDS);
    CHECK(step.event_ended);
    CHECK_NEAR(step.crisis_lowest_population, 42.0, 0.001);
    CHECK(!step.crisis_survived); /* 42 < 100 × 0.70 */

    /* 按纪元设通过线：纪元 1 为 0.65，纪元 2 为 0.70 */
    GameState era1;
    reset_game(&era1, SPECIES_PROTOCELL);
    era1.run.era = 1;
    era1.run.current_environment_id = ENV_GLOBAL_WARMING;
    era1.run.crisis_start_population = 100.0;
    era1.run.crisis_min_population = 100.0;
    era1.run.active_species[0].population = 68.0;
    era1.run.environment_time_left = 0.05;
    EnvironmentStep step1 = environment_calculate_step(&era1, FIXED_STEP_SECONDS);
    CHECK(step1.crisis_survived); /* 68 >= 65 */

    GameState era2 = era1;
    era2.run.era = 2;
    era2.run.environment_time_left = 0.05;
    EnvironmentStep step2 = environment_calculate_step(&era2, FIXED_STEP_SECONDS);
    CHECK(!step2.crisis_survived); /* 68 < 70 */
}

static void test_round13_content_and_fuzz(void)
{
    /* 新增侧支的结构 */
    CHECK(species_phase(SPECIES_PHOTOSYNTHETIC_MAT) == 2);
    CHECK(species_phase(SPECIES_VENT_SWARM) == 2);
    CHECK(species_phase(SPECIES_CHITIN_REEF) == 3);
    CHECK(species_phase(SPECIES_SAPROTROPHIC_FOREST) == 5);
    CHECK(species_phase(SPECIES_HIVE_POLITY) == 6);
    CHECK(species_phase(SPECIES_ORBITAL_SWARM) == 9);
    CHECK(species_has_parent(SPECIES_SAPROTROPHIC_FOREST, SPECIES_FUNGAL_NETWORK));
    CHECK(species_has_parent(SPECIES_ORBITAL_SWARM, SPECIES_SPACE_AGE));

    int required[4];
    CHECK(species_required_mutations(SPECIES_HIVE_POLITY, required, 4) == 2);
    CHECK(required[0] == MUTATION_QUORUM_SENSING);
    CHECK(required[1] == MUTATION_LANGUAGE);
    CHECK(evolution_synergy_count() == 15);

    /* 理念加成突变（侧表实现，不扩 MutationDef） */
    CHECK_NEAR(evolution_mutation_knowledge_bonus(MUTATION_COLLECTIVE_MEMORY), 0.30, 1e-9);
    CHECK_NEAR(evolution_mutation_knowledge_bonus(MUTATION_DEEP_TIME_ARCHIVE), 0.20, 1e-9);
    CHECK_NEAR(evolution_mutation_knowledge_bonus(MUTATION_NUCLEUS), 0.0, 1e-9);

    GameState game;
    reset_game(&game, SPECIES_SAPIENT_LIFE);
    double base_knowledge = evolution_calculate_species_rates(&game, 0).knowledge_per_second;
    game.run.active_species[0].mutations[MUTATION_COLLECTIVE_MEMORY] = true;
    double boosted = evolution_calculate_species_rates(&game, 0).knowledge_per_second;
    CHECK_NEAR(boosted, base_knowledge * 1.30, 1e-6);

    /* 全组合：产率与成本必须有限且非负 */
    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        GameState probe;
        reset_game(&probe, species_id);
        SpeciesRates rates = evolution_calculate_species_rates(&probe, 0);
        CHECK(isfinite(rates.atp_per_second) && rates.atp_per_second >= 0.0);
        CHECK(isfinite(rates.biomass_per_second) && rates.biomass_per_second >= 0.0);
        CHECK(isfinite(rates.dna_per_second) && rates.dna_per_second >= 0.0);
        CHECK(isfinite(rates.knowledge_per_second) && rates.knowledge_per_second >= 0.0);
        CHECK(isfinite(rates.final_population_cap) && rates.final_population_cap >= 1.0);
        CHECK(isfinite(evolution_capacity_cost(&probe)));
        for (int mutation_id = 0; mutation_id < MUTATION_COUNT; ++mutation_id) {
            double cost = evolution_mutation_cost(&probe, mutation_id);
            CHECK(isfinite(cost) && cost >= 0.0);
        }
    }

    /* 模糊测试：随机动作下不变量必须成立 */
    for (int seed = 0; seed < 2; ++seed) {
        GameState fuzz;
        reset_game(&fuzz, SPECIES_PROTOCELL);
        environment_init_run(&fuzz);
        fuzz.run.rng_state = 0xA5A50000u + (unsigned int)seed * 7919u;
        unsigned int actions = 0x1357u + (unsigned int)seed * 104729u;
        for (int step = 0; step < 2500; ++step) {
            simulate_fixed_step(&fuzz, FIXED_STEP_SECONDS);
            unsigned int roll = tof_rng_next(&actions);
            if (roll % 6u == 0) {
                evolution_open_mutation_offer(&fuzz, 0);
                if (fuzz.run.mutation_offer.active) {
                    int pick = (int)(roll % (unsigned int)fuzz.run.mutation_offer.choice_count);
                    evolution_apply_mutation(&fuzz, 0,
                                             fuzz.run.mutation_offer.mutation_ids[pick]);
                }
            }
            if (roll % 9u == 0) {
                evolution_apply_capacity_upgrade(&fuzz);
            }
            if (roll % 13u == 0) {
                evolution_apply_reserve(&fuzz);
            }
            if (roll % 17u == 0) {
                evolution_apply_sub_upgrade(&fuzz, 0);
            }
            if (roll % 23u == 0) {
                int children[SPECIES_COUNT];
                int count = species_children(fuzz.run.active_species[0].species_id, children,
                                             SPECIES_COUNT);
                for (int c = 0; c < count; ++c) {
                    int target = children[((int)roll + c) % count];
                    if (evolution_validate_speciation(&fuzz, 0, target) == ACTION_OK) {
                        evolution_apply_speciation(&fuzz, 0, target, 0);
                        break;
                    }
                }
            }
            if (step % 5 != 0) {
                continue;
            }
            CHECK(isfinite(fuzz.run.atp) && fuzz.run.atp >= 0.0);
            CHECK(isfinite(fuzz.run.biomass) && fuzz.run.biomass >= 0.0);
            CHECK(isfinite(fuzz.run.dna) && fuzz.run.dna >= 0.0);
            CHECK(isfinite(fuzz.run.knowledge) && fuzz.run.knowledge >= 0.0);
            CHECK(fuzz.run.active_species_count >= 1);
            CHECK(fuzz.run.active_species_count <= MAX_ACTIVE_SPECIES);
            CHECK(fuzz.run.current_environment_id >= 0 &&
                  fuzz.run.current_environment_id < ENV_COUNT);
            for (int i = 0; i < fuzz.run.active_species_count; ++i) {
                const SpeciesState *species = &fuzz.run.active_species[i];
                CHECK(species->species_id >= 0 && species->species_id < SPECIES_COUNT);
                CHECK(isfinite(species->population) &&
                      species->population >= MIN_ACTIVE_POPULATION);
                CHECK(fuzz.run.tech_levels[species->species_id] >= 0 &&
                      fuzz.run.tech_levels[species->species_id] <= 3);
            }
            if (g_failures > 0) {
                return;
            }
        }
    }
}

/* 每个物种都能实际分化、每个突变都能实际买到：
 * 直接构造“前置条件齐备”的状态并提交，防止出现不可达的死内容。 */
static void test_every_content_playable(void)
{
    for (int species_id = 1; species_id < SPECIES_COUNT; ++species_id) {
        int parent = -1;
        for (int candidate = 0; candidate < SPECIES_COUNT; ++candidate) {
            if (species_has_parent(species_id, candidate)) {
                parent = candidate;
                break;
            }
        }
        CHECK(parent >= 0);
        if (parent < 0) {
            continue;
        }
        GameState game;
        reset_game(&game, parent);
        game.run.discovered_species[parent] = true;
        int required[4];
        int count = species_required_mutations(species_id, required, 4);
        for (int i = 0; i < count; ++i) {
            game.run.active_species[0].mutations[required[i]] = true;
            game.run.discovered_mutations[required[i]] = true;
        }
        int branch = -1;
        int level = 0;
        if (species_requires_legacy(species_id, &branch, &level) && branch >= 0) {
            game.legacy.levels[branch] = level;
        }
        game.run.biomass = evolution_speciation_cost(&game, species_id) + 1.0;
        CHECK(evolution_validate_speciation(&game, 0, species_id) == ACTION_OK);
        CHECK(evolution_apply_speciation(&game, 0, species_id, 0) == ACTION_OK);
        CHECK(game.run.active_species[0].species_id == species_id);
    }

    for (int mutation_id = 0; mutation_id < MUTATION_COUNT; ++mutation_id) {
        const MutationDef *def = mutation_get(mutation_id);
        CHECK(def != 0);
        if (!def) {
            continue;
        }
        int host = -1;
        for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
            int phase = species_phase(species_id);
            if (phase >= def->min_phase && (def->max_phase < 0 || phase <= def->max_phase)) {
                host = species_id;
                break;
            }
        }
        CHECK(host >= 0);
        if (host < 0) {
            continue;
        }
        GameState game;
        reset_game(&game, host);
        if (def->requires_mutation >= 0) {
            game.run.active_species[0].mutations[def->requires_mutation] = true;
        }
        if (def->requires_branch >= 0) {
            game.legacy.levels[def->requires_branch] = def->requires_level;
        }
        game.run.dna = evolution_mutation_cost(&game, mutation_id) + 1.0;
        CHECK(evolution_mutation_available(&game, 0, mutation_id));
        /* apply_mutation 只接受“当前候选”中的突变，这里构造一个单候选 */
        game.run.mutation_offer.active = true;
        game.run.mutation_offer.species_slot = 0;
        game.run.mutation_offer.choice_count = 1;
        game.run.mutation_offer.mutation_ids[0] = mutation_id;
        game.run.mutation_offer.dna_costs[0] = evolution_mutation_cost(&game, mutation_id);
        CHECK(evolution_apply_mutation(&game, 0, mutation_id) == ACTION_OK);
        CHECK(game.run.active_species[0].mutations[mutation_id]);
    }
}

static void test_codex_and_resilience(void)
{
    LegacyState legacy;
    memset(&legacy, 0, sizeof(legacy));
    CHECK_NEAR(legacy_atp_multiplier(&legacy), 1.0, 0.0001);
    CHECK_NEAR(legacy_biomass_multiplier(&legacy), 1.0, 0.0001);

    for (int id = 0; id < 3; ++id) {
        legacy.codex_mutations[id] = true;
    }
    CHECK_NEAR(legacy_atp_multiplier(&legacy), 1.01, 0.0001);
    CHECK_NEAR(legacy_dna_multiplier(&legacy), 1.01, 0.0001);
    for (int id = 3; id < 6; ++id) {
        legacy.codex_mutations[id] = true;
    }
    CHECK_NEAR(legacy_atp_multiplier(&legacy), 1.02, 0.0001);

    legacy.codex_species[SPECIES_CYANOBACTERIUM] = true;
    legacy.codex_species[SPECIES_CHEMOLITHOTROPH] = true;
    CHECK_NEAR(legacy_biomass_multiplier(&legacy), 1.02 + 0.01, 0.0001);
    CHECK_NEAR(legacy_atp_multiplier(&legacy), 1.02, 0.0001); /* 物种图鉴只加 Biomass */

    /* 生态韧性：每存活一种危机 +5%，最多 +15% */
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.survived_crises[ENV_GLOBAL_WARMING] = true;
    SpeciesRates rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.05, 0.0001);
    game.run.survived_crises[ENV_GLOBAL_COOLING] = true;
    game.run.survived_crises[ENV_LOW_LIGHT_PERIOD] = true;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.15, 0.0001);
    game.run.survived_crises[ENV_OXYGEN_RISE] = true;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.15, 0.0001); /* 封顶 */
}

int main(int argc, char **argv)
{
    static void (*const tests[])(void) = {
        test_rng,
        test_species_data,
        test_mutation_rules,
        test_mutation_offer,
        test_key_mutation_guarantee,
        test_speciation,
        test_rates,
        test_environment_multiplier,
        test_environment_steps,
        test_adaptation_shield,
        test_prestige,
        test_legacy,
        test_two_species_flow,
        test_synergy,
        test_keystone,
        test_knowledge_and_sub_upgrades,
        test_round10_economy,
        test_reachability,
        test_round12_stage11_and_crisis,
        test_round13_content_and_fuzz,
        test_every_content_playable,
        test_codex_and_resilience,
        test_long_run_simulation,
    };
    int group_count = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < group_count; ++i) {
        tests[i]();
    }

    bool json = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) {
            json = true;
        }
    }
    if (json) {
        printf("{\"groups\":%d,\"checks\":%d,\"failures\":%d,\"game_state_bytes\":%d}\n",
               group_count, g_checks, g_failures, (int)sizeof(GameState));
    } else {
        printf("\n%d 组、%d 项检查，%d 项失败\n", group_count, g_checks, g_failures);
    }
    return g_failures == 0 ? 0 : 1;
}
