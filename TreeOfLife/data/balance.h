#ifndef BALANCE_H
#define BALANCE_H

/*
 * 唯一平衡数据源（01 §1、02 §1）
 *
 * 任何模块都不得在 .c 中再写一遍这里的数值或公式。
 * 调整平衡时只改本文件（或 02 文档中的对应表格），不改变机制与接口。
 */

/* ---------------- 02 §3 通用常量 ---------------- */
#define FIXED_STEP_SECONDS 0.1
#define BASE_CLICK_ATP 1.0
#define STARTING_ATP 0.0
#define STARTING_BIOMASS 0.0
/* 第十一轮：只作用于跨轮新纪元（start_new_run）。新游戏从 0 DNA 开始，
 * 首突变约 55 s；如需“开局即买”，改 demo 的 init_game 一处并重跑标定。 */
#ifndef LEGACY_STARTING_DNA
#define LEGACY_STARTING_DNA 20.0
#endif
#define STARTING_POPULATION 10.0
#define MIN_ACTIVE_POPULATION 1.0

/* 调参（第二轮）：危机更频繁、更短，提高危机种类覆盖与 Biomass 存活 */
#ifndef FIRST_EVENT_DELAY
#define FIRST_EVENT_DELAY 60.0
#endif
#ifndef EVENT_INTERVAL
#define EVENT_INTERVAL 30.0
#endif
#ifndef EVENT_DURATION_SCALE
#define EVENT_DURATION_SCALE 0.40
#endif
#define ADAPTATION_PREVIEW_SECONDS 20.0

#define DIVERSITY_SYNERGY_MULTIPLIER 1.10
#define DIVERSITY_SPECIATION_DISCOUNT 0.15

/* ---------------- 02 §4 单轮升级（Member A 使用） ---------------- */
#define RUN_UPGRADE_METABOLISM_BASE_COST 20.0
#ifndef RUN_UPGRADE_METABOLISM_COST_GROWTH
#define RUN_UPGRADE_METABOLISM_COST_GROWTH 1.25
#endif
#define RUN_UPGRADE_REPRODUCTION_BASE_COST 35.0
#ifndef RUN_UPGRADE_REPRODUCTION_COST_GROWTH
#define RUN_UPGRADE_REPRODUCTION_COST_GROWTH 1.30
#endif
#define RUN_UPGRADE_GENOME_BASE_COST 25.0
#ifndef RUN_UPGRADE_GENOME_COST_GROWTH
#define RUN_UPGRADE_GENOME_COST_GROWTH 1.30
#endif
/* v7 P0-4a：商店去硬顶；收益保留线性（乘算在 600 s 内阈值效应过强） */
#define RUN_UPGRADE_MAX_LEVEL (-1)

#define RUN_METABOLISM_BONUS_PER_LEVEL 0.25
#define RUN_REPRODUCTION_BONUS_PER_LEVEL 0.20
#define RUN_GENOME_BONUS_PER_LEVEL 0.25

/* ---------------- 02 §5 物种与分化 ---------------- */
#define SPECIATION_CHILD_SHARE_SOLO 0.70      /* 无第二槽：后代获得亲本 70% */
#define SPECIATION_CHILD_SHARE_KEEP 0.60      /* 保留亲本：后代 60% */
#define SPECIATION_PARENT_SHARE_KEEP 0.40     /* 保留亲本：亲本 40% */
#define SPECIATION_MIN_CHILD_POPULATION 10.0
#define SPECIATION_METEOR_MIN_STAGE 3

