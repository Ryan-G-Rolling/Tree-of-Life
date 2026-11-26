"""Build assets/tuned.js for the second-round parameter tuning report."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "tuned.js")

SCENARIOS = [
    ("base", "调参后基准", "间隔 30s · 时长 40% · 危机线 8%"),
    ("event_sparse", "危机更稀疏", "间隔 45s · 时长 50%"),
    ("event_dense", "危机更密集", "间隔 20s · 时长 35%"),
    ("price_steep", "升级更贵", "价格 1.15/1.20/1.20"),
    ("crisis_tight", "危机线收紧", "通过线 30%"),
]


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


def parse(name):
    path = os.path.join(HERE, name)
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if not line.startswith("BATCH,"):
                continue
            p = line.strip().split(",")
            rows.append({
                "seed": int(p[2]), "ep": int(p[3]), "stages": int(p[4]),
                "mutations": int(p[5]), "species": int(p[6]), "crises": int(p[7]),
                "maxBiomass": float(p[8]), "firstSpe": float(p[9]),
            })
    return rows


def summarize(rows):
    eps = [r["ep"] for r in rows]
    bio = [r["maxBiomass"] for r in rows]
    spe = [r["firstSpe"] for r in rows]
    return {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "box": box(eps)},
        "biomass": {"mean": round(mean(bio), 0), "box": box(bio)},
        "speciation": {"mean": round(mean(spe), 1), "box": box(spe)},
        "crises": {"mean": round(mean([r["crises"] for r in rows]), 2)},
        "mutations": {"mean": round(mean([r["mutations"] for r in rows]), 1)},
        "species": {"mean": round(mean([r["species"] for r in rows]), 1)},
        "stages": {"mean": round(mean([r["stages"] for r in rows]), 1)},
    }


before = summarize(parse("sens2_base.csv"))
after = summarize(parse("tuned_base.csv"))

scenarios = []
for key, label, detail in SCENARIOS:
    rows = parse(f"tuned_{key}.csv")
    if not rows:
        continue
    item = summarize(rows)
    item.update({"key": key, "label": label, "detail": detail})
    item["deltaEp"] = round(item["ep"]["mean"] - before["ep"]["mean"], 2)
    item["deltaBio"] = round(item["biomass"]["mean"] - before["biomass"]["mean"], 0)
    scenarios.append(item)

params = [
    {"name": "首个危机", "old": "90s", "new": "60s"},
    {"name": "危机间隔", "old": "75s", "new": "30s"},
    {"name": "危机时长", "old": "100%", "new": "40%"},
    {"name": "危机通过线", "old": "25%", "new": "8%"},
    {"name": "新物种分化成本", "old": "100%", "new": "45%"},
    {"name": "新突变 DNA 成本", "old": "100%", "new": "70%"},
    {"name": "升级价格增长", "old": "1.35 / 1.40 / 1.40", "new": "1.00 / 1.05 / 1.05"},
]

data = {"before": before, "after": after, "scenarios": scenarios, "params": params}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.TUNED = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"before EP {before['ep']['mean']} bio {before['biomass']['mean']:.0f}")
print(f"after  EP {after['ep']['mean']} bio {after['biomass']['mean']:.0f}")
for s in scenarios:
    print(f"{s['label']:<12} EP {s['ep']['mean']:>5.2f}  bio {s['biomass']['mean']:>9.0f}")
