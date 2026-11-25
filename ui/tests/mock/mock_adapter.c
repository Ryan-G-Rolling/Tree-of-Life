/*
 * mock_adapter.c  ——  ui_adapter.h 的临时实现（相当于 A/B 的只读查询）
 *
 * 集成时替换为：
 *   double ua_run_upgrade_cost(...) { return economy_run_upgrade_cost(...); }
 *   const char *ua_species_name(int id) { return evolution_species_name(id); }
 *   ...
 * 每个函数一行，UI 代码完全不动。
 */
#include <stdio.h>
#include <string.h>

#include "mock_tables.h"
#include "ui_adapter.h"

/* ================= A 侧 ================= */

double ua_run_upgrade_cost(const GameState *game, RunUpgradeType type)
{
    return mock_run_upgrade_cost((int)type, game->run.run_upgrade_levels[type]);
}

const char *ua_run_upgrade_title(RunUpgradeType type)
{
    switch (type) {
    case UPGRADE_METABOLISM:   return "代谢效率";
    case UPGRADE_REPRODUCTION: return "繁殖能力";
    default:                   return "基因复制";
    }
}

const char *ua_run_upgrade_effect(const GameState *game, RunUpgradeType type, char *buf, int buf_size)
{
    int level = game->run.run_upgrade_levels[type];
    switch (type) {
    case UPGRADE_METABOLISM:
        snprintf(buf, (size_t)buf_size, "自动 ATP 与点击收益 ×%.2f", 1.0 + 0.25 * level);
        break;
    case UPGRADE_REPRODUCTION:
        snprintf(buf, (size_t)buf_size, "种群增长与生物量 ×%.2f", 1.0 + 0.20 * level);
        break;
    default:
        snprintf(buf, (size_t)buf_size, "DNA 产出 ×%.2f", 1.0 + 0.25 * level);
        break;
    }
    return buf;
}

void ua_total_rates(const GameState *game, double out[3])
{
    out[0] = out[1] = out[2] = 0.0;
    for (int i = 0; i < game->run.active_species_count; ++i) {
        SpeciesRates r;
        mock_calc_rates(game, i, &r);
        out[0] += r.atp_per_second;
        out[1] += r.biomass_per_second;
        out[2] += r.dna_per_second;
    }
}

double ua_click_atp(const GameState *game)
{
    return 1.0
         * (1.0 + 0.25 * game->run.run_upgrade_levels[UPGRADE_METABOLISM])
         * (game->legacy.levels[LEGACY_METABOLISM] >= 1 ? 1.08 : 1.0)
         * (game->legacy.levels[LEGACY_METABOLISM] >= 4 ? 1.12 : 1.0);
}

/* ================= B 侧：物种 ================= */

const char *ua_species_name(int species_id)
{
    if (species_id < 0 || species_id >= SPECIES_COUNT) return "未知物种";
    return MOCK_SPECIES[species_id].name;
}

const char *ua_ecology_name(int species_id)
{
    if (species_id < 0 || species_id >= SPECIES_COUNT) return "-";
    return MOCK_SPECIES[species_id].ecology;
}

const char *ua_stage_name(int stage)
{
    static const char *names[8] = {
        "原始生命", "原核生物", "真核生物", "多细胞生物",
        "水生动物", "陆生动物", "高级动物", "智慧生命"
    };
    if (stage < 0 || stage > 7) return "-";
    return names[stage];
}

int ua_species_stage(int species_id)
{
    if (species_id < 0 || species_id >= SPECIES_COUNT) return 0;
    return MOCK_SPECIES[species_id].stage;
}

void ua_species_rates(const GameState *game, int species_slot, SpeciesRates *out)
{
    mock_calc_rates(game, species_slot, out);
}

static const char *trait_name(int t)
{
    static const char *names[TRAIT_COUNT] = { "耐热", "耐寒", "微光", "耐氧", "耐毒" };
    if (t < 0 || t >= TRAIT_COUNT) return "-";
    return names[t];
}

const char *ua_fitness_breakdown(const GameState *game, int species_slot, char *buf, int buf_size)
{
    int env = game->run.current_environment_id;
    const MockEnvironment *e = &MOCK_ENVIRONMENTS[env];

    if (game->run.active_species_count <= species_slot) {
        snprintf(buf, (size_t)buf_size, "无活跃物种");
        return buf;
    }

    int score = 0, diff = 0;
    double mult = mock_species_fitness(game, species_slot, &score, &diff);

    if (env == 0) {
        snprintf(buf, (size_t)buf_size, "常态环境 · 无筛选");
        return buf;
    }

    if (e->trait < 0) {
        snprintf(buf, (size_t)buf_size, "最高特征 %d 对难度 %d → ×%.2f", score, diff, mult);
    } else {
        snprintf(buf, (size_t)buf_size, "%s %d 对难度 %d → ×%.2f",
                 trait_name(e->trait), score, diff, mult);
    }
    return buf;
}

