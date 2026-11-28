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
#include "buffs.h"
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

/* v10：产率/价格的公共缩放因子——阶段压缩与 DNA 起手斜坡。
 * 测试断言统一从这里取，避免每次调平衡都要重抄一遍常数。 */
static double test_phase_compress(int phase)
{
    return pow(PHASE_RATE_COMPRESSION, (double)phase);
}

static double test_dna_ramp(const GameState *game)
{
    double t = game->run.elapsed_time;
    double r = t / DNA_RAMP_SECONDS;
    if (r > 1.0) {
        r = 1.0;
    }
    return DNA_RAMP_START + (1.0 - DNA_RAMP_START) * r;
}

/* 子升级价格：max(知识产率 × 秒数, 该物种的压缩地板)，向上取整 */
static double test_sub_upgrade_cost(GameState *game, const SubUpgradeDef *def)
{
    double atp = 0.0, biomass = 0.0, knowledge = 0.0;
    evolution_price_rates(game, &atp, &biomass, &knowledge);
    double floor_cost = def->base_knowledge_cost *
                        test_phase_compress(species_phase(def->species_id));
    double cost = knowledge * SUB_UPGRADE_KNOWLEDGE_SECONDS;
    if (cost < floor_cost) {
        cost = floor_cost;
    }
    return ceil(cost);
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
    CHECK(SPECIES_COUNT == 264);
    CHECK(MUTATION_COUNT == 282);
    CHECK(species_phase(SPECIES_PROTOCELL) == 0);
    CHECK(species_phase(SPECIES_MULTICELLULAR_COLONY) == 3);
    CHECK(species_phase(SPECIES_DIFFERENTIATED_TISSUE) == 4);
    CHECK(species_phase(SPECIES_COGNITIVE_BIOSPHERE) == 5);
    CHECK(species_ecology(SPECIES_CYANOBACTERIUM) == ECO_PHOTOSYNTHETIC);
    CHECK(species_base_trait(SPECIES_CYANOBACTERIUM, TRAIT_LOW_LIGHT) == -1);
    CHECK(species_base_trait(SPECIES_EXTREMOPHILE_COLONY, TRAIT_TOXIN) == 2);
    CHECK_NEAR(species_base_speciation_cost(SPECIES_ALGAL_EUKARYOTE), 350.0, 0.001);

    int children[SPECIES_COUNT];
    CHECK(species_children(SPECIES_PROTOCELL, children, SPECIES_COUNT) == 27);
    CHECK(species_children(SPECIES_PROTOZOAN, children, SPECIES_COUNT) == 6);
    CHECK(species_children(SPECIES_MULTICELLULAR_COLONY, children, SPECIES_COUNT) == 19);
    CHECK(species_children(SPECIES_DIFFERENTIATED_TISSUE, children, SPECIES_COUNT) == 15);
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

    /* v7 P0-3：首突变折扣回到 0.40（18 × 0.35 × 1.8 × 0.60 = 6.804 -> 7） */
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 7.0, 0.001);
    game.legacy.levels[LEGACY_MUTATION] = 1;
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 7.0, 0.001);
    game.legacy.levels[LEGACY_MUTATION] = 2;
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 6.0, 0.001);

    game.legacy.levels[LEGACY_MUTATION] = 0;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_CELL_MEMBRANE));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_PHOTOSYNTHESIS));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_MITOCHONDRIA));
    CHECK(!evolution_mutation_available(&game, 0, MUTATION_NUCLEUS));
    /* 第三轮：两个原 Lv.5 突变取消永久等级前置，单纪元内即可发现 */
    CHECK(evolution_mutation_available(&game, 0, MUTATION_EFFICIENT_RESPIRATION));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_ADAPTIVE_HYPERMUTATION));

    /* 第十轮：光合/化能取消阶段上限，阶段 2 之后可“补课”，价格 ×2。
     * stage 3、首突变 0.60：18 × 0.71 × 2 × 1.8 × 0.60 = 27.65 -> 28 */
    game.legacy.levels[LEGACY_MUTATION] = 0;
    game.run.active_species[0].species_id = SPECIES_MULTICELLULAR_COLONY;
    CHECK(evolution_mutation_available(&game, 0, MUTATION_PHOTOSYNTHESIS));
    CHECK(evolution_mutation_available(&game, 0, MUTATION_CHEMOSYNTHESIS));
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_PHOTOSYNTHESIS), 28.0, 0.001);
    /* 同阶段下不受补课加价的突变：22 × 0.71 × 1.8 × 0.60 = 16.87 -> 17 */
    CHECK_NEAR(evolution_mutation_cost(&game, MUTATION_CELL_WALL), 17.0, 0.001);
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

    /* 成功选择后扣费、记录图鉴并清空候选。
     * v16 F1：阶段 0 起的突变也收 ATP 门票，这里把门票补足再买。 */
    game.run.dna = cost + 5.0;
    game.run.atp += evolution_mutation_atp_cost(&game, chosen);
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

