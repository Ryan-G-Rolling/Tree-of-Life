/*
 * Member B 控制台演示
 *
 * 用最小化的 A 侧经济桩快进跑完一轮，把 B 的规则完整走一遍：
 * 点击与自动生产 -> 突变 -> 物种分化 -> 环境危机 -> EP 结算 -> 永久强化 -> 新轮生效。
 *
 * 这里的升级购买、资源写回只是演示用的 A 侧占位实现，不属于交付模块；
 * 正式游戏由 Member A 的 game.c / economy.c 负责。
 *
 * 用法：
 *   demo_b.exe          人类可读的叙述输出
 *   demo_b.exe --csv    ROW / EVENT 行，供报告脚本生成图表
 */

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "balance.h"
#include "buffs.h"
#include "environment.h"
#include "evolution.h"
#include "game_types.h"
#include "legacy.h"

#ifndef TOF_SCENARIO
#define TOF_SCENARIO base
#endif
#define TOF_STRINGIFY_(value) #value
#define TOF_STRINGIFY(value) TOF_STRINGIFY_(value)

static GameState g;
static double g_click_timer = 0.0;
static bool g_csv = false;
static bool g_quiet = false;
static bool g_sens = false;
static bool g_explore = false;
static unsigned int g_explore_rng = 0x5EED1234u;
static int g_era = 1;
static double g_time = 0.0;
/* v9 P2：本轮已花掉的 ATP，用于计算“ATP 未花比例” */
static double g_atp_spent = 0.0;
/* v9 P2：本轮 ATP 收入（生产 + 点击 + 瞬时增益），用来闭合台账：
 * 期初 + 收入 − 支出 应当等于期末，残差不为 0 就说明有出口没记进账。 */
static double g_atp_income = 0.0;
static double g_first_speciation = -1.0;
/* v9 P2：首次分化发生在纪元第一步时为 true。纪元 3 带着满级跨轮物资开局，
 * 第 0.1 s 就能买下光合 + 完成第一次分化，这个值贴住时钟分辨率下限，
 * 只能说明“即时兑现”，不能当成爬坡时间读。 */
static int g_first_speciation_instant = 0;
/* v9 P2：首次达到本纪元阶段封顶的耗时（-1 表示本纪元没到顶）。
 * 跨纪元可比，用来替代被跨轮物资压平的 first_speciation。 */
static double g_cap_reached_time = -1.0;
static int g_first_species = -1;
static double g_mutation_time[MUTATION_COUNT];
static int g_offer_slot = 0;
static double g_last_purchase_time = -1.0;
static int g_sub_upgrade_buys = 0;
static int g_insight_buys = 0;
static int g_batch_index = 0;
static double g_first_mutation = -1.0;
static int g_last_stage = -1;

static double main_population_value(void);
static int planned_target(int slot);

/* v7 P0-2/4：为下一步主线门票预留 ATP，避免商店把 ATP 花光后卡在阶段 8 */
static double target_atp_reserve(void)
{
    int slot = evolution_main_slot(&g);
    if (slot < 0) {
        return 0.0;
    }
    int target = planned_target(slot);
    if (target < 0) {
        return 0.0;
    }
    /* v8 P0-14：同阶段的横向支线不构成“跨阶段门票”，不为它锁死 ATP */
    if (species_phase(target) <= evolution_main_stage(&g)) {
        return 0.0;
    }
    int required[4];
    int count = species_required_mutations(target, required, 4);
    double reserve = 0.0;
    for (int i = 0; i < count; ++i) {
        if (g.run.active_species[slot].mutations[required[i]]) {
            continue;
        }
        double cost = evolution_mutation_atp_cost(&g, required[i]);
        if (cost > reserve) {
            reserve = cost;
        }
    }
    return reserve;
}

static void mark_purchase(void)
{
    g_last_purchase_time = g_time;
}

static void out(const char *fmt, ...)
{
    if (g_csv || g_quiet) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static void csv_event(const char *type, const char *detail)
{
    if (!g_csv) {
        return;
    }
    printf("EVENT,%d,%.1f,%s,%s\n", g_era, g_time, type, detail);
}

static void csv_row(void)
{
    if (!g_csv) {
        return;
    }
    int slot = evolution_main_slot(&g);
    const SpeciesState *species = &g.run.active_species[slot];
    SpeciesRates rates = evolution_calculate_species_rates(&g, slot);
    int discovered_mutations = 0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (g.run.discovered_mutations[id]) {
            ++discovered_mutations;
        }
    }
    int discovered_species = 0;
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        if (g.run.discovered_species[id]) {
            ++discovered_species;
        }
    }
    printf("ROW,%d,%.1f,%d,%s,%.2f,%.2f,%.3f,%.3f,%.3f,%.1f,%.1f,%s,%d,%d,%d,%d,%d,%.2f,%.3f\n",
           g_era,
           g_time, species_phase(species->species_id), species_name(species->species_id),
           species->population, rates.final_population_cap, rates.atp_per_second,
           rates.biomass_per_second, rates.dna_per_second, g.run.biomass, g.run.atp,
           environment_name(g.run.current_environment_id),
           g.run.run_upgrade_levels[UPGRADE_METABOLISM],
           g.run.run_upgrade_levels[UPGRADE_REPRODUCTION],
           g.run.run_upgrade_levels[UPGRADE_GENOME], discovered_mutations, discovered_species,
           g.run.knowledge, rates.knowledge_per_second);
}

/* 基础物种（无突变、无永久加成）在六种环境事件中的适应度倍率 */
static void csv_fitness_matrix(void)
{
    if (!g_csv) {
        return;
    }
    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        GameState probe;
        memset(&probe, 0, sizeof(probe));
        probe.run.active_species_count = 1;
        probe.run.active_species[0].species_id = species_id;
        probe.run.active_species[0].is_main_lineage = true;
        probe.run.active_species[0].population = STARTING_POPULATION;
        for (int env = 1; env < ENV_COUNT; ++env) {
            probe.run.current_environment_id = env;
            printf("MATRIX,%d,%d,%.3f\n", species_id, env,
                   environment_multiplier_for_slot(&probe, 0));
        }
    }
}

static void line(void)
{
    out("--------------------------------------------------------------\n");
}

/* 演示用的 A 侧占位定价：与 economy.c 同源于 balance.h 的同一组常量。
 * 正式游戏由 A 层 game.c / economy.c 负责，本 demo 不链接 A 层。 */
static double upgrade_cost(int type, int level)
{
    switch (type) {
    case UPGRADE_METABOLISM:
        return floor(RUN_UPGRADE_METABOLISM_BASE_COST *
                     pow(RUN_UPGRADE_METABOLISM_COST_GROWTH, level));
    case UPGRADE_REPRODUCTION:
        return floor(RUN_UPGRADE_REPRODUCTION_BASE_COST *
                     pow(RUN_UPGRADE_REPRODUCTION_COST_GROWTH, level));
    default:
        return floor(RUN_UPGRADE_GENOME_BASE_COST * pow(RUN_UPGRADE_GENOME_COST_GROWTH, level));
    }
}

static void print_status(double time)
{
    int slot = evolution_main_slot(&g);
    const SpeciesState *main_species = &g.run.active_species[slot];
    SpeciesRates rates = evolution_calculate_species_rates(&g, slot);
    out("t=%5.0fs | %-22s | ATP %9.1f (+%5.2f/s) | Bio %9.1f | DNA %6.1f | Knw %7.1f | Pop %5.1f/%-4.0f | %s\n",
        time, species_name(main_species->species_id), g.run.atp, rates.atp_per_second,
        g.run.biomass, g.run.dna, g.run.knowledge, main_species->population,
        rates.final_population_cap,
        environment_name(g.run.current_environment_id));
}

/* v9 P2：记账只认实际扣款。
 * 以前这里累加的是本地算出来的 cost，与真实扣款差一个 floor/取整通道，
 * 平衡表一改价台账就和钱包对不上；现在统一用 before - after 记账。 */
