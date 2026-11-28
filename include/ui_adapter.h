/*
 * ui_adapter.h  ——  Member C 需要的“只读查询”契约（临时文件，集成时替换实现）
 *
 * 为什么存在这个文件：
 *   UI 要显示“升级价格 / 产速 / 适应度来源 / 物种名 / 突变说明 / 结算明细”，
 *   但这些数据不能由 UI 计算（01 §1：界面和结算模块不得复制公式）。
 *   于是 C 把需要的查询集中声明在这里，只声明、不实现：
 *     - 现在是 tests/mock/mock_adapter.c 提供假实现，让界面能并行开发；
 *     - 集成时把 mock_adapter.c 从编译里去掉，换成每个函数一行的转发实现，
 *       转发到 A 的 economy.h / B 的 evolution.h 真实函数。
 *
 * 对应提案：03-C成员的接口补充提案.md
 *   #1 产速汇总与升级价格      → A
 *   #2 适应度解释              → B
 *   #3 名称/说明/前置条件       → B
 *   #5 进化树与阶段名称         → B
 * 所有函数必须是无副作用的纯查询：不修改 GameState，不消耗随机数。
 */
#ifndef UI_ADAPTER_H
#define UI_ADAPTER_H

#include "game_types.h"

/* ================= A 侧：经济与单轮升级 ================= */

/* 下一级价格。level 为该升级当前等级。 */
double ua_run_upgrade_cost(const GameState *game, RunUpgradeType type);

/* 升级名称（如“代谢效率”）。 */
const char *ua_run_upgrade_title(RunUpgradeType type);

/* 当前等级效果描述，例如“ATP 产出 ×1.50”。写入 buf 并返回 buf。 */
const char *ua_run_upgrade_effect(const GameState *game, RunUpgradeType type, char *buf, int buf_size);

/* 三资源每秒总产速，out[0]=ATP out[1]=Biomass out[2]=DNA。
   等价于“所有活跃物种产率之和”，A 已经在 game_update 里算过，这里只是取出来。 */
void ua_total_rates(const GameState *game, double out[3]);

/* 点击一次能获得多少 ATP（用于按钮上的预期数值提示）。 */
double ua_click_atp(const GameState *game);
double ua_insight_cost(const GameState *game);
/* 洞见当前 / 下一级的全物种产率倍率（1.00 表示尚未投资） */
double ua_insight_multiplier(const GameState *game);
double ua_insight_next_multiplier(const GameState *game);
/* 全部在场物种的理念（Knowledge）每秒产量，顶栏与洞见卡共用 */
double ua_knowledge_rate(const GameState *game);
/* 某类限时增益的总时长，用于画皿壁上的剩余时间弧（0 表示一次性资源包） */
double ua_buff_duration(int buff_type);

/* ---- 两个“扩张”：容量（种群上限）与储备（危机减伤）----
 * 它们不走 RunUpgradeType，各有独立等级与价格。规则全在 evolution.c，
 * 这里只转发 + 把等级折成一屏能读懂的百分比：
 *   ua_capacity_cap_bonus : 累计加成（0.15 表示种群上限 +15%）
 *   ua_reserve_reduction  : 累计减伤（0.12 表示危机伤害 -12%） */
int    ua_capacity_level(const GameState *game);
int    ua_reserve_level(const GameState *game);
double ua_capacity_atp_cost(const GameState *game);
double ua_capacity_biomass_cost(const GameState *game);
double ua_reserve_atp_cost(const GameState *game);
double ua_capacity_cap_bonus(const GameState *game);
double ua_reserve_reduction(const GameState *game);
/* 每级的增量（容量 +5%，储备按 0.94 乘算），用于卡上的“每级”小字 */
double ua_capacity_step(void);
double ua_reserve_step(void);

/* ================= B 侧：物种 ================= */

