# v16 修复说明（F1–F10）

> 依 `TreeOfLife-修复方案-v16.md` 执行；v14 的历史结论见 `CHANGELOG-v14.md`，本文只记这次修复。

## 一、改动清单

| # | 位置 | 改动 |
|---|---|---|
| F1 | `data/balance.h` `MUTATION_ATP_MIN_PHASE` | 8 → **0**：阶段 0 起所有突变都收 ATP 门票（此前阶段 1–7 的 70 条 v14 突变完全免费，是「前期 ATP 没出口」的直接原因） |
| F2 | `demo/demo_b.c` 采购策略 | 撤销「绝不买纯属性卡」：刷新用尽后买得起就买最便宜的一张，`BOT_STAT_MUTATION_LIMIT 16` 自限；新增 `--lean` 走旧策略做对照 |
| F3 | `src/ui_adapter.c` `ua_mutation_desc` | 显示值乘 `ROUND15_MUTATION_BONUS_SCALE`（id ≥ `ROUND15_MUTATION_FIRST_ID`）：此前界面按账面值打印，玩家看到「ATP +15%」实际只有 5.25% |
| F4 | `data/balance.h` `RESERVE_MAX_LEVEL` | 26 → **16**（0.90^16 = 0.185 触底 0.20，第 16 级即满级；26 是永远买不到的死常量） |
| F5 | `docs/02-数据与计算规则.md` | 常量块对齐：容量 20、储备 0.90 / 16 / 45 s、突变票 10 s×1.12^k、`MUTATION_ATP_MIN_PHASE 0`、存档 v10 |
| F6 | `data/balance.h` | `SPECIATION_PHASE12_MULT 1.25 → 1.10`、`SPECIATION_HARD_WALL_MULT 1.12 → 1.00`，注释里的「1.35」改为「1.10」 |
| F7 | `src/species.c` | 阶段 13 的 4 个 v14 物种（熵光织网 / 终末拾荒者 / 万有议会 / 虚空孢子）下移到阶段 12；阶段表改 `11 22 26 25 26 27 24 23 12 13 12 12 20 11`，12 阶段配额改 8 个（`QUOTA_8`）、角色 2/4/2；父节点同阶段合法化（方向卡本来就是同阶段侧支） |
| F8 | `src/screens.c` | 星图环半径加 `TREE_MIN_RY 56` 地板 + 画框留白 24px 上限 |
| F9 | `README.md` | 「读到 v9 旧档」→「读到 **v9 及更早**的旧档」 |
| F10 | `tools/reports/` | 门槛 2 改结构口径（票价表 + 纪元 3 收入 / 末端产率推算，与 bot 无关）；新增 `sink_ratio` 门槛与 `bot_mutations` / `coverage_species` / `saturate_*` 记录项；新增 `gate_check.py`、`compare_strategy.py`；删除 `mutations_median` / `mutations_p90` 两条为包住实测写的区间 |

**顺带修的三处**

- `tests/test_b.c` 两处断言随 F1 更新：低阶突变不再断言「ATP 门票为 0」，买卡用例先补足门票（不是放宽，是把期望值改成新规则的推导值）。
- `tests/test_ui_layout.c` 新增 `test_mutation_desc_scaling`：直接比对描述字符串里的百分比，锁住 F3 的显示口径。
- `run_report.py` 的探索批次从 1000 降到 300：F1 让候选池从几十条涨到两百多条，探索模式（每步都可能开一次候选）慢了 9 倍；探索只服务「覆盖率」这一记录项。
- `evolution.c` 的「至少给一张买得起」保底扫描加 48 条上限，同上原因。

## 二、验收（1000 种子，`tools/reports/gate_check.py`）

| # | 门槛 | 目标 | 实测 | 结论 |
|---|---|---|---|---|
| G1 | 编译 + 单测 + 内容校验 | 0 warning / 全 PASS / 11 项 PASS | 0 warning；27 组 130,892 项 0 失败；11 项 PASS | PASS |
| G2 | 突变购买力（结构上限） | 12–16 张 | 中位 **15**（P10 11 / P90 21） | PASS |
| G3 | ATP 出口（支出/收入） | ≥0.90 | **82.5%**（未花 17.5%） | **FAIL** |
| G4 | 单局物种 | 中位 16–18 / P10 ≥14 / P90 ≤20 | 中位 **16**（P10 15 / P90 17） | PASS |
| G5 | 阶段抵达 | 12 为 55–75% / 13 ≥5% | **37.0% / 0.2%** | **FAIL** |

记录项：bot 实买 15 张（P90 16）、覆盖率 45/264、容量满级 472 s（63% 的局到 20 级）、储备满级 210 s、
纪元 3 EP 66（σ 3.24）、首突变 60.1 s、洞见 Lv.2.0、子升级 7.0 层。

## 三、对照跑（同一批 200 局）

| 策略 | 突变 | 物种 | 阶段 | 支出 | 支出/收入 | 容量满级 | 储备满级 |
|---|---|---|---|---|---|---|---|
| 买属性卡（v16 默认） | 15 | 16 | 11 | 3.4e6 | **81.9%** | 63.0% | 210 s |
| 不买属性卡（`--lean`） | 13 | 17 | 12 | 1.36e7 | **86.0%** | 3.5% | 211 s |

## 四、两条门槛为什么没过（按方案要求停手，不调参）

- **G5**：`10 s × 1.12^k` 的 `k` 是「本纪元已买张数」，买属性卡会把下一张**主链门票**一起推贵，
  所以 F2 恢复属性卡后主链反而更慢（默认 37% vs 旧策略 81%），两端都落在 55–75% 之外。
  这不是 F6/F7 的错——它们确实把阶段 12 的门票从 1.25 / hard wall 1.12 降到了 1.10 / 1.00。
- **G3**：两组都花不到 90%：容量 20 级在 472 s 就满、储备 16 级在 210 s 就满，
  最后的 ATP 只剩「单轮升级」一个出口，而它是无硬顶的秒数价，余额越高越买不动。
  要让 G3 达标，得动升级曲线或再加一个 ATP 出口，属于方案层决策。

## 五、复现

```sh
python tools/validate_content.py
mingw32-make SHELL=cmd.exe test
mingw32-make SHELL=cmd.exe test-layout
python tools/reports/run_report.py                 # 1000 / 1000 / 300
python tools/reports/gate_check.py tools/reports/batch3_scan_round15.csv
python tools/reports/compare_strategy.py 200       # 两组策略对照
```

九步自检在 PowerShell 里执行 `./tools/oneshot_check.ps1`（第 6 步会打印 G2–G5）。