static void try_buy_upgrades(void)
{
    for (int type = 0; type < 3; ++type) {
        if (RUN_UPGRADE_MAX_LEVEL >= 0 &&
            g.run.run_upgrade_levels[type] >= RUN_UPGRADE_MAX_LEVEL) {
            continue;
        }
        double cost = upgrade_cost(type, g.run.run_upgrade_levels[type]);
        if (!isfinite(cost)) {
            continue;
        }
        if (type == UPGRADE_GENOME) {
            if (g.run.biomass >= cost) {
                g.run.biomass -= cost;
                ++g.run.run_upgrade_levels[type];
                mark_purchase();
            }
            continue;
        }
        if (g.run.atp - cost < target_atp_reserve()) {
            continue;
        }
        double atp_before = g.run.atp;
        g.run.atp -= cost;
        g_atp_spent += atp_before - g.run.atp;
        ++g.run.run_upgrade_levels[type];
        mark_purchase();
    }
}

/* 第三轮：显式分化计划。
 * v9 P0-18：主线后继改由规则层的 species_main_child 提供，演示与保底逻辑共用一张表。
 * 额外分支：原始细胞 → 化能 → 原生捕食者 → 真菌网络 → 共生超级个体 → 菌根网络。 */
static int main_chain_next(int species_id)
{
    return species_main_child(species_id);
}

static int extra_chain_next(int species_id)
{
    switch (species_id) {
    case SPECIES_PROTOCELL:
        return SPECIES_CHEMOLITHOTROPH;
    case SPECIES_CHEMOLITHOTROPH:
        return SPECIES_PROTOZOAN;
    case SPECIES_PROTOZOAN:
        return SPECIES_FUNGAL_NETWORK;
    case SPECIES_FUNGAL_NETWORK:
        return SPECIES_SYMBIOTIC_SUPERORGANISM;
    case SPECIES_SYMBIOTIC_SUPERORGANISM:
        return SPECIES_MYCORRHIZAL_WEB;
    case SPECIES_ALGAL_EUKARYOTE:
        return SPECIES_CORAL_SYMBIONT;
    case SPECIES_CORAL_SYMBIONT:
        return SPECIES_REEF_METROPOLIS;
    default:
        return -1;
    }
}

/* v8 P1-9：阶段封顶后，横向选择本阶段内可达、优先未发现的子物种。
 * 三个槽位各自沿一条支线推进，形成“同阶段并行支线”。
 * v9 P1-11：优先挑“突变已齐、当场就能分化”的子物种；原先只看阶段高低，
 * 一旦选中一个门票突变没凑齐的侧支就永久卡住（阶段 10 的横向落点因此全是 0）。 */
static bool slot_owns_required(int slot, int target)
{
    const SpeciesState *species = &g.run.active_species[slot];
    int required[4];
    int count = species_required_mutations(target, required, 4);
    for (int i = 0; i < count; ++i) {
        if (!species->mutations[required[i]]) {
            return false;
        }
    }
    return true;
}

static int best_child_under_cap(int slot, int cap)
{
    const SpeciesState *species = &g.run.active_species[slot];
    int children[SPECIES_COUNT];
    int count = species_children(species->species_id, children, SPECIES_COUNT);
    int best = -1;
    int best_rank = -1;
    int best_phase = -1;
    for (int i = 0; i < count; ++i) {
        int id = children[i];
        int phase = species_phase(id);
        if (phase > cap) {
            continue;
        }
        bool undiscovered = !g.run.discovered_species[id];
        bool ready = slot_owns_required(slot, id);
        int rank = (ready ? 2 : 0) + (undiscovered ? 1 : 0);
        if (rank > best_rank || (rank == best_rank && phase > best_phase)) {
            best = id;
            best_rank = rank;
            best_phase = phase;
        }
    }
    return best;
}

static int planned_target(int slot)
{
    if (slot < 0 || slot >= g.run.active_species_count) {
        return -1;
    }
    const SpeciesState *species = &g.run.active_species[slot];
    int cap = evolution_era_stage_cap(&g);
    int chain = species->is_main_lineage ? main_chain_next(species->species_id)
                                         : extra_chain_next(species->species_id);
    if (chain >= 0 && species_phase(chain) <= cap) {
        return chain;
    }
    return best_child_under_cap(slot, cap);
}

/* 自动策略优先购买“该槽下一物种分化所需”的突变 */
static bool mutation_unlocks_slot(int slot, int mutation_id)
{
    int target = planned_target(slot);
    if (target < 0) {
        return false;
    }
    int required[4];
    int count = species_required_mutations(target, required, 4);
    for (int i = 0; i < count; ++i) {
        if (required[i] == mutation_id) {
            return true;
        }
        /* 再向前看一层前置，例如 Decomposer 需要先有多细胞性 */
        const MutationDef *def = mutation_get(required[i]);
        if (def && def->requires_mutation == mutation_id) {
            return true;
        }
    }
    return false;
}

static void choose_mutation(int slot)
{
    MutationOfferState *offer = &g.run.mutation_offer;
    if (!offer->active || offer->species_slot != slot) {
        return;
    }
    int pick = -1;
    /* 第十五轮：只要主线目标还缺前置，就只买前置，不拿旁支消耗 DNA。 */
    bool target_missing = false;
    int target = planned_target(slot);
    if (target >= 0) {
        int required[4];
        int count = species_required_mutations(target, required, 4);
        for (int i = 0; i < count; ++i) {
            if (!g.run.active_species[slot].mutations[required[i]]) {
                target_missing = true;
                break;
            }
        }
    }
    if (g_explore) {
        /* 图鉴策略：优先买尚未发现的，其次在买得起的候选里随机选 */
        int choices[MAX_MUTATION_CHOICES];
        int count = 0;
        for (int i = 0; i < offer->choice_count; ++i) {
            if (g.run.dna >= offer->dna_costs[i] &&
                !g.run.discovered_mutations[offer->mutation_ids[i]]) {
                choices[count++] = i;
            }
        }
        if (count == 0) {
            for (int i = 0; i < offer->choice_count; ++i) {
                if (g.run.dna >= offer->dna_costs[i]) {
                    choices[count++] = i;
                }
            }
        }
        if (count == 0) {
            return;
        }
        pick = choices[tof_rng_next(&g_explore_rng) % (unsigned int)count];
    }
    bool target_offered = false;
    if (!g_explore) {
        for (int i = 0; i < offer->choice_count; ++i) {
            if (mutation_unlocks_slot(slot, offer->mutation_ids[i])) {
                target_offered = true;
                int mid = offer->mutation_ids[i];
                if (g.run.dna >= offer->dna_costs[i] &&
                    g.run.atp >= evolution_mutation_atp_cost(&g, mid)) {
                    pick = i;
                    break;
                }
            }
        }
        if (pick < 0) {
            if (target_missing) {
                return;
            }
            /* 第十五轮：主线前置齐备、只差 Biomass 时，才用富余 DNA 买属性突变。 */
            (void)target_offered;
            int fallback = -1;
            for (int i = 0; i < offer->choice_count; ++i) {
                int mid = offer->mutation_ids[i];
                if (evolution_mutation_is_required(mid) ||
                    g.run.discovered_mutations[mid]) {
                    continue;
                }
                if (g.run.dna < offer->dna_costs[i] ||
                    g.run.atp < evolution_mutation_atp_cost(&g, mid)) {
                    continue;
                }
                if (fallback < 0 ||
                    offer->dna_costs[i] < offer->dna_costs[fallback]) {
                    fallback = i;
                }
            }
            if (fallback < 0) {
                /* 没有纯属性突变时，退而购买任意未发现且买得起的突变，
                 * 维持自动策略的生存力；真实玩家可自行取舍。 */
                for (int i = 0; i < offer->choice_count; ++i) {
                    int mid = offer->mutation_ids[i];
                    if (g.run.discovered_mutations[mid]) {
                        continue;
                    }
                    if (g.run.dna < offer->dna_costs[i] ||
                        g.run.atp < evolution_mutation_atp_cost(&g, mid)) {
                        continue;
                    }
                    if (fallback < 0 ||
                        offer->dna_costs[i] < offer->dna_costs[fallback]) {
                        fallback = i;
                    }
                }
            }
            if (fallback < 0) {
                return;
            }
            pick = fallback;
        }
    }
    if (pick < 0) {
        int cheapest = 0;
        for (int i = 1; i < offer->choice_count; ++i) {
            if (offer->dna_costs[i] < offer->dna_costs[cheapest]) {
                cheapest = i;
            }
        }
        if (g.run.dna < offer->dna_costs[cheapest]) {
            return;
        }
        pick = cheapest;
    }
    int mutation_id = offer->mutation_ids[pick];
    double cost = offer->dna_costs[pick];
    double atp_before = g.run.atp;
    if (evolution_apply_mutation(&g, slot, mutation_id) == ACTION_OK) {
        g_atp_spent += atp_before - g.run.atp; /* P2：用实际扣款记账，避免改价静默漂移 */
        mark_purchase(); /* 第十轮：突变计入统一“末次有效购买”口径 */
        if (g_first_mutation < 0.0) {
            g_first_mutation = g_time;
        }
        const MutationDef *mutation = mutation_get(mutation_id);
        const char *name = mutation ? mutation->name : "Unknown";
        g_mutation_time[mutation_id] = g_time;
        char detail[128];
        snprintf(detail, sizeof(detail), "%s;%.0f", name, cost);
        csv_event("mutation", detail);
        out("  >> Mutation acquired: %-24s (-%.0f DNA)\n", name, cost);
    }
}