/* ---------------- 02 §6 突变 ---------------- */
#define MUTATION_WEIGHT_COMMON 100
#define MUTATION_WEIGHT_RARE 20
#define MUTATION_WEIGHT_RARE_BOOSTED 40
#define MUTATION_CHOICES_BASE 2
#define MUTATION_CHOICES_BONUS 3
#define MUTATION_CHOICES_LV6 4
#ifndef MUTATION_KEY_GUARANTEE_BIOMASS_RATIO
#define MUTATION_KEY_GUARANTEE_BIOMASS_RATIO 0.60
#endif
/* 第十轮：突变基价按当前阶段缩放（早便宜、晚贵），与 1.22^owned 叠加 */
#ifndef MUTATION_PHASE_COST_BASE
#define MUTATION_PHASE_COST_BASE 0.35
#endif
#ifndef MUTATION_PHASE_COST_SLOPE
#define MUTATION_PHASE_COST_SLOPE 0.12
#endif
/* 第十轮：被保底的关键突变享受 40% 折扣（保证“出牌即可支付”） */
#ifndef KEY_MUTATION_DISCOUNT
#define KEY_MUTATION_DISCOUNT 0.40
#endif
/* 第十二轮：尚未拥有任何突变时，首个突变 6 折，让开局 1 分钟内有决策 */
/* 第十五轮：0.40 → 0.15。前期升级过快，首突变只给 15% 折扣 */
#ifndef FIRST_MUTATION_DISCOUNT
/* 第十五轮：0.15。v7 P0-3：回到 0.40，首突变 47.3 s → 39.8 s */
#ifndef FIRST_MUTATION_DISCOUNT
#define FIRST_MUTATION_DISCOUNT 0.40
#endif
#endif
/* 第十轮：早期专属突变（光合/化能）在阶段 2 之后可补课，但价格 ×2 */
#ifndef LATE_MUTATION_COST_MULTIPLIER
#define LATE_MUTATION_COST_MULTIPLIER 2.0
#endif
/* 第十轮：阶段 0–1 的 DNA 产出 ×1.5，抬前段收入 */
#ifndef EARLY_DNA_BONUS
#define EARLY_DNA_BONUS 1.5
#endif
#ifndef EARLY_DNA_MAX_PHASE
#define EARLY_DNA_MAX_PHASE 1
#endif
#define LEGACY_MUTATION_DISCOUNT_LV1 0.06
#define LEGACY_MUTATION_DISCOUNT_LV2 0.12

/* ---------------- 02 §7 环境 ---------------- */
#ifndef CRISIS_PASS_POPULATION_RATIO
#define CRISIS_PASS_POPULATION_RATIO 0.70
#endif
/* 第十二轮：纪元 1 用更宽的通过线（0.65），纪元 2+ 用 0.70，
 * 避免“越后期越松”/“开局就把低尾打穿”。
 * 第十四轮：0.63 → 0.60，把纪元 1 的 6/6 危机存活率拉回 35% 带。 */
#ifndef CRISIS_PASS_RATIO_ERA1
#define CRISIS_PASS_RATIO_ERA1 0.625
#endif
/* v7：纪元 3 收紧到 0.85，让“三纪元全通”重新成为挑战而不是默认 */
#ifndef CRISIS_PASS_RATIO_ERA3
#define CRISIS_PASS_RATIO_ERA3 0.85
#endif
/* 第十一轮：危机判定改为“事件期间最低人口”；事件期产率再打折，
 * 一次性损失随阶段放大，应急储备按级减免。 */
#ifndef CRISIS_EVENT_PRODUCTION_SCALE
#define CRISIS_EVENT_PRODUCTION_SCALE 0.40
#endif
/* 第十一轮：危机期持续伤害倍率，用于把“必然通过”调成真实筛选 */
#ifndef CRISIS_DAMAGE_MULTIPLIER
#define CRISIS_DAMAGE_MULTIPLIER 2.0
#endif
#define INSTANT_LOSS_STAGE_SCALE 0.05
/* v8 P0-16：一次性人口损失只保留象征值，Meteor 的威慑交给持续减产 */
#ifndef INSTANT_LOSS_MAX
#define INSTANT_LOSS_MAX 0.45
#endif
/* Meteor 事件期间所有谱系产率 ×0.4（不再是一次性“要么必死要么白送”） */
#ifndef METEOR_PRODUCTION_SCALE
#define METEOR_PRODUCTION_SCALE 0.4
#endif
#ifndef CRISIS_DAMAGE_STAGE_SCALE
#define CRISIS_DAMAGE_STAGE_SCALE 0.25
#endif
#define CRISIS_SHIELD_POPULATION_RATIO 0.20
#define CRISIS_SHIELD_DAMAGE_REDUCTION 0.80
#define METEOR_INSTANT_LOSS_RATIO 0.35
#define ENVIRONMENT_DURATION_SCALE_LV2 0.85
#define ENVIRONMENT_NEGATIVE_DELTA_SCALE_LV1 0.92
#define ENVIRONMENT_POSITIVE_BONUS_SCALE_LV4 1.15

