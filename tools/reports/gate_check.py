# -*- coding: utf-8 -*-
"""v16 门槛自检：读 demo_b --batch3 的原始输出，打印 G2–G5 与三项记录。

用法：python tools/reports/gate_check.py build/batch3_oneshot.csv
退出码：0 全部达标；1 有门槛不达标（只用于 CI，人工看输出即可）。
"""
import statistics
import sys

# v16 G2：票价表 10 s × 1.12^i（data/balance.h 的 MUTATION_ATP_SECONDS / TICKET_GROWTH）
ATP_SECONDS = 10.0
TICKET_GROWTH = 1.12


def ticket_seconds(k):
    return ATP_SECONDS * (TICKET_GROWTH ** k - 1.0) / (TICKET_GROWTH - 1.0)


def structural_max(income, rate):
    if rate <= 0 or income <= 0:
        return 0
    budget = income / rate
    k = 0
    while ticket_seconds(k + 1) <= budget and k < 200:
        k += 1
    return k


def quantile(values, p):
    values = sorted(values)
    return values[min(int(p * len(values)), len(values) - 1)] if values else 0


def main(path):
    rows = [line.strip().split(",") for line in open(path, encoding="utf-8") if line.strip()]
    b3 = [r for r in rows if r[0] == "BATCH3"]
    if not b3:
        raise SystemExit("没有 BATCH3 行")
    stage = [int(r[6]) for r in b3]
    mut = [int(r[7]) for r in b3]
    spc = [int(r[8]) for r in b3]

    rate = {int(r[1]): float(r[3]) for r in rows if r[0] == "INC" and r[2] == "3"}
    spent = {int(r[1]): float(r[3]) for r in rows if r[0] == "ATPSUM" and r[2] == "3"}
    bal = {int(r[1]): float(r[4]) for r in rows if r[0] == "ATPSUM" and r[2] == "3"}
    struct, sink = [], []
    for index, r in rate.items():
        if index not in spent:
            continue
        income = spent[index] + bal[index]
        if income <= 0:
            continue
        sink.append(spent[index] / income)
        struct.append(structural_max(income, r))

    stage12 = 100.0 * sum(1 for s in stage if s >= 12) / len(stage)
    stage13 = 100.0 * sum(1 for s in stage if s >= 13) / len(stage)
    s_med, s_p10, s_p90 = (statistics.median(spc), quantile(spc, 0.10), quantile(spc, 0.90))
    st_med, st_p10, st_p90 = (statistics.median(struct), quantile(struct, 0.10), quantile(struct, 0.90))
    sink_med = statistics.median(sink) * 100.0

    gates = [
        ("G2 突变购买力（结构上限）12–16 张", 12 <= st_med <= 16, f"中位 {st_med:.0f}（P10 {st_p10:.0f} / P90 {st_p90:.0f}）"),
        ("G3 ATP 出口 支出/收入 ≥0.90", sink_med >= 90.0, f"{sink_med:.1f}%（未花 {100 - sink_med:.1f}%）"),
        ("G4 单局物种 中位 16–18 / P10 ≥14 / P90 ≤20",
         16 <= s_med <= 18 and s_p10 >= 14 and s_p90 <= 20,
         f"中位 {s_med:.0f}（P10 {s_p10:.0f} / P90 {s_p90:.0f}）"),
        ("G5 阶段 12 抵达 55–75% / 13 ≥5%", 55 <= stage12 <= 75 and stage13 >= 5,
         f"12 {stage12:.1f}% / 13 {stage13:.1f}%"),
    ]
    print(f"样本 {len(b3)} 局；纪元 3 突变中位 {statistics.median(mut):.0f} 张（记录项，不设区间）")
    failed = 0
    for name, ok, actual in gates:
        print(f"{'PASS' if ok else 'FAIL'}  {name}：{actual}")
        failed += 0 if ok else 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/batch3_oneshot.csv"))