static void test_mutation_refresh_changes_offer(void)
{
    /* v25：刷新必须真的换一批候选。
     * 旧实现会命中 evolution_generate_mutation_offer_rng 的「同槽已有候选就原样返回」缓存，
     * 结果是 DNA 被扣、刷新次数 +1，候选却一模一样（实测连刷三次都是同一组 ID）。 */
    int sid = -1;
    for (int id = 0; id < SPECIES_COUNT; ++id) {
        if (species_phase(id) == 6) {
            sid = id;
            break;
        }
    }
    CHECK(sid >= 0);

    GameState game;
    reset_game(&game, sid);
    game.run.era = 3;
    game.run.active_species_count = 1;
    game.run.active_species[0].population = 200.0;
    game.run.dna = 1e9;
    game.run.atp = 1e9;

    CHECK(evolution_open_mutation_offer(&game, 0) == ACTION_OK);
    CHECK(game.run.mutation_offer.choice_count > 0);

    int changed = 0;
    for (int step = 0; step < MUTATION_REFRESH_LIMIT; ++step) {
        MutationOfferState before = game.run.mutation_offer;
        double dna_before = game.run.dna;
        int count_before = game.run.mutation_refresh_count;

        CHECK(evolution_refresh_mutation_offer(&game, 0) == ACTION_OK);
        CHECK(game.run.mutation_refresh_count == count_before + 1);
        CHECK(game.run.dna < dna_before);

        int same = game.run.mutation_offer.choice_count == before.choice_count;
        for (int i = 0; i < before.choice_count && same; ++i) {
            if (game.run.mutation_offer.mutation_ids[i] != before.mutation_ids[i]) {
                same = 0;
            }
        }
        if (!same) {
            ++changed;
        }
    }
    /* 阶段 6 的可用池远大于候选数，三次刷新至少换到一次 */
    CHECK(changed >= 1);

    /* 达到每批上限后拒绝，且不扣 DNA */
    double dna_now = game.run.dna;
    CHECK(evolution_refresh_mutation_offer(&game, 0) == ACTION_LEVEL_MAXED);
    CHECK(game.run.dna == dna_now);
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
    /* v10 P0-22：分化门票改时间本位（45 s 生物量产率），种群 100 时
     * 蓝细菌门票 = 45×1.15×2.5 ≈ 129 > 地板 40，把钱包抬到 300 保证买得起 */
    game.run.biomass = 300.0;

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
    double before = game.run.biomass;
    CHECK_NEAR(before, 300.0, 0.001);

    double ticket = evolution_speciation_cost(&game, SPECIES_CYANOBACTERIUM);
    CHECK(evolution_apply_speciation(&game, 0, SPECIES_CYANOBACTERIUM, 0) == ACTION_OK);
    CHECK(game.run.active_species_count == 1);
    CHECK(game.run.active_species[0].species_id == SPECIES_CYANOBACTERIUM);
    CHECK(game.run.active_species[0].is_main_lineage);
    CHECK_NEAR(game.run.active_species[0].population, 70.0, 0.001);
    CHECK_NEAR(game.run.biomass, before - ticket, 0.001);
    CHECK(game.run.discovered_species[SPECIES_CYANOBACTERIUM]);
    CHECK(game.legacy.codex_species[SPECIES_CYANOBACTERIUM]);
    CHECK(game.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS]); /* 继承亲本突变 */
    CHECK(!game.run.mutation_offer.active);

    /* 阶段 2：藻类真核生物需要 Nucleus；v10 门票按 16 × 1.06^phase 秒生物量计价 */
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) ==
          ACTION_REQUIREMENT_NOT_MET);
    game.run.active_species[0].mutations[MUTATION_NUCLEUS] = true;
    game.run.biomass = 1.0;
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) ==
          ACTION_NOT_ENOUGH_BIOMASS);
    double next_cost = evolution_speciation_cost(&game, SPECIES_ALGAL_EUKARYOTE);
    game.run.biomass = next_cost;
    CHECK(evolution_validate_speciation(&game, 0, SPECIES_ALGAL_EUKARYOTE) == ACTION_OK);

    /* 门票公式（与 evolution_speciation_cost 同构）：
     * max(N × Π增长 秒生物量, base × 地板系数 × 压缩^phase) × (1-discount) */
    double atp_r = 0.0, biomass_r = 0.0, knowledge_r = 0.0;
    evolution_price_rates(&game, &atp_r, &biomass_r, &knowledge_r);
    int phase = species_phase(SPECIES_ALGAL_EUKARYOTE);
    double floor_mult = phase <= 1 ? SPECIATION_EARLY_FLOOR_SCALE : SPECIATION_FLOOR_SCALE;
    double base_floor = species_base_speciation_cost(SPECIES_ALGAL_EUKARYOTE) * floor_mult *
                        pow(PHASE_RATE_COMPRESSION, (double)phase);
    double seconds = SPECIATION_TICKET_SECONDS;
    for (int p = 0; p < phase; ++p) {
        seconds *= p < 8 ? SPECIATION_TICKET_PHASE_GROWTH : SPECIATION_TICKET_LATE_GROWTH;
    }
    if (phase >= SPECIATION_HARD_WALL_PHASE) {
        seconds *= SPECIATION_HARD_WALL_MULT;
    }
    if (phase == 12) {
        seconds *= SPECIATION_PHASE12_MULT;
    }
    double expected_ticket = floor(fmax(seconds * biomass_r, base_floor));
    CHECK_NEAR(next_cost, expected_ticket, 1.0);

    /* Diversification Lv.4 降低分化成本 15% */
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 4;
    CHECK_NEAR(evolution_speciation_cost(&game, SPECIES_ALGAL_EUKARYOTE),
               floor(expected_ticket * 0.85), 1.0);
    game.legacy.levels[LEGACY_DIVERSIFICATION] = 0;

    /* 保留亲本：后代 60%，亲本 40%，且亲本不再是主谱系 */
    GameState keep;
    reset_game(&keep, SPECIES_PROTOCELL);
    keep.run.active_species[0].population = 100.0;
    /* v10：门票时间本位——蓝细菌 129 + 化能菌 45×1.15×(0.55×4)≈114，钱包抬到 400 */
    keep.run.biomass = 400.0;
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
    double extreme_cost = evolution_speciation_cost(&extreme, SPECIES_EXTREMOPHILE_COLONY);
    CHECK(extreme_cost > 0.0);
    CHECK(extreme_cost <= extreme.run.biomass);

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
    /* 第十轮：阶段 0–1 的 DNA 产出有早期加成；v10 全局 DNA 收入 ×2 且有起手斜坡 */
    CHECK_NEAR(rates.dna_per_second,
               0.08 * EARLY_DNA_BONUS * DNA_INCOME_SCALE * test_dna_ramp(&game), 0.0001);
    CHECK_NEAR(rates.final_population_cap, 80.0, 0.0001);
    CHECK_NEAR(rates.environment_multiplier, 1.0, 0.0001);
    CHECK(!rates.synergy_active);

    /* 单轮代谢升级保留线性 +25%，但不再有硬顶 */
    game.run.run_upgrade_levels[UPGRADE_METABOLISM] = 1;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.25, 0.0001);

    /* 永久代谢 Lv.1 ×1.08 */
    game.legacy.levels[LEGACY_METABOLISM] = 1;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 0.80 * 1.25 * 1.08, 0.0001);

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

    /* Meteor Impact：阶段 3 解锁，一次性损失被 v8 压到象征值 45% */
    GameState meteor;
    reset_game(&meteor, SPECIES_MULTICELLULAR_COLONY);
    meteor.run.active_species[0].population = 100.0;
    environment_init_run(&meteor);
    meteor.run.event_interval_left = 0.05;
    meteor.run.next_environment_id = ENV_METEOR_IMPACT;
    step = environment_calculate_step(&meteor, FIXED_STEP_SECONDS);
    CHECK(step.event_started);
    CHECK_NEAR(step.instant_population_loss_ratio, INSTANT_LOSS_MAX, 0.0001);
    environment_apply_step(&meteor, &step);
    CHECK_NEAR(meteor.run.active_species[0].population, 100.0 * (1.0 - INSTANT_LOSS_MAX), 0.0001);
    /* v7 P0-1：Meteor 的判定基准重置为“挨完即时损失之后” */
    CHECK_NEAR(meteor.run.crisis_start_population, 55.0, 0.0001);
    CHECK_NEAR(meteor.run.crisis_min_population, 55.0, 0.0001);
    meteor.run.environment_time_left = 0.05;
    step = environment_calculate_step(&meteor, FIXED_STEP_SECONDS);
    CHECK(step.event_ended);
    CHECK(step.crisis_survived);

    /* 应急储备 Lv.5：v14 E5 起乘算 0.90^5 = 0.59049，损失 45% × 0.59049 */
    GameState meteor_reserve;
    reset_game(&meteor_reserve, SPECIES_MULTICELLULAR_COLONY);
    meteor_reserve.run.active_species[0].population = 100.0;
    meteor_reserve.run.reserve_levels = 5;
    environment_init_run(&meteor_reserve);
    meteor_reserve.run.event_interval_left = 0.05;
    meteor_reserve.run.next_environment_id = ENV_METEOR_IMPACT;
    step = environment_calculate_step(&meteor_reserve, FIXED_STEP_SECONDS);
    CHECK_NEAR(step.instant_population_loss_ratio, INSTANT_LOSS_MAX * pow(RESERVE_REDUCTION_STEP, 5.0), 0.0001);

    /* v8 P0-15：储备减伤乘算；v14 E5 步进 0.90，第 18 级触底 0.20 */
    CHECK_NEAR(reserve_damage_scale(0), 1.0, 1e-9);
    CHECK_NEAR(reserve_damage_scale(5), pow(RESERVE_REDUCTION_STEP, 5.0), 1e-9);
    CHECK_NEAR(reserve_damage_scale(18), 0.20, 1e-9);
    CHECK_NEAR(reserve_damage_scale(80), 0.20, 1e-9);

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
    double damage = environment_damage_per_second(ENV_GLOBAL_WARMING);
    /* v9 P0-19：伤害的阶段放大从 CRISIS_EARLY_SCALE 起算（原始细胞阶段 0） */
    double stage_scale = CRISIS_EARLY_SCALE + CRISIS_DAMAGE_STAGE_SCALE * 0.0;
    double raw_loss = population * damage * (2.0 - environment) * stage_scale *
                      CRISIS_DAMAGE_MULTIPLIER;

    SpeciesRates shielded = evolution_calculate_species_rates(&game, 0);
    /* v10：生长项含 POP_GROWTH_SCALE，不再手写绝对值；改为校验“护盾抵消 80% 伤害” */
    GameState unshielded = game;
    unshielded.run.adaptation_shield_active = false;
    SpeciesRates plain = evolution_calculate_species_rates(&unshielded, 0);
    CHECK(plain.population_delta_per_second < shielded.population_delta_per_second);
    CHECK_NEAR(shielded.population_delta_per_second,
               plain.population_delta_per_second + raw_loss * 0.8, 1e-3);

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
    CHECK(reward.era_points == EP_PER_STAGE * 3);
    CHECK(reward.mutation_points == 5 / EP_MUTATIONS_PER_POINT);
    CHECK(reward.species_points == 3 / EP_SPECIES_PER_POINT);
    CHECK(reward.crisis_points == 2 * EP_POINTS_PER_CRISIS);
    CHECK(reward.biomass_points == 2);
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

    /* 第十五轮长线后的理论上限（阶段 10、全内容）：
     * 2*10 + 204/3 + 185/2 + 6 + 10 = 20 + 68 + 92 + 6 + 10 = 196 */
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
    /* v10：理论上限按当前权重推导，避免权重一改测试就红。
     * 突变 EP 有 EP_MUTATION_CAP 上限；危机按 EP_POINTS_PER_CRISIS 计。 */
    int expect_mut_pts = MUTATION_COUNT / EP_MUTATIONS_PER_POINT;
    if (expect_mut_pts > EP_MUTATION_CAP) {
        expect_mut_pts = EP_MUTATION_CAP;
    }
    int expect_species_pts = (SPECIES_COUNT - 1) / EP_SPECIES_PER_POINT;
    int expect_crisis_pts = EP_CRISIS_CAP * EP_POINTS_PER_CRISIS;

    CHECK(max_reward.era_points == EP_PER_STAGE * 10);
    CHECK(max_reward.mutation_points == expect_mut_pts);
    CHECK(max_reward.species_points == expect_species_pts);
    CHECK(max_reward.crisis_points == expect_crisis_pts);
    CHECK(max_reward.biomass_points == 10);
    CHECK(max_reward.total_points == max_reward.era_points + max_reward.mutation_points +
                                        max_reward.species_points + max_reward.crisis_points +
                                        max_reward.biomass_points);

    /* 阶段 11 的理论上限：22 + 68 + 92 + 6 + 10 = 198 */
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
    CHECK(max11.era_points == EP_PER_STAGE * 11);
    CHECK(max11.mutation_points == expect_mut_pts);
    CHECK(max11.species_points == expect_species_pts);
    CHECK(max11.crisis_points == expect_crisis_pts);
    CHECK(max11.biomass_points == 10);
    CHECK(max11.total_points == max11.era_points + max11.mutation_points + max11.species_points +
                                    max11.crisis_points + max11.biomass_points);

    /* 第十五轮阶段 13 的理论上限：26 + 68 + 92 + 6 + 10 = 202 */
    GameState full13;
    reset_game(&full13, SPECIES_COSMIC_WEAVE);
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        full13.run.discovered_mutations[id] = true;
    }
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        full13.run.discovered_species[id] = true;
    }
    for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id) {
        full13.run.survived_crises[id] = true;
    }
    full13.run.max_biomass = 10000000.0;
    EraReward max13 = prestige_preview_reward(&full13);
    CHECK(max13.era_points == EP_PER_STAGE * 13);
    CHECK(max13.total_points == max13.era_points + max13.mutation_points + max13.species_points +
                                    max13.crisis_points + max13.biomass_points);

    /* 危机最多计 EP_CRISIS_CAP 个，每类 EP_POINTS_PER_CRISIS 分 */
    game.run.survived_crises[ENV_LOW_LIGHT_PERIOD] = true;
    game.run.survived_crises[ENV_OXYGEN_RISE] = true;
    game.run.survived_crises[ENV_TOXIC_BLOOM] = true;
    reward = prestige_preview_reward(&game);
    CHECK(reward.crisis_points == 5 * EP_POINTS_PER_CRISIS);
    game.run.survived_crises[ENV_METEOR_IMPACT] = true;
    reward = prestige_preview_reward(&game);
    CHECK(reward.crisis_points == 6 * EP_POINTS_PER_CRISIS);

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
    CHECK_NEAR(main_rates.atp_per_second,
               1.30 * 2.0 * 1.10 * test_phase_compress(species_phase(SPECIES_CYANOBACTERIUM)),
               0.0001);
    CHECK_NEAR(branch_rates.atp_per_second,
               0.80 * 2.0 * 1.10 * test_phase_compress(species_phase(SPECIES_PROTOCELL)),
               0.0001);

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

    buffs_update(game, dt);

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
    /* 第十五轮：候选池扩到 204 后，模拟策略聚焦“阶段最高的子物种”，
     * 只买它的前置突变，避免把 DNA 花在旁支门票上。 */
    int children[SPECIES_COUNT];
    int child_count = species_children(game->run.active_species[main_slot].species_id, children,
                                       SPECIES_COUNT);
    int target = -1;
    int best_phase = -1;
    for (int i = 0; i < child_count; ++i) {
        int phase = species_phase(children[i]);
        if (phase > best_phase) {
            best_phase = phase;
            target = children[i];
        }
    }

    if (!game->run.mutation_offer.active) {
        evolution_open_mutation_offer(game, main_slot);
    }
    if (game->run.mutation_offer.active && target >= 0) {
        int required[4];
        int required_count = species_required_mutations(target, required, 4);
        int pick = -1;
        for (int i = 0; i < game->run.mutation_offer.choice_count && pick < 0; ++i) {
            int id = game->run.mutation_offer.mutation_ids[i];
            for (int r = 0; r < required_count; ++r) {
                const MutationDef *def = mutation_get(required[r]);
                if (id == required[r] ||
                    (def && def->requires_mutation == id)) {
                    if (game->run.dna >= game->run.mutation_offer.dna_costs[i] &&
                        game->run.atp >= evolution_mutation_atp_cost(game, id)) {
                        pick = i;
                    }
                    break;
                }
            }
        }
        if (pick >= 0) {
            evolution_apply_mutation(game, main_slot,
                                     game->run.mutation_offer.mutation_ids[pick]);
        }
    }

    if (target >= 0 && evolution_validate_speciation(game, main_slot, target) == ACTION_OK) {
        evolution_apply_speciation(game, main_slot, target, 0);
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
    int event_steps = 0;
    for (int i = 0; i < steps; ++i) {
        simulate_fixed_step(&game, dt);
        if (game.run.current_environment_id != ENV_NORMAL_OCEAN) {
            ++event_steps;
        }

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

    /* v9 P0-19：环境事件必须真实发生；但“不做任何适应也想通过危机”不再成立，
     * 因此这里只锁“事件发生过”和“通过类型数不超过本纪元的 4 类”。 */
    CHECK(event_steps > 0);
    int crises = 0;
    prestige_run_stats(&game, 0, 0, 0, &crises);
    CHECK(crises <= 4);

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
    CHECK(evolution_synergy_count() == 29);
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
    CHECK_NEAR(rates.population_delta_per_second,
               10.0 * 0.010 * POP_GROWTH_SCALE * 1.10 * (1.0 - 10.0 / 80.0), 0.0001);

    /* 藻类真核：ATP +8% */
    reset_game(&game, SPECIES_ALGAL_EUKARYOTE);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second,
               2.00 * 1.08 * test_phase_compress(species_phase(SPECIES_ALGAL_EUKARYOTE)), 0.0001);

    /* 原生捕食者：DNA +10% */
    reset_game(&game, SPECIES_PROTOZOAN);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.dna_per_second,
               0.20 * 1.10 * DNA_INCOME_SCALE * test_dna_ramp(&game), 0.0001);

    /* 多细胞群落：上限 +10%，增长 +5% */
    reset_game(&game, SPECIES_MULTICELLULAR_COLONY);
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.final_population_cap, 450.0 * 1.10, 0.0001);

    /* 蓝细菌低光 +15%，化能菌毒素 +15%（危机期劣势产率 ×0.4） */
    reset_game(&game, SPECIES_CYANOBACTERIUM);
    game.run.current_environment_id = ENV_LOW_LIGHT_PERIOD;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second,
               1.30 * 0.24 * 1.15 * test_phase_compress(species_phase(SPECIES_CYANOBACTERIUM)),
               0.0001);

    reset_game(&game, SPECIES_CHEMOLITHOTROPH);
    game.run.current_environment_id = ENV_TOXIC_BLOOM;
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second,
               1.10 * 0.34 * 1.15 * test_phase_compress(species_phase(SPECIES_CHEMOLITHOTROPH)),
               0.0001);
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
    CHECK_NEAR(rates.knowledge_per_second, 0.30 * test_phase_compress(6), 0.0001);
    CHECK_NEAR(evolution_knowledge_per_second(&game), 0.30 * test_phase_compress(6), 0.0001);

    game.run.active_species[0].population = 1000.0; /* 规模 100 -> sqrt = 10 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.knowledge_per_second, 3.0 * test_phase_compress(6), 0.0001);
    CHECK_NEAR(evolution_knowledge_per_second(&game), 3.0 * test_phase_compress(6), 0.0001);

    /* 子升级表：6 个高阶物种各 3 层。v10 P0-22 改时间本位——
     * 价格 = max(地板价, 50 s 知识产率)；本局知识产率 3.0/s → 150 < 地板价，
     * 全部落在各层 base_knowledge_cost 地板上。 */
    CHECK(evolution_sub_upgrade_count() == 30);
    const SubUpgradeDef *first = evolution_sub_upgrade_get(0);
    CHECK(first != 0);
    CHECK(first->species_id == SPECIES_SAPIENT_LIFE);
    CHECK(first->tier == 0);
    CHECK_NEAR(evolution_sub_upgrade_cost(&game, first), test_sub_upgrade_cost(&game, first),
               0.001);
    const SubUpgradeDef *second = evolution_sub_upgrade_get(1);
    CHECK_NEAR(evolution_sub_upgrade_cost(&game, second), test_sub_upgrade_cost(&game, second),
               0.001);
    const SubUpgradeDef *last = evolution_sub_upgrade_get(14);
    CHECK(last->species_id == SPECIES_SINGULARITY);
    CHECK(last->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(&game, last), test_sub_upgrade_cost(&game, last), 0.001);
    /* 第十二轮：阶段 11 追加 3 层，地板价 9600 */
    const SubUpgradeDef *omega = evolution_sub_upgrade_get(17);
    CHECK(omega->species_id == SPECIES_TRANSCENDENCE);
    CHECK(omega->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(&game, omega), test_sub_upgrade_cost(&game, omega),
               0.001);
    /* 第十三轮：两条侧支各 3 层，最后一条为 Stellar Logistics（地板价 2400） */
    const SubUpgradeDef *stellar = evolution_sub_upgrade_get(23);
    CHECK(stellar->species_id == SPECIES_ORBITAL_SWARM);
    CHECK(stellar->tier == 2);
    CHECK_NEAR(evolution_sub_upgrade_cost(&game, stellar), test_sub_upgrade_cost(&game, stellar),
               0.001);
    CHECK(evolution_sub_upgrade_get(30) == 0);

    /* 低阶物种没有子升级 */
    reset_game(&game, SPECIES_MULTICELLULAR_COLONY);
    CHECK(evolution_sub_upgrade_next(&game, 0) == 0);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_INVALID_TARGET);

    /* 理念不足 -> 拒绝且不扣资源 */
    reset_game(&game, SPECIES_SAPIENT_LIFE);
    double cost1 = test_sub_upgrade_cost(&game, first);
    game.run.knowledge = cost1 - 1.0;
    CHECK(evolution_sub_upgrade_next(&game, 0) == first);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_NOT_ENOUGH_KNOWLEDGE);
    CHECK_NEAR(game.run.knowledge, cost1 - 1.0, 0.001);
    CHECK(evolution_sub_upgrade_level(&game, 0) == 0);

    /* 购买后逐层推进，乘数生效 */
    game.run.knowledge = cost1;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK);
    CHECK_NEAR(game.run.knowledge, 0.0, 0.001);
    CHECK(evolution_sub_upgrade_level(&game, 0) == 1);
    CHECK(evolution_sub_upgrade_next(&game, 0) == second);
    CHECK_NEAR(evolution_sub_upgrade_multiplier(&game, 0), 1.35, 0.0001);

    /* 乘数同时作用于 ATP 与理念（智慧生命基石 ATP +15%）；阶段 6 需乘压缩因子 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.atp_per_second, 5.20 * 1.15 * 1.35 * test_phase_compress(6), 0.0001);
    CHECK_NEAR(rates.knowledge_per_second, 0.30 * 1.35 * test_phase_compress(6), 0.0001);

    /* 乘数同时作用于三种产率（DNA 不享受阶段 0–1 加成，阶段 6 无加成，但有起手斜坡） */
    CHECK_NEAR(rates.dna_per_second,
               0.70 * 1.35 * DNA_INCOME_SCALE * test_dna_ramp(&game), 0.0001);

    /* 买满 3 层后拒绝；总乘数 1.35^3 */
    game.run.knowledge = 20000.0;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK); /* tier 1，960 */
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_OK); /* tier 2，3072 */
    CHECK(evolution_sub_upgrade_level(&game, 0) == 3);
    CHECK(evolution_sub_upgrade_next(&game, 0) == 0);
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_LEVEL_MAXED);
    CHECK_NEAR(evolution_sub_upgrade_multiplier(&game, 0), 1.35 * 1.35 * 1.35, 0.0001);

    /* 非 RUNNING 阶段拒绝 */
    game.phase = PHASE_END_ERA_PREVIEW;
    CHECK(evolution_apply_sub_upgrade(&game, 0) == ACTION_INVALID_PHASE);
}