const char *ua_species_name(int species_id);
const char *ua_ecology_name(int species_id);   /* 生态标签，用于协同提示 */
const char *ua_stage_name(int stage);          /* 0=原始生命 … 7=智慧生命 */

/* 阶段序号（0..7）。UI 用它显示“阶段 2 · 真核生物”。 */
int ua_species_stage(int species_id);

/* 直接转发 B 的 evolution_calculate_species_rates。 */
void ua_species_rates(const GameState *game, int species_slot, SpeciesRates *out);

/* 适应度解释：把“为什么这个物种在当前环境里乘 0.85”拆成可显示的文本。
   期望格式示例：
     "基础特征 Heat 2 ≥ 难度 2 → ×1.10\n突变 Heat Resist +2 → 已计入\n适应 Lv.1 削减 8% 惩罚"
   返回 buf。 */
const char *ua_fitness_breakdown(const GameState *game, int species_slot, char *buf, int buf_size);

/* ================= B 侧：分化 ================= */

/* 当前主谱系可选的下一物种列表；返回已写入个数（<= max_out）。 */
int ua_speciation_children(const GameState *game, int *out, int max_out);

/* 物种的亲本列表：只读分化表，写入 out 并返回个数（<= max_out）。
   初始物种返回 0；一个子代允许有多个亲本（如多细胞群落）。
   UI 只用它画进化树的连线，不在这里判定“能否分化”——那是 ua_speciation_check 的事。 */
int ua_species_parents(int species_id, int *out, int max_out);
/* 内容里最高的形态阶段（星图环数按它动态生成，不再写死 11） */
int ua_max_species_stage(void);
/* 某物种的全部直接子代（图鉴"相邻"用：让玩家看到下一步能往哪走） */
int ua_species_child_list(int parent_id, int *out, int max_out);

/* 分化到该物种需要多少 Biomass（已含 Diversification Lv.4 折扣）。 */
double ua_speciation_cost(const GameState *game, int target_species_id);

/* 是否满足前置（突变、永久等级）。返回 ACTION_OK 或具体原因。 */
ActionCode ua_speciation_check(const GameState *game, int target_species_id);

/* 前置条件文本，用于按钮下方说明，例如“需要 Nucleus + 已有 Photosynthesis”。 */
const char *ua_speciation_requirement(int target_species_id, char *buf, int buf_size);

/* 这个物种再往后还能分化成什么 —— 分化候选卡上的“后续”。
 * 沿子代取阶段最近的主干走两步，输出“珊瑚共同体 → 真核群落”；
 * 已是末端则写“已到本路线末端”。名字全部走中文图鉴。 */
const char *ua_species_route(int species_id, char *buf, int buf_size);

/* ================= B 侧：突变 ================= */

double ua_mutation_refresh_cost(const GameState *game);
/* 突变是双币：DNA 主价（在 mutation_offer.dna_costs 里）+ ATP 门票。
 * ATP 门票 = 10 s × 1.12^k 的主谱系 ATP 产率（k 只数门票卡），
 * 且随已购张数变化 —— 所以每次打开都要向规则层现问，不能缓存。
 * 直接转发 evolution_mutation_atp_cost，UI 不复制这条公式。 */
double ua_mutation_atp_cost(const GameState *game, int mutation_id);
const char *ua_mutation_name(int mutation_id);
const char *ua_mutation_desc(int mutation_id, char *buf, int buf_size);
bool ua_mutation_is_rare(int mutation_id);

/* 这条突变之后会通向哪里 —— 直接画在候选卡上的一句话。
 * 规则层不提供“路线”这种 UI 语义，所以这里按 前置突变 → 物种 → 子代 现场走一遍：
 *   “解锁 光合细胞 → 藻类群落 → 礁石文明”
 *   “纯数值强化 · 不单独解锁分化路线”（没有任何物种拿它当前置时）
 * 最多走两步，保证一行放得下。 */
const char *ua_mutation_route(int mutation_id, char *buf, int buf_size);