static void try_mutations(void)
{
    if (g.run.mutation_offer.active) {
        choose_mutation(g.run.mutation_offer.species_slot);
        return;
    }
    /* 第十五轮：普通策略只为主线开候选，避免额外分支的待选提案
     * 长期占用唯一候选位，把主线前置卡死。
     * v8 P1-9：主线到本纪元封顶后，轮转所有槽位，让三条并行支线都能推进。 */
    if (!g_explore) {
        int main_slot = evolution_main_slot(&g);
        bool capped = evolution_main_stage(&g) >= evolution_era_stage_cap(&g);
        if (!capped) {
            if (main_slot >= 0 && evolution_open_mutation_offer(&g, main_slot) == ACTION_OK) {
                choose_mutation(main_slot);
            }
            return;
        }
        for (int i = 0; i < g.run.active_species_count; ++i) {
            int slot = (g_offer_slot + i) % g.run.active_species_count;
            if (evolution_open_mutation_offer(&g, slot) == ACTION_OK) {
                g_offer_slot = slot + 1;
                choose_mutation(slot);
                return;
            }
        }
        g_offer_slot = 0;
        return;
    }
    /* 轮转候选槽位，让主谱系与额外分支都能拿到自己的前置突变 */
    for (int i = 0; i < g.run.active_species_count; ++i) {
        int slot = (g_offer_slot + i) % g.run.active_species_count;
        if (evolution_open_mutation_offer(&g, slot) == ACTION_OK) {
            g_offer_slot = slot + 1;
            choose_mutation(slot);
            return;
        }
    }
    g_offer_slot = 0;
}

static void try_speciation_slot(int slot)
{
    int target = planned_target(slot);
    if (g_explore) {
        /* 图鉴策略：分化仍随机（优先新物种会让谱系卡在低阶段，
         * 反而到不了高阶内容）；突变购买才优先未发现项。 */
        int children[SPECIES_COUNT];
        int child_count = species_children(g.run.active_species[slot].species_id, children,
                                           SPECIES_COUNT);
        int valid[SPECIES_COUNT];
        int valid_count = 0;
        for (int i = 0; i < child_count; ++i) {
            if (evolution_validate_speciation(&g, slot, children[i]) == ACTION_OK) {
                valid[valid_count++] = children[i];
            }
        }
        if (valid_count == 0) {
            return;
        }
        target = valid[tof_rng_next(&g_explore_rng) % (unsigned int)valid_count];
    }
    if (target < 0) {
        return;
    }
    if (evolution_validate_speciation(&g, slot, target) != ACTION_OK) {
        return;
    }
    const SpeciesState *source = &g.run.active_species[slot];
    const char *parent_name = species_name(source->species_id);
    const char *child_name = species_name(target);
    int option = 0;
    /* 保留“分支种子”（原始细胞、藻类）作为额外路线起点，仅在还有空槽时 */
    if (source->is_main_lineage &&
        evolution_max_speciation_slots(&g) > g.run.active_species_count &&
        (source->species_id == SPECIES_PROTOCELL ||
         source->species_id == SPECIES_ALGAL_EUKARYOTE)) {
        option = 1;
    }
    double cost = evolution_speciation_cost(&g, target);
    double old_population = source->population;
    if (evolution_apply_speciation(&g, slot, target, option) == ACTION_OK) {
        mark_purchase();
        char detail[160];
        snprintf(detail, sizeof(detail), "%s>%s;%.0f;%.0f;%.0f", parent_name, child_name, cost,
                 old_population, g.run.active_species[slot].population);
        csv_event("speciation", detail);
        if (g_first_speciation < 0.0) {
            g_first_speciation = g_time;
            g_first_species = target;
            g_first_speciation_instant = g_time <= FIXED_STEP_SECONDS + 1e-9;
        }
        out("  >> Speciation: %s -> %s  (Biomass -%.0f, population %.0f -> %.0f%s)\n",
            parent_name, child_name, cost, old_population, g.run.active_species[slot].population,
            option == 1 ? ", parent kept" : "");
    }
}

static void try_speciation(void)
{
    for (int slot = 0; slot < g.run.active_species_count; ++slot) {
        try_speciation_slot(slot);
    }
}

static void try_sinks(void)
{
    /* v8 P0-15：先买储备（几何价 + 乘算减伤），到达 0.20 下限后不再买无收益的等级 */
    while ((RESERVE_MAX_LEVEL < 0 || g.run.reserve_levels < RESERVE_MAX_LEVEL)) {
        if (reserve_damage_scale(g.run.reserve_levels) <= RESERVE_REDUCTION_FLOOR + 1e-12) {
            break;
        }
        double cost = evolution_reserve_cost(&g);
        if (g.run.atp < cost * 1.25 + target_atp_reserve()) {
            break;
        }
        double atp_before = g.run.atp;
        if (evolution_apply_reserve(&g) != ACTION_OK) {
            break;
        }
        g_atp_spent += atp_before - g.run.atp;
        mark_purchase();
    }
    /* v8 P0-14：容量最后买，用来吸收储备留下的余额；
     * 逐币判断，任一货币可支付即可，两种都为 0 才是真的不可购买 */
    while (CAPACITY_MAX_LEVEL < 0 || g.run.capacity_levels < CAPACITY_MAX_LEVEL) {
        double biomass_cost = evolution_capacity_biomass_cost(&g);
        double atp_cost = evolution_capacity_atp_cost(&g);
        double reserve = target_atp_reserve();
        if (atp_cost <= 0.0 && biomass_cost <= 0.0) {
            break;
        }
        if (atp_cost > 0.0 && g.run.atp < atp_cost * 1.25 + reserve) {
            break;
        }
        if (biomass_cost > 0.0 && g.run.biomass < biomass_cost * 1.25) {
            break;
        }
        double atp_before = g.run.atp;
        if (evolution_apply_capacity_upgrade(&g) != ACTION_OK) {
            break;
        }
        g_atp_spent += atp_before - g.run.atp;
        mark_purchase();
    }
}

/* 第九轮：理念只用于子升级，攒够就买（主谱系优先，再轮到额外分支） */
static void try_sub_upgrades(void)
{
    for (int pass = 0; pass < g.run.active_species_count; ++pass) {
        int slot = evolution_main_slot(&g);
        if (pass > 0) {
            slot = -1;
            for (int i = 0; i < g.run.active_species_count; ++i) {
                if (!g.run.active_species[i].is_main_lineage) {
                    slot = i;
                    break;
                }
            }
        }
        if (slot < 0) {
            continue;
        }
        const SubUpgradeDef *next = evolution_sub_upgrade_next(&g, slot);
        while (next) {
            double cost = evolution_sub_upgrade_cost(next);
            if (g.run.knowledge < cost) {
                break;
            }
            if (evolution_apply_sub_upgrade(&g, slot) != ACTION_OK) {
                break;
            }
            mark_purchase();
            ++g_sub_upgrade_buys;
            if (g_quiet) {
                printf("BSUB,%d,%d,%.1f,%s,%.0f,%.0f\n", g_batch_index, g.run.era, g_time,
                       next->name, cost, g.run.knowledge);
            }
            char detail[160];
            snprintf(detail, sizeof(detail), "%s>%s;%.0f",
                     species_name(next->species_id), next->name, cost);
            csv_event("sub_upgrade", detail);
            out("  >> Sub-upgrade: %s (%s x%.1f production, -%.0f Knowledge)\n",
                next->name, species_name(next->species_id), next->multiplier, cost);
            next = evolution_sub_upgrade_next(&g, slot);
        }
    }
}

