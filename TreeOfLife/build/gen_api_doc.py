"""Regenerate the B public-API appendix inside docs/01-三人分工与接口约定.md."""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
DOC = os.path.join(ROOT, "..", "docs", "01-三人分工与接口约定.md")

HEADERS = ["evolution.h", "environment.h", "legacy.h", "game_types.h"]
PROTO = re.compile(
    r"^(?:[A-Za-z_][A-Za-z0-9_]*[\s\*]+)+([a-z_][a-z0-9_]*)\s*\(([^;{]*?)\)\s*;",
    re.MULTILINE,
)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


WRITE_NAMES = {"environment_init_run", "evolution_open_mutation_offer"}


def classify(name):
    if "apply" in name or "commit" in name:
        return "一次性提交（只由 A 调用）", "是"
    if name in WRITE_NAMES:
        return "写状态（只由 A 调用）", "是"
    if "validate" in name:
        return "校验（纯查询）", "否"
    return "纯查询", "否"


def uses_rng(name):
    return name in {
        "evolution_generate_mutation_offer_rng",
        "evolution_open_mutation_offer",
        "environment_init_run",
        "environment_apply_step",
        "environment_calculate_step",
    }


rows = []
seen = set()
for header in HEADERS:
    path = os.path.join(ROOT, "include", header)
    text = strip_comments(open(path, "r", encoding="utf-8").read())
    for match in PROTO.finditer(text):
        name = match.group(1)
        if name in seen or name.startswith("tof_"):
            continue
        seen.add(name)
        category, writes = classify(name)
        rng = "是" if uses_rng(name) else "否"
        rows.append((name, header, category, writes, rng))

rows.sort(key=lambda r: (r[0].split("_")[0], r[0]))

lines = [
    "### 6.5 B 公共接口总表",
    "",
    "下表由 `build/gen_api_doc.py` 从头文件生成，覆盖当前全部公开函数。"
    "“一次性提交”只能由 A 在完成校验后调用；“写状态 / 随机数”两列用于判断接入顺序。",
    "",
    "| 函数 | 头文件 | 类别 | 写状态 | 随机数 |",
    "| --- | --- | --- | --- | --- |",
]
for name, header, category, writes, rng in rows:
    lines.append(f"| `{name}` | {header} | {category} | {writes} | {rng} |")

lines += [
    "",
    "### 6.6 初始化与轮次契约",
    "",
    "1. `game_init` 先写 `RunState.rng_state`，再调用 `environment_init_run`；后者会消耗一次随机数。",
    "2. 新轮顺序固定为：清零 `RunState` → 写 `rng_state` → 设置初始物种与资源 → "
    "`environment_init_run` → `legacy_apply_new_run_bonuses` → 进入 `PHASE_RUNNING`。",
    "3. `discovered_species[SPECIES_PROTOCELL]` 与 `legacy.codex_species[SPECIES_PROTOCELL]` "
    "由 A 在初始化和新轮时写入；B 的规则函数不代劳。",
    "4. `run.era` 由 A 自增；`save_version` 由 A 写为 6（第十二轮新增阶段 11、"
    "Omega Point 与危机最低人口日志字段）；`highest_biomass_milestone` 已删除，"
    "里程碑统一由 `max_biomass` 派生。",
    "5. 新轮与读档时必须把 `run.knowledge` 清零、`run.tech_levels` 整体置零、"
    "`run.crisis_min_population` 置零；理念与危机最低人口只在本轮有效，不跨轮保存。",
    "",
]

content = open(DOC, "r", encoding="utf-8").read()
anchor = "## 7. 每帧数据流"
if anchor not in content:
    raise SystemExit("找不到插入锚点")
start = content.find("### 6.5 B 公共接口总表")
if start != -1:
    content = content[:start] + content[content.find(anchor):]
content = content.replace(anchor, "\n".join(lines) + "\n" + anchor, 1)
open(DOC, "w", encoding="utf-8").write(content)
print(f"inserted {len(rows)} functions")