/* 第十六轮：随机增益因子（刷新 / 收集 / 过期 / 冷却）与洞见升级 */
static void test_buffs_and_insight(void)
{
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    CHECK(!buffs_factor_visible(&game));
    CHECK_NEAR(buffs_production_multiplier(&game), 1.0, 1e-9);
    CHECK_NEAR(buffs_click_multiplier(&game), 1.0, 1e-9);
    CHECK(game.run.buff_spawn_timer >= BUFF_SPAWN_MIN - 1e-9);
    CHECK(game.run.buff_spawn_timer <= BUFF_SPAWN_MAX + 1e-9);

    /* 确定性：相同种子得到相同的刷新序列 */
    GameState a;
    GameState b;
    reset_game(&a, SPECIES_PROTOCELL);
    reset_game(&b, SPECIES_PROTOCELL);
    a.run.rng_state = 0xABCDEF01u;
    b.run.rng_state = 0xABCDEF01u;
    buffs_init_run(&a);
    buffs_init_run(&b);
    for (int step = 0; step < 4000; ++step) {
        buffs_update(&a, 0.1);
        buffs_update(&b, 0.1);
    }
    CHECK(a.run.buff_visible == b.run.buff_visible);
    CHECK(a.run.buff_visible_type == b.run.buff_visible_type);
    CHECK(a.run.buff_visible_icon == b.run.buff_visible_icon);
    CHECK(a.run.buff_spawn_timer == b.run.buff_spawn_timer);
    CHECK(buffs_factor_visible(&a)); /* 45–90 s 内必然出现 */

    /* 普通产率增益：×1.2 / 30 s，到期回到 1.0 */
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_PRODUCTION;
    CHECK(buffs_collect(&game) == ACTION_OK);
    CHECK_NEAR(buffs_production_multiplier(&game), BUFF_PRODUCTION_MULT, 1e-9);
    CHECK_NEAR(buffs_click_multiplier(&game), 1.0, 1e-9);
    SpeciesRates boosted = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(boosted.atp_per_second, 0.80 * BUFF_PRODUCTION_MULT, 1e-4);
    buffs_update(&game, BUFF_PRODUCTION_DURATION - 0.1);
    CHECK_NEAR(buffs_production_multiplier(&game), BUFF_PRODUCTION_MULT, 1e-9);
    buffs_update(&game, 0.2);
    CHECK_NEAR(buffs_production_multiplier(&game), 1.0, 1e-9);
    CHECK_NEAR(game.run.buff_total_active_time, BUFF_PRODUCTION_DURATION, 1e-6);

    /* 点击增益：×2 / 30 s，不放大产率 */
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_CLICK;
    CHECK(buffs_collect(&game) == ACTION_OK);
    CHECK_NEAR(buffs_click_multiplier(&game), BUFF_CLICK_MULT, 1e-9);
    CHECK_NEAR(buffs_production_multiplier(&game), 1.0, 1e-9);

    /* 稀有增益：×1.5 / 15 s，并记录冷却起点 */
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    game.run.buff_clock = 200.0;
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_PRODUCTION_STRONG;
    CHECK(buffs_collect(&game) == ACTION_OK);
    CHECK_NEAR(buffs_production_multiplier(&game), BUFF_STRONG_MULT, 1e-9);
    CHECK_NEAR(game.run.buff_last_rare_time, 200.0, 1e-9);
    CHECK(game.run.buff_rare_collected == 1);
    buffs_update(&game, BUFF_STRONG_DURATION + 0.1);
    CHECK_NEAR(buffs_production_multiplier(&game), 1.0, 1e-9);

    /* 一次性资源包：按当前每秒收入折算，不留下限时增益 */
    reset_game(&game, SPECIES_SAPIENT_LIFE); /* 阶段 6，有理念产出 */
    buffs_init_run(&game);
    double atp_before = game.run.atp;
    double knowledge_before = game.run.knowledge;
    SpeciesRates current = evolution_calculate_species_rates(&game, 0);
    game.run.buff_visible = true;
    game.run.buff_visible_type = BUFF_INSTANT;
    CHECK(buffs_collect(&game) == ACTION_OK);
    CHECK_NEAR(game.run.atp - atp_before, current.atp_per_second * BUFF_INSTANT_ATP_SECONDS, 0.001);
    CHECK_NEAR(game.run.knowledge - knowledge_before,
               current.knowledge_per_second * BUFF_INSTANT_KNOWLEDGE_SECONDS, 0.001);
    CHECK_NEAR(buffs_production_multiplier(&game), 1.0, 1e-9);
    CHECK(game.run.buff_total_active_time == 0.0);

    /* 没有因子时点击无效 */
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    CHECK(buffs_collect(&game) == ACTION_INVALID_TARGET);

    /* 强增益冷却：开局 150 s 内不会刷出，冷却结束后可刷出 */
    reset_game(&game, SPECIES_PROTOCELL);
    buffs_init_run(&game);
    bool early_rare = false;
    for (int spawn = 0; spawn < 20; ++spawn) {
        game.run.buff_visible = false;
        game.run.buff_spawn_timer = 0.0;
        buffs_update(&game, 0.1);
        if (game.run.buff_visible_type == BUFF_PRODUCTION_STRONG) {
            early_rare = true;
        }
    }
    CHECK(!early_rare);
    CHECK(game.run.buff_clock < BUFF_RARE_COOLDOWN);

    buffs_update(&game, BUFF_RARE_COOLDOWN);
    bool late_rare = false;
    for (int spawn = 0; spawn < 240 && !late_rare; ++spawn) {
        game.run.buff_visible = false;
        game.run.buff_spawn_timer = 0.0;
        buffs_update(&game, 0.1);
        if (game.run.buff_visible_type == BUFF_PRODUCTION_STRONG) {
            late_rare = true;
        }
    }
    CHECK(late_rare);

    /* 洞见：价格指数增长，每级 +2% 产率，知识不足时拒绝。
     * v10：基价再乘阶段 6 的压缩因子（与子升级地板同一套缩放）。 */
    reset_game(&game, SPECIES_SAPIENT_LIFE);
    double insight0 = INSIGHT_BASE_COST * pow(PHASE_RATE_COMPRESSION, 6.0);
    CHECK_NEAR(evolution_insight_cost(&game), insight0, 1e-6);
    game.run.knowledge = insight0 - 1.0;
    CHECK(evolution_apply_insight(&game) == ACTION_NOT_ENOUGH_KNOWLEDGE);
    CHECK(game.run.insight_levels == 0);
    game.run.knowledge = insight0;
    CHECK(evolution_apply_insight(&game) == ACTION_OK);
    CHECK(game.run.insight_levels == 1);
    CHECK_NEAR(game.run.knowledge, 0.0, 1e-6);
    CHECK_NEAR(evolution_insight_cost(&game), insight0 * INSIGHT_COST_GROWTH, 1e-6);
    CHECK_NEAR(evolution_insight_multiplier(&game), 1.0 + INSIGHT_BONUS_PER_LEVEL, 1e-9);

    SpeciesRates with_insight = evolution_calculate_species_rates(&game, 0);
    game.run.insight_levels = 0;
    SpeciesRates plain = evolution_calculate_species_rates(&game, 0);
    game.run.insight_levels = 1;
    CHECK_NEAR(with_insight.atp_per_second / plain.atp_per_second, 1.02, 1e-9);
    game.run.insight_levels = 5;
    CHECK_NEAR(evolution_insight_multiplier(&game), pow(1.02, 5.0), 1e-9);

    /* 增益与洞见叠乘 */
    GameState combo;
    reset_game(&combo, SPECIES_PROTOCELL);
    buffs_init_run(&combo);
    combo.run.insight_levels = 2;
    combo.run.buff_visible = true;
    combo.run.buff_visible_type = BUFF_PRODUCTION;
    CHECK(buffs_collect(&combo) == ACTION_OK);
    SpeciesRates both = evolution_calculate_species_rates(&combo, 0);
    CHECK_NEAR(both.atp_per_second, 0.80 * BUFF_PRODUCTION_MULT * pow(1.02, 2.0), 1e-6);

    /* 非 RUNNING 阶段拒绝洞见 */
    game.phase = PHASE_END_ERA_PREVIEW;
    CHECK(evolution_apply_insight(&game) == ACTION_INVALID_PHASE);
}