/* ================= B 侧：分化 ================= */

int ua_speciation_children(const GameState *game, int *out, int max_out)
{
    if (game->run.active_species_count <= 0) return 0;
    int parent = game->run.active_species[0].species_id;
    int n = 0;
    for (int c = 0; c < SPECIES_COUNT && n < max_out; ++c) {
        if (mock_speciation_parent_ok(c, parent)) out[n++] = c;
    }
    return n;
}

int ua_species_parents(int species_id, int *out, int max_out)
{
    if (species_id < 0 || species_id >= SPECIES_COUNT || max_out <= 0) return 0;
    int n = 0;
    for (int p = 0; p < SPECIES_COUNT && n < max_out; ++p) {
        if (mock_speciation_parent_ok(species_id, p)) out[n++] = p;
    }
    return n;
}

double ua_speciation_cost(const GameState *game, int target_species_id)
{
    if (target_species_id < 0 || target_species_id >= SPECIES_COUNT) return 0.0;
    double cost = MOCK_SPECIES[target_species_id].speci_cost;
    if (game->legacy.levels[LEGACY_DIVERSIFICATION] >= 4) cost *= 0.85;
    return cost;
}

ActionCode ua_speciation_check(const GameState *game, int target_species_id)
{
    return mock_validate_speciation(game, target_species_id);
}

const char *ua_speciation_requirement(int target_species_id, char *buf, int buf_size)
{
    switch (target_species_id) {
    case SPECIES_CYANOBACTERIUM:       snprintf(buf, (size_t)buf_size, "需要：光合作用"); break;
    case SPECIES_CHEMOLITHOTROPH:      snprintf(buf, (size_t)buf_size, "需要：化学合成"); break;
    case SPECIES_ALGAL_EUKARYOTE:      snprintf(buf, (size_t)buf_size, "需要：细胞核 + 已有光合作用"); break;
    case SPECIES_PROTOZOAN:            snprintf(buf, (size_t)buf_size, "需要：细胞核 + 线粒体"); break;
    case SPECIES_MULTICELLULAR_COLONY: snprintf(buf, (size_t)buf_size, "需要：多细胞化"); break;
    case SPECIES_EXTREMOPHILE_COLONY:  snprintf(buf, (size_t)buf_size, "需要：耐热 + 耐毒，多样性 Lv.5"); break;
    default:                           snprintf(buf, (size_t)buf_size, "无可用分化"); break;
    }
    return buf;
}

/* ================= B 侧：突变 ================= */

const char *ua_mutation_name(int mutation_id)
{
    if (mutation_id < 0 || mutation_id >= MUTATION_COUNT) return "未知突变";
    return MOCK_MUTATIONS[mutation_id].name;
}

const char *ua_mutation_desc(int mutation_id, char *buf, int buf_size)
{
    if (mutation_id < 0 || mutation_id >= MUTATION_COUNT) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }
    snprintf(buf, (size_t)buf_size, "%s", MOCK_MUTATIONS[mutation_id].desc);
    return buf;
}

bool ua_mutation_is_rare(int mutation_id)
{
    if (mutation_id < 0 || mutation_id >= MUTATION_COUNT) return false;
    return MOCK_MUTATIONS[mutation_id].rare;
}

/* ================= B 侧：环境 ================= */

const char *ua_environment_name(int environment_id)
{
    if (environment_id < 0 || environment_id > 6) return "未知事件";
    return MOCK_ENVIRONMENTS[environment_id].name;
}

const char *ua_environment_desc(int environment_id, char *buf, int buf_size)
{
    if (environment_id < 0 || environment_id > 6) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }
    snprintf(buf, (size_t)buf_size, "%s", MOCK_ENVIRONMENTS[environment_id].desc);
    return buf;
}

int ua_environment_trait(int environment_id)
{
    if (environment_id < 0 || environment_id > 6) return -1;
    return MOCK_ENVIRONMENTS[environment_id].trait;
}

/* ================= B 侧：结算与永久强化 ================= */

