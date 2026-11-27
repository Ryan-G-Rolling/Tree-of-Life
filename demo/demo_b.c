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
/* v10 P0-22/P0-26：单级耗时台账与“逐类型一条记录”的危机台账 */
static int g_upgrade_buys = 0;
static unsigned int g_crisis_logged = 0;

static double main_population_value(void);
static int planned_target(int slot);

/* v7 P0-2/4：为下一步主线门票预留 ATP，避免商店把 ATP 花光后卡在阶段 8。
 * v10 P0-22：改为“预留 1.2 × 下一张门票”（最早能凑齐的那张），不再按最大值预留，
 * 否则商店三格被整笔冻结，ATP 永远在等突变。 */
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
    bool first = true;
    for (int i = 0; i < count; ++i) {
        if (evolution_slot_owns_mutation(&g, slot, required[i])) {
            continue;
        }
        double cost = evolution_mutation_atp_cost(&g, required[i]);
        if (first || cost < reserve) {
            reserve = cost;
            first = false;
        }
    }
    return reserve * 1.2;
}

/* v10 P0-22：生物量同样需要“门票预留”。时间本位下分化门票 = 45 s 生物量产率，
 * 若基因组 / 容量（也吃生物量）每步都把它扫光，主线永远凑不齐门票。
 * v10 收口：整张门票全冻结时，生物量差额永远攒不出来，容量在阶段 12/13 一局只能买 1 级；
 * 但把预留降到 0.5 会让容量在纪元 1 就把开局拖慢一倍（实测首分化 150 → 274 s）。
 * 折中：基因组建保留整张票，容量只保留 30%——容量吃的是 ATP 为主，生物量份额很小。 */
static double speciation_biomass_reserve_for(double factor)
{
    int slot = evolution_main_slot(&g);
    if (slot < 0) {
        return 0.0;
    }
    int target = planned_target(slot);
    if (target < 0 || species_phase(target) <= evolution_main_stage(&g)) {
        return 0.0;
    }
    return evolution_speciation_cost(&g, target) * factor;
}

