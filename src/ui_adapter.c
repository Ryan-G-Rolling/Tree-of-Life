/*
 * ui_adapter.c  ——  ui_adapter.h 的真实实现（集成版）
 *
 * 设计原则（01 §1、03 提案）：
 *   - 每个函数都是无副作用纯查询：不改 GameState，不消耗随机数。
 *   - 数值一律转发到 B 的规则函数或 A 的 economy 函数，绝不在这里重写公式。
 *   - 只有“界面文案”和“成就判定”这类 UI 语义留在本文件，因为 B 不提供文本接口。
 *
 * 与 mock_adapter.c 的差别：所有数据来自 A/B 真实实现，不再有假表。
 */
#include <stdio.h>
#include <string.h>

#include "ui_adapter.h"

#include "balance.h"
#include "economy.h"
#include "environment.h"
#include "evolution.h"
#include "legacy.h"
#include "codex.h"

/* ================= A 侧：经济与单轮升级 ================= */

double ua_insight_cost(const GameState *game)
{
    return evolution_insight_cost(game);
}

double ua_insight_multiplier(const GameState *game)
{
    return evolution_insight_multiplier(game);
}

double ua_insight_next_multiplier(const GameState *game)
{
    if (!game) return 1.0;
    /* 规则层只暴露"当前倍率"，下一级就是再叠一层 2% 的乘算 */
    return evolution_insight_multiplier(game) * (1.0 + INSIGHT_BONUS_PER_LEVEL);
}

double ua_knowledge_rate(const GameState *game)
{
    if (!game) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < game->run.active_species_count && i < MAX_ACTIVE_SPECIES; ++i) {
        SpeciesRates r = evolution_calculate_species_rates(game, i);
        sum += r.knowledge_per_second;
    }
    return sum;
}

double ua_buff_duration(int buff_type)
{
    /* 只转发 balance.h 的常量，倍率与刷新规则仍归 B 的 buffs.c */
    switch (buff_type) {
    case BUFF_PRODUCTION:        return BUFF_PRODUCTION_DURATION;
    case BUFF_PRODUCTION_STRONG: return BUFF_STRONG_DURATION;
    case BUFF_CLICK:             return BUFF_CLICK_DURATION;
    default:                     return 0.0;
    }
}

double ua_run_upgrade_cost(const GameState *game, RunUpgradeType type)
{
    if (!game) return 0.0;
    double cost = economy_upgrade_cost(type, game->run.run_upgrade_levels[type]);
    return cost; /* INFINITY 表示已满级，调用方用 isfinite 判断 */
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
    int level = game ? game->run.run_upgrade_levels[type] : 0;
    double m = economy_run_multiplier(type, level);
    switch (type) {
    case UPGRADE_METABOLISM:
        snprintf(buf, (size_t)buf_size, "自动 ATP 与点击收益 ×%.2f", m);
        break;
    case UPGRADE_REPRODUCTION:
        snprintf(buf, (size_t)buf_size, "种群增长与生物量 ×%.2f", m);
        break;
    default:
        snprintf(buf, (size_t)buf_size, "DNA 产出 ×%.2f", m);
        break;
    }
    return buf;
}

void ua_total_rates(const GameState *game, double out[3])
{
    out[0] = out[1] = out[2] = 0.0;
    if (!game) return;
    for (int i = 0; i < game->run.active_species_count; ++i) {
        SpeciesRates r = evolution_calculate_species_rates(game, i);
        out[0] += r.atp_per_second;
        out[1] += r.biomass_per_second;
        out[2] += r.dna_per_second;
    }
}

double ua_click_atp(const GameState *game)
{
    return economy_click_atp(game);
}

/* ================= B 侧：物种 ================= */

const char *ua_species_name(int species_id)
{
    const CodexEntry *entry = codex_entry(CODEX_SPECIES, species_id);
    return entry ? entry->name : "未知物种";
}

const char *ua_ecology_name(int species_id)
{
    switch (species_ecology(species_id)) {
    case ECO_PRIMITIVE:      return "原始";
    case ECO_PHOTOSYNTHETIC: return "光合";
    case ECO_CHEMOSYNTHETIC: return "化能";
    case ECO_HETEROTROPHIC:  return "异养";
    case ECO_MULTICELLULAR:  return "多细胞";
    case ECO_EXTREMOPHILE:   return "极端环境";
    default:                 return "未知";
    }
}