/* ---------------- 02 §9 EP 结算 ---------------- */
#define EP_PER_STAGE 2
#define EP_MUTATIONS_PER_POINT 3
#define EP_SPECIES_PER_POINT 2
#define EP_CRISIS_CAP 6

/* ---------------- 02 §10 永久强化 ---------------- */
#define LEGACY_MAX_LEVEL 8
#define LEGACY_ATP_MULTIPLIER_LV1 1.08
#define LEGACY_BIOMASS_MULTIPLIER_LV2 1.08
#define LEGACY_ALL_MULTIPLIER_LV4 1.12
#define LEGACY_CAP_MULTIPLIER_LV1 1.10
#define LEGACY_NEW_RUN_ATP_LV3 50.0
#define SPECIATION_MAX_TARGETS 2

/* 第四轮平衡：永久树 5 → 8 级，Lv.5–8 全部有实际效果 */
#define LEGACY_BIOMASS_MULTIPLIER_LV5 1.05
#define LEGACY_CLICK_MULTIPLIER_LV6 1.5
#define LEGACY_NEW_RUN_BIOMASS_LV7 100.0
#define LEGACY_PRODUCTION_MULTIPLIER_LV8 1.10
#define LEGACY_KEY_GUARANTEE_RATIO_LV5 0.40
#define LEGACY_MUTATION_DISCOUNT_LV7 0.20
#define LEGACY_RARE_WEIGHT_LV8 60
#define LEGACY_CRISIS_DAMAGE_LV6 0.15
#define LEGACY_CRISIS_DAMAGE_LV8 0.30
#define LEGACY_POSITIVE_DURATION_LV7 1.5
#define LEGACY_SHIELD_REDUCTION_LV8 0.95
#define LEGACY_CAP_MULTIPLIER_LV6 1.15
#define LEGACY_SYNERGY_MULTIPLIER_LV7 1.15
#define LEGACY_SPECIATION_DISCOUNT_LV8 0.10

/* ---------------- 可玩性扩展：突变共振 ---------------- */
#define SYNERGY_ALGAL_MAT_BIOMASS 0.10
#define SYNERGY_VENT_MAT_GROWTH 0.10
#define SYNERGY_EUKARYOTIC_ENGINE_ATP 0.15
#define SYNERGY_DETOX_TISSUE_CAP 0.05
#define SYNERGY_THERMOSTABLE_TRAIT 1

/* ---------------- 第四轮：新物种基石与新共振 ---------------- */
#define KEYSTONE_FUNGAL_DNA 0.10
#define KEYSTONE_FUNGAL_BIOMASS 0.08
#define KEYSTONE_CORAL_ATP 0.10
#define KEYSTONE_CORAL_CAP 0.10
#define SYNERGY_DECOMPOSER_BIOMASS 0.10
#define SYNERGY_DECOMPOSER_DNA 0.10
#define SYNERGY_CORAL_ATP 0.10
#define SYNERGY_CORAL_CAP 0.06

/* ---------------- 第六轮：两条阶段 5 侧支 ---------------- */
#define KEYSTONE_MYCORRHIZAL_DNA 0.10
#define KEYSTONE_MYCORRHIZAL_GROWTH 0.05
#define KEYSTONE_REEF_BIOMASS 0.10
#define KEYSTONE_REEF_CAP 0.10
#define SYNERGY_FOREST_BIOMASS 0.10
#define SYNERGY_FOREST_DNA 0.10
#define SYNERGY_REEF_ATP 0.10
#define SYNERGY_REEF_CAP 0.08

