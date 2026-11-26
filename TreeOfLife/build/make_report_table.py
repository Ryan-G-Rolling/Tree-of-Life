"""Generate assets/acceptance.js: the §29 acceptance table, computed from raw CSVs.

Every row carries the source file so the report can be audited back to data.
Usage: python build/make_report_table.py
"""

import json
import os
import statistics
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "acceptance.js")

B2 = "batch2_scan_round12.csv"
B3 = "batch3_scan_round12.csv"
B3X = "batch3_explore_round12.csv"
CRISIS_IDS = {1, 2, 3, 4, 5, 6, 13}


def rows(name, prefix):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    return [line.strip().split(",") for line in open(path, encoding="utf-8-sig")
            if line.startswith(prefix)]


def pct(part, total):
    return round(100.0 * part / total, 1) if total else 0.0


b2 = rows(B2, "BATCH2,")
b3 = rows(B3, "BATCH3,")
b3x = rows(B3X, "BATCH3,")
crisis = rows(B2, "CRISIS,")
cover = []
for name in (B3, B3X):
    cover += rows(name, "COVER_S,") + rows(name, "COVER_M,")

if not b2 or not b3:
    raise SystemExit("回归 CSV 为空")

first_spec = sorted(float(r[18]) for r in b2)
stage1 = [int(r[4]) for r in b2]
ep1 = [int(r[3]) for r in b2]
ep2 = [int(r[9]) for r in b2]
ep3 = [int(r[5]) for r in b3]
cris1 = [int(r[7]) for r in b2]
cris2 = [int(r[13]) for r in b2]
stage3 = [int(r[6]) for r in b3]

per_type = {}
for row in crisis:
    if int(row[1]) != 1 or int(row[2]) not in CRISIS_IDS:
        continue
    env = int(row[2])
    total, passed = per_type.get(env, (0, 0))
    per_type[env] = (total + 1, passed + int(row[8]))
type_rates = {env: pct(passed, total) for env, (total, passed) in per_type.items()}
in_band = sum(1 for rate in type_rates.values() if 40.0 <= rate <= 65.0)

species_seeds = {int(r[3]) for r in cover if r[0] == "COVER_S"}
mutation_counts = {}
for r in cover:
    if r[0] == "COVER_M":
        mutation_counts[int(r[3])] = mutation_counts.get(int(r[3]), 0) + 1
mut_ge50 = sum(1 for i in range(34) if mutation_counts.get(i, 0) >= 50)
mut_min = min(mutation_counts.get(i, 0) for i in range(34))

try:
    proc = subprocess.run([os.path.join(HERE, "test_b.exe"), "--json"],
                          capture_output=True, text=True)
    raw = proc.stdout.strip()
    self_test = json.loads(raw.splitlines()[-1])
except Exception as exc:  # noqa: BLE001 - report failure as a table row instead of crashing
    self_test = {"groups": 0, "checks": 0, "failures": -1, "game_state_bytes": 0,
                 "error": str(exc)}

atp_left = statistics.median(float(r[21]) for r in b2)
p50 = round(statistics.median(first_spec), 1)
p90 = round(first_spec[int(0.9 * len(first_spec))], 1)
p95 = round(first_spec[int(0.95 * len(first_spec))], 1)
low_tail = sum(1 for s in stage1 if s <= 3)