void ua_era_breakdown(const GameState *game, int out[5])
{
    int e = 0, m = 0, s = 0, c = 0;
    if (game->run.active_species_count > 0) {
        e = MOCK_SPECIES[game->run.active_species[0].species_id].stage;
    }
    for (int i = 0; i < MUTATION_COUNT; ++i) if (game->run.discovered_mutations[i]) m++;
    for (int i = 1; i < SPECIES_COUNT; ++i) if (game->run.discovered_species[i]) s++;
    for (int i = 1; i < 7; ++i) if (game->run.survived_crises[i]) c++;
    if (c > 3) c = 3;
    out[0] = e;
    out[1] = m;
    out[2] = s;
    out[3] = c;
    out[4] = game->run.highest_biomass_milestone;
}

void ua_era_reward(const GameState *game, EraReward *out)
{
    int c[5];
    ua_era_breakdown(game, c);
    memset(out, 0, sizeof(*out));
    out->era_points = 2 * c[0];
    out->mutation_points = c[1] / 3;
    out->species_points = c[2] / 2;
    out->crisis_points = c[3];
    out->biomass_points = c[4];
    out->total_points = out->era_points + out->mutation_points + out->species_points
                      + out->crisis_points + out->biomass_points;
}

const char *ua_legacy_name(int branch)
{
    switch (branch) {
    case LEGACY_METABOLISM:     return "Metabolism 代谢";
    case LEGACY_MUTATION:       return "Mutation 突变";
    case LEGACY_ADAPTATION:     return "Adaptation 适应";
    case LEGACY_DIVERSIFICATION:return "Diversification 多样性";
    default:                    return "未知路线";
    }
}

int ua_legacy_cost(int branch, int current_level)
{
    (void)branch;
    if (current_level < 0) current_level = 0;
    if (current_level >= 5) return 0;
    return current_level + 1;
}

const char *ua_legacy_effect(const GameState *game, int branch, int level, char *buf, int buf_size)
{
    (void)game;
    static const char *table[4][5] = {
        { "基础 ATP 产出 ×1.08",
          "生物量产出 ×1.08",
          "新轮 +50 ATP，代谢升级从 Lv.1 起",
          "ATP / 生物量 / DNA 产出再 ×1.12",
          "解锁高效呼吸突变" },
        { "突变所需 DNA -6%",
          "突变总折扣 -12%",
          "每次突变 3 个候选",
          "稀有突变权重 20 → 40",
          "解锁适应性超突变" },
        { "环境负面差值减少 8%",
          "危机持续时间 ×0.85",
          "提前 20 秒预览下一事件",
          "正面环境收益 +15%",
          "每轮一次危机保护" },
        { "种群上限 ×1.10",
          "活跃物种槽 1 → 2",
          "不同生态标签协同 ×1.10",
          "分化成本 -15%",
          "解锁极端环境群落" },
    };
    if (branch < 0 || branch > 3 || level < 1 || level > 5) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }
    snprintf(buf, (size_t)buf_size, "%s", table[branch][level - 1]);
    return buf;
}

double ua_milestone_threshold(int level)
{
    if (level < 0) level = 0;
    if (level >= MOCK_MILESTONE_COUNT) return MOCK_MILESTONES[MOCK_MILESTONE_COUNT - 1];
    return MOCK_MILESTONES[level];
}

/* ================= 成就 ================= */

int ua_achievement_count(void) { return 3; }

const char *ua_achievement_name(int index)
{
    switch (index) {
    case 0: return "第一次分化";
    case 1: return "渡过危机";
    case 2: return "图鉴收集者";
    default: return "-";
    }
}

const char *ua_achievement_desc(int index)
{
    switch (index) {
    case 0: return "完成一次物种分化";
    case 1: return "在一次环境危机中存活";
    case 2: return "累计发现 4 个不同物种";
    default: return "";
    }
}

/* ================= 字体字符集 ================= */

void ua_register_font_strings(void)
{
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        ui_font_register(MOCK_SPECIES[i].name);
        ui_font_register(MOCK_SPECIES[i].ecology);
    }
    for (int i = 0; i < MUTATION_COUNT; ++i) {
        ui_font_register(MOCK_MUTATIONS[i].name);
        ui_font_register(MOCK_MUTATIONS[i].desc);
    }
    for (int i = 0; i < 7; ++i) {
        ui_font_register(MOCK_ENVIRONMENTS[i].name);
        ui_font_register(MOCK_ENVIRONMENTS[i].desc);
    }
    for (int i = 0; i < 4; ++i) {
        for (int l = 1; l <= 5; ++l) {
            char buf[128];
            ua_legacy_effect(NULL, i, l, buf, (int)sizeof(buf));
            ui_font_register(buf);
        }
    }
    for (int i = 0; i < ua_achievement_count(); ++i) {
        ui_font_register(ua_achievement_name(i));
        ui_font_register(ua_achievement_desc(i));
    }
}
