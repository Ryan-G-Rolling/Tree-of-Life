"""Build before/after optimization data from two sensitivity exports."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "optimization.js")


def load(path):
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
    return items


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


def summarize(items, label, detail):
    eps = [x["ep"] for x in items]
    speeds = [x["firstSpe"] for x in items]
    biomass = [x["maxBiomass"] for x in items]
    crises = [x["crises"] for x in items]
    return {
        "label": label,
        "detail": detail,
        "runs": items,
        "ep": {
            "mean": round(sum(eps) / len(eps), 2),
            "min": min(eps),
            "max": max(eps),
            "box": quartiles(eps),
        },
        "speciation": {
            "mean": round(sum(speeds) / len(speeds), 1),
            "box": [round(v, 1) for v in quartiles(speeds)],
        },
        "biomass": {
            "mean": round(sum(biomass) / len(biomass), 0),
            "min": round(min(biomass), 0),
            "max": round(max(biomass), 0),
        },
        "crises": {"mean": round(sum(crises) / len(crises), 2)},
    }


before = summarize(
    load(os.path.join(HERE, "sens_before.csv")),
    "调参前",
    "价格增长率 1.65 / 1.70 / 1.75",
)
after = summarize(
    load(os.path.join(HERE, "sens_opt_c.csv")),
    "调参后",
    "价格增长率 1.35 / 1.40 / 1.40",
)

data = {"before": before, "after": after}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.OPT = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("label      meanEP  meanSpe  meanBio  crises")
for item in (before, after):
    print(
        f"{item['label']:<10}{item['ep']['mean']:>6.2f}{item['speciation']['mean']:>9.1f}"
        f"{item['biomass']['mean']:>9.0f}{item['crises']['mean']:>8.2f}"
    )
print(
    "biomass gain:",
    round((after["biomass"]["mean"] / before["biomass"]["mean"] - 1) * 100, 1),
    "%",
)