static double speciation_biomass_reserve(void)
{
    return speciation_biomass_reserve_for(1.2);
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
 * 正式游戏由 A 层 game.c / economy.c 负责，本 demo 不链接 A 层。
 * v10 P0-22：与正式实现一样改时间本位——N × 主谱系产率，常数地板。 */
static double upgrade_cost(int type, int level)
{
    if (level < 0) {
        return INFINITY;
    }
    double atp_rate = 0.0, biomass_rate = 0.0, knowledge_rate = 0.0;
    evolution_price_rates(&g, &atp_rate, &biomass_rate, &knowledge_rate);
    switch (type) {
    case UPGRADE_METABOLISM: {
        double priced = UPGRADE_ATP_SECONDS_METABOLISM * atp_rate;
        return floor(priced > RUN_UPGRADE_METABOLISM_BASE_COST
                         ? priced : RUN_UPGRADE_METABOLISM_BASE_COST);
    }
    case UPGRADE_REPRODUCTION: {
        double priced = UPGRADE_ATP_SECONDS_REPRODUCTION * atp_rate;
        return floor(priced > RUN_UPGRADE_REPRODUCTION_BASE_COST
                         ? priced : RUN_UPGRADE_REPRODUCTION_BASE_COST);
    }
    default: {
        double priced = UPGRADE_BIOMASS_SECONDS_GENOME * biomass_rate;
        return floor(priced > RUN_UPGRADE_GENOME_BASE_COST
                         ? priced : RUN_UPGRADE_GENOME_BASE_COST);
    }
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
/* v10 P0-22：四张价格表都成了“产率秒数”，谁先说谁吃亏——若升级每步先买，
 * 储备与容量永远分不到 ATP（v8 靠指数涨价造成的“余额堆积”才买到）。
 * 改成轮转：每步只尝试一个经济出口，5 个出口均分收入。 */
static int g_buy_turn = 0;
/* v14：每个槽位的候选冷却。放弃候选后冷却 30 秒，
 * 防止靠“反复放弃 + 重开”白嫖候选刷新。 */
static double g_slot_cooldown[MAX_ACTIVE_SPECIES];

static void try_upgrade_type(int type)
{
    if (RUN_UPGRADE_MAX_LEVEL >= 0 && g.run.run_upgrade_levels[type] >= RUN_UPGRADE_MAX_LEVEL) {
        return;
    }
    double cost = upgrade_cost(type, g.run.run_upgrade_levels[type]);
    if (!isfinite(cost)) {
        return;
    }
    if (type == UPGRADE_GENOME) {
        if (g.run.biomass >= cost * 1.02 &&
            g.run.biomass - cost * 1.02 >= speciation_biomass_reserve()) {
            g.run.biomass -= cost;
            ++g.run.run_upgrade_levels[type];
            mark_purchase();
            ++g_upgrade_buys;
        }
        return;
    }
    /* v10 收口：购买门槛 1.15 → 1.02。原来的 15% 余量会让钱包常驻约 1.15 倍单级价格，
     * 纪元 3 结算时未花比例被抬到 11%（验收 ≤10%）。按“够钱就买”即可，
     * 价格本身是时间本位，不会因此变贵。 */
    if (g.run.atp < cost * 1.02 + target_atp_reserve()) {
        return;
    }
    double atp_before = g.run.atp;
    g.run.atp -= cost;
    g_atp_spent += atp_before - g.run.atp;
    ++g.run.run_upgrade_levels[type];
    mark_purchase();
    ++g_upgrade_buys;
}

static void try_reserve_step(void)
{
    if (RESERVE_MAX_LEVEL >= 0 && g.run.reserve_levels >= RESERVE_MAX_LEVEL) {
        return;
    }
    if (reserve_damage_scale(g.run.reserve_levels) <= RESERVE_REDUCTION_FLOOR + 1e-12) {
        return;
    }
    double cost = evolution_reserve_cost(&g);
    if (g.run.atp < cost * 1.02) {
        return;
    }
    double atp_before = g.run.atp;
    if (evolution_apply_reserve(&g) != ACTION_OK) {
        return;
    }
    g_atp_spent += atp_before - g.run.atp;
    mark_purchase();
}

static void try_capacity_step(void)
{
    if (CAPACITY_MAX_LEVEL >= 0 && g.run.capacity_levels >= CAPACITY_MAX_LEVEL) {
        return;
    }
    double biomass_cost = evolution_capacity_biomass_cost(&g);
    double atp_cost = evolution_capacity_atp_cost(&g);
    if (atp_cost <= 0.0 && biomass_cost <= 0.0) {
        return;
    }
    if (atp_cost > 0.0 && g.run.atp < atp_cost * 1.02) {
        return;
    }
    /* v10 收口：只有纪元 3 放开容量预留（门票最大、容量最容易饿死）；
     * 纪元 1/2 仍冻结整张门票，否则容量在阶段 6 之后抢走生物量，阶段 8 抵达率从 98% 掉到 27%。 */
    double capacity_reserve = g.run.era >= 3 ? 0.55 : 1.20;
    if (biomass_cost > 0.0 && g.run.biomass < biomass_cost * 1.02 +
                                                   speciation_biomass_reserve_for(capacity_reserve)) {
        return;
    }
    double atp_before = g.run.atp;
    if (evolution_apply_capacity_upgrade(&g) != ACTION_OK) {
        return;
    }
    g_atp_spent += atp_before - g.run.atp;
    mark_purchase();
}

static void try_buy_upgrades(void)
{
    double before = g_last_purchase_time;
    switch (g_buy_turn % 5) {
    case 0: try_upgrade_type(UPGRADE_METABOLISM); break;
    case 1: try_upgrade_type(UPGRADE_REPRODUCTION); break;
    case 2: try_upgrade_type(UPGRADE_GENOME); break;
    case 3: try_reserve_step(); break;
    default: try_capacity_step(); break;
    }
    ++g_buy_turn;
    /* v10 收口：轮转没花出去、而钱包已鼓到 4 倍单级价格时，直接买代谢。
     * 时间本位下每级价格 = N 秒产率，钱包在两次购买之间自然会堆到 1–2 倍价格；
     * 若某个出口已满级（容量/储备），那一轮就白转，ATP 一路闲置到纪元结束
     * （实测纪元 3 未花比例 11.3%，超过 10% 验收线）。这条兜底只在高水位触发，
     * 不改变前中期节奏。 */
    if (g_last_purchase_time == before) {
        double cost = upgrade_cost(UPGRADE_METABOLISM,
                                   g.run.run_upgrade_levels[UPGRADE_METABOLISM]);
        if (isfinite(cost) && g.run.atp >= cost * 4.0 + target_atp_reserve()) {
            try_upgrade_type(UPGRADE_METABOLISM);
        }
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
    /* v14 共享基因组：额外分支不消耗前置突变，只看生物量门票 */
    if (!g.run.active_species[slot].is_main_lineage) {
        return true;
    }
    int required[4];
    int count = species_required_mutations(target, required, 4);
    for (int i = 0; i < count; ++i) {
        /* v14 共享基因组：支线继承主谱系的突变，不必自己再买一遍 */
        if (!evolution_slot_owns_mutation(&g, slot, required[i])) {
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
    /* v10 P1-11：主谱系阶段 ≥6 时把同阶段支线物种并入候选（2 选 1 的自动版） */
    if (species->is_main_lineage) {
        int branches[SPECIES_COUNT];
        int branch_count = species_branch_candidates(species->species_id, branches, SPECIES_COUNT);
        for (int i = 0; i < branch_count && count < SPECIES_COUNT; ++i) {
            children[count++] = branches[i];
        }
    }
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
        if (slot_owns_required(slot, chain)) {
            return chain;
        }
        /* v10 P1-11：主线门票还没齐时，若同阶段的支线物种已经就绪，直接走支线。
         * 支线与主线节点同 phase，走它一样推进阶段，但能让单局物种从 12 变成 14–18。
         * 只走“还有后继”的支线——叶子物种会把主线卡死（实测 stage 停在 8）。 */
        if (species->is_main_lineage) {
            int branches[SPECIES_COUNT];
            int bn = species_branch_candidates(species->species_id, branches, SPECIES_COUNT);
            for (int i = 0; i < bn; ++i) {
                int onward[8];
                if (species_phase(branches[i]) > cap || !slot_owns_required(slot, branches[i])) {
                    continue;
                }
                if (species_children(branches[i], onward, 8) <= 0) {
                    continue;
                }
                return branches[i];
            }
        }
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
    if (target >= 0 && g.run.active_species[slot].is_main_lineage) {
        int required[4];
        int count = species_required_mutations(target, required, 4);
        for (int i = 0; i < count; ++i) {
            if (!evolution_slot_owns_mutation(&g, slot, required[i])) {
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
    if (!g_explore) {
        for (int i = 0; i < offer->choice_count; ++i) {
            if (mutation_unlocks_slot(slot, offer->mutation_ids[i])) {
                int mid = offer->mutation_ids[i];
                if (g.run.dna >= offer->dna_costs[i] &&
                    g.run.atp >= evolution_mutation_atp_cost(&g, mid)) {
                    pick = i;
                    break;
                }
            }
        }
        if (pick < 0) {
            /* v14 E3：候选里没有能推进分化的卡时，先花 DNA 换一批（每批 ≤3 次）；
             * 换不起或不划算就放弃候选、冷却 30 秒，把候选位让给别的槽位。
             * 绝不买纯属性卡——扫货会把门票链推到 150+ 张，
             * “era3 突变购买 10–16 张”这条硬门槛就失效了。 */
            if (!target_missing) {
                /* 该槽的前置已齐、在等生物量：候选留着只会占住三个槽位共用的候选位，
                 * 让额外分支永远开不了候选（实测支线卡在阶段 1）。 */
                g_slot_cooldown[slot] = g_time + 5.0;
                evolution_dismiss_mutation_offer(&g);
                return;
            }
            double refresh_cost = evolution_mutation_refresh_cost(&g);
            if (g.run.mutation_refresh_count < MUTATION_REFRESH_LIMIT &&
                g.run.dna >= refresh_cost * 3.0 &&
                evolution_refresh_mutation_offer(&g, slot) == ACTION_OK) {
                return;
            }
            g_slot_cooldown[slot] = g_time + 30.0;
            evolution_dismiss_mutation_offer(&g);
            return;
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
    } else {
        /* 买不下（价格在被选中后又被抬高 / 资源被别的出口吃掉）：
         * 候选留着会永久占住候选位，三个槽位就一起卡死。 */
        g_slot_cooldown[slot] = g_time + 3.0;
        evolution_dismiss_mutation_offer(&g);
    }
}

static bool slot_target_missing(int slot)
{
    /* v14 共享基因组：支线不消耗前置突变，永远不需要为分化买卡 */
    if (!g.run.active_species[slot].is_main_lineage) {
        return false;
    }
    int target = planned_target(slot);
    if (target < 0) {
        return false;
    }
    const SpeciesState *species = &g.run.active_species[slot];
    int required[4];
    int count = species_required_mutations(target, required, 4);
    for (int i = 0; i < count; ++i) {
        if (!species->mutations[required[i]]) {
            return true;
        }
    }
    return false;
}

/* v14 硬门槛 2/3：普通策略只买“能推进某个槽位分化”的突变，
 * 不再买纯属性卡（那会把门票链推到 150+ 张，硬门槛 2 直接失效）。
 * 三个槽位按轮转各拿候选——全给主线会让额外分支永远买不到自己的前置，
 * 单局物种数就只能停在主链的 12–13 个。 */
static int slot_needing_mutations(void)
{
    int count = g.run.active_species_count;
    for (int i = 0; i < count; ++i) {
        int slot = (g_offer_slot + i) % count;
        if (g_time < g_slot_cooldown[slot]) {
            continue;
        }
        if (slot_target_missing(slot)) {
            g_offer_slot = slot + 1;
            return slot;
        }
    }
    return -1;
}

static void try_mutations(void)
{
    if (g.run.mutation_offer.active) {
        choose_mutation(g.run.mutation_offer.species_slot);
        return;
    }
    if (!g_explore) {
        int slot = slot_needing_mutations();
        if (slot >= 0 && evolution_open_mutation_offer(&g, slot) == ACTION_OK) {
            choose_mutation(slot);
        }
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

/* v10：储备与容量已并入 try_buy_upgrades 的 5 路轮转，
 * 不再需要“最后扫余额”的独立阶段。 */


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
            double cost = evolution_sub_upgrade_cost(&g, next);
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
            double cost = evolution_sub_upgrade_cost(&g, next);
            if (cost > reserve) {
                reserve = cost;
            }
        }
    }
    /* v10：只预留一半子升级价——全预留会让洞见永远买不到（实测 Lv.0），
     * 全不预留又会饿死子升级（实测每纪元 1 层）。 */
    while (g.run.knowledge >= evolution_insight_cost(&g) + reserve * 0.5) {
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
    g.run.elapsed_time += dt; /* 与 game.c 固定步一致：DNA 起手斜坡依赖这个时钟 */
    if (g.run.biomass > g.run.max_biomass) {
        g.run.max_biomass = g.run.biomass;
    }

    g_click_timer += dt;
    while (g_click_timer >= 1.0) {
        g_click_timer -= 1.0;
        /* v10：点击 = 3 秒主谱系产率 × 永久点击 × 限时点击（与 economy.c 同一公式） */
        double click_rate = 0.0, click_biomass = 0.0, click_knowledge = 0.0;
        evolution_price_rates(&g, &click_rate, &click_biomass, &click_knowledge);
        double click = CLICK_INCOME_SECONDS * click_rate *
                       legacy_click_multiplier(&g.legacy) *
                       buffs_click_multiplier(&g);
        g.run.atp += click;
        g_atp_income += click;
    }

    /* 门票优先，其余出口按轮转均分（见 try_buy_upgrades） */
    try_mutations();
    try_speciation();
    try_buy_upgrades();
    /* v10：洞见先于子升级——子升级会一次吃掉全部理念，洞见就永远轮不上
     * （实测洞见长期 Lv.0，而目标是每纪元 6–14 级）。 */
    try_insight();
    try_sub_upgrades();

    EnvironmentStep step = environment_calculate_step(&g, dt);
    environment_apply_step(&g, &step);

    /* 机器模式：危机结束时输出逐次记录，供覆盖率与通过率统计。
     * v10 P0-26 口径修复：每种危机每局只记一条——第一次通过时记录 survived=1；
     * 到纪元结束仍未通过的，由 simulate_era 统一补一条 survived=0。
     * 旧口径重复事件会把逐类型样本数放大到 1100–1700，通过率被重复抽中稀释。 */
    if ((g_csv || g_quiet) && step.event_ended && environment_is_crisis(step.ended_environment_id)
        && step.crisis_survived
        && !(g_crisis_logged & (1u << step.ended_environment_id))) {
        g_crisis_logged |= 1u << step.ended_environment_id;
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
               threshold, threshold > 0.0 ? lowest / threshold : 0.0, 1,
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
    g.save_version = SAVE_VERSION;
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
    g_crisis_logged = 0;
    g_upgrade_buys = 0;
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
    int atp_mutations; /* v14 硬门槛 2：收 ATP 门票的突变张数（阶段 >= MUTATION_ATP_MIN_PHASE） */
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
    int upgrade_buys;
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
    memset(g_slot_cooldown, 0, sizeof(g_slot_cooldown));
    g_last_purchase_time = -1.0;
    g_sub_upgrade_buys = 0;
    g_insight_buys = 0;
    g_atp_spent = 0.0;
    g_atp_income = 0.0;
    g_last_stage = -1;
    g_upgrade_buys = 0;
    g_crisis_logged = 0;
    double atp_start = g.run.atp;
    for (int step = 1; step <= steps; ++step) {
        g_era = g.run.era;
        g_time = step * FIXED_STEP_SECONDS;
        simulate_step(FIXED_STEP_SECONDS);
        if (g_cap_reached_time < 0.0 && evolution_main_stage(&g) >= evolution_era_stage_cap(&g)) {
            g_cap_reached_time = g_time;
        }
    }
    /* v10 P0-26：纪元结束仍未通过的危机类型补一条 survived=0，
     * 保证“逐类型一条记录”口径下样本数 = 出现过该类型的局数。 */
    for (int id = 1; id < ENV_COUNT && id < 32; ++id) {
        if ((g.run.crisis_seen & (1u << id)) && !g.run.survived_crises[id] &&
            !(g_crisis_logged & (1u << id))) {
            g_crisis_logged |= 1u << id;
            printf("CRISIS,%d,%d,0,0,0,0,0,0,%d\n", g.run.era, id, g_batch_index);
        }
    }
    EraReward reward = prestige_preview_reward(&g);
    summary.ep = reward.total_points;
    prestige_run_stats(&g, &summary.stages, &summary.mutations, &summary.species,
                       &summary.crises);
    /* v14：把“收 ATP 门票的购买张数”单独记一份。规则 B 的等比数列
     * （10 s × 1.12^k）只对这批卡生效，硬门槛 2 必须按这个口径验收，
     * 否则低阶免费突变会把张数灌到三位数。 */
    summary.atp_mutations = 0;
    for (int slot = 0; slot < g.run.active_species_count; ++slot) {
        for (int id = 0; id < MUTATION_COUNT; ++id) {
            if (!g.run.active_species[slot].mutations[id]) continue;
            const MutationDef *def = mutation_get(id);
            if (def && def->min_phase >= MUTATION_ATP_MIN_PHASE) ++summary.atp_mutations;
        }
    }
    if (g_quiet) {
        int slot = evolution_main_slot(&g);
        if (slot >= 0) {
            SpeciesRates r = evolution_calculate_species_rates(&g, slot);
            printf("INC,%d,%d,%.4g,%.4g,%.4g,%.0f,%.0f,%.0f\n", g_batch_index, g.run.era,
                   r.atp_per_second, r.biomass_per_second, r.dna_per_second,
                   g.run.active_species[slot].population, r.final_population_cap,
                   r.atp_per_second * 600.0);
        }
    }
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
    summary.upgrade_buys = g_upgrade_buys;
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
        printf("UPGSUM,%d,1,%d\n", index, era1.upgrade_buys);
        printf("UPGSUM,%d,2,%d\n", index, era2.upgrade_buys);
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
        printf("BATCH3,%d,%u,%d,%d,%d,%d,%d,%d,%d,%.0f,%.1f,%d,%d,%.1f,%.1f,%d,%d,%d,%.1f,%.1f,%.1f,%d\n",
               index, seed,
               era1.ep, era2.ep, era3.ep, era3.stages, era3.mutations, era3.species,
               era3.crises, era3.max_biomass, era3.last_purchase, mutation_level,
               diversification_level, era3.first_speciation, era3.knowledge,
               era1.sub_upgrades, era2.sub_upgrades, era3.sub_upgrades,
               era1.first_mutation, era2.first_mutation, era3.first_mutation,
               era3.atp_mutations);
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
        printf("UPGSUM,%d,1,%d\n", index, era1.upgrade_buys);
        printf("UPGSUM,%d,2,%d\n", index, era2.upgrade_buys);
        printf("UPGSUM,%d,3,%d\n", index, era3.upgrade_buys);
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