const char *ua_stage_name(int stage)
{
    /* B 的内容到第 11 阶（超越）。名称是界面文案，与 species_phase 序号对应。 */
    static const char *names[12] = {
        "原始生命", "原核生物", "真核生物", "多细胞生物",
        "组织分化", "共生超级个体", "认知生物圈", "智慧生命",
        "技术圈", "文明", "太空时代", "超越"
    };
    if (stage < 0 || stage > 11) return "未知阶段";
    return names[stage];
}

int ua_species_stage(int species_id)
{
    return species_phase(species_id);
}

void ua_species_rates(const GameState *game, int species_slot, SpeciesRates *out)
{
    if (!out) return;
    if (!game) {
        memset(out, 0, sizeof *out);
        return;
    }
    *out = evolution_calculate_species_rates(game, species_slot);
}

static const char *trait_name(int t)
{
    static const char *names[TRAIT_COUNT] = { "耐热", "耐寒", "微光", "耐氧", "耐毒" };
    if (t < 0 || t >= TRAIT_COUNT) return "-";
    return names[t];
}

const char *ua_fitness_breakdown(const GameState *game, int species_slot, char *buf, int buf_size)
{
    if (!game || species_slot < 0 || species_slot >= game->run.active_species_count) {
        snprintf(buf, (size_t)buf_size, "无活跃物种");
        return buf;
    }
    int env = game->run.current_environment_id;
    if (env <= 0) {
        snprintf(buf, (size_t)buf_size, "常态环境 · 无筛选");
        return buf;
    }
    int trait = environment_damage_trait(env);
    int score = trait >= 0 ? evolution_slot_trait_total(game, species_slot, (EnvironmentTrait)trait)
                           : 0;
    int diff = environment_difficulty(env);
    double mult = environment_multiplier_for_slot(game, species_slot);
    if (trait < 0) {
        snprintf(buf, (size_t)buf_size, "无对应特征 · 难度 %d → ×%.2f", diff, mult);
    } else {
        snprintf(buf, (size_t)buf_size, "%s %d 对难度 %d → ×%.2f",
                 trait_name(trait), score, diff, mult);
    }
    return buf;
}

/* ================= B 侧：分化 ================= */

int ua_speciation_children(const GameState *game, int *out, int max_out)
{
    if (!game || !out || max_out <= 0) return 0;
    if (game->run.active_species_count <= 0) return 0;
    int parent = game->run.active_species[0].species_id;
    int total = species_children(parent, out, max_out);
    return total < max_out ? total : max_out;
}

int ua_species_parents(int species_id, int *out, int max_out)
{
    if (!out || max_out <= 0 || species_id < 0 || species_id >= SPECIES_COUNT) return 0;
    int n = 0;
    for (int p = 0; p < SPECIES_COUNT && n < max_out; ++p) {
        if (species_has_parent(species_id, p)) out[n++] = p;
    }
    return n;
}

int ua_max_species_stage(void)
{
    int mx = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        int st = species_phase(i);
        if (st > mx) mx = st;
    }
    return mx;
}

int ua_species_child_list(int parent_id, int *out, int max_out)
{
    if (!out || max_out <= 0) return 0;
    return species_children(parent_id, out, max_out);
}

double ua_speciation_cost(const GameState *game, int target_species_id)
{
    if (!game) return 0.0;
    return evolution_speciation_cost(game, target_species_id);
}

ActionCode ua_speciation_check(const GameState *game, int target_species_id)
{
    if (!game) return ACTION_INVALID_TARGET;
    int slot = evolution_main_slot(game);
    if (slot < 0) return ACTION_INVALID_TARGET;
    return evolution_validate_speciation(game, slot, target_species_id);
}

