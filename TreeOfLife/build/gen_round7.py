"""Build assets/round7.js for the seventh-round pacing rebalance section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round7.js")
SPECIES = {
    1: "Cyanobacterium", 2: "Chemolithotroph", 3: "Algal Eukaryote",
    4: "Protozoan Predator", 5: "Multicellular Colony", 6: "Extremophile Colony",
    7: "Differentiated Tissue", 8: "Symbiotic Superorganism", 9: "Cognitive Biosphere",
    10: "Fungal Network", 11: "Coral Symbiont", 12: "Mycorrhizal Web",
    13: "Reef Metropolis", 14: "Sapient Life", 15: "Technosphere",
}


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
                    "ep1": int(p[3]), "stage1": int(p[4]), "mut1": int(p[5]),
                    "sp1": int(p[6]), "cr1": int(p[7]), "bio1": float(p[8]),
                    "ep2": int(p[9]), "stage2": int(p[10]), "mut2": int(p[11]),
                    "sp2": int(p[12]), "cr2": int(p[13]), "bio2": float(p[14]),
                    "first1": float(p[18]), "first2": float(p[17]),
                    "last1": float(p[19]), "last2": float(p[20]),
                    "atp2": float(p[21]), "bio_end2": float(p[22]), "dna2": float(p[23]),
                })
            elif p[0] == "BSPEC1":
                sp1.add(int(p[2]))
            elif p[0] == "BSPEC2":
                sp2.add(int(p[2]))
    return rows, sp1, sp2


def dist(values):
    counts = {}
    for v in values:
        counts[v] = counts.get(v, 0) + 1
    return [{"value": k, "count": counts[k]} for k in sorted(counts)]


def summarize(rows, era):
    eps = [r["ep%d" % era] for r in rows]
    lasts = [r["last%d" % era] for r in rows]
    return {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "std": round(pstdev(eps), 2),
               "min": min(eps), "max": max(eps)},
        "stage": {"mean": round(mean([r["stage%d" % era] for r in rows]), 2),
                  "dist": dist([r["stage%d" % era] for r in rows])},
        "mutations": {"mean": round(mean([r["mut%d" % era] for r in rows]), 1)},
        "species": {"mean": round(mean([r["sp%d" % era] for r in rows]), 1)},
        "crises": {"mean": round(mean([r["cr%d" % era] for r in rows]), 2)},
        "biomass": {"mean": round(mean([r["bio%d" % era] for r in rows]), 0)},
        "firstSpeciation": {"mean": round(mean([r["first%d" % era] for r in rows]), 1)},
        "lastPurchase": {"mean": round(mean(lasts), 1), "min": round(min(lasts), 1),
                         "max": round(max(lasts), 1)},
    }


rows, sp1, sp2 = parse(os.path.join(HERE, "batch2_round7.csv"))
scan, _, _ = parse(os.path.join(HERE, "batch2_scan1000_round7.csv"))
if not rows or not scan:
    raise SystemExit("缺少第七轮回归 CSV")

data = {
    "era1": summarize(rows, 1),
    "era2": summarize(rows, 2),
    "scan": {
        "n": len(scan),
        "era1": {"mean": round(mean([r["ep1"] for r in scan]), 2),
                 "std": round(pstdev([r["ep1"] for r in scan]), 2),
                 "min": min(r["ep1"] for r in scan), "max": max(r["ep1"] for r in scan),
                 "hist": dist([r["ep1"] for r in scan]),
                 "last": round(mean([r["last1"] for r in scan]), 1)},
        "era2": {"mean": round(mean([r["ep2"] for r in scan]), 2),
                 "std": round(pstdev([r["ep2"] for r in scan]), 2),
                 "min": min(r["ep2"] for r in scan), "max": max(r["ep2"] for r in scan),
                 "hist": dist([r["ep2"] for r in scan]),
                 "last": round(mean([r["last2"] for r in scan]), 1)},
        "balance": {
            "atp": round(mean([r["atp2"] for r in scan]), 0),
            "biomass": round(mean([r["bio_end2"] for r in scan]), 0),
            "dna": round(mean([r["dna2"] for r in scan]), 0),
        },
    },
    "seeds": [{"index": r["index"], "ep1": r["ep1"], "ep2": r["ep2"],
               "last1": round(r["last1"], 1), "last2": round(r["last2"], 1)} for r in rows],
    "species": [{"id": sid, "name": SPECIES[sid], "era1": sid in sp1, "era2": sid in sp2}
                for sid in sorted(SPECIES)],
    "grid": [
        {"dna": 0.25, "growth": 1.12, "ep1": "29.0 ± 4.9", "mut": "26.3", "last": "592s"},
        {"dna": 0.25, "growth": 1.15, "ep1": "25.7 ± 4.8", "mut": "23.5", "last": "586s"},
        {"dna": 0.25, "growth": 1.20, "ep1": "21.8 ± 4.5", "mut": "18.5", "last": "575s"},
        {"dna": 0.25, "growth": 1.25, "ep1": "19.6 ± 3.6", "mut": "14.2", "last": "571s"},
        {"dna": 0.50, "growth": 1.15, "ep1": "37.7 ± 2.0", "mut": "27.0", "last": "593s"},
        {"dna": 0.50, "growth": 1.20, "ep1": "35.4 ± 2.6", "mut": "27.0", "last": "595s"},
    ],
    "events": [
        {"name": "Industrial Smog", "trait": "Toxin", "duration": "45s",
         "effect": "危机；阶段 5 后出现，技术圈阶段的额外压力"},
    ],
    "routes": [
        {"name": "Sapient Life", "stage": 6, "parents": "Cognitive Biosphere",
         "mutation": "Abstract Thought", "cost": 5400},
        {"name": "Technosphere", "stage": 7, "parents": "Sapient Life",
         "mutation": "Planetary Industry", "cost": 8100},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND7 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"era1 EP {data['era1']['ep']['mean']} ± {data['era1']['ep']['std']}  last {data['era1']['lastPurchase']['mean']}")
print(f"era2 EP {data['era2']['ep']['mean']} ± {data['era2']['ep']['std']}  last {data['era2']['lastPurchase']['mean']}")
print(f"scan era1 {data['scan']['era1']['mean']} ± {data['scan']['era1']['std']}  last {data['scan']['era1']['last']}")
print(f"scan era2 {data['scan']['era2']['mean']} ± {data['scan']['era2']['std']}  last {data['scan']['era2']['last']}")
print("era2 species:", sorted(sp2))
