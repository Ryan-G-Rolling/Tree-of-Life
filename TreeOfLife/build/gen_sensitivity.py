"""Build sensitivity data from per-scenario CSV exports."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "sensitivity.js")

SCENARIOS = [
    ("base", "基准参数", "通过线 25% · 价格 1.65/1.70/1.75 · 保底 60%"),
    ("price_cheap", "升级更便宜", "价格增长率 1.45/1.50/1.55"),
    ("price_steep", "升级更贵", "价格增长率 1.85/1.90/1.95"),
    ("no_guarantee", "取消关键保底", "保底阈值 0%"),
    ("crisis_075", "危机线收紧到 75%", "通过线 0.75（基准 0.25）"),
]


def quartiles(values):
    data = sorted(values)
    count = len(data)

    def quantile(p):
        if count == 1:
            return data[0]
        position = (count - 1) * p
        lower = int(position)
        upper = min(lower + 1, count - 1)
        fraction = position - lower
        return data[lower] + (data[upper] - data[lower]) * fraction

    return [data[0], quantile(0.25), quantile(0.5), quantile(0.75), data[-1]]


runs = {}
for key, label, detail in SCENARIOS:
    path = os.path.join(HERE, f"sens_{key}.csv")
    if not os.path.exists(path):
        continue
    items = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line.startswith("BATCH,"):
                continue
            parts = line.split(",")
            items.append({
                "i": int(parts[1]),
                "seed": int(parts[2]),
                "ep": int(parts[3]),
                "stages": int(parts[4]),
                "mutations": int(parts[5]),
                "species": int(parts[6]),
                "crises": int(parts[7]),
                "maxBiomass": float(parts[8]),
                "firstSpe": float(parts[9]),
                "firstSpecies": int(parts[10]),
            })
    runs[key] = items

base = runs.get("base", [])


def mean(values):
    return sum(values) / len(values) if values else 0.0


data = {"scenarios": []}
for key, label, detail in SCENARIOS:
    items = runs.get(key, [])
    if not items:
        continue
    eps = [x["ep"] for x in items]
    speeds = [x["firstSpe"] for x in items]
    biomass = [x["maxBiomass"] for x in items]
    crises = [x["crises"] for x in items]
    data["scenarios"].append({
        "key": key,
        "label": label,
        "detail": detail,
        "runs": items,
        "ep": {
            "mean": round(mean(eps), 2),
            "min": min(eps),
            "max": max(eps),
            "box": quartiles(eps),
        },
        "speciation": {
            "mean": round(mean(speeds), 1),
            "min": round(min(speeds), 1),
            "max": round(max(speeds), 1),
            "box": [round(v, 1) for v in quartiles(speeds)],
        },
        "biomass": {
            "mean": round(mean(biomass), 0),
            "min": round(min(biomass), 0),
            "max": round(max(biomass), 0),
        },
        "crises": {"mean": round(mean(crises), 2)},
        "deltaEp": round(mean(eps) - mean([x["ep"] for x in base]), 2) if base else 0.0,
        "deltaSpe": round(mean(speeds) - mean([x["firstSpe"] for x in base]), 1) if base else 0.0,
    })

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.SENS = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("scenario            meanEP   dEP    meanSpe   dSpe   meanBio   crises")
for item in data["scenarios"]:
    print(
        f"{item['label']:<18}{item['ep']['mean']:>7.2f}{item['deltaEp']:>7.2f}"
        f"{item['speciation']['mean']:>9.1f}{item['deltaSpe']:>8.1f}"
        f"{item['biomass']['mean']:>11.0f}{item['crises']['mean']:>8.2f}"
    )
