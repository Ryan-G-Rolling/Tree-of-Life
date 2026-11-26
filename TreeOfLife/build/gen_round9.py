"""Build assets/round9.js for the ninth-round knowledge & sub-upgrade section."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round9.js")


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


def median(v):
    return statistics.median(v) if v else 0.0


s2 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch2_scan_round9.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH2,")]
s3 = [l.strip().split(",") for l in open(os.path.join(HERE, "batch3_scan_round9.csv"),
                                         encoding="utf-8-sig") if l.startswith("BATCH3,")]
bsub = [l.strip().split(",") for l in open(os.path.join(HERE, "batch3_scan_round9.csv"),
                                           encoding="utf-8-sig") if l.startswith("BSUB,")]
if not s2 or not s3:
    raise SystemExit("缺少第九轮回归 CSV")

# BSUB,index,era,time,name,cost,leftover —— 只统计纪元 3 的购买时点
tier_times = {}
for row in bsub:
    if int(row[2]) != 3:
        continue
    tier_times.setdefault(row[4], []).append(float(row[3]))
tier_timeline = [{"name": name, "median_t": round(median(times), 1), "n": len(times)}
                 for name, times in sorted(tier_times.items(),
                                           key=lambda item: median(item[1]))]

data = {
    "species": [
        {"name": "Sapient Life", "stage": 6, "base": 0.10},
        {"name": "Technosphere", "stage": 7, "base": 0.22},
        {"name": "Civilization", "stage": 8, "base": 0.45},
        {"name": "Space Age", "stage": 9, "base": 0.90},
        {"name": "Singularity", "stage": 10, "base": 1.80},
    ],
    "tiers": [
        {"species": "Sapient Life", "name": "Agriculture", "mult": 1.5, "cost": 100},
        {"species": "Sapient Life", "name": "Writing", "mult": 1.5, "cost": 500},
        {"species": "Sapient Life", "name": "Philosophy", "mult": 2.0, "cost": 2500},
        {"species": "Technosphere", "name": "Steam Power", "mult": 1.6, "cost": 300},
        {"species": "Technosphere", "name": "Electricity", "mult": 1.6, "cost": 1500},
        {"species": "Technosphere", "name": "Computing", "mult": 2.0, "cost": 7500},
        {"species": "Civilization", "name": "Printing Press", "mult": 1.7, "cost": 1000},
        {"species": "Civilization", "name": "Scientific Method", "mult": 1.7, "cost": 5000},
        {"species": "Civilization", "name": "Mass Media", "mult": 2.0, "cost": 25000},
        {"species": "Space Age", "name": "Internet", "mult": 1.8, "cost": 3000},
        {"species": "Space Age", "name": "Automation", "mult": 1.8, "cost": 15000},
        {"species": "Space Age", "name": "Fusion Power", "mult": 2.0, "cost": 75000},
        {"species": "Singularity", "name": "Nanotechnology", "mult": 2.0, "cost": 10000},
        {"species": "Singularity", "name": "Quantum Computing", "mult": 2.0, "cost": 50000},
        {"species": "Singularity", "name": "Self-Replication", "mult": 2.5, "cost": 250000},
    ],
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
        "stage_median": {
            "era1": median([int(x[4]) for x in s2]),
            "era2": median([int(x[10]) for x in s2]),
            "era3": median([int(x[6]) for x in s3]),
        },
        "era3_top": round(100.0 * sum(1 for x in s3 if int(x[6]) == 10) / len(s3), 1),
        "knowledge": {
            "era2_median": round(median([float(x[24]) for x in s2]), 0),
            "era3_median": round(median([float(x[15]) for x in s3]), 0),
            "era3_box": box([float(x[15]) for x in s3]),
        },
        "subs": {
            "era2_median": median([int(x[26]) for x in s2]),
            "era2_max": max(int(x[26]) for x in s2),
            "era3_median": median([int(x[18]) for x in s3]),
            "era3_max": max(int(x[18]) for x in s3),
            "era3_dist": dist([int(x[18]) for x in s3]),
        },
    },
    "tier_timeline": tier_timeline,
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND9 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("era1 EP", data["scan"]["ep"]["era1"])
print("era2 EP", data["scan"]["ep"]["era2"])
print("era3 EP", data["scan"]["ep"]["era3"])
print("era3 top", data["scan"]["era3_top"], "%  subs median", data["scan"]["subs"]["era3_median"])
print("tier timeline:", [(t["name"], t["median_t"]) for t in tier_timeline])
