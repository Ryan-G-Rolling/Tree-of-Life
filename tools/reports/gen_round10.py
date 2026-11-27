"""Build assets/round10.js for the tenth-round pacing & economy section."""

import json
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "round10.js")


def load(name, tag):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    return [l.strip().split(",") for l in open(path, encoding="utf-8-sig") if l.startswith(tag)]


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

    return [round(min(d), 1), round(q(0.25), 1), round(q(0.5), 1), round(q(0.75), 1),
            round(max(d), 1)]


def dist(values):
    counts = {}
    for x in values:
        counts[x] = counts.get(x, 0) + 1
    return [{"value": k, "count": counts[k]} for k in sorted(counts)]


b2 = load("batch2_scan_round10.csv", "BATCH2")
b3 = load("batch3_scan_round10.csv", "BATCH3")
stage_rows = load("batch3_scan_round10.csv", "STAGE")
sub_rows = load("batch3_scan_round10.csv", "BSUB")

ci = lambda rows, i: [int(r[i]) for r in rows]
cf = lambda rows, i: [float(r[i]) for r in rows]

stage3 = ci(b3, 6)
ep3 = ci(b3, 5)
ep2 = ci(b2, 9)
ep1 = ci(b2, 3)
stage1 = ci(b2, 4)
stage2 = ci(b2, 10)

# 阶段 6 进入 -> 首个理念升级
enter6, first_sub = {}, {}
for r in stage_rows:
    if int(r[2]) == 3 and int(r[4]) >= 6:
        enter6.setdefault(int(r[1]), float(r[3]))
for r in sub_rows:
    if int(r[2]) == 3:
        first_sub.setdefault(int(r[1]), float(r[3]))
latency = sorted(first_sub[i] - enter6[i] for i in enter6 if i in first_sub)


def grid_growth(growth, tag):
    g2 = load(f"final10_g{tag}_b2.csv", "BATCH2")
    g3 = load(f"final10_g{tag}_b3.csv", "BATCH3")
    s3 = [int(r[6]) for r in g3]
    return {
        "growth": growth,
        "stage1": statistics.median([int(r[4]) for r in g2]),
        "stage2": statistics.median([int(r[10]) for r in g2]),
        "ep1": statistics.median([int(r[3]) for r in g2]),
        "ep3": statistics.median([int(r[5]) for r in g3]),
        "top3": round(100.0 * sum(1 for s in s3 if s == 10) / len(s3), 1),
    }


def grid_spec(scale, tag):
    g2 = load(f"final10_s{tag}_b2.csv", "BATCH2")
    g3 = load(f"final10_s{tag}_b3.csv", "BATCH3")
    s3 = [int(r[6]) for r in g3]
    return {
        "scale": scale,
        "stage1": statistics.median([int(r[4]) for r in g2]),
        "stage2": statistics.median([int(r[10]) for r in g2]),
        "ep3": statistics.median([int(r[5]) for r in g3]),
        "top3": round(100.0 * sum(1 for s in s3 if s == 10) / len(s3), 1),
    }


