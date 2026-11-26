"""Generate / verify the constant block of docs/02 §3 from data/balance.h.

Usage:
  python build/gen_constants.py          # 打印生成结果
  python build/gen_constants.py --check  # 与文档比对，不一致则退出 1
  python build/gen_constants.py --write  # 重写文档中的 ```c 常量块
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
HEADER = os.path.join(ROOT, "data", "balance.h")
DOC = os.path.join(ROOT, "..", "docs", "02-数据与计算规则.md")

ORDER = [
    "FIXED_STEP_SECONDS", "BASE_CLICK_ATP", "STARTING_ATP", "STARTING_BIOMASS",
    "LEGACY_STARTING_DNA", "STARTING_POPULATION", "MIN_ACTIVE_POPULATION",
    "FIRST_EVENT_DELAY", "EVENT_INTERVAL", "EVENT_DURATION_SCALE",
    "ADAPTATION_PREVIEW_SECONDS", "MAX_ACTIVE_SPECIES", "MAX_MUTATION_CHOICES",
    "DNA_POPULATION_EXPONENT", "MUTATION_COST_GROWTH", "MUTATION_PHASE_COST_BASE",
    "MUTATION_PHASE_COST_SLOPE", "KEY_MUTATION_DISCOUNT", "FIRST_MUTATION_DISCOUNT",
    "LATE_MUTATION_COST_MULTIPLIER",
    "EARLY_DNA_BONUS", "EARLY_DNA_MAX_PHASE",
    "CAPACITY_COST_PER_CAP", "CAPACITY_GAIN_PER_LEVEL", "CAPACITY_COST_GROWTH",
    "CAPACITY_MAX_LEVEL", "CAPACITY_ATP_SHARE", "CAPACITY_BIOMASS_SHARE",
    "CAPACITY_BIOMASS_ONLY_LEVELS",
    "RESERVE_COST_ATP", "RESERVE_REDUCTION_PER_LEVEL", "RESERVE_MAX_LEVEL",
    "KNOWLEDGE_POPULATION_EXPONENT", "SUB_UPGRADE_COST_GROWTH",
    "CRISIS_PASS_RATIO_ERA1", "CRISIS_PASS_POPULATION_RATIO", "CRISIS_DAMAGE_STAGE_SCALE",
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
        cur = [l.strip() for l in current.strip().splitlines() if l.strip()]
        want = [l.strip() for l in generated.splitlines() if l.strip()]
        if cur == want:
            print(f"§3 常量与 balance.h 一致（{len(ORDER)} 行）")
            return 0
        print("§3 常量与 balance.h 不一致：")
        for line in want:
            if line not in cur:
                print("  缺少/不同:", line)
        for line in cur:
            if line not in want:
                print("  多余/不同:", line)
        return 1

    print(generated)
    return 0


if __name__ == "__main__":
    sys.exit(main())
