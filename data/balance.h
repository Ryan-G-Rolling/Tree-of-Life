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
/* v10 P0-22：四张价格表整体换成“时间本位”——价格 = N × 主谱系产率。
 * 产率涨多少价格涨多少，节奏恒定；BASE 保留为常数地板（开局产率≈0 时兜底）。
 * 旧指数价格（20 × 1.25^L）与产率脱钩：早期 5 s 一级、点火后被甩开两个量级。 */
#define RUN_UPGRADE_METABOLISM_BASE_COST 20.0
#define RUN_UPGRADE_REPRODUCTION_BASE_COST 35.0
#define RUN_UPGRADE_GENOME_BASE_COST 25.0
/* 每级耗时（秒，按被动产率计）：代谢 60 / 繁殖 70 / 基因组 45。
 * 验收策略每秒点击 1 次、每次 3 秒产率，ATP 实际收入≈被动产率的 4 倍，
 * 换算成墙钟约 15–40 s；N 若继续加大，钱包增速（≈1/增长率）追不上价格，
 * 中后期会彻底停止升级（N=150 时实测每纪元只买 8 级）。
 * 生物量没有点击收入，基因组直接按 45 s 计。 */
#define UPGRADE_ATP_SECONDS_METABOLISM 84.0
#define UPGRADE_ATP_SECONDS_REPRODUCTION 94.0
#define UPGRADE_BIOMASS_SECONDS_GENOME 60.0
/* v7 P0-4a：商店去硬顶 */
#define RUN_UPGRADE_MAX_LEVEL (-1)

/* v10 P0-23：线性加成收一次平方根——87 级 ×22 变成 ×3.3，等级数字回到 18–26。
 * 保留总倍率显示，砍掉的是“两分钟一个量级”的失控。 */
#define RUN_METABOLISM_BONUS_PER_LEVEL 0.25
#define RUN_REPRODUCTION_BONUS_PER_LEVEL 0.20
#define RUN_GENOME_BONUS_PER_LEVEL 0.25
/* v10 P0-23 配套：繁殖倍率从线性 ×4.2 降到 sqrt ×2.8（16 级时），
 * 种群复合速度直接决定 DNA / 生物量收入。用全局生长系数把复合速度补回来，
 * 否则全阶段节奏塌到 stage 3（60 种子实测）。 */
#ifndef POP_GROWTH_SCALE
#define POP_GROWTH_SCALE 3.0
#endif
/* v10 P0-23 配套：DNA 收入 ×2。突变 DNA 价（1.45^链长）不动，但 sqrt 后
 * 基因组倍率从 ×4.2 降到 ×2.3，DNA 收入随之下滑，连锁突变变成全局瓶颈。 */
#ifndef DNA_INCOME_SCALE
#define DNA_INCOME_SCALE 2.0
#endif
/* v10 P0-22 收口：ATP / 生物量 / 理念的“每阶段收入压缩”。
 * 时间本位定价下价格随收入同步缩放，因此压缩收入不改变节奏，只把纪元 3 的
 * 支出量级从 3.5e10 拉回 1e9 以内（验收项）。DNA 不压缩——突变是固定 DNA 价，
 * 压缩 DNA 会直接把阶段推进卡死。 */
#ifndef DNA_RAMP_SECONDS
#define DNA_RAMP_SECONDS 120.0
#endif
#ifndef DNA_RAMP_START
#define DNA_RAMP_START 0.33
#endif
#ifndef PHASE_RATE_COMPRESSION
#define PHASE_RATE_COMPRESSION 0.635
#endif

/* ---------------- 02 §5 物种与分化 ---------------- */
#define SPECIATION_CHILD_SHARE_SOLO 0.70      /* 无第二槽：后代获得亲本 70% */
#define SPECIATION_CHILD_SHARE_KEEP 0.60      /* 保留亲本：后代 60% */
#define SPECIATION_PARENT_SHARE_KEEP 0.40     /* 保留亲本：亲本 40% */
#define SPECIATION_MIN_CHILD_POPULATION 10.0
#define SPECIATION_METEOR_MIN_STAGE 3
/* v10 P0-22：分化门票改时间本位——N × 1.08^阶段 秒的主谱系生物量产率。
 * 校准：13 张票的墙钟成本 Σ14×1.08^p ≈ 266 s，600 s 纪元里装得下 12–13 次分化；
 * 阶段 13 再 ×1.6 作为真正的 hard wall（P0-24）。base_speciation_cost 保留为地板。 */
