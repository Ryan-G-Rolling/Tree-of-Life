"""Build assets/round13.js for the thirteenth-round content expansion."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round13.js")
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


b2 = rows("batch2_scan_round13.csv", "BATCH2,")
b3 = rows("batch3_scan_round13.csv", "BATCH3,")
crisis_rows = rows("batch2_scan_round13.csv", "CRISIS,")
cover = []
for name in ("batch3_scan_round13.csv", "batch3_explore_round13.csv"):
    cover += rows(name, "COVER_S,") + rows(name, "COVER_M,")

ci = lambda r, i: [int(x[i]) for x in r]
cf = lambda r, i: [float(x[i]) for x in r]

ep1, ep2, ep3 = ci(b2, 3), ci(b2, 9), ci(b3, 5)
stage1, stage3 = ci(b2, 4), ci(b3, 6)
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

species_counts, mutation_counts = {}, {}
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
            "stage11_pct": round(100.0 * sum(1 for s in stage3 if s >= 11) / len(stage3), 1),
            "low3": sum(1 for s in stage1 if s <= 3),
        },
        "crisis": {
            "per_type": type_rows,
            "pass6_era1": round(100.0 * sum(1 for x in cris1 if x >= 6) / len(cris1), 1),
            "pass6_era2": round(100.0 * sum(1 for x in cris2 if x >= 6) / len(cris2), 1),
        },
        "speciation": {"p50": round(statistics.median(cf(b2, 18)), 1)},
        "coverage": {
            "species": [{"id": i, "count": species_counts.get(i, 0)} for i in range(1, 26)],
            "species_hit": sum(1 for i in range(1, 26) if species_counts.get(i, 0) > 0),
            "mutation_hit": sum(1 for i in range(44) if mutation_counts.get(i, 0) > 0),
            "mutation_ge50": sum(1 for i in range(44) if mutation_counts.get(i, 0) >= 50),
        },
        "leftover": {
            "atp": round(statistics.median(cf(b2, 21)), 0),
            "dna": round(statistics.median(cf(b2, 23)), 0),
        },
        "subs": {"era1": statistics.median(ci(b3, 16)), "era2": statistics.median(ci(b3, 17)),
                 "era3": statistics.median(ci(b3, 18))},
    },
    "new_species": [
        {"id": 20, "name": "Photosynthetic Mat", "stage": 2, "parent": "Algal Eukaryote",
         "requires": "Photosymbiosis", "role": "ATP / 低光"},
        {"id": 21, "name": "Vent Swarm", "stage": 2, "parent": "Chemolithotroph",
         "requires": "Sulfur Metabolism", "role": "Biomass / 毒素"},
        {"id": 22, "name": "Chitin Reef", "stage": 3, "parent": "Multicellular Colony",
         "requires": "Chitin Wall", "role": "容量"},
        {"id": 23, "name": "Saprotrophic Forest", "stage": 5, "parent": "Fungal Network",
         "requires": "Decomposer + Mycorrhizal", "role": "DNA / Biomass"},
        {"id": 24, "name": "Hive Polity", "stage": 6, "parent": "Sapient Life",
         "requires": "Quorum Sensing + Language", "role": "理念 / 全能"},
        {"id": 25, "name": "Orbital Swarm", "stage": 9, "parent": "Space Age",
         "requires": "Orbital Habitats", "role": "ATP / 容量"},
    ],
    "new_mutations": [
        {"id": 34, "name": "Photonic Skin", "gate": "阶段 2 · Photosymbiosis", "effect": "ATP +30%"},
        {"id": 35, "name": "Sulfur Metabolism", "gate": "阶段 1 · Chemosynthesis", "effect": "Biomass +30%"},
        {"id": 36, "name": "Chitin Scaffold", "gate": "阶段 3 · Chitin Wall", "effect": "Cap +30%"},
        {"id": 37, "name": "Enzyme Cascade", "gate": "阶段 4 · Decomposer", "effect": "Biomass +25% / DNA +15%"},
        {"id": 38, "name": "Collective Memory", "gate": "阶段 6 · Quorum Sensing", "effect": "理念 +30% / Cap +15%"},
        {"id": 39, "name": "Orbital Industry", "gate": "阶段 9 · Orbital Habitats", "effect": "ATP +30% / DNA +20%"},
        {"id": 40, "name": "Symbiotic Cognition", "gate": "阶段 5 · Neural Signaling", "effect": "理念 +20% / ATP +15%"},
        {"id": 41, "name": "Cryo Archive", "gate": "阶段 7", "effect": "DNA +30%"},
        {"id": 42, "name": "Atmospheric Control", "gate": "阶段 8", "effect": "增长 +8% / Cap +10%"},
        {"id": 43, "name": "Deep Time Archive", "gate": "阶段 10", "effect": "DNA +25% / 理念 +20%"},
    ],
    "params": [
        {"name": "物种", "before": "20（19 非初始）", "after": "26（25 非初始）", "note": "6 条侧支"},
        {"name": "突变", "before": "34", "after": "44", "note": "10 个构筑件"},
        {"name": "共振", "before": "12", "after": "15", "note": "3 条新构筑"},
        {"name": "子升级", "before": "18 层", "after": "24 层", "note": "蜂群 / 轨道各 3 层"},
        {"name": "EP 上限", "before": "58", "after": "64", "note": "M/3 + S/2"},
        {"name": "MUTATION_COST_GROWTH", "before": "1.48", "after": "1.47", "note": "候选池变宽后回调"},
        {"name": "Smog 伤害", "before": "0.022", "after": "0.026", "note": "逐类型回到 40–65%"},
        {"name": "理念加成", "before": "无", "after": "Collective Memory 等 3 个突变", "note": "侧表实现，不改公开结构"},
        {"name": "稳定性", "before": "36,498 项", "after": "48,153 项（含 2×2500 步模糊测试）", "note": "23 组"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND13 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

s = data["scan"]
print("EP", s["ep"]["era1"]["median"], s["ep"]["era2"]["median"], s["ep"]["era3"]["median"])
print("stage", s["stage"]["median"], "s11", s["stage"]["stage11_pct"], "low3", s["stage"]["low3"])
print("crisis", s["crisis"]["pass6_era1"], s["crisis"]["pass6_era2"], type_rows)
print("coverage", s["coverage"]["species_hit"], "/25", s["coverage"]["mutation_hit"], "/44")
