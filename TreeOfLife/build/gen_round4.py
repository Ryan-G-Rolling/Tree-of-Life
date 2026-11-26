"""Build assets/round4.js for the fourth-round content expansion report section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round4.js")
SPECIES = {
    1: "Cyanobacterium",
    2: "Chemolithotroph",
    3: "Algal Eukaryote",
    4: "Protozoan Predator",
    5: "Multicellular Colony",
    6: "Extremophile Colony",
    7: "Differentiated Tissue",
    8: "Symbiotic Superorganism",
    9: "Cognitive Biosphere",
    10: "Fungal Network",
    11: "Coral Symbiont",
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


def parse():
    rows, era1_species, era2_species = [], set(), set()
    path = os.path.join(HERE, "batch2_round4.csv")
    with open(path, "r", encoding="utf-8-sig") as handle:
        for line in handle:
            p = line.strip().split(",")
            if p[0] == "BATCH2":
                rows.append({
                    "index": int(p[1]), "seed": int(p[2]),
                    "ep1": int(p[3]), "st1": int(p[4]), "mut1": int(p[5]),
                    "sp1": int(p[6]), "cr1": int(p[7]), "bio1": float(p[8]),
                    "ep2": int(p[9]), "st2": int(p[10]), "mut2": int(p[11]),
                    "sp2": int(p[12]), "cr2": int(p[13]), "bio2": float(p[14]),
                    "legacy_mutation": int(p[15]), "legacy_diversification": int(p[16]),
                    "first_spe2": float(p[17]), "first_spe1": float(p[18]),
                })
            elif p[0] == "BSPEC1":
                era1_species.add(int(p[2]))
            elif p[0] == "BSPEC2":
                era2_species.add(int(p[2]))
    return rows, era1_species, era2_species


def summarize(rows, keys):
    eps = [r[keys["ep"]] for r in rows]
    bio = [r[keys["bio"]] for r in rows]
    return {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "box": box(eps)},
        "biomass": {"mean": round(mean(bio), 0), "box": box(bio)},
        "stages": {"mean": round(mean([r[keys["st"]] for r in rows]), 1)},
        "mutations": {"mean": round(mean([r[keys["mut"]] for r in rows]), 1)},
        "species": {"mean": round(mean([r[keys["sp"]] for r in rows]), 1)},
        "crises": {"mean": round(mean([r[keys["cr"]] for r in rows]), 1)},
        "firstSpeciation": {
            "mean": round(mean([r[keys["first"]] for r in rows]), 1),
            "box": box([r[keys["first"]] for r in rows]),
        },
    }


rows, era1_species, era2_species = parse()
if not rows:
    raise SystemExit("batch2_round4.csv 缺少 BATCH2 行")

data = {
    "era1": summarize(rows, {"ep": "ep1", "bio": "bio1", "sp": "sp1", "mut": "mut1",
                             "st": "st1", "cr": "cr1", "first": "first_spe1"}),
    "era2": summarize(rows, {"ep": "ep2", "bio": "bio2", "sp": "sp2", "mut": "mut2",
                             "st": "st2", "cr": "cr2", "first": "first_spe2"}),
    "legacy": {
        "mutation": rows[0]["legacy_mutation"],
        "diversification": rows[0]["legacy_diversification"],
    },
    "seeds": [{
        "index": r["index"], "ep1": r["ep1"], "bio1": round(r["bio1"], 0),
        "ep2": r["ep2"], "bio2": round(r["bio2"], 0), "firstSpe2": round(r["first_spe2"], 1),
    } for r in rows],
    "ladder": [
        {"label": "第二轮", "E": 10, "M": 5, "S": 2, "C": 5.6, "B": 10, "total": 32.6},
        {"label": "第三轮 · 纪元 2", "E": 10, "M": 6, "S": 4, "C": 6, "B": 10, "total": 36},
        {"label": "第四轮 · 纪元 1", "E": 10, "M": 7, "S": 2, "C": 6, "B": 10, "total": 35},
        {"label": "第四轮 · 纪元 2", "E": 10, "M": 7, "S": 5, "C": 6, "B": 10, "total": 38},
    ],
    "species": [{"id": sid, "name": SPECIES[sid],
                 "era1": sid in era1_species, "era2": sid in era2_species}
                for sid in sorted(era1_species | era2_species)],
    "events": [
        {"name": "Nutrient Upwelling", "kind": "正面", "effect": "全谱系产出 ×1.10"},
        {"name": "Thermal Surge", "kind": "正面", "effect": "耐热 ≥1 的谱系 ×1.25，其余 ×1.10"},
        {"name": "Spore Cloud", "kind": "正面", "effect": "耐毒 ≥1 的谱系 ×1.25，其余 ×1.10"},
    ],
    "mutations": [
        {"name": "Decomposer Enzymes", "effect": "Biomass +25%、DNA +10%，解锁真菌网络"},
        {"name": "Photosymbiosis", "effect": "ATP +20%、Biomass +15%、Cap +10%，解锁珊瑚共生体"},
        {"name": "Chitin Wall", "effect": "Cap +30%、Biomass +15%"},
        {"name": "Nitrogen Sense", "effect": "DNA +25%、增长 +4%"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND4 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"era1 EP {data['era1']['ep']['mean']}  bio {data['era1']['biomass']['mean']:.0f}")
print(f"era2 EP {data['era2']['ep']['mean']}  bio {data['era2']['biomass']['mean']:.0f}")
print(f"era2 species {data['era2']['species']['mean']}  mutations {data['era2']['mutations']['mean']}")
print("era1 species:", sorted(era1_species))
print("era2 species:", sorted(era2_species))