/* 第十六轮：知识富余时买洞见（先保证每个物种的下一层子升级买得起） */
static void try_insight(void)
{
    double reserve = 0.0;
    for (int i = 0; i < g.run.active_species_count; ++i) {
        const SubUpgradeDef *next = evolution_sub_upgrade_next(&g, i);
        if (next) {
            double cost = evolution_sub_upgrade_cost(next);
            if (cost > reserve) {
                reserve = cost;
            }
        }
    }
    while (g.run.knowledge >= evolution_insight_cost(&g) + reserve) {
        double cost = evolution_insight_cost(&g);
        if (evolution_apply_insight(&g) != ACTION_OK) {
            break;
        }
        mark_purchase();
        ++g_insight_buys;
        if (g_quiet) {
            printf("BINSIGHT,%d,%d,%.1f,%.0f,%.0f,%d\n", g_batch_index, g.run.era, g_time, cost,
                   g.run.knowledge, g.run.insight_levels);
        }
        char detail[96];
        snprintf(detail, sizeof(detail), "insight;%.0f;%d", cost, g.run.insight_levels);
        csv_event("insight", detail);
        out("  >> Insight Lv.%d (+2%% production, -%.0f Knowledge)\n", g.run.insight_levels, cost);
    }
}

static EnvironmentStep simulate_step(double dt)
{
    SpeciesRates rates[MAX_ACTIVE_SPECIES];
    double populations[MAX_ACTIVE_SPECIES];
    double atp = 0.0;
    double biomass = 0.0;
    double dna = 0.0;
    double knowledge = 0.0;

    /* 第十六轮：先推进增益倒计时；演示策略 100% 点击出现的因子 */
    buffs_update(&g, dt);
    double atp_before_buff = g.run.atp;
    if (buffs_factor_visible(&g)) {
        int buff_type = g.run.buff_visible_type;
        int buff_icon = g.run.buff_visible_icon;
        if (buffs_collect(&g) == ACTION_OK) {
            char detail[128];
            snprintf(detail, sizeof(detail), "%s;%s;%.0f", buffs_type_name(buff_type),
                     buffs_icon_name(buff_icon), g_time);
            csv_event("buff", detail);
            out("  ~~ Factor collected: %s (%s)\n", buffs_type_name(buff_type),
                buffs_icon_name(buff_icon));
        }
    }
    g_atp_income += g.run.atp - atp_before_buff;

    for (int i = 0; i < g.run.active_species_count; ++i) {
        rates[i] = evolution_calculate_species_rates(&g, i);
        atp += rates[i].atp_per_second;
        biomass += rates[i].biomass_per_second;
        dna += rates[i].dna_per_second;
        knowledge += rates[i].knowledge_per_second;
        double population = g.run.active_species[i].population +
                            rates[i].population_delta_per_second * dt;
        if (population < MIN_ACTIVE_POPULATION) {
            population = MIN_ACTIVE_POPULATION;
        }
        if (population > rates[i].final_population_cap) {
            population = rates[i].final_population_cap;
        }
        populations[i] = population;
    }
    for (int i = 0; i < g.run.active_species_count; ++i) {
        g.run.active_species[i].population = populations[i];
    }
    g.run.atp += atp * dt;
    g_atp_income += atp * dt;
    g.run.biomass += biomass * dt;
    g.run.dna += dna * dt;
    g.run.knowledge += knowledge * dt;
    if (g.run.biomass > g.run.max_biomass) {
        g.run.max_biomass = g.run.biomass;
    }

    g_click_timer += dt;
    while (g_click_timer >= 1.0) {
        g_click_timer -= 1.0;
        double click = BASE_CLICK_ATP *
                       (1.0 + RUN_METABOLISM_BONUS_PER_LEVEL *
                                  g.run.run_upgrade_levels[UPGRADE_METABOLISM]) *
                       legacy_atp_multiplier(&g.legacy) * legacy_click_multiplier(&g.legacy) *
                       buffs_click_multiplier(&g);
        g.run.atp += click;
        g_atp_income += click;
    }

    try_buy_upgrades();
    try_mutations();
    try_speciation();
    try_sinks();
    try_sub_upgrades();
    try_insight();

    EnvironmentStep step = environment_calculate_step(&g, dt);
    environment_apply_step(&g, &step);

    /* 机器模式：危机结束时输出逐次记录，供覆盖率与通过率统计 */
    if ((g_csv || g_quiet) && step.event_ended) {
        /* v7 P2：打印真实通过线（纪元 1 是 0.625，纪元 2+ 是 0.70） */
        double pass_ratio = g_era <= 1
                                ? CRISIS_PASS_RATIO_ERA1
                                : (g_era == 2 ? CRISIS_PASS_POPULATION_RATIO
                                              : CRISIS_PASS_RATIO_ERA3);
        double threshold = fmax(MIN_ACTIVE_POPULATION,
                                g.run.crisis_start_population * pass_ratio);
        double end_population = main_population_value();
        double lowest = step.crisis_lowest_population > 0.0 ? step.crisis_lowest_population
                                                            : g.run.crisis_start_population;
        printf("CRISIS,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n", g_era,
               step.ended_environment_id, g.run.crisis_start_population, lowest, end_population,
               threshold, threshold > 0.0 ? lowest / threshold : 0.0, step.crisis_survived ? 1 : 0,
               g_batch_index);
    }

    /* 机器模式下记录主谱系阶段进入时点，用于验证理念升级的响应速度 */
    int stage_now = evolution_main_stage(&g);
    if (stage_now != g_last_stage) {
        if (g_csv || g_quiet) {
            printf("STAGE,%d,%d,%.1f,%d\n", g_batch_index, g_era, g_time, stage_now);
        }
        g_last_stage = stage_now;
    }
    return step;
}

static double main_population_value(void)
{
    int slot = evolution_main_slot(&g);
    return slot >= 0 ? g.run.active_species[slot].population : 0.0;
}

static void log_environment(const EnvironmentStep *step)
{
    if (step->event_started) {
        int slot = evolution_main_slot(&g);
        double multiplier = environment_multiplier_for_slot(&g, slot);
        char detail[160];
        snprintf(detail, sizeof(detail), "%s;%.0f;%.2f;%.2f",
                 environment_name(step->current_environment_id), step->environment_time_left,
                 multiplier, step->instant_population_loss_ratio);
        csv_event("env_start", detail);
        out("  !! Environment: %s\n     duration %.0fs | main lineage fitness x%.2f%s\n",
            environment_name(step->current_environment_id), step->environment_time_left,
            multiplier,
            step->instant_population_loss_ratio > 0.0 ? " | instant -35% population" : "");
    }
    if (step->event_ended) {
        char detail[128];
        snprintf(detail, sizeof(detail), "%s;%d",
                 environment_name(step->ended_environment_id), step->crisis_survived ? 1 : 0);
        csv_event("env_end", detail);
        out("  !! %-16s ended: %s\n", environment_name(step->ended_environment_id),
            step->crisis_survived ? "crisis survived" : "population crash (not counted)");
    }
}

static void buy_legacy_to(LegacyBranch branch, int target_level)
{
    while (g.legacy.levels[branch] < target_level && g.legacy.levels[branch] < LEGACY_MAX_LEVEL) {
        int cost = legacy_upgrade_cost(g.legacy.levels[branch]);
        if (cost <= 0 || g.legacy.evolution_points < cost) {
            break;
        }
        if (legacy_validate_upgrade(&g, branch) != ACTION_OK) {
            break;
        }
        legacy_apply_upgrade(&g, branch);
        char detail[64];
        snprintf(detail, sizeof(detail), "%d;%d;%d", (int)branch, g.legacy.levels[branch], cost);
        csv_event("legacy", detail);
        out("  ++ Legacy upgrade: branch %d -> Lv.%d  (-%d EP, %d EP left)\n", (int)branch,
            g.legacy.levels[branch], cost, g.legacy.evolution_points);
    }
}

static void spend_evolution_points(void)
{
    /* 先保第三槽与突变门槛（各 5 级），再逐步点满；避免把 EP 全砸在单条线上 */
    buy_legacy_to(LEGACY_MUTATION, 5);
    buy_legacy_to(LEGACY_DIVERSIFICATION, 5);
    buy_legacy_to(LEGACY_MUTATION, LEGACY_MAX_LEVEL);
    buy_legacy_to(LEGACY_DIVERSIFICATION, LEGACY_MAX_LEVEL);
    buy_legacy_to(LEGACY_METABOLISM, LEGACY_MAX_LEVEL);
    buy_legacy_to(LEGACY_ADAPTATION, LEGACY_MAX_LEVEL);
}

