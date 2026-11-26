"""Build assets/round3.js for the third-round EP 34/36 report section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round3.js")
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
    path = os.path.join(HERE, "batch2_base.csv")
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
                    "first_spe2": float(p[17]),
                    "first_spe1": float(p[18]),
                })
            elif p[0] == "BSPEC1":
                era1_species.add(int(p[2]))
            elif p[0] == "BSPEC2":
                era2_species.add(int(p[2]))
    return rows, era1_species, era2_species


def summarize(rows, ep_key, bio_key, sp_key, mut_key, st_key, cr_key, first_key=None):
    eps = [r[ep_key] for r in rows]
    bio = [r[bio_key] for r in rows]
    item = {
        "n": len(rows),
        "ep": {"mean": round(mean(eps), 2), "box": box(eps)},
        "biomass": {"mean": round(mean(bio), 0), "box": box(bio)},
        "stages": {"mean": round(mean([r[st_key] for r in rows]), 1)},
        "mutations": {"mean": round(mean([r[mut_key] for r in rows]), 1)},
        "species": {"mean": round(mean([r[sp_key] for r in rows]), 1)},
        "crises": {"mean": round(mean([r[cr_key] for r in rows]), 1)},
    }
    if first_key:
        item["firstSpeciation"] = {
            "mean": round(mean([r[first_key] for r in rows]), 1),
            "box": box([r[first_key] for r in rows]),
        }
    return item


rows, era1_species, era2_species = parse()
if not rows:
    raise SystemExit("batch2_base.csv 缺少 BATCH2 行")

data = {
    "era1": summarize(rows, "ep1", "bio1", "sp1", "mut1", "st1", "cr1", "first_spe1"),
    "era2": summarize(rows, "ep2", "bio2", "sp2", "mut2", "st2", "cr2", "first_spe2"),
    "legacy": {
        "mutation": rows[0]["legacy_mutation"],
        "diversification": rows[0]["legacy_diversification"],
    },
    "seeds": [{
        "index": r["index"], "ep1": r["ep1"], "bio1": round(r["bio1"], 0),
        "ep2": r["ep2"], "bio2": round(r["bio2"], 0), "firstSpe2": round(r["first_spe2"], 1),
    } for r in rows],
    # EP 构成：第二轮基准为 10 个种子的均值（C=5.6），第三轮两项均稳定到整数
    "ladder": [
        {"label": "第二轮基准", "era": "纪元 1", "E": 10, "M": 5, "S": 2, "C": 5.6, "B": 10,
         "total": 32.6},
        {"label": "纪元 1", "era": "第三轮", "E": 10, "M": 6, "S": 2, "C": 6, "B": 10,
         "total": 34},
        {"label": "纪元 2", "era": "第三轮", "E": 10, "M": 6, "S": 4, "C": 6, "B": 10,
         "total": 36},
    ],
    "species": [{"id": sid, "name": SPECIES[sid],
                 "era1": sid in era1_species, "era2": sid in era2_species}
                for sid in sorted(era2_species | era1_species)],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND3 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"era1 EP {data['era1']['ep']['mean']}  bio {data['era1']['biomass']['mean']:.0f}")
print(f"era2 EP {data['era2']['ep']['mean']}  bio {data['era2']['biomass']['mean']:.0f}")
print(f"era2 species {data['era2']['species']['mean']}  mutations {data['era2']['mutations']['mean']}")
print("era1 species:", sorted(era1_species))
print("era2 species:", sorted(era2_species))