#ifndef SPECIATION_TICKET_SECONDS
#define SPECIATION_TICKET_SECONDS 20.0
#endif
#ifndef SPECIATION_TICKET_PHASE_GROWTH
#define SPECIATION_TICKET_PHASE_GROWTH 1.06
#endif
#ifndef SPECIATION_EARLY_FLOOR_SCALE
#define SPECIATION_EARLY_FLOOR_SCALE 6.3
#endif
#ifndef SPECIATION_TICKET_LATE_GROWTH
#define SPECIATION_TICKET_LATE_GROWTH 1.20
#endif
#ifndef SPECIATION_HARD_WALL_PHASE
#define SPECIATION_HARD_WALL_PHASE 13
#endif
#ifndef SPECIATION_HARD_WALL_MULT
#define SPECIATION_HARD_WALL_MULT 1.12
#endif
/* v10 收口：阶段 12 单独加一道溢价。阶段 13 的门票已经由 hard wall 卡住，
 * 但 11→12 此前对所有人都是“按时到达”（100% 抵达），验收要求 55–80% 的抵达率，
 * 需要让阶段 12 本身成为一次真正的取舍：多付 1.35 倍门票，慢下来的局就停在 11。 */
#ifndef SPECIATION_PHASE12_MULT
#define SPECIATION_PHASE12_MULT 1.25
#endif
/* v10 P0-22 配套：常数地板整体 ×0.45。地板原按“线性收入的旧曲线”定的
 * （40/350/1200/1350…），sqrt 之后收入复合慢了一半，中段每张票要等 100–240 s，
 * 60 种子实测卡在 stage 4。缩地板恢复“每张票 15–60 s”的节奏（0.45 → 0.25）。 */
#ifndef SPECIATION_FLOOR_SCALE
#define SPECIATION_FLOOR_SCALE 1.4
#endif
/* v10 P1-11：阶段 6 起主谱系分化提供“2 选 1”——主链节点或同阶段支线物种 */
#ifndef BRANCH_SPECIATION_MIN_PHASE
#define BRANCH_SPECIATION_MIN_PHASE 6
#endif
/* v10：点击收益 = 3 秒主谱系产率（BASE_CLICK_ATP 保留为地板的一部分见 economy.c） */
#ifndef CLICK_INCOME_SECONDS
#define CLICK_INCOME_SECONDS 3.0
#endif

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
/* 第十轮：阶段 0–1 的 DNA 产出 ×1.5，抬前段收入。
 * v10 收口：1.5 → 1.40，把首突变从 34.5 s 拉进 35–60 s 验收带（只影响阶段 0–1）。 */
#ifndef EARLY_DNA_BONUS
#define EARLY_DNA_BONUS 1.40
#endif
#ifndef EARLY_DNA_MAX_PHASE
#define EARLY_DNA_MAX_PHASE 1
#endif
#define LEGACY_MUTATION_DISCOUNT_LV1 0.06
#define LEGACY_MUTATION_DISCOUNT_LV2 0.12

/* ---------------- 02 §7 环境 ---------------- */
#ifndef CRISIS_PASS_POPULATION_RATIO
#define CRISIS_PASS_POPULATION_RATIO 0.74
#endif
/* 第十二轮：纪元 1 用更宽的通过线（0.65），纪元 2+ 用 0.70，
 * 避免“越后期越松”/“开局就把低尾打穿”。
 * 第十四轮：0.63 → 0.60，把纪元 1 的 6/6 危机存活率拉回 35% 带。
 * v9 P0-19：4 类危机轮换后定在 0.672。实测 0.672 → 全清 37%，0.675 → 6%：
 * 单局危机损失集中在 33% 附近，这条线对数值极敏感，调整前必须复跑 1000 种子。 */
#ifndef CRISIS_PASS_RATIO_ERA1
#define CRISIS_PASS_RATIO_ERA1 0.78
#endif
/* v7：纪元 3 收紧到 0.85。v9 P0-19：改为 0.75 —— 0.85 时单局危机次数
 * 被失败重抽拉到 7 次以上，全清率压在 3% 以下；0.75 后回到 30% 带，
 * 同时纪元 3 仍是三纪元里通过线最高的一档。 */