/* 突变共振（同一物种凑齐两个突变才生效的构筑加成）。
   “突变图谱”弹层要列出本轮已触发的共振，判定必须问 B，UI 不自己比对。 */
int ua_synergy_count(void);
const char *ua_synergy_name(int index);
const char *ua_synergy_desc(int index, char *buf, int buf_size);
/* 该共振由哪两个突变触发，out[0] / out[1] 为突变 id。 */
void ua_synergy_pair(int index, int out[2]);
/* 该物种槽当前是否已激活这条共振。 */
bool ua_synergy_active(const GameState *game, int species_slot, int index);

/* ================= B 侧：环境 ================= */

const char *ua_environment_name(int environment_id);
const char *ua_environment_desc(int environment_id, char *buf, int buf_size);
/* 该事件针对哪个特征维度，用于界面高亮，-1 表示无。 */
int ua_environment_trait(int environment_id);

/* 该环境是否属于危机（会按秒掉种群）。UI 只用来决定横幅用红/青，
   不参与任何伤害计算。 */
bool ua_environment_is_crisis(int environment_id);

/* 倒计时一次给全：remaining 剩多少秒、total 这一段总共多少秒、in_event 是否在事件中。
 *
 * 为什么需要它：常态海洋阶段 RunState.environment_time_left 恒为 0，真正的倒计时
 * 是 event_interval_left（距下一次扰动）。UI 若直接读 environment_time_left，
 * 常态阶段会永远显示 00:00。哪一段在走、总时长是多少属于规则知识，收在适配层，
 * 界面只负责画环。 */
void ua_environment_timer(const GameState *game, double *remaining, double *total, bool *in_event);

/* ================= B 侧：结算与永久强化 ================= */

/* 逐项明细：out[0]=E out[1]=M out[2]=S out[3]=C out[4]=B。
   数值必须与 prestige_preview_reward 同源，保证“明细相加 = 总数”。 */
void ua_era_breakdown(const GameState *game, int out[5]);

/* 直接转发 prestige_preview_reward，保证预览与正式结算同一个函数。 */
void ua_era_reward(const GameState *game, EraReward *out);

/* Biomass 里程碑门槛（02 §9.1 的表），UI 用它画进度条。 */
double ua_milestone_threshold(int level);

/* 当前达到的里程碑等级：直接转发 B 的 prestige_biomass_milestone_level(max_biomass)。
   UI 不再从 RunState 读缓存等级，避免 A/B 双源不同步。 */
int ua_milestone_level(const GameState *game);

/* 里程碑总档数（含 0 档），UI 用它判断是否已满级。 */
int ua_milestone_count(void);

/* 四条永久路线。cost 传入“当前等级”，返回升到下一级所需 EP。 */
const char *ua_legacy_name(int branch);
int ua_legacy_cost(int branch, int current_level);
/* 永久树单条线的最高等级（B 的内容是 8，C 单独开发时只有 5）。 */
int ua_legacy_max_level(void);
const char *ua_legacy_effect(const GameState *game, int branch, int level, char *buf, int buf_size);

/* ================= B 侧：成就 ================= */

int ua_achievement_count(void);
const char *ua_achievement_name(int index);
const char *ua_achievement_desc(int index);

/* 成就是否达成：从 discovered_species / survived_crises 实时派生，
   不写回 GameState。B 若将来补 achievement_flags，只需在这里改判定源。 */
bool ua_achievement_unlocked(const GameState *game, int index);

/* ================= 字体字符集 ================= */

/* 把所有可能出现在界面上的中文串登记给字体加载器。
   B 接入真实物种/突变表后，请在这里补一次登记，否则生僻字会显示成 '?'。 */
void ua_register_font_strings(void);

/* 字体字形登记（实现在 render.c）：ua_register_font_strings 用它登记数据表里的中文。 */
void ui_font_register(const char *utf8);

#endif /* UI_ADAPTER_H */

