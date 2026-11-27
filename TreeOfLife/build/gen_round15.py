"""Build assets/round15.js for the fifteenth-round content expansion."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round15.js")
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


b2 = rows("batch2_scan_round15.csv", "BATCH2,")
b3 = rows("batch3_scan_round15.csv", "BATCH3,")
crisis_rows = rows("batch2_scan_round15.csv", "CRISIS,")
cover = []
for name in ("batch3_scan_round15.csv", "batch3_explore_round15.csv"):
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

species_stage = {}
for i in range(1, 186):
    species_stage[i] = (i - 1) // 10 if i >= 46 else 0

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
            "stage13_pct": round(100.0 * sum(1 for s in stage3 if s >= 13) / len(stage3), 1),
            "low3": sum(1 for s in stage1 if s <= 3),
        },
        "crisis": {
            "per_type": type_rows,
            "pass6_era1": round(100.0 * sum(1 for x in cris1 if x >= 6) / len(cris1), 1),
            "pass6_era2": round(100.0 * sum(1 for x in cris2 if x >= 6) / len(cris2), 1),
        },
        "speciation": {"p50": round(statistics.median(cf(b2, 18)), 1)},
        "coverage": {
            "species": [{"id": i, "count": species_counts.get(i, 0)} for i in range(1, 186)],
            "species_hit": sum(1 for i in range(1, 186) if species_counts.get(i, 0) > 0),
            "mutation_hit": sum(1 for i in range(204) if mutation_counts.get(i, 0) > 0),
            "mutation_ge50": sum(1 for i in range(204) if mutation_counts.get(i, 0) >= 50),
        },
        "leftover": {
            "atp": round(statistics.median(cf(b2, 21)), 0),
            "dna": round(statistics.median(cf(b2, 23)), 0),
        },
        "subs": {"era1": statistics.median(ci(b3, 16)), "era2": statistics.median(ci(b3, 17)),
                 "era3": statistics.median(ci(b3, 18))},
    },
    "families": [
        {"stage": 0, "species": "Lipid Vesicle … Early Replicator", "theme": "前细胞化学"},
        {"stage": 1, "species": "Purple Sulfur Bacterium … Sulfate Reducer", "theme": "细菌代谢"},
        {"stage": 2, "species": "Diatom … Choanoflagellate", "theme": "原生生物"},
        {"stage": 3, "species": "Sponge Primitive … Early Metazoan", "theme": "早期后生动物"},
        {"stage": 4, "species": "Flatworm … Early Mollusc", "theme": "身体构型"},
        {"stage": 5, "species": "Trilobite … Lamprey", "theme": "寒武纪海洋"},
        {"stage": 6, "species": "Lobe-Finned Fish … Swamp Conifer", "theme": "登陆与森林"},
        {"stage": 7, "species": "Dinosaur Herd … Grassland", "theme": "中生代陆地"},
        {"stage": 8, "species": "Primate Troop … Island Chiefdom", "theme": "文明萌芽"},
        {"stage": 9, "species": "Industrial Nation … Mass Society", "theme": "工业社会"},
        {"stage": 10, "species": "Orbital Station … Interplanetary Union", "theme": "行星际"},
        {"stage": 11, "species": "Neural Archive … Quantum Network", "theme": "机器与星际"},
        {"stage": 12, "species": "Simulation World … Cognition Tide", "theme": "理念世界"},
        {"stage": 13, "species": "Cosmic Loom … Absolute Mind", "theme": "宇宙终局"},
    ],
    "params": [
        {"name": "物种", "before": "46", "after": "186", "note": "14 阶 ×10 条新谱系"},
        {"name": "突变", "before": "64", "after": "204", "note": "14 阶 ×10 个"},
        {"name": "EP 上限", "before": "85", "after": "202", "note": "M/3 + S/2"},
        {"name": "首突变折扣", "before": "40%", "after": "15%", "note": "前期升级更快被卡住"},
        {"name": "分段 DNA 倍率", "before": "无", "after": "1.8 / 2.0 / 2.5", "note": "阶段 0–3 / 4–7 / 8+"},
        {"name": "高阶 ATP 费用", "before": "无", "after": "1e7 × 8^(阶-8)", "note": "阶段 8 起"},
        {"name": "突变成本链", "before": "已拥有全部必需突变", "after": "只看下一步前置", "note": "侧支门票不拖贵主线"},
        {"name": "单轮升级上限", "before": "15", "after": "25", "note": "ATP 出口"},
        {"name": "储备上限", "before": "5", "after": "10", "note": "ATP 出口"},
        {"name": "结构上限", "before": "64 / 128", "after": "256 / 256", "note": "物种 / 突变"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND15 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

s = data["scan"]
print("EP", s["ep"]["era1"]["median"], s["ep"]["era2"]["median"], s["ep"]["era3"]["median"])
print("stage", s["stage"]["median"], "s11", s["stage"]["stage11_pct"],
      "s13", s["stage"]["stage13_pct"], "low3", s["stage"]["low3"])
print("crisis", s["crisis"]["pass6_era1"], s["crisis"]["pass6_era2"], type_rows)
print("coverage", s["coverage"]["species_hit"], "/185", s["coverage"]["mutation_hit"], "/204")
