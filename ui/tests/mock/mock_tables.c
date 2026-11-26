/*
 * mock_tables.c  ——  临时平衡表（照 02 文档抄录，仅 UI 并行开发用）
 */
#include <math.h>
#include <stdio.h>

#include "mock_tables.h"

const MockSpecies MOCK_SPECIES[SPECIES_COUNT] = {
    { "原始细胞",     "原始",   0, 0.80, 0.25, 0.08, 0.010,  80.0,    0.0, { 0, 0,  0, 0, 0 } },
    { "光合细菌",     "光合",   1, 1.30, 0.38, 0.10, 0.012, 160.0,   80.0, { 1, 0, -1, 0, 0 } },
    { "化能细菌",     "化能",   1, 1.10, 0.48, 0.10, 0.011, 150.0,   80.0, { 1, 0,  2, 0, 1 } },
    { "藻类真核生物", "光合",   2, 2.00, 0.72, 0.16, 0.013, 260.0,  350.0, { 0, 0, -1, 1, 0 } },
    { "原生捕食者",   "异养",   2, 1.75, 0.68, 0.20, 0.014, 240.0,  350.0, { 0, 0,  0, 2, 0 } },
    { "简单多细胞群落", "多细胞", 3, 3.00, 1.20, 0.26, 0.012, 450.0, 1200.0, { 0, 1,  0, 1, 0 } },
    { "极端环境群落", "极端",   3, 2.50, 1.00, 0.28, 0.010, 380.0, 1000.0, { 2, 2,  1, 0, 2 } },
};

const MockMutation MOCK_MUTATIONS[MUTATION_COUNT] = {
    { "细胞膜",   "生物量 +10% · 耐毒 +1",            12.0, 0.00, 0.10, 0.00, 0.00, 0.00, { 0, 0, 0, 0, 1 }, false, -1, 9, -1, -1, 0 },
    { "光合作用", "ATP +25% · 微光耐受 -1",           18.0, 0.25, 0.00, 0.00, 0.00, 0.00, { 0, 0, -1, 0, 0 }, false, 0, 1, -1, -1, 0 },
    { "化学合成", "生物量 +20% · 微光耐受 +2",        18.0, 0.00, 0.20, 0.00, 0.00, 0.00, { 0, 0, 2, 0, 0 }, false, 0, 1, -1, -1, 0 },
    { "细胞壁",   "种群上限 +20% · 耐寒 +1 耐毒 +1",  22.0, 0.00, 0.00, 0.00, 0.20, 0.00, { 0, 1, 0, 0, 1 }, false, -1, 9, -1, -1, 0 },
    { "耐热性",   "种群增长 +5% · 耐热 +2",           28.0, 0.00, 0.00, 0.00, 0.00, 0.05, { 2, 0, 0, 0, 0 }, false, -1, 9, -1, -1, 0 },
    { "耐寒性",   "生物量 +5% · 耐寒 +2",             28.0, 0.00, 0.05, 0.00, 0.00, 0.00, { 0, 2, 0, 0, 0 }, false, -1, 9, -1, -1, 0 },
    { "细胞核",   "DNA +30% · 耐氧 +1",               45.0, 0.00, 0.00, 0.30, 0.00, 0.00, { 0, 0, 0, 1, 0 }, false, 1, 9, -1, -1, 0 },
    { "线粒体",   "ATP +30% · 耐氧 +2",               55.0, 0.30, 0.00, 0.00, 0.00, 0.00, { 0, 0, 0, 2, 0 }, false, -1, 9, MUTATION_NUCLEUS, -1, 0 },
    { "多细胞化", "上限 +40% · 生物量 +20% · 耐寒 +1", 90.0, 0.00, 0.20, 0.00, 0.40, 0.00, { 0, 1, 0, 0, 0 }, false, 2, 9, -1, -1, 0 },
    { "耐毒性",   "DNA +5% · 耐毒 +2",                50.0, 0.00, 0.00, 0.05, 0.00, 0.00, { 0, 0, 0, 0, 2 }, true,  -1, 9, -1, -1, 0 },
    { "高效呼吸", "ATP +35% · 耐氧 +1",               80.0, 0.35, 0.00, 0.00, 0.00, 0.00, { 0, 0, 0, 1, 0 }, true,  -1, 9, -1, LEGACY_METABOLISM, 5 },
    { "适应性超突变", "DNA +25% · 耐热 +1 耐寒 +1",   100.0, 0.00, 0.00, 0.25, 0.00, 0.00, { 1, 1, 0, 0, 0 }, false, -1, 9, -1, LEGACY_MUTATION, 5 },
};

const MockEnvironment MOCK_ENVIRONMENTS[7] = {
    { "平静海洋",   "环境倍率 1.00，无额外损失",         -1, 0, 0.0,  0.000 },
    { "全球变暖",   "高温筛选：耐热高的物种受益",         0, 2, 45.0, 0.014 },
    { "全球变冷",   "低温筛选：耐寒高的物种受益",         1, 2, 45.0, 0.014 },
    { "微光时期",   "光照骤降：光合路线产出明显降低",     2, 2, 50.0, 0.004 },
    { "氧气上升",   "有氧路线受益，厌氧路线承压",         3, 1, 45.0, 0.010 },
    { "毒潮",       "毒性爆发：耐毒不足的物种大量损失",    4, 2, 40.0, 0.013 },
    { "陨石冲击",   "开场即损失 35% 种群，随后持续衰减",   -1, 3, 30.0, 0.016 },
};