static void start_new_run(void)
{
    int era = g.run.era + 1;
    memset(&g.run, 0, sizeof(g.run));
    g.run.era = era;
    g.run.rng_state = 0xBEEF0000u + (unsigned int)era;
    g.run.active_species_count = 1;
    g.run.active_species[0].species_id = SPECIES_PROTOCELL;
    g.run.active_species[0].is_main_lineage = true;
    g.run.active_species[0].population = STARTING_POPULATION;
    g.run.atp = STARTING_ATP;
    g.run.biomass = STARTING_BIOMASS;
    g.run.dna = LEGACY_STARTING_DNA;
    g.run.discovered_species[SPECIES_PROTOCELL] = true;
    g_atp_spent = 0.0;
    g_atp_income = 0.0;
    environment_init_run(&g);
    buffs_init_run(&g);
    legacy_apply_new_run_bonuses(&g);
    g.phase = PHASE_RUNNING;
    g_click_timer = 0.0;
}

/* 突变收益探针：以前置阶段允许的物种、种群 10、无其他突变、常态环境为基准 */
static void csv_mutation_impact(void)
{
    if (!g_csv) {
        return;
    }
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        const MutationDef *def = mutation_get(id);
        int species_id = SPECIES_PROTOCELL;
        if (def->min_phase >= 2) {
            species_id = SPECIES_ALGAL_EUKARYOTE;
        } else if (def->min_phase >= 1) {
            species_id = SPECIES_CYANOBACTERIUM;
        }

        GameState probe;
        memset(&probe, 0, sizeof(probe));
        probe.run.active_species_count = 1;
        probe.run.active_species[0].species_id = species_id;
        probe.run.active_species[0].is_main_lineage = true;
        probe.run.active_species[0].population = STARTING_POPULATION;
        probe.run.current_environment_id = ENV_NORMAL_OCEAN;

        SpeciesRates before = evolution_calculate_species_rates(&probe, 0);
        probe.run.active_species[0].mutations[id] = true;
        SpeciesRates after = evolution_calculate_species_rates(&probe, 0);

        double atp = before.atp_per_second > 0.0
                         ? (after.atp_per_second / before.atp_per_second - 1.0) * 100.0
                         : 0.0;
        double biomass = before.biomass_per_second > 0.0
                             ? (after.biomass_per_second / before.biomass_per_second - 1.0) * 100.0
                             : 0.0;
        double dna = before.dna_per_second > 0.0
                         ? (after.dna_per_second / before.dna_per_second - 1.0) * 100.0
                         : 0.0;
        double cap = before.final_population_cap > 0.0
                         ? (after.final_population_cap / before.final_population_cap - 1.0) * 100.0
                         : 0.0;
        printf("MUTIMPACT,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f\n", id, atp, biomass, dna, cap,
               def->growth_bonus * 100.0, evolution_mutation_cost(&probe, id));
    }
}

static void init_game(unsigned int seed)
{
    memset(&g, 0, sizeof(g));
    g.save_version = 7;
    g.phase = PHASE_RUNNING;
    g.run.era = 1;
    g.run.rng_state = seed;
    /* 第十一轮：新游戏显式从 0 DNA 开始；20 DNA 只用于跨轮新纪元 */
    g.run.dna = 0.0;
    g.run.active_species_count = 1;
    g.run.active_species[0].species_id = SPECIES_PROTOCELL;
    g.run.active_species[0].is_main_lineage = true;
    g.run.active_species[0].population = STARTING_POPULATION;
    g.run.discovered_species[SPECIES_PROTOCELL] = true;
    environment_init_run(&g);
    buffs_init_run(&g);
    g_click_timer = 0.0;
    g_time = 0.0;
    g_era = 1;
    g_atp_spent = 0.0;
    g_atp_income = 0.0;
    g_first_speciation = -1.0;
    g_first_speciation_instant = 0;
    g_cap_reached_time = -1.0;
    g_first_species = -1;
    g_last_stage = -1;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        g_mutation_time[id] = -1.0;
    }
}

static void print_coverage(int index, int era)
{
    if (!g_quiet) {
        return;
    }
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        if (g.run.discovered_species[id]) {
            printf("COVER_S,%d,%d,%d\n", index, era, id);
        }
    }
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (g.run.discovered_mutations[id]) {
            printf("COVER_M,%d,%d,%d\n", index, era, id);
        }
    }
}

/* 批量回放：同一策略、不同随机种子，统计 EP 分布与分化时点 */
static void run_batch(int count, int steps)
{
    g_quiet = true;
    for (int index = 1; index <= count; ++index) {
        unsigned int seed = 0xC0FFEE01u + (unsigned int)index * 2654435761u;
        init_game(seed);
        for (int step = 1; step <= steps; ++step) {
            g_time = step * FIXED_STEP_SECONDS;
            simulate_step(FIXED_STEP_SECONDS);
        }
        EraReward reward = prestige_preview_reward(&g);
        int stages = 0;
        int mutations = 0;
        int species = 0;
        int crises = 0;
        prestige_run_stats(&g, &stages, &mutations, &species, &crises);
        printf("BATCH,%d,%u,%d,%d,%d,%d,%d,%.0f,%.1f,%d\n", index, seed, reward.total_points,
               stages, mutations, species, crises, g.run.max_biomass, g_first_speciation,
               g_first_species);
        print_coverage(index, 1);
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            if (g.run.discovered_mutations[id]) {
                printf("BMUT,%d,%d,%.1f\n", index, id, g_mutation_time[id]);
            }
        }
    }
    g_quiet = false;
}

typedef struct {
    int ep;
    int stages;
    int mutations;
    int species;
    int crises;
    double max_biomass;
    double first_speciation;
    int first_speciation_instant;
    double cap_reached_time;
    double last_purchase;
    double atp;
    double biomass;
    double dna;
    double knowledge;
    int sub_upgrades;
    double first_mutation;
    double atp_spent;
    double atp_residual;
    int buff_collected;
    int buff_rare;
    double buff_uptime;
    int insight_buys;
    int insight_levels;
    int capacity_levels;
    int reserve_levels;
} EraSummary;

/* 跑完一整轮并汇总 EP 明细；调用方负责 g_time / g_era 与阶段切换 */
static EraSummary simulate_era(int steps)
{
    EraSummary summary;
    memset(&summary, 0, sizeof(summary));
    g_first_speciation = -1.0;
    g_first_speciation_instant = 0;
    g_cap_reached_time = -1.0;
    g_first_species = -1;
    g_first_mutation = -1.0;
    g_last_purchase_time = -1.0;
    g_sub_upgrade_buys = 0;
    g_insight_buys = 0;
    g_atp_spent = 0.0;
    g_atp_income = 0.0;
    g_last_stage = -1;
    double atp_start = g.run.atp;
    for (int step = 1; step <= steps; ++step) {
        g_era = g.run.era;
        g_time = step * FIXED_STEP_SECONDS;
        simulate_step(FIXED_STEP_SECONDS);
        if (g_cap_reached_time < 0.0 && evolution_main_stage(&g) >= evolution_era_stage_cap(&g)) {
            g_cap_reached_time = g_time;
        }
    }
    EraReward reward = prestige_preview_reward(&g);
    summary.ep = reward.total_points;
    prestige_run_stats(&g, &summary.stages, &summary.mutations, &summary.species,
                       &summary.crises);
    summary.max_biomass = g.run.max_biomass;
    summary.first_speciation = g_first_speciation;
    summary.first_speciation_instant = g_first_speciation_instant;
    summary.cap_reached_time = g_cap_reached_time;
    summary.last_purchase = g_last_purchase_time;
    summary.atp = g.run.atp;
    summary.biomass = g.run.biomass;
    summary.dna = g.run.dna;
    summary.knowledge = g.run.knowledge;
    summary.sub_upgrades = g_sub_upgrade_buys;
    summary.first_mutation = g_first_mutation;
    summary.atp_residual = atp_start + g_atp_income - g_atp_spent - g.run.atp;
    summary.atp_spent = g_atp_spent;
    summary.buff_collected = g.run.buff_collected;
    summary.buff_rare = g.run.buff_rare_collected;
    summary.buff_uptime = g.run.buff_total_active_time;
    summary.insight_buys = g_insight_buys;
    summary.insight_levels = g.run.insight_levels;
    summary.capacity_levels = g.run.capacity_levels;
    summary.reserve_levels = g.run.reserve_levels;
    return summary;
}

