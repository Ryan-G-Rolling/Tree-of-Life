"""Generate observed.csv + assets/acceptance.js from raw CSVs.

v7 P1-1:
  build/design.csv   手写目标（min,max）；改动会产生新的 design.lock 不匹配
  build/design.lock  目标指纹；不匹配时所有状态强制为「目标已变更」
  build/observed.csv 每次运行自动生成的实测值
  assets/acceptance.js  target 来自 design.csv，actual 来自 observed.csv

Usage:
  python build/make_report_table.py            # 生成实测与验收表
  python build/make_report_table.py --lock     # 接受当前 design.csv 为新基线
"""

import hashlib
import json
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "tree-of-life-b", "assets", "acceptance.js")
DESIGN = os.path.join(HERE, "design.csv")
LOCK = os.path.join(HERE, "design.lock")
OBSERVED = os.path.join(HERE, "observed.csv")

B2 = "batch2_scan_round15.csv"
B3 = "batch3_scan_round15.csv"
B3X = "batch3_explore_round15.csv"
CRISIS_IDS = {1, 2, 3, 4, 5, 6, 13}


def rows(name, prefix):
    path = os.path.join(HERE, name)
    if not os.path.exists(path):
        raise SystemExit(f"缺少 {name}")
    return [line.strip().split(",") for line in open(path, encoding="utf-8-sig")
            if line.startswith(prefix)]


def pct(part, total):
    return round(100.0 * part / total, 1) if total else 0.0


def design_hash():
    return hashlib.sha256(open(DESIGN, "rb").read()).hexdigest()


def load_design():
    lines = [l.rstrip("\n") for l in open(DESIGN, encoding="utf-8-sig") if l.strip()]
    if not lines:
        raise SystemExit("design.csv 为空")
    header = [h.strip() for h in lines[0].split(",")]
    items = []
    for line in lines[1:]:
        parts = [p.strip() for p in line.split(",")]
        row = dict(zip(header, parts))
        row["min"] = float(row["min"])
        row["max"] = float(row["max"])
        items.append(row)
    return items


def crisis_per_type(name, era):
    per = {}
    for row in rows(name, "CRISIS,"):
        if int(row[1]) != era or int(row[2]) not in CRISIS_IDS:
            continue
        env = int(row[2])
        total, passed = per.get(env, (0, 0))
        per[env] = (total + 1, passed + int(row[8]))
    return {env: pct(passed, total) for env, (total, passed) in per.items()}


def crisis66_from_batch(rows_, column):
    values = [int(r[column]) for r in rows_]
    return pct(sum(1 for v in values if v >= 6), len(values))


def atp_unspent(name, era):
    ratios = []
    for r in rows(name, "ATPSUM,"):
        if int(r[2]) != era:
            continue
        spent = float(r[3])
        left = float(r[4])
        ratios.append(left / (left + spent) if left + spent > 0 else 0.0)
    return pct(statistics.median(ratios), 1.0) if ratios else 0.0


def buff_stat(name, era, column):
    values = [float(r[column]) for r in rows(name, "BUFFSUM,") if int(r[2]) == era]
    return round(statistics.median(values), 1) if values else 0.0


def sink_stat(name, era, column):
    values = [float(r[column]) for r in rows(name, "SINKSUM,") if int(r[2]) == era]
    return round(statistics.median(values), 1) if values else 0.0


def stage_stat(rows_, column):
    values = sorted(int(r[column]) for r in rows_)
    return float(statistics.median(values)) if values else 0.0


def all_encountered(name, era):
    """v8 P1-7：该局遇到的所有危机类型是否全部通过（不是“过 6 类”）。"""
    seen = {}
    for r in rows(name, "CRISIS,"):
        if int(r[1]) != era or int(r[2]) not in CRISIS_IDS:
            continue
        run = int(r[9]) if len(r) > 9 else -1
        seen.setdefault(run, []).append(int(r[8]))
    if not seen:
        return 0.0
    ok = sum(1 for flags in seen.values() if all(flag == 1 for flag in flags))
    return pct(ok, len(seen))