/* ---------------- 第七轮：节奏与经济 ---------------- */
#ifndef DNA_POPULATION_EXPONENT
#define DNA_POPULATION_EXPONENT 0.25
#endif
/* 第十四轮：1.47 → 1.45。再扩 20 个突变后候选池进一步变宽，
 * 必需链降一档，把纪元 2/3 与阶段 11 达成率拉回验收带。 */
#ifndef MUTATION_COST_GROWTH
#define MUTATION_COST_GROWTH 1.45
#endif
/* v8 P0-14：容量价格 = 0.5 × 未含容量加成的上限 × 1.08^级；配合 ×1.25 缓冲，
 * 让 ATP 在 600 s 内真的能换成容量，而不是永远攒不够第一级。 */
#ifndef CAPACITY_COST_PER_CAP
#define CAPACITY_COST_PER_CAP 0.5
#endif
/* v7 P0-4b：容量价格带“收入的 20 秒”下限，避免收入指数上涨后 ATP 无处可花 */
#define CAPACITY_ATP_SECONDS 5.0
#define CAPACITY_GAIN_PER_LEVEL 0.05
#define CAPACITY_COST_GROWTH 1.08
/* 第十轮：容量扩张改为无上限（-1 表示无限），Biomass 永远有出口 */
#ifndef CAPACITY_MAX_LEVEL
#define CAPACITY_MAX_LEVEL (-1)
#endif
/* 第十一轮：容量支付改为双币（前 3 级仍 100% Biomass），给 ATP 出口 */
/* v7 P0-4b：容量改为纯 ATP，生物量专心喂分化 */
#ifndef CAPACITY_ATP_SHARE
#define CAPACITY_ATP_SHARE 1.0
#endif
#ifndef CAPACITY_BIOMASS_SHARE
#define CAPACITY_BIOMASS_SHARE 0.0
#endif
#define CAPACITY_BIOMASS_ONLY_LEVELS 0
#define RESERVE_COST_ATP 1000.0
#define RESERVE_COST_GROWTH 1.6
/* v8 P0-15：储备减伤改为乘算，第 6 级之后仍有边际收益，约 22 级触底 */
#ifndef RESERVE_REDUCTION_STEP
#define RESERVE_REDUCTION_STEP 0.94
#endif
#ifndef RESERVE_REDUCTION_FLOOR
#define RESERVE_REDUCTION_FLOOR 0.20
#endif
#define RESERVE_MAX_LEVEL (-1)

/* 储备减伤系数：0.94^levels，最低 0.20。环境与产率两处共用同一实现。 */
static inline double reserve_damage_scale(int levels)
{
    double scale = 1.0;
    for (int i = 0; i < levels && scale > RESERVE_REDUCTION_FLOOR; ++i) {
        scale *= RESERVE_REDUCTION_STEP;
    }
    return scale < RESERVE_REDUCTION_FLOOR ? RESERVE_REDUCTION_FLOOR : scale;
}

/* v8 P0-17：三纪元的阶段深度上限（0 表示不限制），到顶后只能横向扩展 */
#ifndef ERA_STAGE_CAP_1
#define ERA_STAGE_CAP_1 8
#endif
#ifndef ERA_STAGE_CAP_2
#define ERA_STAGE_CAP_2 10
#endif
#ifndef ERA_STAGE_CAP_3
#define ERA_STAGE_CAP_3 13
#endif

/* 阶段 6/7 的基石与共振 */
#define KEYSTONE_SAPIENT_ATP 0.15
#define KEYSTONE_SAPIENT_GROWTH 0.05
#define KEYSTONE_TECHNO_PRODUCTION 0.15
#define KEYSTONE_TECHNO_CAP 0.15
#define SYNERGY_NOOSPHERE_ATP 0.10
#define SYNERGY_NOOSPHERE_DNA 0.10