data = {
    "scan": {
        "n2": len(b2), "n3": len(b3),
        "ep": {
            "era1": {"median": statistics.median(ep1), "mean": round(mean(ep1), 2),
                     "std": round(pstdev(ep1), 2), "box": box(ep1)},
            "era2": {"median": statistics.median(ep2), "mean": round(mean(ep2), 2),
                     "std": round(pstdev(ep2), 2), "box": box(ep2)},
            "era3": {"median": statistics.median(ep3), "mean": round(mean(ep3), 2),
                     "std": round(pstdev(ep3), 2), "box": box(ep3)},
        },
        "stage": {
            "era1": dist(stage1), "era2": dist(stage2), "era3": dist(stage3),
            "median": {"era1": statistics.median(stage1), "era2": statistics.median(stage2),
                       "era3": statistics.median(stage3)},
            "low2_pct": round(100.0 * sum(1 for s in stage1 if s <= 2) / len(stage1), 2),
            "top3_pct": round(100.0 * sum(1 for s in stage3 if s == 10) / len(stage3), 1),
        },
        "subs": {
            "era1": statistics.median(ci(b3, 16)),
            "era2": statistics.median(ci(b3, 17)),
            "era3": statistics.median(ci(b3, 18)),
            "dist_era3": dist(ci(b3, 18)),
            "latency": {
                "median": round(statistics.median(latency), 1),
                "p90": round(sorted(latency)[int(0.9 * len(latency))], 1),
                "within120": round(100.0 * sum(1 for x in latency if x <= 120) / len(latency), 1),
                "n": len(latency),
            },
        },
        "first": {
            "speciation": round(statistics.median(cf(b2, 18)), 1),
            "speciation_p90": round(sorted(cf(b2, 18))[int(0.9 * len(b2))], 1),
            "mutation": round(statistics.median(cf(b2, 28)), 1),
            "last_purchase": round(statistics.median(cf(b3, 11)), 1),
        },
        "leftover": {
            "era2_biomass": round(statistics.median(cf(b2, 22)), 0),
            "era2_dna": round(statistics.median(cf(b2, 23)), 0),
            "era2_knowledge": round(statistics.median(cf(b2, 24)), 0),
            "era3_knowledge": round(statistics.median(cf(b3, 15)), 0),
        },
    },
    "grid_growth": [grid_growth(1.22, "1p22"), grid_growth(1.28, "1p28"),
                    grid_growth(1.34, "1p34"), grid_growth(1.40, "1p40")],
    "grid_spec": [grid_spec(0.45, "0p45"), grid_spec(0.9, "0p9"),
                  grid_spec(1.5, "1p5"), grid_spec(2.5, "2p5")],
    "acceptance": [
        {"item": "首突变", "target": "≤70s", "actual": "0.1s（开局即买）", "status": "达标"},
        {"item": "首次分化", "target": "≤180s", "actual": "179.5s（中位）", "status": "达标"},
        {"item": "阶段 ≤2 的局", "target": "<5%", "actual": "0.0%", "status": "达标"},
        {"item": "纪元 1 阶段中位", "target": "5–6", "actual": "5", "status": "达标"},
        {"item": "纪元 1 EP 中位", "target": "26–30", "actual": "26", "status": "达标"},
        {"item": "纪元 2 阶段中位", "target": "7–8", "actual": "8", "status": "达标"},
        {"item": "第三纪元仍有目标", "target": "有人到 10", "actual": "37.7% 到 10", "status": "达标"},
        {"item": "阶段 6 后首个理念升级", "target": "≤120s", "actual": "12.8s（中位）", "status": "达标"},
        {"item": "单轮理念升级数", "target": "3–6", "actual": "纪元 3 中位 6", "status": "达标"},
        {"item": "纪元 1 EP 标准差", "target": "3–6", "actual": "2.17", "status": "低于区间"},
        {"item": "纪元 1 EP 极差", "target": "≤2×", "actual": "1.5×", "status": "达标"},
        {"item": "末次有效购买（含突变）", "target": "统一口径", "actual": "594.5s", "status": "达标"},
    ],
    "params": [
        {"name": "STARTING_DNA", "before": "0", "after": "20", "note": "开局即可买首个突变"},
        {"name": "阶段 0–1 base_dna", "before": "×1.0", "after": "×1.5", "note": "抬前段收入"},
        {"name": "突变基价", "before": "base", "after": "base × (0.35 + 0.12×阶段)", "note": "早便宜晚贵"},
        {"name": "MUTATION_COST_GROWTH", "before": "1.22", "after": "1.34", "note": "补偿基价下调"},
        {"name": "保底关键突变", "before": "原价", "after": "−40%，峰值达标也触发", "note": "出牌即可支付"},
        {"name": "光合/化能阶段上限", "before": "1", "after": "无上限，阶段≥2 ×2 价", "note": "消除永久锁"},
        {"name": "容量扩张", "before": "20 级、+8%/级、×1.15", "after": "无上限、+5%/级、×1.12", "note": "Biomass 永有出口"},
        {"name": "阶段 7–10 分化成本", "before": "18k/120k/400k/1.2M", "after": "45k/300k/1M/3M", "note": "三轮接近"},
        {"name": "base_knowledge", "before": "0.10→1.80", "after": "0.30→1.00", "note": "温和上升"},
        {"name": "子升级乘数 / 成本增长", "before": "1.5–2.5 / ×5", "after": "统一 ×1.35 / ×6", "note": "总额约 ×2.46"},
        {"name": "首次分化成本", "before": "80", "after": "55", "note": "压进 180s"},
    ],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ROUND10 = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

print("era1 EP", data["scan"]["ep"]["era1"]["median"],
      "era2", data["scan"]["ep"]["era2"]["median"],
      "era3", data["scan"]["ep"]["era3"]["median"])
print("stage medians", data["scan"]["stage"]["median"])
print("latency", data["scan"]["subs"]["latency"])
print("grid growth", [(g["growth"], g["stage1"], g["stage2"], g["top3"]) for g in data["grid_growth"]])
print("grid spec", [(g["scale"], g["stage1"], g["stage2"], g["top3"]) for g in data["grid_spec"]])
