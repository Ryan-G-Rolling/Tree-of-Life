"""Generate / verify the constant block of docs/02 §3 from data/balance.h.

Usage:
  python tools/reports/gen_constants.py          # 打印生成结果
  python tools/reports/gen_constants.py --check  # 与文档比对，不一致则退出 1
  python tools/reports/gen_constants.py --write  # 重写文档中的 ```c 常量块
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
HEADER = os.path.join(ROOT, "data", "balance.h")
DOC = os.path.join(ROOT, "docs", "02-数据与计算规则.md")

ORDER = [
    "FIXED_STEP_SECONDS", "BASE_CLICK_ATP", "STARTING_ATP", "STARTING_BIOMASS",
    "LEGACY_STARTING_DNA", "STARTING_POPULATION", "MIN_ACTIVE_POPULATION",
    "FIRST_EVENT_DELAY", "EVENT_INTERVAL", "EVENT_DURATION_SCALE",
    "ADAPTATION_PREVIEW_SECONDS", "DNA_POPULATION_EXPONENT",
    "MUTATION_COST_GROWTH", "MUTATION_PHASE_COST_BASE", "MUTATION_PHASE_COST_SLOPE",
    "KEY_MUTATION_DISCOUNT", "FIRST_MUTATION_DISCOUNT", "LATE_MUTATION_COST_MULTIPLIER",
    "EARLY_DNA_BONUS", "EARLY_DNA_MAX_PHASE",
    "CAPACITY_GAIN_PER_LEVEL", "CAPACITY_MAX_LEVEL", "CAPACITY_PRICE_GROWTH",
    "RESERVE_COST_ATP", "RESERVE_REDUCTION_STEP", "RESERVE_REDUCTION_FLOOR",
    "RESERVE_MAX_LEVEL", "RESERVE_PRICE_GROWTH", "RESERVE_ATP_SECONDS",
    "MUTATION_ATP_SECONDS", "MUTATION_TICKET_GROWTH",
    "DNA_TICKET_SECONDS", "DNA_TICKET_MIN_PHASE", "DNA_TICKET_GROWTH",
    "MUTATION_ATP_MIN_PHASE",
    "SAVE_VERSION", "KNOWLEDGE_POPULATION_EXPONENT",
    "CRISIS_PASS_RATIO_ERA1", "CRISIS_PASS_RATIO_ERA3",
    "CRISIS_PASS_POPULATION_RATIO", "CRISIS_DAMAGE_STAGE_SCALE", "CRISIS_EARLY_SCALE",
    "CRISIS_EVENT_PRODUCTION_SCALE", "CRISIS_DAMAGE_MULTIPLIER",
    "INSTANT_LOSS_STAGE_SCALE", "INSTANT_LOSS_MAX",
    "DIVERSITY_SYNERGY_MULTIPLIER", "DIVERSITY_SPECIATION_DISCOUNT",
]


def read_values():
    sources = [os.path.join(ROOT, "data", "balance.h"),
               os.path.join(ROOT, "include", "game_types.h")]
    texts = [open(path, encoding="utf-8").read() for path in sources]
    values = {}
    for name in ORDER:
        pattern = r"^#define\s+" + name + r"\s+([^\n/]+)"
        for text in texts:
            match = re.search(pattern, text, re.MULTILINE)
            if match:
                values[name] = match.group(1).strip()
                break
        if name not in values:
            raise SystemExit(f"头文件缺少常量 {name}")
    return values


def block_text(values):
    return "\n".join(f"#define {name} {values[name]}" for name in ORDER)


def find_block(doc):
    head = doc.find("## 3. 通用常量")
    if head < 0:
        raise SystemExit("找不到 §3")
    start = doc.find("```c", head)
    end = doc.find("```", start + 4)
    if start < 0 or end < 0:
        raise SystemExit("§3 缺少 ```c 常量块")
    return start + 4, end


def main():
    values = read_values()
    generated = block_text(values)
    doc = open(DOC, encoding="utf-8").read()
    start, end = find_block(doc)
    current = doc[start:end]

    if "--write" in sys.argv:
        open(DOC, "w", encoding="utf-8").write(doc[:start] + "\n" + generated + "\n" + doc[end:])
        print(f"已写入 §3：{len(ORDER)} 个常量")
        return 0

    if "--check" in sys.argv:
        # 文档块允许保留行尾注释；比较 #define 名称与数值本身。
        cur = {}
        for line in current.splitlines():
            m = re.match(r"\s*#define\s+([A-Z_][A-Z0-9_]*)\s+([^\s/]+)", line)
            if m:
                cur[m.group(1)] = m.group(2)
        missing = [n for n in ORDER if n not in cur]
        changed = [(n, cur[n], values[n]) for n in ORDER if n in cur and cur[n] != values[n]]
        extra = [n for n in cur if n not in ORDER]
        if not missing and not changed:
            print(f"§3 常量与 balance.h 一致（{len(ORDER)} 个）")
            if extra:
                print("  文档中另有未纳入检查的常量:", ", ".join(extra))
            return 0
        print("§3 常量与 balance.h 不一致：")
        for name in missing:
            print("  缺少:", name, "=", values[name])
        for name, got, want in changed:
            print(f"  不同: {name} 文档={got} balance={want}")
        return 1

    print(generated)
    return 0


if __name__ == "__main__":
    sys.exit(main())
