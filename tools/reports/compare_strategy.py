# -*- coding: utf-8 -*-
"""v19 对照跑：同一批种子下「买属性卡」与「不买属性卡」两组并列。

方案第 4 节要求：只有两组都在报告里，G3（ATP 出口）才算被证明。
用法：python tools/reports/compare_strategy.py [种子数，默认 200]
"""
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DEMO = os.path.join(ROOT, "build", "demo_b.exe")


def run(args):
    out = subprocess.run([DEMO] + args, capture_output=True, text=True,
                         encoding="utf-8", errors="replace").stdout
    rows = [line.strip().split(",") for line in out.splitlines() if line]
    mut, spc, stage = [], [], []
    for r in rows:
        if r[0] == "BATCH3":
            stage.append(int(r[6]))
            mut.append(int(r[7]))
            spc.append(int(r[8]))
    rate, spent, bal = {}, {}, {}
    for r in rows:
        if r[0] == "INC" and r[2] == "3":
            rate[int(r[1])] = float(r[3])
        elif r[0] == "ATPSUM" and r[2] == "3":
            spent[int(r[1])] = float(r[3])
            bal[int(r[1])] = float(r[4])
    cap, res = {}, {}
    for r in rows:
        if r[0] == "SATFULL":
            # v19 D10 之后的列序：index, cap_level, cap_hit, cap_time, res_level, res_time
            cap[int(r[1])] = float(r[4])
            res[int(r[1])] = float(r[6])
    ratio = [spent[i] / (spent[i] + bal[i]) for i in spent if spent[i] + bal[i] > 0]
    cap_full = [t for t in cap.values() if t > 0]
    res_full = [t for t in res.values() if t > 0]
    return {
        "n": len(mut),
        "mut": statistics.median(mut) if mut else 0,
        "spc": statistics.median(spc) if spc else 0,
        "stage": statistics.median(stage) if stage else 0,
        "spent": statistics.median(spent.values()) if spent else 0,
        "ratio": 100 * statistics.median(ratio) if ratio else 0,
        "cap_full": 100 * len(cap_full) / len(cap) if cap else 0,
        "cap_time": statistics.median(cap_full) if cap_full else 0,
        "res_full": 100 * len(res_full) / len(res) if res else 0,
        "res_time": statistics.median(res_full) if res_full else 0,
    }


def main():
    n = sys.argv[1] if len(sys.argv) > 1 else "200"
    groups = (("买属性卡（v16 默认）", []), ("不买属性卡（--lean 旧策略）", ["--lean"]))
    results = [(name, run(["--batch3", n] + extra)) for name, extra in groups]
    print(f"v19 对照跑：同一批种子 n={n}，纪元 3 中位（D1 门票卡口径后）")
    print(f"{'策略':26}{'突变':>6}{'物种':>6}{'阶段':>6}{'支出':>12}{'支出/收入':>10}"
          f"{'容量满级':>10}{'储备满级':>12}")
    for name, r in results:
        print(f"{name:26}{r['mut']:>6.0f}{r['spc']:>6.0f}{r['stage']:>6.0f}"
              f"{r['spent']:>12.3g}{r['ratio']:>9.1f}%{r['cap_full']:>9.1f}%"
              f"{r['res_time']:>9.0f}s")
    print("\n注：容量列是「触顶 26 级（CAPACITY_MAX_LEVEL）的局占比」；储备列为满级时间中位。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