#ifndef CRISIS_PASS_RATIO_ERA3
#define CRISIS_PASS_RATIO_ERA3 0.80
#endif
/* v10 P0-26：危机池从“每纪元固定 4 类”改为“6 选 4”——每个纪元给 6 类候选，
 * 由 environment_init_run 按 run 种子抽 4 类（同一局固定，跨局不同）。
 * 11 类危机里，三个纪元池尽量少重叠，单局去重后能达到 8–11 类。 */
#define ERA_CRISIS_PER_ERA 4
#ifndef ERA_CRISIS_POOL_1
#define ERA_CRISIS_POOL_1 ((1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5))
#endif
#ifndef ERA_CRISIS_POOL_2
#define ERA_CRISIS_POOL_2 ((1u << 6) | (1u << 13) | (1u << 14) | (1u << 15))
#endif
#ifndef ERA_CRISIS_POOL_3
#define ERA_CRISIS_POOL_3 ((1u << 1) | (1u << 5) | (1u << 16) | (1u << 17))
#endif
static inline unsigned int era_crisis_pool(int era)
{
    if (era <= 1) {
        return ERA_CRISIS_POOL_1;
    }
    return era == 2 ? ERA_CRISIS_POOL_2 : ERA_CRISIS_POOL_3;
}

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
/* v9 P0-19：危机伤害的阶段放大改为从 0.80 起算（原为 1.0 + 0.25×stage）。
 * 每个纪元第一次危机落在 60 s、主谱系还在阶段 1–2，旧公式下“没有对应适应性状”
 * 就等于必死：纪元 2 的微光期首次通过率只有 0.3%，“全清”被开局抽签决定。
 * 从 0.80 起算把前期伤害压掉约两成，后期（阶段 10）几乎不变。 */
#ifndef CRISIS_EARLY_SCALE
#define CRISIS_EARLY_SCALE 0.80
#endif
#define CRISIS_SHIELD_POPULATION_RATIO 0.20
#define CRISIS_SHIELD_DAMAGE_REDUCTION 0.80
#define METEOR_INSTANT_LOSS_RATIO 0.35
#define ENVIRONMENT_DURATION_SCALE_LV2 0.85
#define ENVIRONMENT_NEGATIVE_DELTA_SCALE_LV1 0.92
#define ENVIRONMENT_POSITIVE_BONUS_SCALE_LV4 1.15

/* ---------------- 02 §9 EP 结算 ---------------- */
/* v9 P0-18：关键突变保底的深度上限。主线后继的所需突变在候选池里会被强制占位，
 * 上限设在阶段 12 —— 阶段 12（无生命场）从 0.4% 拉回可追求区间；
 * 阶段 13（宇宙织体）不再保底，继续靠稀有突变自然达成（目标 1–8%）。 */
#ifndef MAINLINE_GUARANTEE_MAX_PHASE
#define MAINLINE_GUARANTEE_MAX_PHASE 12
#endif

#define EP_PER_STAGE 2
#define EP_MUTATIONS_PER_POINT 3
/* v10：突变 EP 收入封顶——时间本位后单局突变从 58 涨到 120+，
 * 不封顶会让纪元 3 的 EP 冲到 110，遗产层数连锁膨胀（纪元 3 支出随之到 3e9）。
 * v10 收口：8 → 4，把 EP 的方差挪给“危机存活”这一真正的局间差异来源。 */
#ifndef EP_MUTATION_CAP
#define EP_MUTATION_CAP 3
#endif
#define EP_SPECIES_PER_POINT 1
#define EP_CRISIS_CAP 6
/* v10：危机每通过 1 类记 2 EP——存活数量是局间差异的主要来源，
 * EP 标准差因此回到 4 以上（验收要求 σ≥4，否则没有局间差异）。
 * v10 收口：3 → 5。3 点/类的实测 σ 只有 3.2，够不到 4；提到 5 点/类，
 * 同时把突变 EP 从 8 降到 4 抵消均值上涨。 */
#ifndef EP_POINTS_PER_CRISIS
#define EP_POINTS_PER_CRISIS 5
#endif

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
/* v10 P0-22：容量也换时间本位——ATP 部分 40 s 产率 + 生物量部分 2 s 产率。
 * 生物量份额压到极小，容量就不再和分化门票抢同一种资源（纪元 3 门票可达 90 s 生物量），
 * 改由 ATP 单独决定档位；预期每纪元 6–14 级。 */