const char *ua_speciation_requirement(int target_species_id, char *buf, int buf_size)
{
    if (target_species_id < 0 || target_species_id >= SPECIES_COUNT) {
        snprintf(buf, (size_t)buf_size, "无可用分化");
        return buf;
    }

    size_t used = 0;
    buf[0] = '\0';
    used += (size_t)snprintf(buf + used, (size_t)(buf_size - (int)used), "需要：");

    int need = 0;
    int muts[TRAIT_COUNT + 8];
    int n = species_required_mutations(target_species_id, muts, (int)(sizeof muts / sizeof muts[0]));
    for (int i = 0; i < n; ++i) {
        /* 用图鉴里的中文名：B 的规则表是英文名，直接显示会在一句中文里夹一个英文词 */
        const CodexEntry *entry = codex_entry(CODEX_MUTATION, muts[i]);
        const MutationDef *def = mutation_get(muts[i]);
        const char *name = entry ? entry->name : (def ? def->name : "未知突变");
        used += (size_t)snprintf(buf + used, (size_t)(buf_size - (int)used), "%s%s",
                                 need++ > 0 ? " + " : "", name);
    }

    int branch = -1, level = 0;
    if (species_requires_legacy(target_species_id, &branch, &level)) {
        static const char *branch_name[4] = { "代谢", "突变", "适应", "多样性" };
        const char *bn = (branch >= 0 && branch < 4) ? branch_name[branch] : "永久";
        used += (size_t)snprintf(buf + used, (size_t)(buf_size - (int)used), "%s%s Lv.%d",
                                 need++ > 0 ? " + " : "", bn, level);
    }

    if (need == 0) {
        snprintf(buf, (size_t)buf_size, "无前置条件");
    }
    return buf;
}

/* ================= B 侧：突变 ================= */

const char *ua_mutation_name(int mutation_id)
{
    const CodexEntry *entry = codex_entry(CODEX_MUTATION, mutation_id);
    return entry ? entry->name : "未知突变";
}

/* 追加一小段文案；snprintf 返回的是"本该写多长"，必须夹回缓冲区真实长度。 */
static void text_append(char *buf, int size, size_t *used, const char *piece)
{
    if (!buf || size <= 1 || *used >= (size_t)size - 1) return;
    int n = snprintf(buf + *used, (size_t)size - *used, "%s", piece);
    if (n <= 0) return;
    *used += (size_t)n;
    if (*used > (size_t)size - 1) *used = (size_t)size - 1;
}

const char *ua_mutation_desc(int mutation_id, char *buf, int buf_size)
{
    const MutationDef *def = mutation_get(mutation_id);
    if (!buf || buf_size <= 0) return buf;
    buf[0] = '\0';
    if (!def) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }

    /* 规则表的 description 是英文（"A stage-0 adaptation that improves light capture."），
     * 而且同一阶段的多条突变共用一句模板，既不是中文也没有区分度。
     * 这里直接按 MutationDef 的真实数值生成中文摘要：玩家看到的就是这一条到底加什么。 */
    size_t used = 0;
    char piece[64];
    snprintf(piece, sizeof(piece), "阶段 %d", def->min_phase);
    text_append(buf, buf_size, &used, piece);

    int shown = 0;
    const struct { double v; const char *label; } gains[5] = {
        { def->atp_bonus, "ATP" },
        { def->biomass_bonus, "生物量" },
        { def->dna_bonus, "DNA" },
        { def->cap_bonus, "上限" },
        { def->growth_bonus, "增速" },
    };
    for (int i = 0; i < 5; ++i) {
        if (gains[i].v < 0.0005) continue;
        snprintf(piece, sizeof(piece), " · %s +%d%%", gains[i].label,
                 (int)(gains[i].v * 100.0 + 0.5));
        text_append(buf, buf_size, &used, piece);
        ++shown;
    }

    /* 抗性加的是"环境等级"，不是百分比 */
    static const char *trait_zh[TRAIT_COUNT] = { "耐热", "耐寒", "耐暗", "耐氧", "耐毒" };
    for (int t = 0; t < TRAIT_COUNT; ++t) {
        if (def->trait_bonus[t] == 0) continue;
        snprintf(piece, sizeof(piece), " · %s%+d", trait_zh[t], def->trait_bonus[t]);
        text_append(buf, buf_size, &used, piece);
        ++shown;
    }

    double kb = evolution_mutation_knowledge_bonus(mutation_id);
    if (kb > 0.0005) {
        snprintf(piece, sizeof(piece), " · 理念 +%d%%", (int)(kb * 100.0 + 0.5));
        text_append(buf, buf_size, &used, piece);
        ++shown;
    }

    if (shown == 0) text_append(buf, buf_size, &used, " · 无产出加成");
    return buf;
}

bool ua_mutation_is_rare(int mutation_id)
{
    const MutationDef *def = mutation_get(mutation_id);
    return def && def->rarity >= 1;
}

