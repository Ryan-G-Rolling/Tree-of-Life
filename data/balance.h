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
#define RUN_UPGRADE_MAX_LEVEL 15

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
#ifndef FIRST_MUTATION_DISCOUNT
#define FIRST_MUTATION_DISCOUNT 0.40
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
 * 避免“越后期越松”/“开局就把低尾打穿”。 */
#ifndef CRISIS_PASS_RATIO_ERA1
#define CRISIS_PASS_RATIO_ERA1 0.63
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
#define INSTANT_LOSS_MAX 0.90
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
/* 第十一轮：1.34 → 1.48。可选突变已移出成本链，链内只剩必需突变，
 * 需要更陡的指数才能把纪元 1 压回阶段 5（见报告第 28 节网格）。 */
#ifndef MUTATION_COST_GROWTH
#define MUTATION_COST_GROWTH 1.48
#endif
#define CAPACITY_COST_PER_CAP 25.0
#define CAPACITY_GAIN_PER_LEVEL 0.05
#define CAPACITY_COST_GROWTH 1.12
/* 第十轮：容量扩张改为无上限（-1 表示无限），Biomass 永远有出口 */
#ifndef CAPACITY_MAX_LEVEL
#define CAPACITY_MAX_LEVEL (-1)
#endif
/* 第十一轮：容量支付改为双币（前 3 级仍 100% Biomass），给 ATP 出口 */
#ifndef CAPACITY_ATP_SHARE
#define CAPACITY_ATP_SHARE 0.70
#endif
#ifndef CAPACITY_BIOMASS_SHARE
#define CAPACITY_BIOMASS_SHARE 0.30
#endif
#define CAPACITY_BIOMASS_ONLY_LEVELS 3
#define RESERVE_COST_ATP 1000.0
#define RESERVE_REDUCTION_PER_LEVEL 0.10
#define RESERVE_MAX_LEVEL 5

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

/* ---------------- 第九轮：理念（Knowledge）与子升级 ---------------- */
/* 理念产率随种群规模开方增长（参照 DNA 的 0.25 次幂，放缓后期膨胀） */
#ifndef KNOWLEDGE_POPULATION_EXPONENT
#define KNOWLEDGE_POPULATION_EXPONENT 0.5
#endif
/* 子升级理念成本逐层 ×6（第十轮：配合逐层 ×1.35 与温和的理念曲线） */
#ifndef SUB_UPGRADE_COST_GROWTH
#define SUB_UPGRADE_COST_GROWTH 6.0
#endif
#define SUB_UPGRADE_MIN_PHASE 6

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

/* ---------------- 存档格式（Member A 维护） ----------------
 * 第十二轮状态：RunState 含 knowledge / capacity_levels / reserve_levels /
 * tech_levels，LegacyState 含 achievement_flags。结构一旦变化必须升版本，
 * save_read 会按键值与 sizeof(GameState) 双重校验。 */
#define SAVE_VERSION 6

#endif
