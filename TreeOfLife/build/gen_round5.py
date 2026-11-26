"""Build assets/round5.js for the fifth-round rebalance report section."""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round5.js")


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
    if not values:
        return 0.0
    m = mean(values)
    return (sum((v - m) ** 2 for v in values) / len(values)) ** 0.5


def parse(path):
    rows = []
    with open(path, "r", encoding="utf-8-sig") as handle:
        for line in handle:
            if line.startswith("BATCH2,"):
                p = line.strip().split(",")
                rows.append({
                    "index": int(p[1]),
                    "ep1": int(p[3]), "mut1": int(p[5]), "sp1": int(p[6]),
                    "cr1": int(p[7]), "bio1": float(p[8]),
                    "ep2": int(p[9]), "mut2": int(p[11]), "sp2": int(p[12]),
                    "cr2": int(p[13]), "bio2": float(p[14]),
                    "first2": float(p[17]), "first1": float(p[18]),
                })
    return rows


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


rows = parse(os.path.join(HERE, "batch2_round5.csv"))
scan = parse(os.path.join(HERE, "batch2_scan1000.csv"))
if not rows or not scan:
    raise SystemExit("缺少 batch2_round5.csv 或 batch2_scan1000.csv")

data = {
    "era1": summarize(rows, "ep1", "bio1", "sp1", "mut1", "cr1", "first1"),
    "era2": summarize(rows, "ep2", "bio2", "sp2", "mut2", "cr2", "first2"),
    "before": {"era1": 35, "era2": 38},
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
    "legacy": {
        "maxLevel": 8,
        "costPerLine": 36,
        "total": 144,
        "effects": [
            {"line": "Metabolism", "lv5": "Biomass ×1.05", "lv6": "点击 ×1.5",
             "lv7": "新轮起始 Biomass 100", "lv8": "全局产率 ×1.10"},
            {"line": "Mutation", "lv5": "关键突变保底 60%→40%", "lv6": "候选 3→4",
             "lv7": "DNA 折扣 12%→20%", "lv8": "稀有权重 40→60"},
            {"line": "Adaptation", "lv5": "每轮一次保护", "lv6": "危机伤害 −15%",
             "lv7": "正面事件时长 +50%", "lv8": "减伤 80%→95%"},
            {"line": "Diversification", "lv5": "第三槽 + 极端环境", "lv6": "种群上限 ×1.15",
             "lv7": "协同 1.10→1.15", "lv8": "分化成本再 −10%"},
        ],
    },
    "ladder": [
        {"label": "第四轮 · 纪元 1", "E": 10, "M": 7, "S": 2, "C": 6, "B": 7, "total": 32},
        {"label": "第四轮 · 纪元 2", "E": 10, "M": 7, "S": 5, "C": 6, "B": 9, "total": 37},
        {"label": "旧上限参考", "E": 10, "M": 7, "S": 5, "C": 6, "B": 10, "total": 38},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND5 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print(f"era1 EP {data['era1']['ep']['mean']} ± {data['era1']['ep']['std']}")
print(f"era2 EP {data['era2']['ep']['mean']} ± {data['era2']['ep']['std']}")
print(f"scan era1 {data['scan']['era1']['mean']} ± {data['scan']['era1']['std']}")
print(f"scan era2 {data['scan']['era2']['mean']} ± {data['scan']['era2']['std']}")