int ua_synergy_count(void)
{
    return evolution_synergy_count();
}

const char *ua_synergy_name(int index)
{
    const MutationSynergyDef *def = evolution_synergy_get(index);
    return def ? def->name : "未知共振";
}

const char *ua_synergy_desc(int index, char *buf, int buf_size)
{
    const MutationSynergyDef *def = evolution_synergy_get(index);
    if (!def) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }
    snprintf(buf, (size_t)buf_size, "%s", def->description);
    return buf;
}

void ua_synergy_pair(int index, int out[2])
{
    if (!out) return;
    const MutationSynergyDef *def = evolution_synergy_get(index);
    out[0] = def ? def->mutation_a : -1;
    out[1] = def ? def->mutation_b : -1;
}

bool ua_synergy_active(const GameState *game, int species_slot, int index)
{
    return evolution_synergy_active(game, species_slot, index);
}

/* ================= B 侧：环境 ================= */

const char *ua_environment_name(int environment_id)
{
    return environment_name(environment_id);
}

const char *ua_environment_desc(int environment_id, char *buf, int buf_size)
{
    const char *desc = environment_description(environment_id);
    snprintf(buf, (size_t)buf_size, "%s", desc ? desc : "-");
    return buf;
}

int ua_environment_trait(int environment_id)
{
    return environment_damage_trait(environment_id);
}

bool ua_environment_is_crisis(int environment_id)
{
    return environment_is_crisis(environment_id);
}

void ua_environment_timer(const GameState *game, double *remaining, double *total, bool *in_event)
{
    double rem = 0.0;
    double tot = 0.0;
    bool event = false;

    if (game) {
        int env = game->run.current_environment_id;
        if (env > ENV_NORMAL_OCEAN && env < ENV_COUNT) {
            /* 事件期：倒数是事件剩余；总量按基础时长 × 事件缩放。
               Adaptation 等级会再缩短时长，环的起点因此可能不满格 —— 这是真实值，
               不要为了"从满格开始"去猜。 */
            event = true;
            rem = game->run.environment_time_left;
            tot = environment_base_duration(env) * EVENT_DURATION_SCALE;
        } else {
            /* 常态海洋：倒数是距下一次事件。首轮用 FIRST_EVENT_DELAY，之后是
               EVENT_INTERVAL，两者相差一倍，靠剩余值是否超过常规间隔区分。 */
            rem = game->run.event_interval_left;
            tot = (rem > EVENT_INTERVAL) ? FIRST_EVENT_DELAY : EVENT_INTERVAL;
        }
    }

    if (rem < 0.0) rem = 0.0;
    if (tot < rem) tot = rem;
    if (tot <= 0.0) tot = 1.0;

    if (remaining) *remaining = rem;
    if (total) *total = tot;
    if (in_event) *in_event = event;
}

/* ================= B 侧：结算与永久强化 ================= */

void ua_era_breakdown(const GameState *game, int out[5])
{
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    if (!game) return;
    int stages = 0, mutations = 0, species = 0, crises = 0;
    prestige_run_stats(game, &stages, &mutations, &species, &crises);
    out[0] = stages;
    out[1] = mutations;
    out[2] = species;
    out[3] = crises;
    out[4] = prestige_biomass_milestone_level(game->run.max_biomass);
}

void ua_era_reward(const GameState *game, EraReward *out)
{
    if (!out) return;
    if (!game) {
        memset(out, 0, sizeof *out);
        return;
    }
    *out = prestige_preview_reward(game);
}

double ua_milestone_threshold(int level)
{
    return prestige_milestone_threshold(level);
}

int ua_milestone_level(const GameState *game)
{
    if (!game) return 0;
    return prestige_biomass_milestone_level(game->run.max_biomass);
}

int ua_milestone_count(void)
{
    return prestige_milestone_count();
}

const char *ua_legacy_name(int branch)
{
    switch (branch) {
    case LEGACY_METABOLISM:      return "Metabolism 代谢";
    case LEGACY_MUTATION:        return "Mutation 突变";
    case LEGACY_ADAPTATION:      return "Adaptation 适应";
    case LEGACY_DIVERSIFICATION: return "Diversification 多样性";
    default:                     return "未知路线";
    }
}