#ifndef CAPACITY_ATP_SECONDS
#define CAPACITY_ATP_SECONDS 80.0
#endif
#ifndef CAPACITY_BIOMASS_SECONDS
#define CAPACITY_BIOMASS_SECONDS 10.0
#endif
#ifndef CAPACITY_COST_FLOOR
#define CAPACITY_COST_FLOOR 15.0
#endif
#ifndef CAPACITY_BIOMASS_FLOOR
#define CAPACITY_BIOMASS_FLOOR 10.0
#endif
#define CAPACITY_GAIN_PER_LEVEL 0.05
/* v10 P0-23：容量倍率保持线性（1 + 0.15×L），但 L 封顶 20（实测上限曾到 42） */
#define CAPACITY_MAX_LEVEL 20
/* v10 P0-22：储备改时间本位——每级 120 s 被动 ATP 产率（≈30 s 墙钟，含点击收入），
 * 每纪元 5–7 级，到 26 级要跨 3–4 个纪元；RESERVE_COST_ATP 保留为地板。 */
#define RESERVE_COST_ATP 1000.0
#ifndef RESERVE_ATP_SECONDS
#define RESERVE_ATP_SECONDS 110.0
#endif
/* v8 P0-15：储备减伤改为乘算，第 6 级之后仍有边际收益，约 22 级触底 */
#ifndef RESERVE_REDUCTION_STEP
#define RESERVE_REDUCTION_STEP 0.94
#endif
#ifndef RESERVE_REDUCTION_FLOOR
#define RESERVE_REDUCTION_FLOOR 0.20
#endif
/* v9 P0-20：储备减伤 0.94^n 在第 26 级触底（0.94^26 ≈ 0.199 < 0.20），
 * 第 27 级起是“花钱买 0 收益”。显式截断在 26 级，游戏内不再允许继续购买。 */
#ifndef RESERVE_MAX_LEVEL
#define RESERVE_MAX_LEVEL 26
#endif

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
/* v7 P0-2/4d：阶段 8 起的高阶突变按“当前收入的 N 秒”计价。
 * v10 P0-22：删掉 5e8 上限（它把“45 秒”压成“5 秒”，纪元 3 一局买 58 张）；
 * 只保留 1e7 地板。定价锚是被动产率，而钱包存量约等于 1/增长率（30–60 s 产率），
 * N 必须小于它才买得起——N=220/90 时阶段 8 以上的门票永远凑不齐（实测卡在 stage 8）。
 * v10 收口：30 → 8。纪元 3 一局要买 150+ 张突变，30 秒/张把 ATP 全吃光，
 * 容量的生物量份额与三条升级轨被饿死（实测单级耗时 100 s、容量 1 级）。
 * 8 秒仍保留“突变要花 ATP”的机制，但不再是压倒性的出口。 */
#ifndef MUTATION_ATP_SECONDS
#define MUTATION_ATP_SECONDS 8.0
#endif
#ifndef MUTATION_ATP_COST_MIN
#define MUTATION_ATP_COST_MIN 10000000.0
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
/* 子升级理念成本：v10 P0-22 改时间本位——每层 50 s 主谱系知识产率，
 * 知识收入随子升级乘算自限（买一层 → 收入 ×1.2 → 下一层贵 20%），
 * 预期每纪元 4–6 层（旧 3.2^ 曲线一局买了 16 层）。base_knowledge_cost 保留为地板。 */
#ifndef SUB_UPGRADE_KNOWLEDGE_SECONDS
#define SUB_UPGRADE_KNOWLEDGE_SECONDS 42.0
#endif
#define SUB_UPGRADE_MIN_PHASE 6

/* 第十六轮：洞见——无上限知识升级，每级 +2% 全产率，价格指数增长 */
#ifndef INSIGHT_BASE_COST
#define INSIGHT_BASE_COST 600.0
#endif
#ifndef INSIGHT_COST_GROWTH
#define INSIGHT_COST_GROWTH 1.6
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

/* Unified A/B/C state; old saves remain at their original paths. */
/* v10 P0-26：RunState 新增 crisis_selection / crisis_seen（6 选 4 危机池与逐类型台账），
 * 存档结构变化，版本 8 → 9；更早的存档仍不自动迁移。 */
#define SAVE_VERSION 9

#endif