/* 第八轮：阶段 8/9/10 的基石与共振 */
#define KEYSTONE_CIV_BIOMASS 0.15
#define KEYSTONE_CIV_CAP 0.15
#define KEYSTONE_SPACE_ATP 0.20
#define KEYSTONE_SPACE_DNA 0.15
#define KEYSTONE_SINGULARITY_PRODUCTION 0.25
#define KEYSTONE_SINGULARITY_CAP 0.25
#define SYNERGY_GLOBAL_NETWORK_BIOMASS 0.12
#define SYNERGY_GLOBAL_NETWORK_DNA 0.12
#define SYNERGY_DYSON_ATP 0.15
#define SYNERGY_DYSON_CAP 0.10

/* 第十二轮：阶段 11 的基石 */
#define KEYSTONE_TRANSCENDENCE_PRODUCTION 0.30
#define KEYSTONE_TRANSCENDENCE_CAP 0.30

/* 第十三轮：6 条侧支的基石与 3 条共振 */
#define KEYSTONE_MAT_ATP 0.12
#define KEYSTONE_MAT_LOW_LIGHT 0.15
#define KEYSTONE_VENT_BIOMASS 0.15
#define KEYSTONE_VENT_TOXIN 0.15
#define KEYSTONE_CHITIN_CAP 0.15
#define KEYSTONE_CHITIN_BIOMASS 0.10
#define KEYSTONE_SAPRO_DNA 0.20
#define KEYSTONE_SAPRO_BIOMASS 0.12
#define KEYSTONE_HIVE_PRODUCTION 0.10
#define KEYSTONE_HIVE_CAP 0.15
#define KEYSTONE_ORBITAL_ATP 0.30
#define KEYSTONE_ORBITAL_CAP 0.20

#define SYNERGY_PHOTONIC_ATP 0.12
#define SYNERGY_PHOTONIC_CAP 0.06
#define SYNERGY_VENT_BIOMASS 0.12
#define SYNERGY_VENT_GROWTH 0.05
#define SYNERGY_CHITIN_CAP 0.10
#define SYNERGY_CHITIN_GROWTH 0.05

/* ---------------- 第十四轮：20 个新物种的基石 ---------------- */
#define KEYSTONE_THERMO_ARCHAEA_BIOMASS 0.15
#define KEYSTONE_THERMO_ARCHAEA_DAMAGE 0.15
#define KEYSTONE_NITROPLAST_ATP 0.12
#define KEYSTONE_NITROPLAST_GROWTH 0.04
#define KEYSTONE_SLIME_MOLD_BIOMASS 0.12
#define KEYSTONE_SLIME_MOLD_DNA 0.10
#define KEYSTONE_RADIOLARIAN_CAP 0.15
#define KEYSTONE_RADIOLARIAN_DAMAGE 0.10
#define KEYSTONE_LICHEN_ATP 0.10
#define KEYSTONE_LICHEN_BIOMASS 0.12
#define KEYSTONE_TUBEWORM_BIOMASS 0.15
#define KEYSTONE_TUBEWORM_TOXIN 0.15
#define KEYSTONE_CORAL_SPIRE_ATP 0.12
#define KEYSTONE_CORAL_SPIRE_CAP 0.10
#define KEYSTONE_PRAIRIE_DNA 0.12
#define KEYSTONE_PRAIRIE_GROWTH 0.05
#define KEYSTONE_INSECT_GROWTH 0.12
#define KEYSTONE_INSECT_CAP 0.10
#define KEYSTONE_CETACEAN_ATP 0.10
#define KEYSTONE_CETACEAN_DNA 0.10
#define KEYSTONE_ARCHIVE_DNA 0.20
#define KEYSTONE_ARCHIVE_BIOMASS 0.10
#define KEYSTONE_FOUNDRY_ATP 0.20
#define KEYSTONE_FOUNDRY_CAP 0.15
#define KEYSTONE_ARCOLOGY_BIOMASS 0.15
#define KEYSTONE_ARCOLOGY_CAP 0.15
#define KEYSTONE_DYSON_ATP 0.30
#define KEYSTONE_DYSON_CAP 0.20
#define KEYSTONE_PROBE_DNA 0.20
#define KEYSTONE_PROBE_ATP 0.20
#define KEYSTONE_FORGE_ATP 0.30
#define KEYSTONE_FORGE_BIOMASS 0.20
#define KEYSTONE_QUANTUM_ATP 0.25
#define KEYSTONE_QUANTUM_GROWTH 0.08
#define KEYSTONE_MULTIVERSE_ATP 0.30
#define KEYSTONE_MULTIVERSE_CAP 0.25
#define KEYSTONE_NOETIC_PRODUCTION 0.30
#define KEYSTONE_NOETIC_CAP 0.25
#define KEYSTONE_COSMIC_PRODUCTION 0.40
#define KEYSTONE_COSMIC_CAP 0.35