/* 两轮回归：第一纪元结算 -> 按新优先级加点 -> 第二纪元再用双谱系冲刺 */
static void run_batch2(int count, int steps)
{
    g_quiet = true;
    for (int index = 1; index <= count; ++index) {
        unsigned int seed = 0xC0FFEE01u + (unsigned int)index * 2654435761u;
        g_batch_index = index;
        init_game(seed);
        EraSummary era1 = simulate_era(steps);
        int era1_species_ids[SPECIES_COUNT];
        int era1_species_count = 0;
        for (int id = 1; id < SPECIES_COUNT; ++id) {
            if (g.run.discovered_species[id]) {
                era1_species_ids[era1_species_count++] = id;
            }
        }
        g.phase = PHASE_END_ERA_PREVIEW;
        prestige_commit_end_era(&g);
        spend_evolution_points();
        int mutation_level = g.legacy.levels[LEGACY_MUTATION];
        int diversification_level = g.legacy.levels[LEGACY_DIVERSIFICATION];
        start_new_run();
        /* start_new_run 用固定的 BEEF0000+era 重置随机数，批量回归时改由种子派生，
         * 保证 10 个种子的第二纪元互相独立 */
        g.run.rng_state = seed * 2654435761u + 0x9E3779B9u;
        if (g.run.rng_state == 0) {
            g.run.rng_state = 1u;
        }
        buffs_init_run(&g);
        EraSummary era2 = simulate_era(steps);
        printf("BATCH2,%d,%u,%d,%d,%d,%d,%d,%.0f,%d,%d,%d,%d,%d,%.0f,%d,%d,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.1f,%d,%d,%.1f,%.1f\n",
               index, seed,
               era1.ep, era1.stages, era1.mutations, era1.species, era1.crises, era1.max_biomass,
               era2.ep, era2.stages, era2.mutations, era2.species, era2.crises, era2.max_biomass,
               mutation_level, diversification_level, era2.first_speciation,
               era1.first_speciation, era1.last_purchase, era2.last_purchase, era2.atp,
               era2.biomass, era2.dna, era2.knowledge,                era1.sub_upgrades, era2.sub_upgrades,
               era1.first_mutation, era2.first_mutation);
        printf("RAMPTIME,%d,1,%.1f,%d\n", index, era1.cap_reached_time,
               era1.first_speciation_instant);
        printf("RAMPTIME,%d,2,%.1f,%d\n", index, era2.cap_reached_time,
               era2.first_speciation_instant);
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            if (g.run.discovered_mutations[id]) {
                printf("BMUT2,%d,%d,%.1f\n", index, id, g_mutation_time[id]);
            }
        }
        for (int id = 1; id < SPECIES_COUNT; ++id) {
            if (g.run.discovered_species[id]) {
                printf("BSPEC2,%d,%d\n", index, id);
            }
        }
        for (int i = 0; i < era1_species_count; ++i) {
            printf("BSPEC1,%d,%d\n", index, era1_species_ids[i]);
        }
        printf("ATPSUM,%d,1,%.0f,%.0f,%.3f\n", index, era1.atp_spent, era1.atp,
               era1.atp_residual);
        printf("ATPSUM,%d,2,%.0f,%.0f,%.3f\n", index, era2.atp_spent, era2.atp,
               era2.atp_residual);
        printf("BUFFSUM,%d,1,%d,%d,%.1f,%d,%d\n", index, era1.buff_collected, era1.buff_rare,
               era1.buff_uptime, era1.insight_buys, era1.insight_levels);
        printf("BUFFSUM,%d,2,%d,%d,%.1f,%d,%d\n", index, era2.buff_collected, era2.buff_rare,
               era2.buff_uptime, era2.insight_buys, era2.insight_levels);
        printf("SINKSUM,%d,1,%d,%d\n", index, era1.capacity_levels, era1.reserve_levels);
        printf("SINKSUM,%d,2,%d,%d\n", index, era2.capacity_levels, era2.reserve_levels);
        print_coverage(index, 2);
    }
    g_quiet = false;
}

/* 三纪元回归：用于观察长线路线（阶段 8–10）的跨轮推进 */
static void run_batch3(int count, int steps)
{
    g_quiet = true;
    for (int index = 1; index <= count; ++index) {
        unsigned int seed = 0xC0FFEE01u + (unsigned int)index * 2654435761u;
        g_batch_index = index;
        init_game(seed);
        EraSummary era1 = simulate_era(steps);
        g.phase = PHASE_END_ERA_PREVIEW;
        prestige_commit_end_era(&g);
        spend_evolution_points();
        start_new_run();
        g.run.rng_state = seed * 2654435761u + 0x9E3779B9u;
        if (g.run.rng_state == 0) {
            g.run.rng_state = 1u;
        }
        buffs_init_run(&g);
        EraSummary era2 = simulate_era(steps);
        g.phase = PHASE_END_ERA_PREVIEW;
        prestige_commit_end_era(&g);
        spend_evolution_points();
        int mutation_level = g.legacy.levels[LEGACY_MUTATION];
        int diversification_level = g.legacy.levels[LEGACY_DIVERSIFICATION];
        start_new_run();
        g.run.rng_state = seed * 2246822519u + 0x85EBCA6Bu;
        if (g.run.rng_state == 0) {
            g.run.rng_state = 1u;
        }
        buffs_init_run(&g);
        EraSummary era3 = simulate_era(steps);
        printf("BATCH3,%d,%u,%d,%d,%d,%d,%d,%d,%d,%.0f,%.1f,%d,%d,%.1f,%.1f,%d,%d,%d,%.1f,%.1f,%.1f\n",
               index, seed,
               era1.ep, era2.ep, era3.ep, era3.stages, era3.mutations, era3.species,
               era3.crises, era3.max_biomass, era3.last_purchase, mutation_level,
               diversification_level, era3.first_speciation, era3.knowledge,
               era1.sub_upgrades, era2.sub_upgrades, era3.sub_upgrades,
               era1.first_mutation, era2.first_mutation, era3.first_mutation);
        printf("RAMPTIME,%d,1,%.1f,%d\n", index, era1.cap_reached_time,
               era1.first_speciation_instant);
        printf("RAMPTIME,%d,2,%.1f,%d\n", index, era2.cap_reached_time,
               era2.first_speciation_instant);
        printf("RAMPTIME,%d,3,%.1f,%d\n", index, era3.cap_reached_time,
               era3.first_speciation_instant);
        for (int id = 1; id < SPECIES_COUNT; ++id) {
            if (g.run.discovered_species[id]) {
                printf("BSPEC3,%d,%d\n", index, id);
            }
        }
        printf("ATPSUM,%d,1,%.0f,%.0f,%.3f\n", index, era1.atp_spent, era1.atp,
               era1.atp_residual);
        printf("ATPSUM,%d,2,%.0f,%.0f,%.3f\n", index, era2.atp_spent, era2.atp,
               era2.atp_residual);
        printf("ATPSUM,%d,3,%.0f,%.0f,%.3f\n", index, era3.atp_spent, era3.atp,
               era3.atp_residual);
        printf("BUFFSUM,%d,1,%d,%d,%.1f,%d,%d\n", index, era1.buff_collected, era1.buff_rare,
               era1.buff_uptime, era1.insight_buys, era1.insight_levels);
        printf("BUFFSUM,%d,2,%d,%d,%.1f,%d,%d\n", index, era2.buff_collected, era2.buff_rare,
               era2.buff_uptime, era2.insight_buys, era2.insight_levels);
        printf("BUFFSUM,%d,3,%d,%d,%.1f,%d,%d\n", index, era3.buff_collected, era3.buff_rare,
               era3.buff_uptime, era3.insight_buys, era3.insight_levels);
        printf("SINKSUM,%d,1,%d,%d\n", index, era1.capacity_levels, era1.reserve_levels);
        printf("SINKSUM,%d,2,%d,%d\n", index, era2.capacity_levels, era2.reserve_levels);
        printf("SINKSUM,%d,3,%d,%d\n", index, era3.capacity_levels, era3.reserve_levels);
        print_coverage(index, 3);
    }
    g_quiet = false;
}