int ua_legacy_cost(int branch, int current_level)
{
    (void)branch;
    return legacy_upgrade_cost(current_level);
}

int ua_legacy_max_level(void)
{
    return LEGACY_MAX_LEVEL;
}

/* 永久树 8 级的效果文案。数值与 data/balance.h 的 LEGACY_* 常量一一对应；
 * 改平衡时只改 balance.h，文案在这里同步。 */
const char *ua_legacy_effect(const GameState *game, int branch, int level, char *buf, int buf_size)
{
    (void)game;
    static const char *table[4][8] = {
        { "基础 ATP 产出 ×1.08",
          "Biomass 产出 ×1.08",
          "新轮 +50 ATP，代谢升级从 Lv.1 起",
          "ATP / Biomass / DNA 产出再 ×1.12",
          "Biomass 产出再 ×1.05",
          "点击收益 ×1.5",
          "新轮 +100 Biomass",
          "全部产率 ×1.10" },
        { "突变所需 DNA -6%",
          "突变总折扣 -12%",
          "每次突变 3 个候选",
          "稀有突变权重提升",
          "关键突变保底 Biomass 40%",
          "—",
          "突变总折扣 -20%",
          "稀有突变权重 60" },
        { "环境负面差值减少 8%",
          "危机持续时间 ×0.85",
          "提前 20 秒预览下一事件",
          "正面环境收益 +15%",
          "每轮一次危机保护",
          "危机伤害 -15%",
          "正面事件持续 ×1.5",
          "危机伤害 -30%，护盾减伤 95%" },
        { "种群上限 ×1.10",
          "活跃物种槽 1 → 2",
          "不同生态标签协同",
          "分化成本 -15%",
          "活跃物种槽 2 → 3",
          "种群上限 ×1.15",
          "协同加成 ×1.15",
          "分化成本再 -10%" },
    };
    if (branch < 0 || branch > 3 || level < 1 || level > 8) {
        snprintf(buf, (size_t)buf_size, "-");
        return buf;
    }
    snprintf(buf, (size_t)buf_size, "%s", table[branch][level - 1]);
    return buf;
}

/* ================= B 侧：成就 ================= */

int ua_achievement_count(void)
{
    return 3;
}

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

/* 成就判定从玩家状态实时派生：B 不写 achievement_flags，UI 也不写回 GameState。
 * 这样成就页在真实存档上立刻有内容，不依赖后续 B 补字段。 */
bool ua_achievement_unlocked(const GameState *game, int index)
{
    if (!game) return false;
    switch (index) {
    case 0: {
        for (int id = 1; id < SPECIES_COUNT; ++id)
            if (game->run.discovered_species[id] || game->legacy.codex_species[id]) return true;
        return false;
    }
    case 1: {
        for (int id = 1; id < ENV_COUNT && id < MAX_ENVIRONMENTS; ++id)
            if (game->run.survived_crises[id]) return true;
        return false;
    }
    case 2: {
        int count = 0;
        for (int id = 0; id < SPECIES_COUNT; ++id)
            if (game->run.discovered_species[id] || game->legacy.codex_species[id]) ++count;
        return count >= 4;
    }
    default:
        return false;
    }
}

/* ================= 字体字符集 ================= */

void ua_register_font_strings(void)
{
    char buf[192];
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        ui_font_register(species_name(i));
        ui_font_register(species_description(i));
        ui_font_register(ua_ecology_name(i));
    }
    for (int i = 0; i < MUTATION_COUNT; ++i) {
        const MutationDef *def = mutation_get(i);
        if (def) {
            ui_font_register(def->name);
            ui_font_register(def->description);
        }
    }
    for (int i = 0; i < ENV_COUNT; ++i) {
        ui_font_register(environment_name(i));
        ui_font_register(environment_description(i));
    }
    for (int stage = 0; stage <= 11; ++stage) ui_font_register(ua_stage_name(stage));
    for (int i = 0; i < 4; ++i) {
        ui_font_register(ua_legacy_name(i));
        for (int l = 1; l <= 8; ++l) {
            ua_legacy_effect(NULL, i, l, buf, (int)sizeof(buf));
            ui_font_register(buf);
        }
    }
    for (int i = 0; i < ua_achievement_count(); ++i) {
        ui_font_register(ua_achievement_name(i));
        ui_font_register(ua_achievement_desc(i));
    }
}

