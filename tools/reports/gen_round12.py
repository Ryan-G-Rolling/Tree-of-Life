"""Build assets/round12.js for the twelfth-round (stage 11 + crisis calibration)."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round12.js")
CRISIS_IDS = {1, 2, 3, 4, 5, 6, 13}


def rows(name, prefix):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    return [l.strip().split(",") for l in open(path, encoding="utf-8-sig") if l.startswith(prefix)]


def mean(v):
    return sum(v) / len(v) if v else 0.0


def pstdev(v):
    m = mean(v)
    return (sum((x - m) ** 2 for x in v) / len(v)) ** 0.5


def box(v):
    d = sorted(v)

    def q(p):
        pos = (len(d) - 1) * p
        lo = int(pos)
        hi = min(lo + 1, len(d) - 1)
        return d[lo] + (d[hi] - d[lo]) * (pos - lo)

    return [round(min(d), 1), round(q(0.25), 1), round(q(0.5), 1), round(q(0.75), 1),
            round(max(d), 1)]


def dist(values):
    counts = {}
    for x in values:
        counts[x] = counts.get(x, 0) + 1
    return [{"value": k, "count": counts[k]} for k in sorted(counts)]


b2 = rows("batch2_scan_round12.csv", "BATCH2,")
b3 = rows("batch3_scan_round12.csv", "BATCH3,")
cover = []
for name in ("batch3_scan_round12.csv", "batch3_explore_round12.csv"):
    cover += rows(name, "COVER_S,") + rows(name, "COVER_M,")
crisis_rows = rows("batch2_scan_round12.csv", "CRISIS,")

ci = lambda r, i: [int(x[i]) for x in r]
cf = lambda r, i: [float(x[i]) for x in r]

first_spec = sorted(cf(b2, 18))
stage1, stage3 = ci(b2, 4), ci(b3, 6)
ep1, ep2, ep3 = ci(b2, 3), ci(b2, 9), ci(b3, 5)
cris1, cris2 = ci(b2, 7), ci(b2, 13)

per_type = {}
for row in crisis_rows:
    if int(row[1]) != 1 or int(row[2]) not in CRISIS_IDS:
        continue
    env = int(row[2])
    total, passed = per_type.get(env, (0, 0))
    per_type[env] = (total + 1, passed + int(row[8]))
type_rows = [{"env": env, "rate": round(100.0 * passed / total, 1)}
             for env, (total, passed) in sorted(per_type.items())]

species_counts = {}
mutation_counts = {}
for r in cover:
    target = species_counts if r[0] == "COVER_S" else mutation_counts
    key = int(r[3])
    target[key] = target.get(key, 0) + 1

data = {
    "scan": {
        "n2": len(b2), "n3": len(b3),
        "ep": {
            "era1": {"median": statistics.median(ep1), "std": round(pstdev(ep1), 2),
                     "box": box(ep1)},
            "era2": {"median": statistics.median(ep2), "box": box(ep2)},
            "era3": {"median": statistics.median(ep3), "box": box(ep3)},
        },
        "stage": {
            "era1": dist(stage1), "era3": dist(stage3),
            "median": {"era1": statistics.median(stage1), "era3": statistics.median(stage3)},
            "low3": sum(1 for s in stage1 if s <= 3),
            "stage11_pct": round(100.0 * sum(1 for s in stage3 if s >= 11) / len(stage3), 1),
        },
        "speciation": {
            "p50": round(statistics.median(first_spec), 1),
            "p90": round(first_spec[int(0.9 * len(first_spec))], 1),
            "p95": round(first_spec[int(0.95 * len(first_spec))], 1),
        },
        "crisis": {
            "per_type": type_rows,
            "pass6_era1": round(100.0 * sum(1 for x in cris1 if x >= 6) / len(cris1), 1),
            "pass6_era2": round(100.0 * sum(1 for x in cris2 if x >= 6) / len(cris2), 1),
        },
        "coverage": {
            "species": [{"id": i, "count": species_counts.get(i, 0)} for i in range(1, 20)],
            "mutation_covered": sum(1 for i in range(34) if mutation_counts.get(i, 0) > 0),
            "mutation_ge50": sum(1 for i in range(34) if mutation_counts.get(i, 0) >= 50),
            "mutation_min": min(mutation_counts.get(i, 0) for i in range(34)),
        },
        "leftover": {
            "atp": round(statistics.median(cf(b2, 21)), 0),
            "biomass": round(statistics.median(cf(b2, 22)), 0),
            "dna": round(statistics.median(cf(b2, 23)), 0),
        },
        "subs": {"era1": statistics.median(ci(b3, 16)), "era2": statistics.median(ci(b3, 17)),
                 "era3": statistics.median(ci(b3, 18))},
    },
    "tuning": [
        {"step": "起点（v5 建议）", "ll": 73, "o2": 83, "meteor": 84, "smog": 82,
         "pass6": 67.0, "p50": 223.8, "low": 0},
        {"step": "伤害 +40%", "ll": 44, "o2": 43, "meteor": 37, "smog": 71,
         "pass6": 17.2, "p50": 261.9, "low": 91},
        {"step": "回头取中", "ll": 48, "o2": 47, "meteor": 68, "smog": 73,
         "pass6": 31.0, "p50": 239.9, "low": 3},
        {"step": "抬 Meteor / Smog", "ll": 50, "o2": 47, "meteor": 37, "smog": 68,
         "pass6": 25.2, "p50": 237.5, "low": 1},
        {"step": "纪元 1 通过线 0.63", "ll": 50, "o2": 49, "meteor": 71, "smog": 57,
         "pass6": 41.3, "p50": 237.5, "low": 0},
        {"step": "最终（Cooling/Meteor 微调）", "ll": 50, "o2": 49, "meteor": 61, "smog": 64,
         "pass6": 37.5, "p50": 238.7, "low": 0},
    ],
    "params": [
        {"name": "新游戏 DNA", "before": "0", "after": "0（维持）", "note": "重定验收带"},
        {"name": "首个突变", "before": "原价", "after": "6 折", "note": "开局 1 分钟内有决策"},
        {"name": "危机通过线", "before": "统一 0.70", "after": "纪元1 0.63 / 纪元2+ 0.70", "note": "按纪元"},
        {"name": "Low Light 伤害", "before": "0.004", "after": "0.013", "note": "98% → 50%"},
        {"name": "Oxygen 伤害", "before": "0.010", "after": "0.015", "note": "74% → 49%"},
        {"name": "Toxic 伤害", "before": "0.013", "after": "0.0145", "note": "52% → 64%"},
        {"name": "Meteor 伤害", "before": "0.016", "after": "0.0245", "note": "72% → 61%"},
        {"name": "Smog 伤害", "before": "0.014", "after": "0.022", "note": "65% → 64%"},
        {"name": "候选保底", "before": "生物量 ≥60% 才保底", "after": "始终强制 1 个必需位", "note": "命中率不再依赖 Biomass"},
        {"name": "候选可支付", "before": "无", "after": "至少 1 张 ≤ DNA+15s", "note": "开局不再全买不起"},
        {"name": "阶段 11", "before": "—", "after": "Transcendence + Omega Point", "note": "8.6% 抵达"},
        {"name": "存档版本", "before": "5", "after": "6（824 B）", "note": "A 需同步"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND12 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

s = data["scan"]
print("EP", s["ep"]["era1"]["median"], s["ep"]["era2"]["median"], s["ep"]["era3"]["median"])
print("speciation", s["speciation"], "low3", s["stage"]["low3"], "stage11",
      s["stage"]["stage11_pct"])
print("crisis", s["crisis"]["pass6_era1"], s["crisis"]["pass6_era2"], type_rows)
print("coverage", s["coverage"]["mutation_covered"], "/34 ge50", s["coverage"]["mutation_ge50"])