/* 物种 × 突变可用性矩阵（阶段、前置、永久等级全部生效） */
static void csv_availability(void)
{
    if (!g_csv) {
        return;
    }
    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        for (int mutation_id = 0; mutation_id < MUTATION_COUNT; ++mutation_id) {
            GameState probe;
            memset(&probe, 0, sizeof(probe));
            probe.run.active_species_count = 1;
            probe.run.active_species[0].species_id = species_id;
            probe.run.active_species[0].is_main_lineage = true;
            probe.run.active_species[0].population = STARTING_POPULATION;
            probe.run.current_environment_id = ENV_NORMAL_OCEAN;
            int available = evolution_mutation_available(&probe, 0, mutation_id) ? 1 : 0;
            printf("AVAIL,%d,%d,%d\n", species_id, mutation_id, available);
        }
    }
}

/* 候选生成频率：同一物种重复开 2000 次候选，统计每个突变的出现次数 */
static void csv_candidate_stats(int draws)
{
    if (!g_csv) {
        return;
    }
    const int scenarios[4] = {0, 3, 4, 5};
    for (int s = 0; s < 4; ++s) {
        init_game(0xCA11D1E5u + (unsigned int)s * 7919u);
        g.legacy.levels[LEGACY_MUTATION] = scenarios[s];
        g.run.biomass = 0.0; /* 关闭关键突变保底，只看权重 */

        int counts[MUTATION_COUNT] = {0};
        long total_choices = 0;
        for (int draw = 0; draw < draws; ++draw) {
            g.run.mutation_offer.active = false;
            unsigned int rng = g.run.rng_state;
            MutationOfferState offer = evolution_generate_mutation_offer_rng(&g, 0, &rng);
            g.run.rng_state = rng;
            for (int i = 0; i < offer.choice_count; ++i) {
                ++counts[offer.mutation_ids[i]];
            }
            total_choices += offer.choice_count;
        }
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            printf("CAND,%d,%d,%d\n", scenarios[s], id, counts[id]);
        }
        printf("CANDAVG,%d,%.3f\n", scenarios[s], (double)total_choices / (double)draws);
    }
}

static void csv_tree(void)
{
    if (!g_csv) {
        return;
    }
    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        int children[SPECIES_COUNT];
        int count = species_children(species_id, children, SPECIES_COUNT);
        for (int i = 0; i < count; ++i) {
            printf("TREE,%d,%d\n", species_id, children[i]);
        }
    }
}

static void csv_mutation_traits(void)
{
    if (!g_csv) {
        return;
    }
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        const MutationDef *def = mutation_get(id);
        printf("MTRAIT,%d,%d,%d,%d,%d,%d\n", id, def->trait_bonus[TRAIT_HEAT],
               def->trait_bonus[TRAIT_COLD], def->trait_bonus[TRAIT_LOW_LIGHT],
               def->trait_bonus[TRAIT_OXYGEN], def->trait_bonus[TRAIT_TOXIN]);
    }
}

/* 可玩性扩展：突变共振、物种基石、图鉴里程碑与生态韧性 */
static void csv_expansion(void)
{
    if (!g_csv) {
        return;
    }
    for (int index = 0; index < evolution_synergy_count(); ++index) {
        const MutationSynergyDef *synergy = evolution_synergy_get(index);
        GameState probe;
        memset(&probe, 0, sizeof(probe));
        probe.run.active_species_count = 1;
        probe.run.active_species[0].species_id = SPECIES_PROTOCELL;
        probe.run.active_species[0].is_main_lineage = true;
        probe.run.active_species[0].population = STARTING_POPULATION;
        probe.run.active_species[0].mutations[synergy->mutation_a] = true;
        probe.run.active_species[0].mutations[synergy->mutation_b] = true;

        double atp = 1.0;
        double biomass = 1.0;
        double dna = 1.0;
        double cap = 1.0;
        double growth = 1.0;
        evolution_synergy_bonuses(&probe, 0, &atp, &biomass, &dna, &cap, &growth, 0);
        printf("SYN,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%.1f\n", index, synergy->mutation_a,
               synergy->mutation_b, (atp - 1.0) * 100.0, (biomass - 1.0) * 100.0,
               (dna - 1.0) * 100.0, (cap - 1.0) * 100.0, (growth - 1.0) * 100.0);
        printf("SYNNAME,%d,%s\n", index, synergy->name);
    }

    for (int species_id = 0; species_id < SPECIES_COUNT; ++species_id) {
        const SpeciesKeystone *keystone = species_keystone(species_id);
        if (!keystone) {
            continue;
        }
        printf("KEY,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", species_id,
               keystone->atp_bonus * 100.0, keystone->biomass_bonus * 100.0,
               keystone->dna_bonus * 100.0, keystone->cap_bonus * 100.0,
               keystone->growth_bonus * 100.0, keystone->low_light_bonus * 100.0,
               keystone->toxin_bonus * 100.0, keystone->crisis_damage_reduction * 100.0);
        printf("KEYNAME,%d,%s\n", species_id, keystone->name);
    }

    /* 图鉴里程碑：突变图鉴 0..MUTATION_COUNT，物种图鉴 0..SPECIES_COUNT-1 */
    LegacyState legacy;
    memset(&legacy, 0, sizeof(legacy));
    printf("CODEXM,0,%.3f,%.3f,%.3f\n", legacy_atp_multiplier(&legacy),
           legacy_biomass_multiplier(&legacy), legacy_dna_multiplier(&legacy));
    for (int count = 1; count <= MUTATION_COUNT; ++count) {
        legacy.codex_mutations[count - 1] = true;
        printf("CODEXM,%d,%.3f,%.3f,%.3f\n", count, legacy_atp_multiplier(&legacy),
               legacy_biomass_multiplier(&legacy), legacy_dna_multiplier(&legacy));
    }
    memset(&legacy, 0, sizeof(legacy));
    for (int count = 1; count < SPECIES_COUNT; ++count) {
        legacy.codex_species[count] = true;
        printf("CODEXS,%d,%.3f\n", count, legacy_biomass_multiplier(&legacy));
    }

    /* 生态韧性：存活 0..ENV_COUNT-1 种危机时的产率倍率 */
    for (int count = 0; count < ENV_COUNT; ++count) {
        GameState probe;
        memset(&probe, 0, sizeof(probe));
        probe.run.active_species_count = 1;
        probe.run.active_species[0].species_id = SPECIES_PROTOCELL;
        probe.run.active_species[0].is_main_lineage = true;
        probe.run.active_species[0].population = STARTING_POPULATION;
        for (int id = 1; id <= count; ++id) {
            probe.run.survived_crises[id] = true;
        }
        SpeciesRates rates = evolution_calculate_species_rates(&probe, 0);
        printf("RESIL,%d,%.3f\n", count, rates.atp_per_second / 0.80);
    }
}

/* 双物种槽实验：Diversification Lv.3，保留亲本 60/40，观察独立种群与协同 */
static void run_dual_species(void)
{
    if (!g_csv) {
        return;
    }
    init_game(0xD0A1BEEFu);
    g.legacy.levels[LEGACY_DIVERSIFICATION] = 3;
    g.run.active_species[0].population = 100.0;
    g.run.biomass = 200.0;
    g.run.active_species[0].mutations[MUTATION_PHOTOSYNTHESIS] = true;
    evolution_apply_speciation(&g, 0, SPECIES_CYANOBACTERIUM, 1);

    g.run.biomass = 0.0;
    g.run.atp = 0.0;
    g.run.dna = 0.0;
    g.run.max_biomass = 0.0;

    for (int step = 1; step <= 1800; ++step) {
        g_time = step * FIXED_STEP_SECONDS;
        SpeciesRates rates[MAX_ACTIVE_SPECIES];
        double populations[MAX_ACTIVE_SPECIES];
        for (int i = 0; i < g.run.active_species_count; ++i) {
            rates[i] = evolution_calculate_species_rates(&g, i);
            double population = g.run.active_species[i].population +
                                rates[i].population_delta_per_second * FIXED_STEP_SECONDS;
            if (population < MIN_ACTIVE_POPULATION) {
                population = MIN_ACTIVE_POPULATION;
            }
            if (population > rates[i].final_population_cap) {
                population = rates[i].final_population_cap;
            }
            populations[i] = population;
        }
        for (int i = 0; i < g.run.active_species_count; ++i) {
            g.run.active_species[i].population = populations[i];
        }
        g.run.atp += (rates[0].atp_per_second + rates[1].atp_per_second) * FIXED_STEP_SECONDS;
        g.run.biomass +=
            (rates[0].biomass_per_second + rates[1].biomass_per_second) * FIXED_STEP_SECONDS;
        g.run.dna += (rates[0].dna_per_second + rates[1].dna_per_second) * FIXED_STEP_SECONDS;

        EnvironmentStep environment_step = environment_calculate_step(&g, FIXED_STEP_SECONDS);
        environment_apply_step(&g, &environment_step);

        if (step % 10 == 0) {
            printf("DROW,%.1f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%d\n", g_time,
                   g.run.active_species[0].population, g.run.active_species[1].population,
                   rates[0].atp_per_second, rates[1].atp_per_second,
                   rates[0].atp_per_second + rates[1].atp_per_second,
                   rates[0].biomass_per_second + rates[1].biomass_per_second,
                   rates[0].synergy_active ? 1 : 0);
        }
    }
}