/* 第十四轮：5 条新共振 */
#define SYNERGY_THERMO_BIOMASS 0.10
#define SYNERGY_NITROGEN_BIOMASS 0.12
#define SYNERGY_NITROGEN_DNA 0.08
#define SYNERGY_SILICA_CAP 0.10
#define SYNERGY_DEEP_SIGNAL_ATP 0.10
#define SYNERGY_DEEP_SIGNAL_DNA 0.10
#define SYNERGY_COSMIC_MIND_ATP 0.15
#define SYNERGY_COSMIC_MIND_CAP 0.10

/* ---------------- 第十五轮：进化商店分段提价与 ATP 出口 ---------------- */
/* 突变 DNA 基价按可购买阶段分段乘算：前期贵、后期相对温和 */
#ifndef MUTATION_COST_BAND_EARLY
#define MUTATION_COST_BAND_EARLY 1.80
#endif
#ifndef MUTATION_COST_BAND_MID
#define MUTATION_COST_BAND_MID 2.00
#endif
#ifndef MUTATION_COST_BAND_LATE
#define MUTATION_COST_BAND_LATE 2.50
#endif
#define MUTATION_COST_BAND_MID_PHASE 4
#define MUTATION_COST_BAND_LATE_PHASE 8
/* v7 P0-2/4d：阶段 8 起的高阶突变按“当前收入的 45 秒”计价，
 * 并有上下限，避免收入抽签与票价追不上收入。 */
#ifndef MUTATION_ATP_SECONDS
#define MUTATION_ATP_SECONDS 45.0
#endif
#ifndef MUTATION_ATP_COST_MIN
#define MUTATION_ATP_COST_MIN 10000000.0
#endif
#ifndef MUTATION_ATP_COST_MAX
#define MUTATION_ATP_COST_MAX 500000000.0
#endif
#define MUTATION_ATP_MIN_PHASE 8
/* 第十五轮：新增 140 条突变数量大，单条百分比按 0.35 计入产率，
 * 避免“买满后叠乘爆炸”把后期节奏再次压平。只作用于 ID ≥ 64 的新内容。 */
#ifndef ROUND15_MUTATION_BONUS_SCALE
#define ROUND15_MUTATION_BONUS_SCALE 0.35
#endif
#define ROUND15_MUTATION_FIRST_ID 64

/* ---------------- 第九轮：理念（Knowledge）与子升级 ---------------- */
/* 理念产率随种群规模开方增长（参照 DNA 的 0.25 次幂，放缓后期膨胀） */
#ifndef KNOWLEDGE_POPULATION_EXPONENT
#define KNOWLEDGE_POPULATION_EXPONENT 0.5
#endif
/* 子升级理念成本逐层增长（第十六轮：6.0 → 3.2，让中期真的买得起） */
#ifndef SUB_UPGRADE_COST_GROWTH
#define SUB_UPGRADE_COST_GROWTH 3.2
#endif
#define SUB_UPGRADE_MIN_PHASE 6

/* 第十六轮：洞见——无上限知识升级，每级 +2% 全产率，价格指数增长 */
#ifndef INSIGHT_BASE_COST
#define INSIGHT_BASE_COST 2000.0
#endif
#ifndef INSIGHT_COST_GROWTH
#define INSIGHT_COST_GROWTH 1.9
#endif
#define INSIGHT_BONUS_PER_LEVEL 0.02