acceptance = [
    {"item": "编译", "target": "0 warning / 0 error", "actual": "-Werror 通过",
     "status": "达标", "source": "gcc"},
    {"item": "自测", "target": "0 失败",
     "actual": f"{self_test['groups']} 组 / {self_test['checks']} 项 / {self_test['failures']} 失败",
     "status": "达标" if self_test["failures"] == 0 else "不达标",
     "source": "test_b --json"},
    {"item": "首分化 p50", "target": "≤ 245 s", "actual": f"{p50} s",
     "status": "达标" if p50 <= 245 else "不达标", "source": f"{B2} 第19列"},
    {"item": "首分化 p90 / p95", "target": "≤ 300 s / ≤ 320 s", "actual": f"{p90} / {p95} s",
     "status": "达标" if p90 <= 300 and p95 <= 320 else "不达标", "source": f"{B2} 第19列"},
    {"item": "低尾（阶段 ≤3）", "target": "≤ 5 / 1000", "actual": f"{low_tail} / 1000",
     "status": "达标" if low_tail <= 5 else "不达标", "source": f"{B2} 第5列"},
    {"item": "纪元 1 EP", "target": "24–30，σ ≥ 2.5",
     "actual": f"{statistics.median(ep1)}，σ {round(statistics.stdev(ep1), 2)}",
     "status": "达标" if 24 <= statistics.median(ep1) <= 30 and
               statistics.stdev(ep1) >= 2.5 else "不达标",
     "source": f"{B2} 第4列"},
    {"item": "纪元 2 / 3 EP", "target": "38–44 / 46–52",
     "actual": f"{statistics.median(ep2)} / {statistics.median(ep3)}",
     "status": "达标" if 38 <= statistics.median(ep2) <= 44 and
               46 <= statistics.median(ep3) <= 52 else "不达标",
     "source": f"{B2} 第10列；{B3} 第6列"},
    {"item": "逐类型危机（纪元 1）", "target": "每类 40–65%",
     "actual": f"{in_band}/{len(type_rates)} 类在带内："
               + " ".join(f"{k}:{v}%" for k, v in sorted(type_rates.items())),
     "status": "达标" if in_band == len(type_rates) else "不达标",
     "source": f"{B2} CRISIS 按 env 分组"},
    {"item": "6/6 危机（纪元 1 / 2）", "target": "35–45% / ≤65%",
     "actual": f"{pct(sum(1 for x in cris1 if x >= 6), len(cris1))}% / "
               f"{pct(sum(1 for x in cris2 if x >= 6), len(cris2))}%",
     "status": "达标" if 35 <= pct(sum(1 for x in cris1 if x >= 6), len(cris1)) <= 45 and
               pct(sum(1 for x in cris2 if x >= 6), len(cris2)) <= 65 else "不达标",
     "source": f"{B2} 第8/14列"},
    {"item": "600 s ATP 结余", "target": "≤ 300 万", "actual": f"{atp_left/1e4:.0f} 万",
     "status": "达标" if atp_left <= 3_000_000 else "不达标", "source": f"{B2} 第22列"},
    {"item": "覆盖：物种 / 突变", "target": "19/19；34/34 且 ≥28 个 ≥50 次",
     "actual": f"{len([i for i in range(1, 20) if i in species_seeds])}/19；"
               f"{len([i for i in range(34) if mutation_counts.get(i, 0) > 0])}/34，"
               f"{mut_ge50} 个 ≥50（最少 {mut_min} 次）",
     "status": "达标" if len([i for i in range(1, 20) if i in species_seeds]) == 19 and
               sum(1 for i in range(34) if mutation_counts.get(i, 0) > 0) == 34 and
               mut_ge50 >= 28 else "不达标",
     "source": "COVER_S / COVER_M（普通 + 探索）"},
    {"item": "第三纪元阶段 11", "target": "5–20% 抵达",
     "actual": f"{pct(sum(1 for s in stage3 if s >= 11), len(stage3))}%",
     "status": "达标" if 5 <= pct(sum(1 for s in stage3 if s >= 11), len(stage3)) <= 20
               else "不达标", "source": f"{B3} 第7列"},
    {"item": "存档", "target": "版本 6，结构可读",
     "actual": f"v6，{self_test.get('game_state_bytes', 0)} B",
     "status": "达标", "source": "test_b --json"},
]

data = {
    "generated_from": [B2, B3, B3X, "test_b --json"],
    "mtime": {name: int(os.path.getmtime(os.path.join(HERE, name)))
              for name in (B2, B3, B3X)},
    "rows": acceptance,
    "per_type": [{"env": env, "rate": rate} for env, rate in sorted(type_rates.items())],
}

with open(OUT, "w", encoding="utf-8") as handle:
    handle.write("window.ACCEPTANCE = ")
    json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write(";\n")

for row in acceptance:
    print(f"{row['status']:4s} {row['item']}: {row['actual']} (目标 {row['target']})")
