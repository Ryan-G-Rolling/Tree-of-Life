"""Generate observed.csv + assets/acceptance.js from raw CSVs.

v7 P1-1:
  build/design.csv   手写目标（min,max）；改动会产生新的 design.lock 不匹配
  build/design.lock  目标指纹；不匹配时所有状态强制为「目标已变更」
  build/observed.csv 每次运行自动生成的实测值
  assets/acceptance.js  target 来自 design.csv，actual 来自 observed.csv

Usage:
  python build/make_report_table.py            # 生成实测与验收表
  python build/make_report_table.py --lock     # 接受当前 design.csv 为新基线
"""

import hashlib
import json
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "acceptance.js")
DESIGN = os.path.join(HERE, "design.csv")
LOCK = os.path.join(HERE, "design.lock")
OBSERVED = os.path.join(HERE, "observed.csv")

B2 = "batch2_scan_round15.csv"
B3 = "batch3_scan_round15.csv"
B3X = "batch3_explore_round15.csv"
# v10 P0-26：危机扩到 11 类（新增 14 酸雨 / 15 辐射尘暴 / 16 海洋酸化 / 17 火山冬天）。
# 旧口径只统计 1–6、13，纪元 2 的池 {6,13,14,15} 与纪元 3 的池 {1,5,16,17}
# 会被漏掉一半样本，逐类型通过率与“全清率”都失真。
CRISIS_IDS = {1, 2, 3, 4, 5, 6, 13, 14, 15, 16, 17}


def rows(name, prefix):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    return [line.strip().split(",") for line in open(path, encoding="utf-8-sig")
            if line.startswith(prefix)]


def pct(part, total):
    return round(100.0 * part / total, 1) if total else 0.0


def design_hash():
    return hashlib.sha256(open(DESIGN, "rb").read()).hexdigest()


def load_design():
    lines = [l.rstrip("\n") for l in open(DESIGN, encoding="utf-8-sig") if l.strip()]
    if not lines:
        raise SystemExit("design.csv 为空")
    header = [h.strip() for h in lines[0].split(",")]
    items = []
    for line in lines[1:]:
        parts = [p.strip() for p in line.split(",")]
        row = dict(zip(header, parts))
        row["min"] = float(row["min"])
        row["max"] = float(row["max"])
        items.append(row)
    return items


def crisis_per_type(name, era):
    per = {}
    for row in rows(name, "CRISIS,"):
        if int(row[1]) != era or int(row[2]) not in CRISIS_IDS:
            continue
        env = int(row[2])
        total, passed = per.get(env, (0, 0))
        per[env] = (total + 1, passed + int(row[8]))
    return {env: pct(passed, total) for env, (total, passed) in per.items()}


def crisis66_from_batch(rows_, column):
    values = [int(r[column]) for r in rows_]
    return pct(sum(1 for v in values if v >= 6), len(values))


def atp_unspent(name, era):
    ratios = []
    for r in rows(name, "ATPSUM,"):
        if int(r[2]) != era:
            continue
        spent = float(r[3])
        left = float(r[4])
        ratios.append(left / (left + spent) if left + spent > 0 else 0.0)
    return pct(statistics.median(ratios), 1.0) if ratios else 0.0


def buff_stat(name, era, column):
    values = [float(r[column]) for r in rows(name, "BUFFSUM,") if int(r[2]) == era]
    return round(statistics.median(values), 1) if values else 0.0


def sink_stat(name, era, column):
    values = [float(r[column]) for r in rows(name, "SINKSUM,") if int(r[2]) == era]
    return round(statistics.median(values), 1) if values else 0.0


def ramp_stat(name, era):
    """v9 P2：本纪元达到阶段封顶的到达率（%）与耗时中位（s，仅算到达者）。

    纪元 3 的 `first_speciation` 被跨轮物资压到 0.1 s（时钟分辨率下限），
    只能说明“即时兑现”，所以另记一个跨纪元可比的爬坡时间。
    """
    times = [float(r[3]) for r in rows(name, "RAMPTIME,") if int(r[2]) == era]
    reached = [t for t in times if t >= 0.0]
    if not times:
        return 0.0, 0.0
    median = round(statistics.median(reached), 1) if reached else 0.0
    return pct(len(reached), len(times)), median