/* ---------------- 第十六轮：随机增益因子 ---------------- */
/* 普通因子每 45–90 s 刷新一个；最多同时存在 1 个 */
#ifndef BUFF_SPAWN_MIN
#define BUFF_SPAWN_MIN 45.0
#endif
#ifndef BUFF_SPAWN_MAX
#define BUFF_SPAWN_MAX 90.0
#endif
/* 类型权重（总和 100）：稀有（强）10、点击 25、资源包 25、普通产率 40 */
#define BUFF_WEIGHT_RARE 10
#define BUFF_WEIGHT_CLICK 25
#define BUFF_WEIGHT_INSTANT 25
/* 强增益 ≥90 s 冷却，且开局 90 s 内不会出现（v8 P1-8：150 → 90） */
#ifndef BUFF_RARE_COOLDOWN
#define BUFF_RARE_COOLDOWN 90.0
#endif
#define BUFF_PRODUCTION_MULT 1.2
#define BUFF_PRODUCTION_DURATION 30.0
#define BUFF_STRONG_MULT 1.5
#define BUFF_STRONG_DURATION 15.0
#define BUFF_CLICK_MULT 2.0
#define BUFF_CLICK_DURATION 30.0
/* 一次性资源包：按当前每秒收入折算的秒数（v8 P1-8：ATP 60 → 5，避免后期白送） */
#define BUFF_INSTANT_ATP_SECONDS 5.0
#define BUFF_INSTANT_BIOMASS_SECONDS 30.0
#define BUFF_INSTANT_DNA_SECONDS 15.0
#define BUFF_INSTANT_KNOWLEDGE_SECONDS 15.0

/* ---------------- 可玩性扩展：物种基石 ---------------- */
#define KEYSTONE_PROTOCELL_GROWTH 0.10
#define KEYSTONE_CYANO_LOW_LIGHT 0.15
#define KEYSTONE_CHEMO_TOXIN 0.15
#define KEYSTONE_ALGAL_ATP 0.08
#define KEYSTONE_PROTOZOAN_DNA 0.10
#define KEYSTONE_MULTICELL_CAP 0.10
#define KEYSTONE_MULTICELL_GROWTH 0.05
#define KEYSTONE_EXTREMOPHILE_DAMAGE_REDUCTION 0.15
#define KEYSTONE_TISSUE_BIOMASS 0.12
#define KEYSTONE_TISSUE_CAP 0.08
#define KEYSTONE_SYMBIOSIS_ATP 0.10
#define KEYSTONE_SYMBIOSIS_DNA 0.08
#define KEYSTONE_BIOSPHERE_PRODUCTION 0.10
#define KEYSTONE_BIOSPHERE_CAP 0.15
#define KEYSTONE_BIOSPHERE_GROWTH 0.05

/* ---------------- 内容扩容调参杠杆（只作用于新增物种/突变） ---------------- */
#ifndef EXPANSION_SPECIATION_COST_SCALE
#define EXPANSION_SPECIATION_COST_SCALE 0.45
#endif
#ifndef EXPANSION_MUTATION_COST_SCALE
#define EXPANSION_MUTATION_COST_SCALE 0.70
#endif

/* ---------------- 可玩性扩展：生态韧性与图鉴里程碑 ---------------- */
#define RESILIENCE_PER_CRISIS 0.05
#define RESILIENCE_CAP 0.15
#define CODEX_MUTATIONS_PER_BONUS 3
#define CODEX_MUTATION_BONUS 0.01
#define CODEX_MUTATION_BONUS_CAP 0.06
#define CODEX_SPECIES_PER_BONUS 2
#define CODEX_SPECIES_BONUS 0.01
#define CODEX_SPECIES_BONUS_CAP 0.03

/* ---------------- 02 §11 随机数（A/B 共用的唯一实现） ---------------- */
static inline unsigned int tof_rng_next(unsigned int *state)
{
    unsigned int value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

#endif
