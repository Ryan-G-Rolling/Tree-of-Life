# -*- coding: utf-8 -*-
"""v19 门槛自检：读 demo_b --batch3 的原始输出，打印 G3–G5 与全部记录项。

G2（门票卡张数）已降为记录项：它测的是 bot 自限，不是经济约束。
这里额外打印一条与 bot 无关的结构口径（第 12 张门票的秒产率），只打印不判定。

v25 定稿：G5 改为下限判定（≥55% 即达标），撤销 80% 上限——阶段 12 已有 20 个物种，
抵达率越高内容越容易被看到；「12 是取舍点」是 D1 之前的旧意图，已不成立。

用法：python tools/reports/gate_check.py build/batch3_oneshot.csv [探索批次 csv]
退出码：0 全部门槛达标；1 有门槛不达标（人工看输出即可）。
"""
import os
import re
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BALANCE = os.path.join(HERE, "..", "..", "data", "balance.h")


def balance_define(name):
    """从 data/balance.h 读一个数值宏，避免报告脚本与实现各写一份常量。"""
    text = open(BALANCE, encoding="utf-8-sig").read()
    m = re.search(r"^#define\s+%s\s+([0-9.eE+-]+)" % name, text, re.M)
    if not m:
        raise SystemExit(f"balance.h 里找不到 {name}")
    return float(m.group(1))


def quantile(values, p):
    values = sorted(values)
    return values[min(int(p * len(values)), len(values) - 1)] if values else 0


def main(path, explore_path=None):
    rows = [line.strip().split(",") for line in open(path, encoding="utf-8") if line.strip()]
    b3 = [r for r in rows if r[0] == "BATCH3"]
    if not b3:
        raise SystemExit("没有 BATCH3 行")
    stage = [int(r[6]) for r in b3]
    total_mut = [int(r[7]) for r in b3]
    spc = [int(r[8]) for r in b3]
    gate = [int(r[2]) for r in rows if r[0] == "GATEMUT"]
    if not gate:
        raise SystemExit("没有 GATEMUT 行（demo_b 未编译 v19 版本？）")

    # G3：三段窗口（前 20% / 中 60% / 后 20%）
    early, mid, late = [], [], []
    for r in (r for r in rows if r[0] == "SINKWIN"):
        i0, s0, i1, s1, i2, s2 = (float(x) for x in r[2:8])
        if i0 > 0:
            early.append(s0 / i0)
        if i1 > 0:
            mid.append(s1 / i1)
        if i2 > 0:
            late.append(s2 / i2)
    mid_rate = statistics.median(mid) if mid else 0.0
    late_rate = statistics.median(late) if late else 0.0
    late_vs_mid = late_rate / mid_rate if mid_rate > 0 else 0.0

    # 记录项：全局支出比、饱和度
    spent = {int(r[1]): float(r[3]) for r in rows if r[0] == "ATPSUM" and r[2] == "3"}
    bal = {int(r[1]): float(r[4]) for r in rows if r[0] == "ATPSUM" and r[2] == "3"}
    sink = [spent[i] / (spent[i] + bal[i]) for i in spent if spent[i] + bal[i] > 0]
    sat = [r for r in rows if r[0] == "SATFULL"]
    cap_levels = [int(float(r[2])) for r in sat]
    cap_hit = [int(r[3]) for r in sat]
    res_levels = [int(float(r[5])) for r in sat]
    res_fill = [float(r[6]) for r in sat if float(r[6]) > 0]

    coverage = ""
    if explore_path:
        # 允许逗号分隔多个批次：覆盖率是「所有跑到过的物种」的并集，
        # 只给探索批次会低报（实测探索 36 / 三批并集 54）。
        try:
            seen = set()
            for path_i in explore_path.split(","):
                for line in open(path_i, encoding="utf-8"):
                    if line.startswith("BSPEC"):
                        seen.add(line.split(",")[2])
            coverage = f"{len(seen)}/264"
        except OSError:
            coverage = "n/a"

    stage12 = 100.0 * sum(1 for s in stage if s >= 12) / len(stage)
    stage13 = 100.0 * sum(1 for s in stage if s >= 13) / len(stage)
    s_med, s_p10, s_p90 = statistics.median(spc), quantile(spc, 0.10), quantile(spc, 0.90)
    g_med, g_p10, g_p90 = statistics.median(gate), quantile(gate, 0.10), quantile(gate, 0.90)

    gates = [
        ("G3 末 20% 支出率 ≥ 0.8× 中段 60%", late_vs_mid >= 0.8,
         f"{late_rate:.3f} / {mid_rate:.3f} = {late_vs_mid:.3f}"),
        ("G4 单局物种 中位 16–18 / P10 ≥14 / P90 ≤20",
         16 <= s_med <= 18 and s_p10 >= 14 and s_p90 <= 20,
         f"中位 {s_med:.0f}（P10 {s_p10:.0f} / P90 {s_p90:.0f}）"),
        # v25 定稿口径：只设下限（阶段 12 已有 20 个物种，抵达率越高内容越容易被看到）。
        # 旧口径 55–80% 的上限来自「希望 12 是取舍点」，D1 之后该意图已不成立。
        ("G5 阶段 12 抵达 ≥ 55%（下限）", stage12 >= 55,
         f"12 {stage12:.1f}% / 13 {stage13:.1f}%"),
    ]
    print(f"样本 {len(b3)} 局")
    failed = 0
    for name, ok, actual in gates:
        print(f"{'PASS' if ok else 'FAIL'}  {name}：{actual}")
        failed += 0 if ok else 1

    ticket_base = balance_define("MUTATION_ATP_SECONDS")
    ticket_growth = balance_define("MUTATION_TICKET_GROWTH")
    ticket12 = ticket_base * ticket_growth ** 12
    print("记录项（不作门槛）：")
    print(f"  G2 门票卡张数：中位 {g_med:.0f}（P10 {g_p10:.0f} / P90 {g_p90:.0f}），"
          f"自限 16 张——测 bot 不测经济")
    print(f"  G2 结构口径（与 bot 无关）：第 12 张门票 = {ticket_base:g} s × {ticket_growth:g}^12 "
          f"= {ticket12:.1f} s 产率，只打印不判定")
    print(f"  三段支出率：前 20% {statistics.median(early) if early else 0:.3f} / 中 60% {mid_rate:.3f} / "
          f"后 20% {late_rate:.3f}（后段绝对值参考线 0.40）")
    print(f"  全局 支出/收入 {statistics.median(sink) * 100:.1f}%")
    print(f"  bot 实买突变 中位 {statistics.median(total_mut):.0f} 张"
          f"（P90 {quantile(total_mut, 0.9):.0f}，含 16 张自限）")
    if coverage:
        print(f"  物种覆盖 {coverage}（1000 局口径）")
    print(f"  容量：结束时级数 中位 {statistics.median(cap_levels):.0f}，触顶 26 级占比 "
          f"{100.0 * sum(cap_hit) / len(cap_hit):.1f}%")
    print(f"  储备：满级占比 {100.0 * len(res_fill) / len(res_levels):.1f}%，满级时间中位 "
          f"{statistics.median(res_fill) if res_fill else 0:.0f} s"
          f"（{(statistics.median(res_fill) / 600) if res_fill else 0:.2f}×T）")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/batch3_oneshot.csv",
                  sys.argv[2] if len(sys.argv) > 2 else None))