static void test_round10_economy(void)
{
    /* v10 P0-22/P0-23：容量改时间本位双币（ATP 45 s + 生物量 45 s），20 级封顶 */
    GameState game;
    reset_game(&game, SPECIES_PROTOCELL);
    game.run.biomass = 1e12;
    game.run.atp = 1e12;
    game.run.capacity_levels = CAPACITY_MAX_LEVEL;
    CHECK(evolution_apply_capacity_upgrade(&game) == ACTION_LEVEL_MAXED);
    CHECK(game.run.capacity_levels == CAPACITY_MAX_LEVEL);

    /* 双币并行：ATP 与生物量各付自己的 45 s 产率份额 */
    GameState split;
    reset_game(&split, SPECIES_PROTOCELL);
    split.run.capacity_levels = 2;
    double atp_part = evolution_capacity_atp_cost(&split);
    double biomass_part = evolution_capacity_biomass_cost(&split);
    CHECK(atp_part > 0.0);
    CHECK(biomass_part > 0.0);
    CHECK_NEAR(evolution_capacity_cost(&split), atp_part + biomass_part, 0.001);

    /* ATP 不足时拒绝且不改等级与 Biomass（双币都要扣） */
    GameState poor;
    reset_game(&poor, SPECIES_PROTOCELL);
    poor.run.capacity_levels = 3;
    poor.run.biomass = 1e9;
    poor.run.atp = 0.0;
    CHECK(evolution_apply_capacity_upgrade(&poor) == ACTION_NOT_ENOUGH_ATP);
    CHECK_NEAR(poor.run.biomass, 1e9, 1.0);
    CHECK(poor.run.capacity_levels == 3);

    /* 时间本位：价格随产率缩放；v19 D4 起 ATP 半边再按 CAPACITY_PRICE_GROWTH^级 等比涨价，
     * 生物量半边不涨（两币的定价口径不同，这里只断言 ATP 半边）。 */
    reset_game(&game, SPECIES_PROTOCELL);
    double atp_cost0 = evolution_capacity_atp_cost(&game);
    game.run.capacity_levels = 5;
    double atp_cost5 = evolution_capacity_atp_cost(&game);
    CHECK(atp_cost0 > 0.0);
    CHECK_NEAR(atp_cost5, atp_cost0 * pow(CAPACITY_PRICE_GROWTH, 5.0), 2.0);

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
    CHECK_NEAR(rates.dna_per_second,
               0.10 * EARLY_DNA_BONUS * DNA_INCOME_SCALE * test_dna_ramp(&game), 0.0001);
    reset_game(&game, SPECIES_ALGAL_EUKARYOTE); /* 阶段 2 */
    rates = evolution_calculate_species_rates(&game, 0);
    CHECK_NEAR(rates.dna_per_second, 0.16 * DNA_INCOME_SCALE * test_dna_ramp(&game), 0.0001);
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
        cheap.run.dna = 15.0;
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
    era1.run.active_species[0].population = era1.run.crisis_start_population *
                                            CRISIS_PASS_RATIO_ERA1 + 3.0;
    era1.run.environment_time_left = 0.05;
    EnvironmentStep step1 = environment_calculate_step(&era1, FIXED_STEP_SECONDS);
    CHECK(step1.crisis_survived); /* 略高于纪元 1 通过线 */

    GameState era2 = era1;
    era2.run.era = 2;
    era2.run.active_species[0].population = era2.run.crisis_start_population *
                                            CRISIS_PASS_POPULATION_RATIO - 1.0;
    era2.run.crisis_min_population = era2.run.active_species[0].population;
    era2.run.environment_time_left = 0.05;
    EnvironmentStep step2 = environment_calculate_step(&era2, FIXED_STEP_SECONDS);
    CHECK(!step2.crisis_survived); /* 低于纪元 2 通过线 */

    /* v8 P0-17：阶段封顶 8 / 10 / 13，到顶后不能再跨阶段分化 */
    GameState cap;
    reset_game(&cap, SPECIES_CIVILIZATION);
    cap.run.active_species[0].mutations[MUTATION_ROCKETRY] = true;
    cap.run.biomass = 1.0e12;
    CHECK(evolution_era_stage_cap(&cap) == 8);
    CHECK(evolution_validate_speciation(&cap, 0, SPECIES_SPACE_AGE) ==
          ACTION_REQUIREMENT_NOT_MET);
    cap.run.era = 2;
    CHECK(evolution_era_stage_cap(&cap) == 10);
    CHECK(evolution_validate_speciation(&cap, 0, SPECIES_SPACE_AGE) == ACTION_OK);
    cap.run.era = 3;
    CHECK(evolution_era_stage_cap(&cap) == 13);
    CHECK(evolution_validate_speciation(&cap, 0, SPECIES_SPACE_AGE) == ACTION_OK);
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
    CHECK(evolution_synergy_count() == 29);

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
            if (roll % 11u == 0 && buffs_factor_visible(&fuzz)) {
                buffs_collect(&fuzz);
            }
            if (roll % 23u == 0) {
                int children[SPECIES_COUNT];
                int count = species_children(fuzz.run.active_species[0].species_id, children,
                                             SPECIES_COUNT);
                for (int c = 0; c < count; ++c) {
                    int target = children[(roll % (unsigned int)count + (unsigned int)c) % (unsigned int)count];
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
            CHECK(fuzz.run.buff_active_time_left >= 0.0);
            CHECK(fuzz.run.insight_levels >= 0);
            CHECK(fuzz.run.buff_visible_type >= 0 && fuzz.run.buff_visible_type < BUFF_TYPE_COUNT);
            CHECK(fuzz.run.buff_visible_icon >= 0 && fuzz.run.buff_visible_icon < BUFF_ICON_COUNT);
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

/* v9 P0-18：主线后继必须是显式数据，而且每一环都是真实的分化边。
 * 这里锁死“超越 → 无生命场 → 宇宙织体”，防止再退回按 ID 最小者猜主线。 */
static void test_main_lineage(void)
{
    CHECK(species_main_child(SPECIES_PROTOCELL) == SPECIES_CYANOBACTERIUM);
    CHECK(species_main_child(SPECIES_CYANOBACTERIUM) == SPECIES_ALGAL_EUKARYOTE);
    CHECK(species_main_child(SPECIES_ALGAL_EUKARYOTE) == SPECIES_MULTICELLULAR_COLONY);
    CHECK(species_main_child(SPECIES_MULTICELLULAR_COLONY) == SPECIES_DIFFERENTIATED_TISSUE);
    CHECK(species_main_child(SPECIES_DIFFERENTIATED_TISSUE) == SPECIES_COGNITIVE_BIOSPHERE);
    CHECK(species_main_child(SPECIES_COGNITIVE_BIOSPHERE) == SPECIES_SAPIENT_LIFE);
    CHECK(species_main_child(SPECIES_SAPIENT_LIFE) == SPECIES_TECHNOSPHERE);
    CHECK(species_main_child(SPECIES_TECHNOSPHERE) == SPECIES_CIVILIZATION);
    CHECK(species_main_child(SPECIES_CIVILIZATION) == SPECIES_SPACE_AGE);
    CHECK(species_main_child(SPECIES_SPACE_AGE) == SPECIES_SINGULARITY);
    CHECK(species_main_child(SPECIES_SINGULARITY) == SPECIES_TRANSCENDENCE);
    CHECK(species_main_child(SPECIES_TRANSCENDENCE) == SPECIES_NOETIC_FIELD);
    CHECK(species_main_child(SPECIES_NOETIC_FIELD) == SPECIES_COSMIC_WEAVE);
    CHECK(species_main_child(SPECIES_COSMIC_WEAVE) == -1);
    CHECK(species_main_child(SPECIES_PHOTOSYNTHETIC_MAT) == -1);

    /* 每一环都必须是合法的分化边，并且阶段严格递增 */
    int chain[16];
    int count = 0;
    for (int id = SPECIES_PROTOCELL; id >= 0 && count < 16; id = species_main_child(id)) {
        chain[count++] = id;
    }
    CHECK(count == 14);
    for (int i = 1; i < count; ++i) {
        CHECK(species_has_parent(chain[i], chain[i - 1]));
        CHECK(species_phase(chain[i]) > species_phase(chain[i - 1]));
    }
    /* 阶段 12 的门槛必须由保底深度覆盖，阶段 13 不被保底 */
    CHECK(species_phase(SPECIES_NOETIC_FIELD) <= MAINLINE_GUARANTEE_MAX_PHASE);
    CHECK(species_phase(SPECIES_COSMIC_WEAVE) > MAINLINE_GUARANTEE_MAX_PHASE);
}

/* 第十四轮：20 物种 / 20 突变 / 两个新阶段的结构与可达性 */
static void test_round14_content(void)
{
    CHECK(SPECIES_COUNT == 264);
    CHECK(MUTATION_COUNT == 282);
    CHECK(species_phase(SPECIES_THERMOPHILIC_ARCHAEA) == 2);
    CHECK(species_phase(SPECIES_DYSON_SWARM) == 8);
    CHECK(species_phase(SPECIES_NOETIC_FIELD) == 12);
    CHECK(species_phase(SPECIES_COSMIC_WEAVE) == 13);
    CHECK(species_has_parent(SPECIES_NOETIC_FIELD, SPECIES_TRANSCENDENCE));
    CHECK(species_has_parent(SPECIES_COSMIC_WEAVE, SPECIES_NOETIC_FIELD));
    CHECK(species_has_parent(SPECIES_NITROPLAST_CONSORTIUM, SPECIES_ALGAL_EUKARYOTE));
    CHECK(species_has_parent(SPECIES_INSECT_METROPOLIS, SPECIES_HIVE_POLITY));

    int required[4];
    CHECK(species_required_mutations(SPECIES_NOETIC_FIELD, required, 4) == 2);
    CHECK(required[0] == MUTATION_OMEGA_POINT);
    CHECK(required[1] == MUTATION_COSMIC_AWARENESS);
    CHECK(species_required_mutations(SPECIES_COSMIC_WEAVE, required, 4) == 2);
    CHECK(required[0] == MUTATION_COSMIC_AWARENESS);
    CHECK(required[1] == MUTATION_ENTROPY_REVERSAL);
    CHECK(evolution_synergy_count() == 29);
    CHECK(evolution_sub_upgrade_count() == 30);
    CHECK_NEAR(evolution_mutation_knowledge_bonus(MUTATION_COSMIC_AWARENESS), 0.30, 1e-9);

    /* 新物种的每个前置突变，必须至少有一个亲本在其阶段就能买到 */
    for (int species_id = SPECIES_THERMOPHILIC_ARCHAEA; species_id < SPECIES_COUNT; ++species_id) {
        int parent_phase = -1;
        for (int candidate = 0; candidate < SPECIES_COUNT; ++candidate) {
            if (species_has_parent(species_id, candidate)) {
                int phase = species_phase(candidate);
                if (phase > parent_phase) {
                    parent_phase = phase;
                }
            }
        }
        CHECK(parent_phase >= 0);
        if (parent_phase < 0) {
            continue;
        }
        int count = species_required_mutations(species_id, required, 4);
        for (int i = 0; i < count; ++i) {
            const MutationDef *def = mutation_get(required[i]);
            CHECK(def != 0);
            if (def) {
                CHECK(def->min_phase <= parent_phase);
            }
        }
    }

    /* 新阶段的分化成本必须高于前一阶段 */
    CHECK(species_base_speciation_cost(SPECIES_COSMIC_WEAVE) >
          species_base_speciation_cost(SPECIES_NOETIC_FIELD));
}

/* 第十五轮：14 个阶段各 10 物种 + 10 突变，并检查 ATP 出口 */
static void test_round15_content(void)
{
    CHECK(SPECIES_COUNT == 264);
    CHECK(MUTATION_COUNT == 282);

    int species_per_stage[14] = { 0 };
    for (int id = 46; id < SPECIES_COUNT; ++id) {
        int phase = species_phase(id);
        CHECK(phase >= 0 && phase < 14);
        if (phase >= 0 && phase < 14) {
            ++species_per_stage[phase];
        }
    }
    int mutation_per_stage[14] = { 0 };
    for (int id = 64; id < MUTATION_COUNT; ++id) {
        const MutationDef *def = mutation_get(id);
        CHECK(def != 0);
        if (def && def->min_phase >= 0 && def->min_phase < 14) {
            ++mutation_per_stage[def->min_phase];
        }
    }
    for (int stage = 0; stage < 14; ++stage) {
        CHECK(species_per_stage[stage] >= 10);
        CHECK(mutation_per_stage[stage] >= 10);
    }

    /* 突变票（N 秒产率；地板 3 秒产率）。v16 F1：MUTATION_ATP_MIN_PHASE 8 → 0，
     * 阶段 0 的突变同样收门票，不再有“低阶免费”的例外。 */
    GameState atp_game;
    reset_game(&atp_game, SPECIES_PROTOCELL);
    CHECK(evolution_mutation_atp_cost(&atp_game, MUTATION_CELL_MEMBRANE) > 0.0);
    double atp_low = evolution_mutation_atp_cost(&atp_game, MUTATION_ION_DRIVE);
    double atp_high = evolution_mutation_atp_cost(&atp_game, MUTATION_ENTROPY_REVERSAL);
    CHECK(atp_low > 0.0);
    CHECK(atp_high > 0.0);
    /* 公式核对：N 秒 × 定价产率，且不低于 3 秒产率的地板 */
    double pr_atp = 0.0, pr_biomass = 0.0, pr_knowledge = 0.0;
    evolution_price_rates(&atp_game, &pr_atp, &pr_biomass, &pr_knowledge);
    double expected_ticket = pr_atp * MUTATION_ATP_SECONDS;
    if (expected_ticket < pr_atp * 3.0) {
        expected_ticket = pr_atp * 3.0;
    }
    CHECK_NEAR(evolution_mutation_atp_cost(&atp_game, MUTATION_ION_DRIVE),
               ceil(expected_ticket), 1.0);

    /* 新物种的基石可读，且分化成本随阶段递增 */
    CHECK(species_keystone(SPECIES_ABSOLUTE_MIND) != 0);
    CHECK(species_base_speciation_cost(SPECIES_ABSOLUTE_MIND) >
          species_base_speciation_cost(SPECIES_LIPID_VESICLE));
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
        game.run.era = 3; /* v8 阶段封顶：让所有阶段都可构造 */
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
        game.run.dna = 1.0e12;
        game.run.atp = 1.0e15;
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
        test_mutation_refresh_changes_offer,
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
        test_buffs_and_insight,
        test_round10_economy,
        test_reachability,
        test_round12_stage11_and_crisis,
        test_round13_content_and_fuzz,
        test_main_lineage,
        test_round14_content,
        test_round15_content,
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

