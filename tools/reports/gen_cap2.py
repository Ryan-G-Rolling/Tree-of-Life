"""Build assets/cap2.js for the EP/Biomass cap-doubling experiment."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "cap2.js")

SCENARIOS = [
    ("base", "基准参数", "价格 1.35/1.40/1.40 · 危机线 25% · 保底 60%"),
    ("price_cheap", "升级更便宜", "价格增长率 1.15/1.20/1.20"),
    ("price_steep", "升级更贵", "价格增长率 1.55/1.60/1.60"),
    ("no_guarantee", "取消关键保底", "保底阈值 0%"),
    ("crisis_075", "危机线收紧到 75%", "通过线 0.75（基准 0.25）"),
]

OLD_MILESTONES = [0, 100, 500, 2000, 10000, 50000]
NEW_MILESTONES = [0, 100, 500, 2000, 10000, 50000, 60000, 70000, 80000, 90000, 100000]


def quantile(values, p):
    data = sorted(values)
    if len(data) == 1:
        return data[0]
    pos = (len(data) - 1) * p
    lo = int(pos)
    hi = min(lo + 1, len(data) - 1)
    return data[lo] + (data[hi] - data[lo]) * (pos - lo)


def box(values):
    return [min(values), quantile(values, 0.25), quantile(values, 0.5),
            quantile(values, 0.75), max(values)]


def mean(values):
    return sum(values) / len(values) if values else 0.0


def milestone(max_biomass, table):
    level = 0
    for i in range(1, len(table)):
        if max_biomass >= table[i]:
            level = i
    return level


def parse(path):
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if not line.startswith("BATCH,"):
                continue
            p = line.strip().split(",")
            rows.append({
                "seed": int(p[2]),
                "ep": int(p[3]),
                "stages": int(p[4]),
                "mutations": int(p[5]),
                "species": int(p[6]),
                "crises": int(p[7]),
                "maxBiomass": float(p[8]),
                "firstSpe": float(p[9]),
            })
    return rows


def summarize(rows, table):
    eps = [r["ep"] for r in rows]
    bio = [r["maxBiomass"] for r in rows]
    spe = [r["firstSpe"] for r in rows]
    return {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "min": min(eps), "max": max(eps),
               "box": [round(v, 2) for v in box(eps)]},
        "biomass": {"mean": round(mean(bio), 0), "min": round(min(bio), 0),
                    "max": round(max(bio), 0), "box": [round(v, 0) for v in box(bio)]},
        "speciation": {"mean": round(mean(spe), 1), "box": [round(v, 1) for v in box(spe)]},
        "milestone": round(mean([milestone(r["maxBiomass"], table) for r in rows]), 2),
    }


# 对照基线：调参后、内容扩容前的 10 个配对种子（1.35/1.40/1.40）
old_base = summarize(parse(os.path.join(HERE, "sens_opt_c.csv")), OLD_MILESTONES)

scenarios = []
base = parse(os.path.join(HERE, "sens2_base.csv"))
for key, label, detail in SCENARIOS:
    rows = parse(os.path.join(HERE, f"sens2_{key}.csv"))
    if not rows:
        continue
    item = summarize(rows, NEW_MILESTONES)
    item.update({"key": key, "label": label, "detail": detail, "runs": rows})
    item["deltaEp"] = round(item["ep"]["mean"] - old_base["ep"]["mean"], 2)
    item["deltaBio"] = round(item["biomass"]["mean"] - old_base["biomass"]["mean"], 0)
    scenarios.append(item)

breakdown = {
    # 旧内容实测拆解：E3 + M10/3 + S3/2 + C4(截 3) + B5 = 18
    "old": [
        {"name": "阶段 E", "value": 6},
        {"name": "突变 M", "value": 3},
        {"name": "物种 S", "value": 1},
        {"name": "危机 C", "value": 3},
        {"name": "Biomass B", "value": 5},
    ],
    "new": [
        {"name": "阶段 E", "value": 10},
        {"name": "突变 M", "value": 6},
        {"name": "物种 S", "value": 4},
        {"name": "危机 C", "value": 6},
        {"name": "Biomass B", "value": 10},
    ],
}

data = {
    "epCap": 36,
    "biomassCap": 100000,
    "species": 10,
    "mutations": 18,
    "milestones": 10,
    "crisisCap": 6,
    "oldBase": old_base,
    "scenarios": scenarios,
    "breakdown": breakdown,
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.CAP2 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("old base:", old_base["ep"]["mean"], old_base["biomass"]["mean"])
for s in scenarios:
    print(f"{s['label']:<14} EP {s['ep']['mean']:>5.2f}  bio {s['biomass']['mean']:>9.0f}  "
          f"spe {s['speciation']['mean']:>6.1f}")