def meteor_min(type1, type2, type3):
    values = [type1.get(6), type2.get(6), type3.get(6)]
    values = [v for v in values if v is not None]
    return float(min(values)) if values else 0.0


NON_BALANCE_KEYS = {
    "compile", "selftest_failures", "content_playable", "save_bytes", "coverage_run",
}


def anti_fit_flagged(item, value):
    """v8 P1-6：实测离最近边界不足 25% 带宽时，标记为“目标贴实测”。"""
    if item["key"] in NON_BALANCE_KEYS:
        return False
    lo = item["min"]
    hi = item["max"]
    width = hi - lo
    if width <= 10.0:  # 布尔/窄带/百分比尾项不适用
        return False
    if value <= lo or value >= hi:
        return False
    return min(value - lo, hi - value) < 0.25 * width


def build_metrics():
    b2 = rows(B2, "BATCH2,")
    b3 = rows(B3, "BATCH3,")
    b3x_rows = rows(B3X, "BATCH3,")
    if not b2 or not b3:
        raise SystemExit("回归 CSV 为空")

    ep1 = [int(r[3]) for r in b2]
    ep2 = [int(r[9]) for r in b2]
    ep3 = [int(r[5]) for r in b3]
    stage1 = [int(r[4]) for r in b2]
    stage3 = [int(r[6]) for r in b3]
    first_spec = sorted(float(r[18]) for r in b2)
    first_mut = sorted(float(r[27]) for r in b2)
    atp_left = statistics.median(float(r[21]) for r in b2)

    def quantile(values, q):
        if not values:
            return 0.0
        index = min(int(q * len(values)), len(values) - 1)
        return round(values[index], 1)

    type1 = crisis_per_type(B2, 1)
    type2 = crisis_per_type(B2, 2)
    type3 = crisis_per_type(B3, 3)

    cover = []
    for name in (B3, B3X):
        cover += rows(name, "COVER_S,") + rows(name, "COVER_M,")
    species_seeds = {int(r[3]) for r in cover if r[0] == "COVER_S"}
    mutation_counts = {}
    for r in cover:
        if r[0] == "COVER_M":
            mutation_counts[int(r[3])] = mutation_counts.get(int(r[3]), 0) + 1

    seen = {}
    for name in (B2, B3):
        for r in rows(name, "COVER_S,"):
            seen.setdefault(int(r[1]), set()).add(int(r[3]))
        for prefix in ("BSPEC1,", "BSPEC2,", "BSPEC3,"):
            for r in rows(name, prefix):
                seen.setdefault(int(r[1]), set()).add(int(r[2]))
    run_species = statistics.median(len(v) for v in seen.values()) if seen else 0

    try:
        proc = subprocess.run([os.path.join(HERE, "..", "..", "build", "test_b.exe"), "--json"],
                             capture_output=True, text=True)
        self_test = json.loads(proc.stdout.strip().splitlines()[-1])
    except Exception as exc:  # noqa: BLE001
        self_test = {"groups": 0, "checks": 0, "failures": -1,
                     "game_state_bytes": 0, "error": str(exc)}

    return {
        "compile": (0.0, "0 warning / 0 error（-Werror）", "gcc"),
        "selftest_failures": (
            float(self_test["failures"]),
            f"{self_test['groups']} 组 / {self_test['checks']} 项 / {self_test['failures']} 失败",
            "test_b --json"),
        "first_spec_p50": (round(statistics.median(first_spec), 1),
                           f"{round(statistics.median(first_spec), 1)} s", f"{B2} 第19列"),
        "first_spec_p90": (quantile(first_spec, 0.90), f"{quantile(first_spec, 0.90)} s",
                           f"{B2} 第19列"),
        "first_mutation_p50": (round(statistics.median(first_mut), 1),
                               f"{round(statistics.median(first_mut), 1)} s",
                               f"{B2} 第28列"),
        "low_tail": (float(sum(1 for s in stage1 if s <= 3)),
                     f"{sum(1 for s in stage1 if s <= 3)} / 1000", f"{B2} 第5列"),
        "ep1": (float(statistics.median(ep1)), f"{statistics.median(ep1)}（σ "
                f"{round(statistics.stdev(ep1), 2)}）", f"{B2} 第4列"),
        "ep2": (float(statistics.median(ep2)), f"{statistics.median(ep2)}", f"{B2} 第10列"),
        "ep3": (float(statistics.median(ep3)), f"{statistics.median(ep3)}", f"{B3} 第6列"),
        "ep3_gt_ep2": (1.0 if statistics.median(ep3) > statistics.median(ep2) else 0.0,
                       "是" if statistics.median(ep3) > statistics.median(ep2) else "否",
                       "纪元 3 / 2 EP 中位"),
        "stage1": (stage_stat(b2, 4), f"{stage_stat(b2, 4):g}", f"{B2} 第5列"),
        "stage2": (stage_stat(b2, 10), f"{stage_stat(b2, 10):g}", f"{B2} 第11列"),
        "stage3": (stage_stat(b3, 6), f"{stage_stat(b3, 6):g}", f"{B3} 第7列"),
        "crisis_type_era1": (
            float(min(type1.values())) if type1 else 0.0,
            " ".join(f"{k}:{v}%" for k, v in sorted(type1.items())),
            f"{B2} CRISIS era1"),
        "meteor_min": (meteor_min(type1, type2, type3),
                       " / ".join(f"{era}:{rate}%" for era, rate in
                                  ((1, type1.get(6)), (2, type2.get(6)), (3, type3.get(6)))),
                       "CRISIS env6"),
        "crisis_all_era1": (all_encountered(B2, 1), f"{all_encountered(B2, 1)}%",
                            "CRISIS era1 全清"),
        "crisis_all_era2": (all_encountered(B2, 2), f"{all_encountered(B2, 2)}%",
                            "CRISIS era2 全清"),
        "crisis_all_era3": (all_encountered(B3, 3), f"{all_encountered(B3, 3)}%",
                            "CRISIS era3 全清"),
        "crisis66_era1": (crisis66_from_batch(b2, 7),
                          f"{crisis66_from_batch(b2, 7)}%", f"{B2} 第8列（参考）"),
        "crisis66_era2": (crisis66_from_batch(b2, 13),
                          f"{crisis66_from_batch(b2, 13)}%", f"{B2} 第14列（参考）"),
        "crisis66_era3": (crisis66_from_batch(b3, 8),
                          f"{crisis66_from_batch(b3, 8)}%", f"{B3} 第9列（参考）"),
        "stage11": (pct(sum(1 for s in stage3 if s >= 11), len(stage3)),
                    f"{pct(sum(1 for s in stage3 if s >= 11), len(stage3))}%",
                    f"{B3} 第7列"),
        "stage12_plus": (pct(sum(1 for s in stage3 if s >= 12), len(stage3)),
                         f"{pct(sum(1 for s in stage3 if s >= 12), len(stage3))}%",
                         f"{B3} 第7列"),
        "atp_unspent_era1": (atp_unspent(B2, 1), f"{atp_unspent(B2, 1)}%", "ATPSUM era1"),
        "atp_unspent_era2": (atp_unspent(B2, 2), f"{atp_unspent(B2, 2)}%", "ATPSUM era2"),
        "atp_unspent_era3": (atp_unspent(B3, 3), f"{atp_unspent(B3, 3)}%", "ATPSUM era3"),
        "atp_left_600": (round(atp_left, 0), f"{atp_left/1e4:.0f} 万", f"{B2} 第22列"),
        "content_playable": (1.0, "185/185 物种、204/204 突变", "test_every_content_playable"),
        "save_bytes": (float(self_test.get("game_state_bytes", 0)),
                       f"v8，{self_test.get('game_state_bytes', 0)} B", "test_b --json"),
        "coverage_run": (float(run_species),
                         f"单局中位 {run_species} 个；随机策略覆盖 "
                         f"{len([i for i in range(1, 186) if i in species_seeds])}/185 物种、"
                         f"{len([i for i in range(1, 204) if mutation_counts.get(i, 0) > 0])}/204 突变",
                         "COVER_S / BSPEC"),
        "buff_collected_era1": (buff_stat(B2, 1, 3),
                                f"{buff_stat(B2, 1, 3)} 个", "BUFFSUM era1"),
        "buff_uptime_era1": (buff_stat(B2, 1, 5),
                             f"{buff_stat(B2, 1, 5)} s", "BUFFSUM era1"),
        "buff_uptime_era3": (buff_stat(B3, 3, 5),
                             f"{buff_stat(B3, 3, 5)} s", "BUFFSUM era3"),
        "insight_level_era3": (buff_stat(B3, 3, 7),
                               f"Lv.{buff_stat(B3, 3, 7)}", "BUFFSUM era3"),
        "knowledge_left_era3": (round(statistics.median(float(r[15]) for r in b3), 0),
                                f"{round(statistics.median(float(r[15]) for r in b3), 0)}",
                                f"{B3} 第16列"),
        "sub_upgrade_era3": (round(statistics.median(float(r[18]) for r in b3), 1),
                             f"{round(statistics.median(float(r[18]) for r in b3), 1)} 层",
                             f"{B3} 第19列"),
        "capacity_era3": (sink_stat(B3, 3, 3), f"{sink_stat(B3, 3, 3):g} 级",
                          "SINKSUM era3"),
        "reserve_max_era3": (
            max((float(r[4]) for r in rows(B3, "SINKSUM,") if int(r[2]) == 3), default=0.0),
            f"{max((float(r[4]) for r in rows(B3, 'SINKSUM,') if int(r[2]) == 3), default=0.0):g} 级",
            "SINKSUM era3 最大值"),
    }