const double MOCK_MILESTONES[MOCK_MILESTONE_COUNT] = { 0.0, 100.0, 500.0, 2000.0, 10000.0, 50000.0 };

/* ---------------- 派生 ---------------- */

double mock_mutation_multiplier(const SpeciesState *sp, int which)
{
    double sum = 0.0;
    for (int i = 0; i < MUTATION_COUNT; ++i) {
        if (!sp->mutations[i]) continue;
        const MockMutation *m = &MOCK_MUTATIONS[i];
        switch (which) {
        case 0: sum += m->atp_pct; break;
        case 1: sum += m->biomass_pct; break;
        case 2: sum += m->dna_pct; break;
        case 3: sum += m->cap_pct; break;
        case 4: sum += m->growth_pct; break;
        default: break;
        }
    }
    return 1.0 + sum;
}

int mock_speciation_parent_ok(int child, int parent)
{
    switch (child) {
    case SPECIES_CYANOBACTERIUM:       return parent == SPECIES_PROTOCELL;
    case SPECIES_CHEMOLITHOTROPH:      return parent == SPECIES_PROTOCELL;
    case SPECIES_ALGAL_EUKARYOTE:      return parent == SPECIES_CYANOBACTERIUM;
    case SPECIES_PROTOZOAN:            return parent == SPECIES_CHEMOLITHOTROPH;
    case SPECIES_MULTICELLULAR_COLONY: return parent == SPECIES_ALGAL_EUKARYOTE || parent == SPECIES_PROTOZOAN;
    case SPECIES_EXTREMOPHILE_COLONY:  return parent == SPECIES_CHEMOLITHOTROPH || parent == SPECIES_PROTOZOAN;
    default: return 0;
    }
}

double mock_mutation_cost(const GameState *game, int mutation_id)
{
    int lv = game->legacy.levels[LEGACY_MUTATION];
    double discount = lv >= 2 ? 0.12 : (lv >= 1 ? 0.06 : 0.0);
    return ceil(MOCK_MUTATIONS[mutation_id].cost * (1.0 - discount));
}

bool mock_mutation_available(const GameState *game, int species_slot, int mutation_id)
{
    if (species_slot < 0 || species_slot >= game->run.active_species_count) return false;
    const SpeciesState *sp = &game->run.active_species[species_slot];
    if (sp->mutations[mutation_id]) return false;

    const MockMutation *m = &MOCK_MUTATIONS[mutation_id];
    int stage = MOCK_SPECIES[sp->species_id].stage;
    if (stage < m->req_stage) return false;
    if (stage > m->req_stage_max) return false;
    if (m->req_mutation >= 0 && !sp->mutations[m->req_mutation]) return false;
    if (m->req_legacy_branch >= 0 && game->legacy.levels[m->req_legacy_branch] < m->req_legacy_level) return false;

    /* 分化关键突变需要亲本阶段对得上 */
    if (mutation_id == MUTATION_MULTICELLULARITY && stage < 2) return false;
    return true;
}

/* 返回环境倍率；out_score/out_diff 用于解释文本 */
double mock_species_fitness(const GameState *game, int species_slot, int *out_score, int *out_diff)
{
    int env = game->run.current_environment_id;
    const MockEnvironment *e = &MOCK_ENVIRONMENTS[env];
    const SpeciesState *sp = &game->run.active_species[species_slot];

    int trait_index = e->trait;
    int score = 0;
    int base_score = 0;

    if (trait_index >= 0) {
        base_score = MOCK_SPECIES[sp->species_id].traits[trait_index];
        score = base_score;
        for (int i = 0; i < MUTATION_COUNT; ++i) {
            if (sp->mutations[i]) score += MOCK_MUTATIONS[i].traits[trait_index];
        }
    } else if (env == 6) {
        /* 陨石：取最高环境特征 */
        for (int t = 0; t < TRAIT_COUNT; ++t) {
            int v = MOCK_SPECIES[sp->species_id].traits[t];
            for (int i = 0; i < MUTATION_COUNT; ++i) {
                if (sp->mutations[i]) v += MOCK_MUTATIONS[i].traits[t];
            }
            if (v > score) score = v;
        }
        base_score = score;
    }

    int diff = e->difficulty;
    double mult = 1.0;
    if (trait_index < 0 && env == 0) {
        mult = 1.0;
    } else if (score >= diff + 1) {
        mult = 1.25;
    } else if (score == diff) {
        mult = 1.10;
    } else if (score == diff - 1) {
        mult = 0.85;
    } else {
        mult = 0.60;
    }

    int adapt = game->legacy.levels[LEGACY_ADAPTATION];
    if (adapt >= 1 && mult < 1.0) mult = 1.0 - (1.0 - mult) * 0.92;
    if (adapt >= 4 && mult > 1.0) mult = 1.0 + (mult - 1.0) * 1.15;

    if (out_score) *out_score = score;
    if (out_diff) *out_diff = diff;
    return mult;
}