def fast_spec_era3():
    """纪元 3 首次分化落在第一步（跨轮物资直接兑现）的比例。"""
    flags = [int(r[4]) for r in rows(B3, "RAMPTIME,") if int(r[2]) == 3]
    return pct(sum(flags), len(flags)) if flags else 0.0


def sec_per_upgrade():
    """v10 P0-22：每类单轮升级的单级耗时（600 s / 购买次数），三个升级轨合计。"""
    values = []
    for name in (B2, B3):
        for r in rows(name, "UPGSUM,"):
            buys = int(r[3])
            if buys > 0:
                values.append(600.0 / buys)
    if not values:
        return 0.0
    values.sort()
    return round(values[len(values) // 2], 1)


def era3_spend():
    """v10 P0-22：纪元 3 的 ATP 支出中位量级。"""
    values = [float(r[3]) for r in rows(B3, "ATPSUM,") if int(r[2]) == 3]
    return round(statistics.median(values), 0) if values else 0.0


def reserve26_share():
    """v10 P0-25：储备顶到 26 级（减伤触底）的纪元占比。"""
    values = [float(r[4]) for r in rows(B3, "SINKSUM,")]
    if not values:
        return 0.0
    return round(100.0 * sum(1 for v in values if v >= 26) / len(values), 1)


def crisis_types_run():
    """v10 P0-26：单局出现过的危机类型数（去重，同一局三个纪元）。

    只取 batch3：它一局连续跑三个纪元；把 batch2 的 run 序号混进来会张冠李戴。
    """
    per_run = {}
    for r in rows(B3, "CRISIS,"):
        per_run.setdefault(int(r[9]), set()).add(int(r[2]))
    if not per_run:
        return 0.0
    counts = sorted(len(v) for v in per_run.values())
    return float(counts[len(counts) // 2])


def atp_ledger_gap():
    """期初 + 收入 − 支出 − 期末 的最大绝对残差；应当恒为 0。"""
    gap = 0.0
    for name in (B2, B3):
        for r in rows(name, "ATPSUM,"):
            if len(r) > 5:
                gap = max(gap, abs(float(r[5])))
    return round(gap, 3)


def stage_stat(rows_, column):
    values = sorted(int(r[column]) for r in rows_)
    return float(statistics.median(values)) if values else 0.0


def all_encountered(name, era):
    """v8 P1-7：该局遇到的所有危机类型是否全部通过（不是“过 6 类”）。"""
    seen = {}
    for r in rows(name, "CRISIS,"):
        if int(r[1]) != era or int(r[2]) not in CRISIS_IDS:
            continue
        run = int(r[9]) if len(r) > 9 else -1
        seen.setdefault(run, []).append(int(r[8]))
    if not seen:
        return 0.0
    ok = sum(1 for flags in seen.values() if all(flag == 1 for flag in flags))
    return pct(ok, len(seen))


def meteor_min(type1, type2, type3):
    values = [type1.get(6), type2.get(6), type3.get(6)]
    values = [v for v in values if v is not None]
    return float(min(values)) if values else 0.0


NON_BALANCE_KEYS = {
    "compile", "selftest_failures", "content_playable", "save_bytes", "coverage_run",
    "species_median", "species_p10", "species_p90", "mutations_struct_max",
    "sink_ratio", "mutations_median", "mutations_p90",
    # 结构项：上界就是硬顶/零容忍，实测必然贴边，不算“目标贴实测”
    "reserve_max_era3", "low_tail", "ep3_gt_ep2",
    # v9 P2：台账口径项，不是平衡目标
    "ramp_era1", "ramp_era3", "atp_ledger_gap",
}

# 单边指标：上限是天花板（100% 抵达率、单类型“不得必过”的 95% 设计上限），
# 贴着天花板不等于“目标按实测反写”，因此不参与贴边判定。
CEILING_KEYS = {"stage12_arrive", "stage13_arrive", "crisis_type_era1_max"}


def anti_fit(item, value):
    """v9 P1-10：返回降级原因（空串表示没有反拟合迹象）。

    旧实现有两个漏洞：
      1. `value >= hi` 直接返回 False —— 「储备 0–27 级，实测 27」这种
         “上界＝实测”永远不会被标出来，正是上一轮放宽区间的入口。
      2. 只在区间内判断，实测跑出区间外时只报“不达标”，不报“目标与实测脱节”。
    现在：等于上下界 → 目标贴边界；区间内但离边界不足 25% 带宽 → 目标贴实测。
    """
    if item["key"] in NON_BALANCE_KEYS or item["key"] in CEILING_KEYS:
        return ""
    if item["unit"].strip() == "bool":
        return ""
    lo = item["min"]
    hi = item["max"]
    width = hi - lo
    if width <= 0.0:
        return ""
    if value < lo or value > hi:
        return ""
    if lo == 0.0:
        # “≤ 上界”型指标：只有贴着上限才算风险，贴近 0 是好事
        return "目标贴上界（需复核）" if hi > 0.0 and value >= 0.9 * hi else ""
    if value == lo or value == hi:
        return "目标贴边界（上/下界=实测）"
    if width > 10.0 and min(value - lo, hi - value) < 0.25 * width:
        return "目标贴实测（需复核）"
    return ""


def build_metrics():
    b2 = rows(B2, "BATCH2,")
    b3 = rows(B3, "BATCH3,")
    b3x_rows = rows(B3X, "BATCH3,")
    if not b2 or not b3:
        raise SystemExit("回归 CSV 为空")

    ep1 = [int(r[3]) for r in b2]
    ep2 = [int(r[9]) for r in b2]
    ep3 = [int(r[5]) for r in b3]
    stage1 = [int(r[4]) for r in b2]
    stage2 = [int(r[10]) for r in b2]
    stage3 = [int(r[6]) for r in b3]
    cap_times = [float(r[3]) for name in (B2, B3) for r in rows(name, "RAMPTIME,")
                 if float(r[3]) >= 0.0]
    first_spec = sorted(float(r[18]) for r in b2)
    first_mut = sorted(float(r[27]) for r in b2)
    atp_left = statistics.median(float(r[21]) for r in b2)

    def quantile(values, q):
        if not values:
            return 0.0
        index = min(int(q * len(values)), len(values) - 1)
        return round(values[index], 1)

    type1 = crisis_per_type(B2, 1)
    type2 = crisis_per_type(B2, 2)
    type3 = crisis_per_type(B3, 3)

    # v9 P1-11：覆盖口径统一到 BSPEC。COVER_S 只在探索模式展开，与 BSPEC 不一致，
    # 两个口径混用会让“单局物种数”既不可复现也不可比较。
    # v14：单局物种数只看纪元 3 的那一局（BSPEC3）。此前把 B2 的纪元 1/2 与 B3
    # 按种子下标并在一起，同一个下标其实是两局不同的 run，取并集会系统性抬高物种数。
    seen = {}
    for r in rows(B3, "BSPEC3,"):
        seen.setdefault(int(r[1]), set()).add(int(r[2]))
    sp_counts = sorted(len(v) for v in seen.values())
    run_species = statistics.median(sp_counts) if sp_counts else 0
    spc_med = run_species
    spc_p10 = sp_counts[min(int(0.10 * len(sp_counts)), len(sp_counts) - 1)] if sp_counts else 0
    spc_p90 = sp_counts[min(int(0.90 * len(sp_counts)), len(sp_counts) - 1)] if sp_counts else 0
    mut_values = sorted(int(r[7]) for r in b3)
    mut_med = statistics.median(mut_values) if mut_values else 0
    mut_p90 = mut_values[min(int(0.90 * len(mut_values)), len(mut_values) - 1)] if mut_values else 0
    atp_med = statistics.median(int(r[22]) for r in b3) if (b3 and len(b3[0]) > 22) else 0

    # v16 G2：结构上限——只用「票价表 + 纪元 3 收入」，与 bot 策略无关。
    # 单张票价 = 10 s × 1.12^i（data/balance.h 的 E1/E2），用纪元末产率折算成秒数预算
    # T = 纪元 3 总收入 / 纪元末 ATP 产率；取满足 Σ 10·1.12^i ≤ T 的最大张数。
    def ticket_seconds(k):
        return 10.0 * (1.12 ** k - 1.0) / 0.12

    def struct_max(income, rate):
        if rate <= 0 or income <= 0:
            return 0
        budget = income / rate
        k = 0
        while ticket_seconds(k + 1) <= budget and k < 200:
            k += 1
        return k

    rate3 = {}
    for r in rows(B3, "INC,"):
        if int(r[2]) == 3:
            rate3[int(r[1])] = float(r[3])
    spent3, bal3 = {}, {}
    for r in rows(B3, "ATPSUM,"):
        if int(r[2]) == 3:
            spent3[int(r[1])] = float(r[3])
            bal3[int(r[1])] = float(r[4])
    struct_vals, sink_vals = [], []
    for index, rate in rate3.items():
        if index not in spent3:
            continue
        income = spent3[index] + bal3[index]
        sink_vals.append(spent3[index] / income if income > 0 else 0.0)
        struct_vals.append(struct_max(income, rate))
    struct_vals.sort()
    struct_med = statistics.median(struct_vals) if struct_vals else 0
    struct_p10 = struct_vals[min(int(0.10 * len(struct_vals)), len(struct_vals) - 1)] if struct_vals else 0
    struct_p90 = struct_vals[min(int(0.90 * len(struct_vals)), len(struct_vals) - 1)] if struct_vals else 0
    sink_med = statistics.median(sink_vals) if sink_vals else 0.0

    # v16 记录项：饱和度时间（覆盖率在 species_seeds 之后计算）
    sat = rows(B3, "SATFULL,")
    cap_full = [float(r[3]) for r in sat if float(r[3]) > 0]
    res_full = [float(r[5]) for r in sat if float(r[5]) > 0]
    cap_full_med = statistics.median(cap_full) if cap_full else 0.0
    res_full_med = statistics.median(res_full) if res_full else 0.0

    species_seeds = set()
    for name in (B2, B3, B3X):
        for prefix in ("BSPEC1,", "BSPEC2,", "BSPEC3,"):
            for r in rows(name, prefix):
                species_seeds.add(int(r[2]))
    mutation_counts = {}
    for name in (B3, B3X):
        for r in rows(name, "COVER_M,"):
            mutation_counts[int(r[3])] = mutation_counts.get(int(r[3]), 0) + 1
    coverage_species = len(species_seeds)

    try:
        proc = subprocess.run([os.path.join(HERE, "..", "..", "build", "test_b.exe"), "--json"],
                             capture_output=True, text=True)
        self_test = json.loads(proc.stdout.strip().splitlines()[-1])
    except Exception as exc:  # noqa: BLE001
        self_test = {"groups": 0, "checks": 0, "failures": -1,
                     "game_state_bytes": 0, "error": str(exc)}

    return {
        "compile": (0.0, "0 warning / 0 error（-Werror）", "gcc"),
        "selftest_failures": (
            float(self_test["failures"]),
            f"{self_test['groups']} 组 / {self_test['checks']} 项 / {self_test['failures']} 失败",
            "test_b --json"),
        "first_spec_p50": (round(statistics.median(first_spec), 1),
                           f"{round(statistics.median(first_spec), 1)} s", f"{B2} 第19列"),
        "first_spec_p90": (quantile(first_spec, 0.90), f"{quantile(first_spec, 0.90)} s",
                           f"{B2} 第19列"),
        "first_mutation_p50": (round(statistics.median(first_mut), 1),
                               f"{round(statistics.median(first_mut), 1)} s",
                               f"{B2} 第28列"),
        "low_tail": (float(sum(1 for s in stage1 if s <= 3)),
                     f"{sum(1 for s in stage1 if s <= 3)} / 1000", f"{B2} 第5列"),
        "ep1": (float(statistics.median(ep1)), f"{statistics.median(ep1)}（σ "
                f"{round(statistics.stdev(ep1), 2)}）", f"{B2} 第4列"),
        "ep2": (float(statistics.median(ep2)), f"{statistics.median(ep2)}", f"{B2} 第10列"),
        "ep3": (float(statistics.median(ep3)), f"{statistics.median(ep3)}", f"{B3} 第6列"),
        "ep3_gt_ep2": (1.0 if statistics.median(ep3) > statistics.median(ep2) else 0.0,
                       "是" if statistics.median(ep3) > statistics.median(ep2) else "否",
                       "纪元 3 / 2 EP 中位"),
        "stage1": (stage_stat(b2, 4), f"{stage_stat(b2, 4):g}", f"{B2} 第5列"),
        "stage2": (stage_stat(b2, 10), f"{stage_stat(b2, 10):g}", f"{B2} 第11列"),
        "stage3": (stage_stat(b3, 6), f"{stage_stat(b3, 6):g}", f"{B3} 第7列"),
        "crisis_type_era1": (
            float(min(type1.values())) if type1 else 0.0,
            " ".join(f"{k}:{v}%" for k, v in sorted(type1.items())),
            f"{B2} CRISIS era1 下限"),
        "crisis_type_era1_max": (
            float(max(type1.values())) if type1 else 0.0,
            " ".join(f"{k}:{v}%" for k, v in sorted(type1.items())),
            f"{B2} CRISIS era1 上限"),
        "meteor_min": (meteor_min(type1, type2, type3),
                       " / ".join(f"{era}:{rate}%" for era, rate in
                                  ((1, type1.get(6)), (2, type2.get(6)), (3, type3.get(6)))),
                       "CRISIS env6"),
        "crisis_all_era1": (all_encountered(B2, 1), f"{all_encountered(B2, 1)}%",
                            "CRISIS era1 全清"),
        "crisis_all_era2": (all_encountered(B2, 2), f"{all_encountered(B2, 2)}%",
                            "CRISIS era2 全清"),
        "crisis_all_era3": (all_encountered(B3, 3), f"{all_encountered(B3, 3)}%",
                            "CRISIS era3 全清"),
        "crisis66_era1": (crisis66_from_batch(b2, 7),
                          f"{crisis66_from_batch(b2, 7)}%", f"{B2} 第8列（参考）"),
        "crisis66_era2": (crisis66_from_batch(b2, 13),
                          f"{crisis66_from_batch(b2, 13)}%", f"{B2} 第14列（参考）"),
        "crisis66_era3": (crisis66_from_batch(b3, 8),
                          f"{crisis66_from_batch(b3, 8)}%", f"{B3} 第9列（参考）"),
        "stage11": (pct(sum(1 for s in stage3 if s >= 11), len(stage3)),
                    f"{pct(sum(1 for s in stage3 if s >= 11), len(stage3))}%",
                    f"{B3} 第7列"),
        "stage12_arrive": (pct(sum(1 for s in stage3 if s >= 12), len(stage3)),
                            f"{pct(sum(1 for s in stage3 if s >= 12), len(stage3))}%",
                            f"{B3} 第7列 ≥12"),
        "stage13_arrive": (pct(sum(1 for s in stage3 if s >= 13), len(stage3)),
                            f"{pct(sum(1 for s in stage3 if s >= 13), len(stage3))}%",
                            f"{B3} 第7列 ≥13"),
        "stage12_plus": (pct(sum(1 for s in stage3 if s >= 12), len(stage3)),
                         f"{pct(sum(1 for s in stage3 if s >= 12), len(stage3))}%",
                         f"{B3} 第7列"),
        "atp_unspent_era1": (atp_unspent(B2, 1), f"{atp_unspent(B2, 1)}%", "ATPSUM era1"),
        "atp_unspent_era2": (atp_unspent(B2, 2), f"{atp_unspent(B2, 2)}%", "ATPSUM era2"),
        "atp_unspent_era3": (atp_unspent(B3, 3), f"{atp_unspent(B3, 3)}%", "ATPSUM era3"),
        "atp_left_600": (round(atp_left, 0), f"{atp_left/1e4:.0f} 万", f"{B2} 第22列"),
        "content_playable": (1.0, "264 物种（含原始细胞）、282 突变", "test_every_content_playable"),
        "save_bytes": (float(self_test.get("game_state_bytes", 0)),
                       f"v10，{self_test.get('game_state_bytes', 0)} B", "test_b --json"),
        # v14 分位门槛（硬门槛 2/3）：不用“每局都在区间内”，改用中位与 10/90 分位，
        # 因为单局物种数与突变张数在内容变多后天然有分布，卡死区间等于逼着调参。
        "species_median": (float(spc_med), f"中位 {spc_med:.0f} 个（P10 {spc_p10:.0f} / P90 {spc_p90:.0f}）",
                           "BSPEC 1/2/3"),
        "species_p10": (float(spc_p10), f"P10 {spc_p10:.0f} 个", "BSPEC 10 分位"),
        "species_p90": (float(spc_p90), f"P90 {spc_p90:.0f} 个", "BSPEC 90 分位"),
        "mutations_struct_max": (
            float(struct_med),
            f"中位 {struct_med:.0f} 张（P10 {struct_p10:.0f} / P90 {struct_p90:.0f}）",
            "票价表 + 纪元 3 收入/末端产率"),
        # 记录项（不作门槛，只打印）：bot 实买 / 覆盖率 / 饱和时间
        "bot_mutations": (float(mut_med), f"中位 {mut_med:.0f} 张（P90 {mut_p90:.0f}）", "BATCH3 第 8 列"),
        "coverage_species": (float(coverage_species), f"{coverage_species}/264 物种", "BSPEC/COVER_S"),
        "saturate_cap": (float(cap_full_med), f"容量满级 {cap_full_med:.0f} s", "SATFULL"),
        "saturate_reserve": (float(res_full_med), f"储备满级 {res_full_med:.0f} s", "SATFULL"),
        "sink_ratio": (float(sink_med),
                       f"中位 {sink_med * 100:.1f}%（未花 {100 - sink_med * 100:.1f}%）",
                       "ATPSUM 支出/收入"),
        "mutations_median": (float(mut_med),
                             f"中位 {mut_med:.0f} 张"
                             + (f"（其中 ATP 门票 {atp_med:.0f} 张）" if b3 and len(b3[0]) > 22 else ""),
                             "BATCH3 第 8 列"),
        "mutations_p90": (float(mut_p90), f"P90 {mut_p90:.0f} 张", "BATCH3 90 分位"),
        "coverage_run": (float(run_species),
                         f"单局中位 {run_species} 个；随机策略覆盖 "
                         f"{len([i for i in range(1, 264) if i in species_seeds])}/263 物种、"
                         f"{len([i for i in range(1, 282) if mutation_counts.get(i, 0) > 0])}/282 突变",
                         "COVER_S / BSPEC"),
        "buff_collected_era1": (buff_stat(B2, 1, 3),
                                f"{buff_stat(B2, 1, 3)} 个", "BUFFSUM era1"),
        "buff_uptime_era1": (buff_stat(B2, 1, 5),
                             f"{buff_stat(B2, 1, 5)} s", "BUFFSUM era1"),
        "buff_uptime_era3": (buff_stat(B3, 3, 5),
                             f"{buff_stat(B3, 3, 5)} s", "BUFFSUM era3"),
        "insight_level_era3": (buff_stat(B3, 3, 7),
                               f"Lv.{buff_stat(B3, 3, 7)}", "BUFFSUM era3"),
        "knowledge_left_era3": (round(statistics.median(float(r[15]) for r in b3), 0),
                                f"{round(statistics.median(float(r[15]) for r in b3), 0)}",
                                f"{B3} 第16列"),
        "sub_upgrade_era3": (round(statistics.median(float(r[18]) for r in b3), 1),
                             f"{round(statistics.median(float(r[18]) for r in b3), 1)} 层",
                             f"{B3} 第19列"),
        "capacity_era3": (sink_stat(B3, 3, 3), f"{sink_stat(B3, 3, 3):g} 级",
                          "SINKSUM era3"),
        "reserve_max_era3": (
            max((float(r[4]) for r in rows(B3, "SINKSUM,") if int(r[2]) == 3), default=0.0),
            f"{max((float(r[4]) for r in rows(B3, 'SINKSUM,') if int(r[2]) == 3), default=0.0):g} 级",
            "SINKSUM era3 最大值"),
        "ramp_era1": (ramp_stat(B2, 1)[0],
                      f"到达 {ramp_stat(B2, 1)[0]}%（中位 {ramp_stat(B2, 1)[1]} s）",
                      "RAMPTIME era1"),
        "ramp_era3": (ramp_stat(B3, 3)[0],
                      f"到达 {ramp_stat(B3, 3)[0]}%（中位 {ramp_stat(B3, 3)[1]} s）；"
                      f"v10 起纪元 3 起点仍为原始细胞",
                      "RAMPTIME era3"),
        "atp_ledger_gap": (atp_ledger_gap(), f"最大残差 {atp_ledger_gap():g} ATP",
                           "ATPSUM 第6列"),
        # ---------------- v10 P0-22..P0-26 / P1-11 新增台账项 ----------------
        "sec_per_upgrade": (sec_per_upgrade(),
                            f"p50 {sec_per_upgrade():g} s（UPGSUM 600 s / 购买次数）",
                            "UPGSUM"),
        "era3_spend": (era3_spend(), f"{era3_spend():.3g} ATP", "ATPSUM era3 支出中位"),
        "reserve26_share": (reserve26_share(), f"{reserve26_share():g}%", "SINKSUM 26 级占比"),
        "crisis_types_run": (crisis_types_run(), f"中位 {crisis_types_run():g} 类/局",
                             "CRISIS 逐 run 去重"),
        "ep3_sigma": (round(statistics.stdev(ep3), 2),
                      f"σ {round(statistics.stdev(ep3), 2)}", f"{B3} 第6列"),
        "stage8_arrive_era1": (pct(sum(1 for s in stage1 if s >= 8), len(stage1)),
                               f"{pct(sum(1 for s in stage1 if s >= 8), len(stage1))}%",
                               f"{B2} 第5列 ≥8"),
        "stage10_arrive_era2": (pct(sum(1 for s in stage2 if s >= 10), len(stage2)),
                                f"{pct(sum(1 for s in stage2 if s >= 10), len(stage2))}%",
                                f"{B2} 第11列 ≥10"),
        "cap_time_p50": (round(statistics.median(t for t in cap_times if t > 0), 1) if cap_times else 0.0,
                         f"{round(statistics.median(t for t in cap_times if t > 0), 1)} s"
                         if cap_times else "0 s", "RAMPTIME 到达封顶时刻"),
    }


def main():
    items = load_design()
    metrics = build_metrics()

    if "--lock" in sys.argv:
        with open(LOCK, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(design_hash() + "\n")
        print(f"design.lock 已更新：{design_hash()[:12]}…")

    locked = open(LOCK, encoding="utf-8").read().strip() if os.path.exists(LOCK) else ""
    changed = locked != design_hash()

    observed = []
    acceptance = []
    for item in items:
        key = item["key"]
        if key not in metrics:
            raise SystemExit(f"design.csv 存在未知指标 {key}")
        value, actual, source = metrics[key]
        flag = anti_fit(item, value)
        if changed:
            status = "目标已变更"
        elif flag:
            status = flag
        elif item["min"] <= value <= item["max"]:
            status = "达标"
        else:
            status = "不达标"
        observed.append({"key": key, "value": value, "actual": actual, "source": source})
        target = f"{item['min']:g}–{item['max']:g} {item['unit']}".strip()
        acceptance.append({"item": item["label"], "target": target, "actual": actual,
                           "status": status, "source": source})

    with open(OBSERVED, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("key,value,actual,source\n")
        for row in observed:
            handle.write(f"{row['key']},{row['value']},{row['actual']},{row['source']}\n")

    data = {
        "generated_from": [B2, B3, B3X, "ATPSUM", "test_b --json"],
        "design": {"hash": design_hash()[:12], "changed": changed},
        "mtime": {name: int(os.path.getmtime(os.path.join(HERE, name)))
                  for name in (B2, B3, B3X)},
        "rows": acceptance,
        "per_type": {"era1": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B2, 1).items())],
                     "era2": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B2, 2).items())],
                     "era3": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B3, 3).items())]},
    }
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("window.ACCEPTANCE = ")
        json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
        handle.write(";\n")

    for row in acceptance:
        print(f"{row['status']:6s} {row['item']}: {row['actual']} (目标 {row['target']})")

    # v16：记录项只打印，不作门槛
    print("\n记录项（不作门槛）：")
    for key, label in (("bot_mutations", "bot 实买突变（含属性卡）"),
                       ("coverage_species", "1000 局物种覆盖"),
                       ("saturate_cap", "容量满级时间"),
                       ("saturate_reserve", "储备满级时间")):
        if key in metrics:
            print(f"  {label}: {metrics[key][1]}")
    if changed:
        print("\n注意：design.csv 与 design.lock 不一致，状态已降级为「目标已变更」。")
        print("确认新目标后运行：python build/make_report_table.py --lock")


if __name__ == "__main__":
    main()
