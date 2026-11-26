"""Build assets/round6.js for the sixth-round content expansion section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round6.js")
SPECIES = {
    1: "Cyanobacterium", 2: "Chemolithotroph", 3: "Algal Eukaryote",
    4: "Protozoan Predator", 5: "Multicellular Colony", 6: "Extremophile Colony",
    7: "Differentiated Tissue", 8: "Symbiotic Superorganism", 9: "Cognitive Biosphere",
    10: "Fungal Network", 11: "Coral Symbiont",
    12: "Mycorrhizal Web", 13: "Reef Metropolis",
}


def quantile(values, p):
    data = sorted(values)
    if len(data) == 1:
        return data[0]
    pos = (len(data) - 1) * p
    lo = int(pos)
    hi = min(lo + 1, len(data) - 1)
    return data[lo] + (data[hi] - data[lo]) * (pos - lo)


def box(values):
    return [round(min(values), 2), round(quantile(values, 0.25), 2),
            round(quantile(values, 0.5), 2), round(quantile(values, 0.75), 2),
            round(max(values), 2)]


def mean(values):
    return sum(values) / len(values) if values else 0.0


def pstdev(values):
    m = mean(values)
    return (sum((v - m) ** 2 for v in values) / len(values)) ** 0.5


def parse(path):
    rows, sp1, sp2 = [], set(), set()
    with open(path, "r", encoding="utf-8-sig") as handle:
        for line in handle:
            p = line.strip().split(",")
            if p[0] == "BATCH2":
                rows.append({
                    "index": int(p[1]),
                    "ep1": int(p[3]), "mut1": int(p[5]), "sp1": int(p[6]),
                    "cr1": int(p[7]), "bio1": float(p[8]),
                    "ep2": int(p[9]), "mut2": int(p[11]), "sp2": int(p[12]),
                    "cr2": int(p[13]), "bio2": float(p[14]),
                    "first2": float(p[17]), "first1": float(p[18]),
                })
            elif p[0] == "BSPEC1":
                sp1.add(int(p[2]))
            elif p[0] == "BSPEC2":
                sp2.add(int(p[2]))
    return rows, sp1, sp2


def summarize(rows, ep, bio, sp, mut, cr, first):
    eps = [r[ep] for r in rows]
    return {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "std": round(pstdev(eps), 2),
               "min": min(eps), "max": max(eps), "box": box(eps)},
        "biomass": {"mean": round(mean([r[bio] for r in rows]), 0),
                    "min": round(min(r[bio] for r in rows), 0),
                    "max": round(max(r[bio] for r in rows), 0)},
        "species": {"mean": round(mean([r[sp] for r in rows]), 1)},
        "mutations": {"mean": round(mean([r[mut] for r in rows]), 1)},
        "crises": {"mean": round(mean([r[cr] for r in rows]), 2)},
        "firstSpeciation": {"mean": round(mean([r[first] for r in rows]), 1)},
    }


def histogram(values):
    counts = {}
    for v in values:
        counts[v] = counts.get(v, 0) + 1
    return [{"ep": k, "count": counts[k]} for k in sorted(counts)]


rows, sp1, sp2 = parse(os.path.join(HERE, "batch2_round6.csv"))
scan, _, _ = parse(os.path.join(HERE, "batch2_scan1000_round6.csv"))
if not rows or not scan:
    raise SystemExit("缺少第六轮回归 CSV")

data = {
    "era1": summarize(rows, "ep1", "bio1", "sp1", "mut1", "cr1", "first1"),
    "era2": summarize(rows, "ep2", "bio2", "sp2", "mut2", "cr2", "first2"),
    "before": {"era1": 32.3, "era2": 37.0},
    "scan": {
        "n": len(scan),
        "era1": {"mean": round(mean([r["ep1"] for r in scan]), 2),
                 "std": round(pstdev([r["ep1"] for r in scan]), 2),
                 "min": min(r["ep1"] for r in scan), "max": max(r["ep1"] for r in scan),
                 "hist": histogram([r["ep1"] for r in scan])},
        "era2": {"mean": round(mean([r["ep2"] for r in scan]), 2),
                 "std": round(pstdev([r["ep2"] for r in scan]), 2),
                 "min": min(r["ep2"] for r in scan), "max": max(r["ep2"] for r in scan),
                 "hist": histogram([r["ep2"] for r in scan])},
    },
    "seeds": [{
        "index": r["index"], "ep1": r["ep1"], "bio1": round(r["bio1"], 0),
        "ep2": r["ep2"], "bio2": round(r["bio2"], 0),
    } for r in rows],
    "species": [{"id": sid, "name": SPECIES[sid], "era1": sid in sp1, "era2": sid in sp2}
                for sid in sorted(SPECIES)],
    "ladder": [
        {"label": "第五轮 · 纪元 2", "E": 10, "M": 8, "S": 5, "C": 6, "B": 8, "total": 37},
        {"label": "第六轮 · 纪元 1", "E": 10, "M": 8, "S": 2, "C": 6, "B": 8, "total": 34},
        {"label": "第六轮 · 纪元 2", "E": 10, "M": 8, "S": 6, "C": 6, "B": 10, "total": 40},
    ],
    "events": [
        {"name": "Oxygen Pulse", "trait": "Oxygen", "duration": "40s",
         "effect": "氧气特征 ≥1 的谱系 ×1.25，其余 ×1.10"},
        {"name": "Cold Current", "trait": "Cold", "duration": "40s",
         "effect": "耐寒 ≥1 的谱系 ×1.25，其余 ×1.10"},
        {"name": "Long Summer", "trait": "Heat", "duration": "50s",
         "effect": "耐热 ≥1 的谱系 ×1.25，其余 ×1.10"},
    ],
    "routes": [
        {"name": "Mycorrhizal Web", "stage": 5, "parents": "Fungal Network / Symbiotic Superorganism",
         "mutation": "Mycorrhizal Symbiosis", "cost": 2700},
        {"name": "Reef Metropolis", "stage": 5, "parents": "Coral Symbiont / Algal Eukaryote",
         "mutation": "Reef Architecture", "cost": 2925},
    ],
    "mutations": [
        {"name": "Mycorrhizal Symbiosis", "effect": "Biomass +30%、DNA +15%、Cap +10%、增长 +5%"},
        {"name": "Reef Architecture", "effect": "ATP +15%、Biomass +20%、Cap +25%、增长 +5%"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND6 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"era1 EP {data['era1']['ep']['mean']} ± {data['era1']['ep']['std']}")
print(f"era2 EP {data['era2']['ep']['mean']} ± {data['era2']['ep']['std']}")
print(f"scan era1 {data['scan']['era1']['mean']} ± {data['scan']['era1']['std']}")
print(f"scan era2 {data['scan']['era2']['mean']} ± {data['scan']['era2']['std']}")
print("era2 species:", sorted(sp2))
