"""Build assets/round14.js for the fourteenth-round content expansion."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round14.js")
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


b2 = rows("batch2_scan_round14.csv", "BATCH2,")
b3 = rows("batch3_scan_round14.csv", "BATCH3,")
crisis_rows = rows("batch2_scan_round14.csv", "CRISIS,")
cover = []
for name in ("batch3_scan_round14.csv", "batch3_explore_round14.csv"):
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
            "species": [{"id": i, "count": species_counts.get(i, 0)} for i in range(1, 46)],
            "species_hit": sum(1 for i in range(1, 46) if species_counts.get(i, 0) > 0),
            "mutation_hit": sum(1 for i in range(64) if mutation_counts.get(i, 0) > 0),
            "mutation_ge50": sum(1 for i in range(64) if mutation_counts.get(i, 0) >= 50),
        },
        "leftover": {
            "atp": round(statistics.median(cf(b2, 21)), 0),
            "dna": round(statistics.median(cf(b2, 23)), 0),
        },
        "subs": {"era1": statistics.median(ci(b3, 16)), "era2": statistics.median(ci(b3, 17)),
                 "era3": statistics.median(ci(b3, 18))},
    },
    "new_species": [
        {"id": 26, "name": "Thermophilic Archaea", "stage": 2, "parent": "Chemolithotroph",
         "requires": "Heat Resistance + Thermophilic Enzymes", "role": "Biomass / 抗伤"},
        {"id": 27, "name": "Nitroplast Consortium", "stage": 2, "parent": "Algal Eukaryote",
         "requires": "Nitrogen Sense + Nitroplast", "role": "ATP / 增长"},
        {"id": 28, "name": "Slime Mold Network", "stage": 3, "parent": "Multicellular Colony",
         "requires": "Decomposer + Amoeboid", "role": "Biomass / DNA"},
        {"id": 29, "name": "Radiolarian Shell", "stage": 3, "parent": "Protozoan",
         "requires": "Chitin Wall + Silica Shell", "role": "容量 / 抗伤"},
        {"id": 30, "name": "Lichen Consortium", "stage": 4, "parent": "Coral Symbiont",
         "requires": "Photosymbiosis + Symbiotic Lichen", "role": "ATP / Biomass"},
        {"id": 31, "name": "Hydrothermal Tubeworm", "stage": 4, "parent": "Vent Swarm",
         "requires": "Sulfur Metabolism + Hemoglobin", "role": "Biomass / 毒素"},
        {"id": 32, "name": "Coral Spire", "stage": 5, "parent": "Reef Metropolis",
         "requires": "Reef Architecture + Photonic Skin", "role": "ATP / 容量"},
        {"id": 33, "name": "Prairie Rootweb", "stage": 5, "parent": "Mycorrhizal Web",
         "requires": "Mycorrhizal Symbiosis", "role": "DNA / 增长"},
        {"id": 34, "name": "Insect Metropolis", "stage": 5, "parent": "Hive Polity",
         "requires": "Quorum Sensing + Neural Lace", "role": "增长 / 容量"},
        {"id": 35, "name": "Cetacean Culture", "stage": 6, "parent": "Sapient Life",
         "requires": "Abstract Thought + Echolocation", "role": "ATP / DNA"},
        {"id": 36, "name": "Fungal Archive", "stage": 6, "parent": "Saprotrophic Forest",
         "requires": "Distributed Memory", "role": "DNA / Biomass"},
        {"id": 37, "name": "Orbital Foundry", "stage": 7, "parent": "Space Age",
         "requires": "Rocketry + Fusion Torch", "role": "ATP / 容量"},
        {"id": 38, "name": "Ocean Arcologies", "stage": 7, "parent": "Civilization",
         "requires": "Urbanization + Seasteading", "role": "Biomass / 容量"},
        {"id": 39, "name": "Dyson Swarm", "stage": 8, "parent": "Space Age",
         "requires": "Orbital Habitats + Dyson Collectors", "role": "ATP / 容量"},
        {"id": 40, "name": "Galactic Probe Net", "stage": 9, "parent": "Space Age",
         "requires": "Warp Probe", "role": "DNA / ATP"},
        {"id": 41, "name": "Iron Star Forge", "stage": 10, "parent": "Singularity",
         "requires": "Stellar Foundry", "role": "ATP / Biomass"},
        {"id": 42, "name": "Quantum Biosphere", "stage": 11, "parent": "Transcendence",
         "requires": "Quantum Cognition", "role": "ATP / 增长"},
        {"id": 43, "name": "Multiverse Garden", "stage": 12, "parent": "Transcendence",
         "requires": "Multiverse Tuning", "role": "ATP / 容量"},
        {"id": 44, "name": "Noetic Field", "stage": 12, "parent": "Transcendence",
         "requires": "Omega Point + Cosmic Awareness", "role": "主线 · 全能"},
        {"id": 45, "name": "Cosmic Weave", "stage": 13, "parent": "Noetic Field",
         "requires": "Cosmic Awareness + Entropy Reversal", "role": "主线 · 终局"},
    ],
    "new_mutations": [
        {"id": 44, "name": "Thermophilic Enzymes", "gate": "阶段 0", "effect": "Biomass +10% / 增长 +5%"},
        {"id": 45, "name": "Nitroplast", "gate": "阶段 2 · Nitrogen Sense", "effect": "ATP +10% / Biomass +20%"},
        {"id": 46, "name": "Amoeboid Movement", "gate": "阶段 2", "effect": "Biomass +12% / DNA +10%"},
        {"id": 47, "name": "Silica Shell", "gate": "阶段 2 · Chitin Wall", "effect": "Cap +25%"},
        {"id": 48, "name": "Symbiotic Lichen", "gate": "阶段 3 · Photosymbiosis", "effect": "ATP +10% / Biomass +15% / Cap +15%"},
        {"id": 49, "name": "Hemoglobin", "gate": "阶段 2 · Sulfur Metabolism", "effect": "ATP +15% / Biomass +10%"},
        {"id": 50, "name": "Bioluminescence", "gate": "阶段 4 · Photonic Skin", "effect": "ATP +20% / DNA +5%"},
        {"id": 51, "name": "Neural Lace", "gate": "阶段 5 · Neural Signaling", "effect": "DNA +15% / Cap +5%"},
        {"id": 52, "name": "Echolocation", "gate": "阶段 6 · Neural Signaling", "effect": "ATP +15% / 增长 +4%"},
        {"id": 53, "name": "Chemical Language", "gate": "阶段 6 · Language", "effect": "DNA +25% / Cap +10%"},
        {"id": 54, "name": "Distributed Memory", "gate": "阶段 5 · Signal Literacy", "effect": "DNA +20% / Cap +15%"},
        {"id": 55, "name": "Fusion Torch", "gate": "阶段 8 · Rocketry", "effect": "ATP +25% / Cap +20%"},
        {"id": 56, "name": "Seasteading", "gate": "阶段 7 · Urbanization", "effect": "Biomass +20% / Cap +20%"},
        {"id": 57, "name": "Dyson Collectors", "gate": "阶段 9 · Orbital Habitats", "effect": "ATP +30% / Cap +25%"},
        {"id": 58, "name": "Warp Probe", "gate": "阶段 9 · Rocketry", "effect": "DNA +25% / Cap +15%"},
        {"id": 59, "name": "Stellar Foundry", "gate": "阶段 10 · Machine Intelligence", "effect": "ATP +30% / Biomass +25%"},
        {"id": 60, "name": "Quantum Cognition", "gate": "阶段 11 · Recursive Self-Improvement", "effect": "ATP +30% / DNA +30% / 理念 +30%"},
        {"id": 61, "name": "Multiverse Tuning", "gate": "阶段 11 · Omega Point", "effect": "ATP +30% / Cap +25% / 理念 +20%"},
        {"id": 62, "name": "Cosmic Awareness", "gate": "阶段 11 · Omega Point", "effect": "ATP +35% / Cap +25% / 理念 +30%"},
        {"id": 63, "name": "Entropy Reversal", "gate": "阶段 12 · Cosmic Awareness", "effect": "全产率 +30% 量级 / 理念 +20%"},
    ],
    "params": [
        {"name": "物种", "before": "26（25 非初始）", "after": "46（45 非初始）", "note": "18 侧支 + 阶段 12/13"},
        {"name": "突变", "before": "44", "after": "64", "note": "20 个构筑件与门票"},
        {"name": "共振", "before": "15", "after": "20", "note": "5 条新构筑"},
        {"name": "子升级", "before": "24 层", "after": "30 层", "note": "心智场 / 宇宙织锦各 3 层"},
        {"name": "EP 上限", "before": "64", "after": "85", "note": "阶段 13 + 64 突变 + 45 物种"},
        {"name": "MUTATION_COST_GROWTH", "before": "1.47", "after": "1.46", "note": "候选池再变宽后回调"},
        {"name": "纪元 1 危机通过线", "before": "0.63", "after": "0.625", "note": "6/6 回到 35–45%"},
        {"name": "Warming / Toxic / Meteor 伤害", "before": "0.013 / 0.0145 / 0.0245", "after": "0.0132 / 0.0148 / 0.0240", "note": "逐类型回到 40–65%"},
        {"name": "结构上限", "before": "MAX_SPECIES 32 / MAX_MUTATIONS 64", "after": "64 / 128", "note": "为下一批 20 预留"},
        {"name": "稳定性", "before": "48,153 项", "after": "50,404 项（含新内容可达性）", "note": "24 组"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND14 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

s = data["scan"]
print("EP", s["ep"]["era1"]["median"], s["ep"]["era2"]["median"], s["ep"]["era3"]["median"])
print("stage", s["stage"]["median"], "s11", s["stage"]["stage11_pct"],
      "s13", s["stage"]["stage13_pct"], "low3", s["stage"]["low3"])
print("crisis", s["crisis"]["pass6_era1"], s["crisis"]["pass6_era2"], type_rows)
print("coverage", s["coverage"]["species_hit"], "/45", s["coverage"]["mutation_hit"], "/64")
