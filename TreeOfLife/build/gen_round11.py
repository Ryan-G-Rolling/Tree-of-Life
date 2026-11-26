"""Build assets/round11.js for the eleventh-round structural fixes."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round11.js")


def load(name, prefixes):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    rows = []
    for line in open(path, encoding="utf-8-sig"):
        if line.startswith(prefixes):
            rows.append(line.strip().split(","))
    return rows


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


b2 = load("batch2_scan_round11.csv", ("BATCH2,",))
b3 = load("batch3_scan_round11.csv", ("BATCH3,",))
crisis2 = load("batch2_scan_round11.csv", ("CRISIS,",))
cover_n2 = load("batch2_scan_round11.csv", ("COVER_S,", "COVER_M,"))
cover_n3 = load("batch3_scan_round11.csv", ("COVER_S,", "COVER_M,"))
cover_e3 = load("batch3_explore_round11.csv", ("COVER_S,", "COVER_M,"))

ci = lambda rows, i: [int(r[i]) for r in rows]
cf = lambda rows, i: [float(r[i]) for r in rows]

ep1, ep2, ep3 = ci(b2, 3), ci(b2, 9), ci(b3, 5)
stage1, stage2, stage3 = ci(b2, 4), ci(b2, 10), ci(b3, 6)
cris1, cris2 = ci(b2, 7), ci(b2, 13)

# 逐危机类型通过率（只统计真正的危机，排除正面事件 7–12）
CRISIS_IDS = {1, 2, 3, 4, 5, 6, 13}
per_type = {}
for era in (1, 2):
    agg = {}
    for row in crisis2:
        if int(row[1]) != era or int(row[2]) not in CRISIS_IDS:
            continue
        env = int(row[2])
        total, passed = agg.get(env, (0, 0))
        agg[env] = (total + 1, passed + int(row[8]))
    per_type[str(era)] = [{"env": env, "pass": passed, "total": total,
                           "rate": round(100.0 * passed / total, 1) if total else 0.0}
                          for env, (total, passed) in sorted(agg.items())]

# 覆盖率：普通与探索策略的并集
species_seeds, mutation_seeds = {}, {}
for rows in (cover_n2, cover_n3, cover_e3):
    for row in rows:
        table = species_seeds if row[0] == "COVER_S" else mutation_seeds
        key = int(row[3])
        table[key] = table.get(key, 0) + 1

mutation_counts = mutation_seeds
species_counts = species_seeds
mut_ge50 = sum(1 for i in range(33) if mutation_counts.get(i, 0) >= 50)
mut_ge1 = sum(1 for i in range(33) if mutation_counts.get(i, 0) >= 1)
sp_ge1 = sum(1 for i in range(1, 19) if species_counts.get(i, 0) >= 1)


def grid_growth(growth, tag):
    g2 = load(f"final11_g{tag}_b2.csv", ("BATCH2,",))
    g3 = load(f"final11_g{tag}_b3.csv", ("BATCH3,",))
    s3 = [int(r[6]) for r in g3]
    return {
        "growth": growth,
        "stage1": statistics.median([int(r[4]) for r in g2]),
        "pass1": round(100.0 * sum(1 for x in [int(r[7]) for r in g2] if x >= 6) / len(g2), 1),
        "ep1": statistics.median([int(r[3]) for r in g2]),
        "top3": round(100.0 * sum(1 for s in s3 if s == 10) / len(s3), 1),
    }


def grid_ratio(ratio, tag):
    rows = load(f"final11_r{tag}_b2.csv", ("BATCH2,",))
    c1 = [int(r[7]) for r in rows]
    c2 = [int(r[13]) for r in rows]
    return {
        "ratio": ratio,
        "pass1": round(100.0 * sum(1 for x in c1 if x >= 6) / len(rows), 1),
        "pass2": round(100.0 * sum(1 for x in c2 if x >= 6) / len(rows), 1),
        "stage1": statistics.median([int(r[4]) for r in rows]),
        "ep1": statistics.median([int(r[3]) for r in rows]),
    }


data = {
    "scan": {
        "n2": len(b2), "n3": len(b3),
        "ep": {
            "era1": {"median": statistics.median(ep1), "mean": round(mean(ep1), 2),
                     "std": round(pstdev(ep1), 2), "box": box(ep1)},
            "era2": {"median": statistics.median(ep2), "mean": round(mean(ep2), 2),
                     "std": round(pstdev(ep2), 2), "box": box(ep2)},
            "era3": {"median": statistics.median(ep3), "mean": round(mean(ep3), 2),
                     "std": round(pstdev(ep3), 2), "box": box(ep3)},
        },
        "stage": {
            "era1": dist(stage1), "era2": dist(stage2), "era3": dist(stage3),
            "median": {"era1": statistics.median(stage1), "era2": statistics.median(stage2),
                       "era3": statistics.median(stage3)},
            "low2_pct": round(100.0 * sum(1 for s in stage1 if s <= 2) / len(stage1), 2),
            "top3_pct": round(100.0 * sum(1 for s in stage3 if s == 10) / len(stage3), 1),
        },
        "crisis": {
            "pass6_era1": round(100.0 * sum(1 for x in cris1 if x >= 6) / len(cris1), 1),
            "pass6_era2": round(100.0 * sum(1 for x in cris2 if x >= 6) / len(cris2), 1),
            "per_type": per_type,
        },
        "first": {
            "mutation_era1": round(statistics.median(cf(b2, 27)), 1),
            "mutation_era2": round(statistics.median(cf(b2, 28)), 1),
            "speciation_era1": round(statistics.median(cf(b2, 18)), 1),
            "last_purchase_era3": round(statistics.median(cf(b3, 11)), 1),
        },
        "leftover": {
            "atp": round(statistics.median(cf(b2, 21)), 0),
            "biomass": round(statistics.median(cf(b2, 22)), 0),
            "dna": round(statistics.median(cf(b2, 23)), 0),
            "knowledge": round(statistics.median(cf(b2, 24)), 0),
        },
        "subs": {
            "era1": statistics.median(ci(b3, 16)),
            "era2": statistics.median(ci(b3, 17)),
            "era3": statistics.median(ci(b3, 18)),
        },
        "coverage": {
            "species": sp_ge1, "species_total": 18,
            "mutation": mut_ge1, "mutation_total": 33,
            "mutation_ge50": mut_ge50,
            "species_counts": [{"id": i, "count": species_counts.get(i, 0)} for i in range(1, 19)],
            "mutation_min": min(mutation_counts.get(i, 0) for i in range(33)),
        },
    },
    "grid_growth": [grid_growth(1.40, "1p40"), grid_growth(1.44, "1p44"),
                    grid_growth(1.48, "1p48"), grid_growth(1.52, "1p52")],
    "grid_ratio": [grid_ratio(0.55, "0p55"), grid_ratio(0.65, "0p65"),
                   grid_ratio(0.70, "0p70"), grid_ratio(0.75, "0p75")],
    "params": [
        {"name": "新游戏 DNA", "before": "0（文档写 20）", "after": "0；跨轮才给 20", "note": "三处口径统一"},
        {"name": "容量支付", "before": "100% Biomass", "after": "70% ATP + 30% Biomass（前 3 级例外）", "note": "ATP 有出口"},
        {"name": "危机判定", "before": "结束瞬间 vs 40%", "after": "期间最低人口 vs 70%", "note": "不能靠复利蒙混"},
        {"name": "危机伤害", "before": "×1.0", "after": "×2.0，劣势产率再 ×0.4", "note": "真实筛选"},
        {"name": "即时损失", "before": "固定 35%", "after": "0.35 + 0.05×阶段（上限 90%）", "note": "阶段越高越危险"},
        {"name": "突变增长", "before": "1.34^全池", "after": "1.48^必需链；可选不进链", "note": "可选突变不再是陷阱"},
        {"name": "存档版本", "before": "4", "after": "5（+crisis_min_population）", "note": "A 需同步"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND11 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

s = data["scan"]
print("EP", s["ep"]["era1"]["median"], s["ep"]["era2"]["median"], s["ep"]["era3"]["median"])
print("stage", s["stage"]["median"], "crisis", s["crisis"]["pass6_era1"], s["crisis"]["pass6_era2"])
print("coverage", s["coverage"]["species"], "/18,", s["coverage"]["mutation"], "/33, ge50",
      s["coverage"]["mutation_ge50"], "min", s["coverage"]["mutation_min"])
print("grid growth", [(g["growth"], g["stage1"], g["pass1"], g["top3"]) for g in data["grid_growth"]])
print("grid ratio", [(g["ratio"], g["pass1"], g["pass2"]) for g in data["grid_ratio"]])
