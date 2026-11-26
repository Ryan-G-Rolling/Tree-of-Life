"""Build assets/round8.js for the eighth-round long-route section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round8.js")
SPECIES = {
    1: "Cyanobacterium", 2: "Chemolithotroph", 3: "Algal Eukaryote",
    4: "Protozoan Predator", 5: "Multicellular Colony", 6: "Extremophile Colony",
    7: "Differentiated Tissue", 8: "Symbiotic Superorganism", 9: "Cognitive Biosphere",
    10: "Fungal Network", 11: "Coral Symbiont", 12: "Mycorrhizal Web",
    13: "Reef Metropolis", 14: "Sapient Life", 15: "Technosphere",
    16: "Civilization", 17: "Space Age", 18: "Singularity",
}


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

    return [round(min(d), 2), round(q(0.25), 2), round(q(0.5), 2), round(q(0.75), 2),
            round(max(d), 2)]


def dist(values):
    counts = {}
    for x in values:
        counts[x] = counts.get(x, 0) + 1
    return [{"value": k, "count": counts[k]} for k in sorted(counts)]


b2 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch2_round8.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH2,")]
b3 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch3_round8.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH3,")]
s2 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch2_scan_round8.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH2,")]
s3 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch3_scan_round8.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH3,")]
if not b2 or not b3 or not s2 or not s3:
    raise SystemExit("缺少第八轮回归 CSV")

data = {
    "era1": {
        "n": len(b2),
        "ep": {"mean": round(mean([int(x[3]) for x in b2]), 2),
               "std": round(pstdev([int(x[3]) for x in b2]), 2)},
        "stage": {"mean": round(mean([int(x[4]) for x in b2]), 2)},
        "mutations": round(mean([int(x[5]) for x in b2]), 1),
        "species": round(mean([int(x[6]) for x in b2]), 1),
        "last": round(mean([float(x[19]) for x in b2]), 1),
    },
    "era2": {
        "n": len(b2),
        "ep": {"mean": round(mean([int(x[9]) for x in b2]), 2),
               "std": round(pstdev([int(x[9]) for x in b2]), 2)},
        "stage": {"mean": round(mean([int(x[10]) for x in b2]), 2)},
        "mutations": round(mean([int(x[11]) for x in b2]), 1),
        "species": round(mean([int(x[12]) for x in b2]), 1),
        "last": round(mean([float(x[20]) for x in b2]), 1),
    },
    "era3": {
        "n": len(b3),
        "ep": {"mean": round(mean([int(x[5]) for x in b3]), 2),
               "std": round(pstdev([int(x[5]) for x in b3]), 2)},
        "stage": {"mean": round(mean([int(x[6]) for x in b3]), 2)},
        "mutations": round(mean([int(x[7]) for x in b3]), 1),
        "species": round(mean([int(x[8]) for x in b3]), 1),
        "last": round(mean([float(x[11]) for x in b3]), 1),
    },
    "scan": {
        "n2": len(s2), "n3": len(s3),
        "ep": {
            "era1": {"mean": round(mean([int(x[3]) for x in s2]), 2),
                     "std": round(pstdev([int(x[3]) for x in s2]), 2),
                     "box": box([int(x[3]) for x in s2])},
            "era2": {"mean": round(mean([int(x[9]) for x in s2]), 2),
                     "std": round(pstdev([int(x[9]) for x in s2]), 2),
                     "box": box([int(x[9]) for x in s2])},
            "era3": {"mean": round(mean([int(x[5]) for x in s3]), 2),
                     "std": round(pstdev([int(x[5]) for x in s3]), 2),
                     "box": box([int(x[5]) for x in s3])},
        },
        "stage": {
            "era1": dist([int(x[4]) for x in s2]),
            "era2": dist([int(x[10]) for x in s2]),
            "era3": dist([int(x[6]) for x in s3]),
        },
        "era3_top": round(100.0 * sum(1 for x in s3 if int(x[6]) == 10) / len(s3), 1),
    },
    "grid": [
        {"growth": 1.15, "ep1": "25.9 ± 5.2", "stage1": "5.1", "stage2": "9.8"},
        {"growth": 1.18, "ep1": "23.1 ± 5.1", "stage1": "4.3", "stage2": "9.0"},
        {"growth": 1.22, "ep1": "20.9 ± 4.2", "stage1": "3.7", "stage2": "7.5"},
    ],
    "route": [
        {"name": "Civilization", "stage": 8, "parent": "Technosphere",
         "mutation": "Language", "cost": 54000},
        {"name": "Space Age", "stage": 9, "parent": "Civilization",
         "mutation": "Rocketry", "cost": 180000},
        {"name": "Singularity", "stage": 10, "parent": "Space Age",
         "mutation": "Recursive Self-Improvement", "cost": 540000},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND8 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

for era in ("era1", "era2", "era3"):
    item = data[era]
    print(f"{era} EP {item['ep']['mean']} ± {item['ep']['std']}  stage {item['stage']['mean']}")
print(f"scan era3 top {data['scan']['era3_top']}%")