int main(int argc, char **argv)
{
    int batch_count = 0;
    int batch2_count = 0;
    int batch3_count = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--csv") == 0) {
            g_csv = true;
        } else if (strcmp(argv[i], "--sensitivity") == 0) {
            g_sens = true;
            g_csv = true;
        } else if (strcmp(argv[i], "--explore") == 0) {
            g_explore = true;
        } else if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            batch_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--batch2") == 0 && i + 1 < argc) {
            batch2_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--batch3") == 0 && i + 1 < argc) {
            batch3_count = atoi(argv[++i]);
        }
    }

    if (g_sens) {
        printf("SCENARIO,%s\n", TOF_STRINGIFY(TOF_SCENARIO));
        run_batch(batch_count > 0 ? batch_count : 10, 6000);
        return 0;
    }

    if (batch2_count > 0) {
        run_batch2(batch2_count, 6000);
        return 0;
    }

    if (batch3_count > 0) {
        run_batch3(batch3_count, 6000);
        return 0;
    }

    init_game(0xC0FFEE01u);

    if (g_csv) {
        printf("# era,time,stage,species,population,cap,atp_per_s,biomass_per_s,dna_per_s,biomass,"
               "atp,environment,metab_lv,repro_lv,genome_lv,disc_mut,disc_spe,knowledge,"
               "knowledge_per_s\n");
        csv_fitness_matrix();
        csv_mutation_impact();
        csv_expansion();
    }
    line();
    out("Tree of Life - Member B demo (evolution / selection / legacy)\n");
    line();
    out("Era 1 begins with a single protocell. No legacy upgrades yet.\n");
    out("Keystone: %s - %s\n\n",
        species_keystone(SPECIES_PROTOCELL)->name,
        species_keystone(SPECIES_PROTOCELL)->description);

    const double dt = FIXED_STEP_SECONDS;
    const int total_steps = 6000; /* 600 秒 */
    double era1_rate_at_60 = 0.0;
    for (int step = 1; step <= total_steps; ++step) {
        g_era = 1;
        g_time = step * dt;
        EnvironmentStep environment_step = simulate_step(dt);
        log_environment(&environment_step);
        if (step % 10 == 0) {
            csv_row();
        }
        if (step % 600 == 0) {
            print_status(g_time);
        }
        if (step == 600) {
            era1_rate_at_60 =
                evolution_calculate_species_rates(&g, evolution_main_slot(&g)).atp_per_second;
        }
    }

    line();
    int main_slot = evolution_main_slot(&g);
    out("Evolution synergies active: %d\n", evolution_active_synergy_count(&g, main_slot));
    for (int i = 0; i < evolution_synergy_count(); ++i) {
        if (evolution_synergy_active(&g, main_slot, i)) {
            out("  * %s\n", evolution_synergy_get(i)->name);
        }
    }
    out("End of Era preview\n");
    int stages = 0;
    int mutations = 0;
    int species = 0;
    int crises = 0;
    prestige_run_stats(&g, &stages, &mutations, &species, &crises);
    EraReward reward = prestige_preview_reward(&g);
    out("  E stages=%d  M mutations=%d  S species=%d  C crises=%d  B biomass=%.0f (milestone %d)\n",
        stages, mutations, species, crises, g.run.max_biomass, reward.biomass_points);
    out("  EP = %d*%d + %d/%d + %d/%d + min(%d,%d) + %d = %d\n", EP_PER_STAGE, stages, mutations,
        EP_MUTATIONS_PER_POINT, species, EP_SPECIES_PER_POINT, crises, EP_CRISIS_CAP,
        reward.biomass_points, reward.total_points);

    char era_detail[160];
    snprintf(era_detail, sizeof(era_detail), "%d;%d;%d;%d;%d;%d;%.0f", reward.total_points, stages,
             mutations, species, crises, reward.biomass_points, g.run.max_biomass);
    csv_event("era_end", era_detail);

    g.phase = PHASE_END_ERA_PREVIEW;
    if (prestige_commit_end_era(&g) == ACTION_OK) {
        out("  Confirmed. EP balance = %d (claimed once)\n", g.legacy.evolution_points);
    }
    if (prestige_commit_end_era(&g) == ACTION_ALREADY_CLAIMED) {
        out("  Second confirm rejected: reward already claimed.\n");
    }

    out("\nSpending EP in the Legacy tree\n");
    spend_evolution_points();
    out("  Levels: Metabolism %d | Mutation %d | Adaptation %d | Diversification %d | EP left %d\n",
        g.legacy.levels[LEGACY_METABOLISM], g.legacy.levels[LEGACY_MUTATION],
        g.legacy.levels[LEGACY_ADAPTATION], g.legacy.levels[LEGACY_DIVERSIFICATION],
        g.legacy.evolution_points);

    line();
    out("Starting Era 2 with the same species reset\n");
    start_new_run();
    g_era = 2;
    out("  Start ATP = %.0f, free Metabolism run upgrade = Lv.%d\n", g.run.atp,
        g.run.run_upgrade_levels[UPGRADE_METABOLISM]);
    out("  Permanent multipliers: ATP x%.2f, Biomass x%.2f, DNA x%.2f\n",
        legacy_atp_multiplier(&g.legacy), legacy_biomass_multiplier(&g.legacy),
        legacy_dna_multiplier(&g.legacy));

    double era2_rate_at_60 = 0.0;
    for (int step = 1; step <= 1800; ++step) {
        g_time = step * dt;
        EnvironmentStep environment_step = simulate_step(dt);
        log_environment(&environment_step);
        if (step % 10 == 0) {
            csv_row();
        }
        if (step % 600 == 0) {
            print_status(g_time);
        }
        if (step == 600) {
            era2_rate_at_60 =
                evolution_calculate_species_rates(&g, evolution_main_slot(&g)).atp_per_second;
        }
    }

    int codex_species = 0;
    for (int id = 1; id < SPECIES_COUNT; ++id) {
        if (g.legacy.codex_species[id]) {
            ++codex_species;
        }
    }
    int codex_mutations = 0;
    for (int id = 0; id < MUTATION_COUNT; ++id) {
        if (g.legacy.codex_mutations[id]) {
            ++codex_mutations;
        }
    }

    if (batch_count > 0) {
        run_batch(batch_count, 6000);
    }
    if (g_csv) {
        csv_tree();
        csv_mutation_traits();
        csv_availability();
        csv_candidate_stats(2000);
        run_dual_species();
    }

    line();
    out("Era 2 ATP/s at t=60s: %.2f  vs  Era 1: %.2f  (x%.2f from permanent upgrades)\n",
        era2_rate_at_60, era1_rate_at_60, era1_rate_at_60 > 0.0 ? era2_rate_at_60 / era1_rate_at_60
                                                               : 0.0);
    out("Codex kept across eras: species %d / %d, mutations %d / %d\n", codex_species,
        SPECIES_COUNT - 1, codex_mutations, MUTATION_COUNT);
    out("Best records: stage %d, EP %d, biomass %.0f\n", g.legacy.best_era,
        g.legacy.best_evolution_points, g.legacy.best_biomass);
    line();
    return 0;
}