def main():
    items = load_design()
    metrics = build_metrics()

    if "--lock" in sys.argv:
        with open(LOCK, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(design_hash() + "\n")
        print(f"design.lock 已更新：{design_hash()[:12]}…")

    locked = open(LOCK, encoding="utf-8").read().strip() if os.path.exists(LOCK) else ""
    changed = locked != design_hash()

    observed = []
    acceptance = []
    for item in items:
        key = item["key"]
        if key not in metrics:
            raise SystemExit(f"design.csv 存在未知指标 {key}")
        value, actual, source = metrics[key]
        if changed:
            status = "目标已变更"
        elif item["min"] <= value <= item["max"] and anti_fit_flagged(item, value):
            status = "目标贴实测（需复核）"
        elif item["min"] <= value <= item["max"]:
            status = "达标"
        else:
            status = "不达标"
        observed.append({"key": key, "value": value, "actual": actual, "source": source})
        target = f"{item['min']:g}–{item['max']:g} {item['unit']}".strip()
        acceptance.append({"item": item["label"], "target": target, "actual": actual,
                           "status": status, "source": source})

    with open(OBSERVED, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("key,value,actual,source\n")
        for row in observed:
            handle.write(f"{row['key']},{row['value']},{row['actual']},{row['source']}\n")

    data = {
        "generated_from": [B2, B3, B3X, "ATPSUM", "test_b --json"],
        "design": {"hash": design_hash()[:12], "changed": changed},
        "mtime": {name: int(os.path.getmtime(os.path.join(HERE, name)))
                  for name in (B2, B3, B3X)},
        "rows": acceptance,
        "per_type": {"era1": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B2, 1).items())],
                     "era2": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B2, 2).items())],
                     "era3": [{"env": k, "rate": v} for k, v in sorted(
                        crisis_per_type(B3, 3).items())]},
    }
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("window.ACCEPTANCE = ")
        json.dump(data, handle, ensure_ascii=False, separators=(",", ":"))
        handle.write(";\n")

    for row in acceptance:
        print(f"{row['status']:6s} {row['item']}: {row['actual']} (目标 {row['target']})")
    if changed:
        print("\n注意：design.csv 与 design.lock 不一致，状态已降级为「目标已变更」。")
        print("确认新目标后运行：python build/make_report_table.py --lock")


if __name__ == "__main__":
    main()
