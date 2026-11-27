# Tree of Life 工作日志

自 2026-11-26 起记录项目改动。后续所有项目对话均遵守根目录 `AGENTS.md` 的日志要求；本文件只追加，不记录临时操作流水。

## 起始基线

A/B/C 可运行整合版已在此前完成，Git 基线为 `44e2c0d`；既有结果与接口说明见 `README.md` 和 `INTEGRATION_NOTES.md`。本日志不追溯补写此前的操作过程。

## 2026-11-26 · Codex · 工作区清理与持续日志规则

- **原因**：用户要求删除工作中途产物，只保留结果，并要求所有后续项目对话记录项目改动。
- **规则**：新增 `AGENTS.md`，明确每轮先读工作日志、改动后追加记录、记录验证结果和未完成事项；新增本工作日志，并在 `README.md` 增加入口。
- **清理**：删除 `.local/` 下本轮整合用的十个临时 Python 脚本、下载后的重复 raylib 压缩包、`build/` 中的测试程序/测试存档/调试和验证日志、根目录重复的 `shot_*.png`、`ui/assets/sfx/` 中的重复生成音效及空的 `tools/` 目录。
- **保留**：全部源码、测试源码、构建脚本、最终接口文档、`treeoflife.exe`、`assets/` 运行资源、解压后的 raylib 构建依赖、`deliverables/` 中的试玩目录/压缩包/结果截图；用户原有 `data.zip`、当前 `saves/`、IDE 文件及队友提交的报告不动。
- **文档**：`README.md` 补充日志与工作区目录说明，说明回归测试存档由完整构建重新生成后才能用于截图。
- **验证**：清理前核对文件清单，删除前校验绝对路径位于项目内且目标不含重解析点；清理后检查临时文件消失、最终文件存在、当前存档哈希保持一致、Git 差异无空白错误。本轮未修改游戏代码，不重新生成测试产物。
- **后续**：之后每次项目改动均追加本日志；正式操作结果以完成后的检查为准。

- **持续生效**：已将项目日志偏好同步到共享记忆库的 shared/preferences.md（来源：Codex + 2026-11-26 用户明确要求），项目内规则仍以 AGENTS.md 为准。本轮 README.md、AGENTS.md、WORK_LOG.md 为本地改动，尚未提交或推送 GitHub。

## 2026-11-26 · Codex · 同步远端与进度核对

- **原因**：用户要求同步项目并汇报当前进度。
- **同步**：执行 `git fetch origin`，将本地 `main` 从 `44e2c0d` 快进到远端 `612712d`；此提交只在 README 简介末尾增加 `111`，未改动游戏代码。Git 自动暂存并恢复本地 README 改动，AGENTS.md、既有 WORK_LOG.md 和原有 data.zip 保留。
- **发现**：远端新增 `final` 分支，提交 `7136537`，包含界面重做、星图进化树、数据中文化、目录迁移、构建方式与测试调整。已获取并阅读提交和 README，尚未合入 main，未据分支名称将其视作已验收版本。
- **核对**：本地试玩目录、20261126 压缩包及 12 张整合截图存在；当前 main 的已有验收结果见 INTEGRATION_NOTES.md。本轮仅同步文档提交，不重新编译或运行游戏；final 的测试通过为该分支提交说明中的报告，本轮未复验。
- **未完成**：final 与 main 的实现差异及测试迁移仍需审查；本地日志/规则/README 改动尚未提交或推送。无本轮临时产物需要清理。

## 2026-11-26 · Codex · 全分支统一与旧版本清理

- **授权与原因**：用户要求查看当前仓库、合并全部分支、统一进度并删除老版本，最终只保留一个分支。
- **输入与历史**：本轮远端 main 更新到 `806ec29`（含 `55eff6f`、`4ca1ca4` 的第十二/十三轮规则），final 为 `7136537`；main-latest、member-b、member-c 及本地 member-a/integrate-abc 的提交已由这些历史覆盖。先提交本地项目规则与日志为 `e1b0bd7`，再以保留双方历史的合并统一 main 与 final。身份通过单次 Git 参数设为 Codex，不改全局配置。
- **唯一工程**：最新 B 规则迁入根 src/、include/、data/，保留 26 物种、44 突变、15 共振、14 环境；采用 final 的新版培养皿/星图界面和原有中文化文本。保留原 main 的完整动作入口、容量/储备、跨轮奖励、升级上限与 EP 溢出检查。移除失效的里程碑缓存字段，保留 UI 的里程碑查询接口；图鉴行高随最新内容量适配。增加 4/5/6 容量/储备/科技快捷键。
- **存档与构建**：统一 v8 存档路径 saves/treeoflife-v8.sav，保留所有旧用户存档；程序按可执行文件目录加载资源。build.ps1/build.bat/Makefile 指向同一套源码，使用现有 raylib 5.5 静态库；首次构建可下载官方依赖。Windows 等线字体保留本地，从源码跟踪移除，构建时本地复制。Makefile 增加头文件依赖与 demo 目标。
- **源码与文档清理**：删除 b/、TreeOfLife/、ui/ 中已被统一工程取代的旧游戏副本、模拟层和旧截图；保留队友研究报告 tree-of-life-b/ 及设计/美术资料。报告生成源脚本从 TreeOfLife/build 迁到 tools/reports/，实验入口迁到 demo/，修正头文件/文档路径。README 改为唯一进度和构建入口。早期替身测试不进入本版，原真实集成用例保留为 tests/test_regression.c。
- **验证**：严格 -O2/-Wall/-Wextra/-Wpedantic/-Werror 编译规则与控制层；B 的 23 组、48,153 项检查 0 失败，A+B 接线 36 项通过。原真实集成的候选锁定、失败回滚、三谱系、容量双币、理念科技、Lv.8、存档往返、结算防重领、陨石单次扣除、事件循环、固定步一致性通过。固定策略 60 分钟回归为阶段 3、23 突变、4 分化、30 升级、29 EP。可选 demo 编译通过；正式图形程序启动并导出 16 张 3840×2160 截图，已检查主界面与 26 物种图鉴布局。既有 saves 文件 SHA256 保持不变。
- **交付与收尾**：新试玩目录 deliverables/Tree-of-Life/，新压缩包 deliverables/Tree-of-Life-20261126.zip，最新截图统一放 deliverables/screens/。新包生成后删除旧 Tree-of-Life-ABC 目录/压缩包、docs/screens 重复图和根目录 shot_log.txt。清理仅针对已核对属于 Agent 的旧版本与本轮中间文件；保留 data.zip、IDE 文件、报告源材料、用户存档和 .local/deps 构建依赖。Git 分支删除及推送结果另行追加。
- **限制**：第十三轮新增条目部分英文；报告历史数据不代表统一版当前试玩结果。尚未穷举所有随机路线、团队平衡试玩、音效听感及 sanitizer。

- **收尾复核**：最后一次完整 build.ps1 再次通过上述测试与游戏编译；demo 编译通过；16 张 PNG 尺寸及 ZIP 完整性检查通过。确认本地资源与临时备份哈希相同后，删除本轮 .local/unify*.py、验证脚本、哈希清单、资源备份和 build/ 测试中间产物，更新新试玩包内可执行文件。

- **Git 完成结果**：统一合并提交为 `e052737`，已通过原子推送更新 GitHub main，并删除远端 codex/member-b、codex/member-c、final、main-latest；删除前逐一验证其提交均为 main 的祖先，删除操作携带远端提交校验。删除本地已合并的 codex/integrate-abc、codex/member-a。随后重新 fetch/prune 与 ls-remote --heads，确认远端只剩 main，本地也只剩 main，原分支提交历史保留在 main 中。此条完成记录单独提交并同步。

## 2026-11-26 · Codex · 修复点击飘字 +0 与持续同步约定

- **原因**：用户报告点击后显示 +0，要求修复后直接同步 GitHub，并明确后续在原有分支替换旧实现、持续记日志。
- **根因与修复**：src/ui.c 的点击飘字误读 ActionResult.amount_spent；点击不消耗资源，此字段为 0，但 A 已正常增加 ATP。现通过只读适配接口 ua_click_atp 查询真实经济公式的点击收益，保持规则与金额来源一致，不改变经济或存档格式。
- **验证改进**：新增 tests/test_click_feedback.c 和 Makefile test-ui 目标，build.ps1 完整构建自动执行；覆盖基础、单轮升级、永久代谢、两者叠加 4 类状态共 20 次点击，逐次比较实际 ATP 增量与飘字数值（含三槽轮换）。截图模式 src/main.c 改走真实 game_apply_action + ui_notify_result，不再注入虚构的 1240 收益；新点击截图可见 +3.5，与该演示局面的点击收益一致。
- **结果**：build.ps1 通过：48,153 项规则检查、36 项接线检查、60 分钟真实集成回归、新增 20 次点击反馈检查均通过；正式程序编译、启动及 16 张截图导出通过。更新同名根目录程序、deliverables/Tree-of-Life/treeoflife.exe、Tree-of-Life-20261126.zip 与截图，不另留旧交付版本。
- **长期约定**：AGENTS.md 追加用户明确授权：后续直接沿用 main，修改并验证原有实现后提交同步 GitHub，不另开平行版本；README.md 增加界面反馈测试入口。本轮沿用 main 提交与推送。
- **收尾**：核实本轮 build/ 为刚生成的编译对象、测试程序与回归存档后删除；截图已移至 deliverables/screens/，无根目录残留截图。用户存档和原有资料保留。

## 2026-11-26 · Agent · 工作区清理 + 单文件 exe

- **授权与原因**：用户要求清理旧产物，并额外产出一个能直接运行游戏的单独 exe。
- **清理**（移到 `E:/智C作业/小组作业/_trash_20261126/`，未删除，可随时找回，合计约 351 MB）：
  工作区根 `tree/`（B 旧工程）、`Tree-of-Life-main/` 与 `Tree-of-Life-main.zip`（A 旧工程）、
  `TreeOfLife/`（C 旧工程）、`.gitpush_tmp/`（推送用的临时克隆）；
  工程内 `docs/screens/final/`（已被 deliverables/screens 取代的旧截图）、
  根目录 16 张 `shot_*.png` 与 `shot_log.txt`、`imcrop.exe`。
  另删除根目录 `libraylib.dll`、`glfw3.dll` —— raylib 改为静态链接后不再需要。
- **单文件版**：新增 `include/embedded.h`、`src/embedded.c`（`emb_get()` / `emb_any()`），
  字体（render.c）、星云（atmosphere.c）、标本底片（tex.c）、三个音效（ui.c）四类加载点改为
  内嵌优先、失败退回文件路径；`Makefile` 增加 `EMB_OBJS`（`objcopy -I binary -O pe-x86-64`
  把 6 个资源转成 .o）、`build/emb_obj/embedded.o`（`-DEMBED_ASSETS`）与 `dist` 目标
  （`-static -static-libgcc`，链接补 `-lole32 -lshell32`）。`.local/deps/raylib-5.5_win64_mingw-w64`
  换成 raylib 官方静态包（glfw 已编入；原 MSYS2 包引 `__imp_glfw*`，无法静态链接）。
- **验证**：`OS=Unix mingw32-make`（Git Bash 必须带 `OS=Unix`）0 warning / 0 error；
  `test` 48,153 项规则 + 36 项接线 + 60 分钟回归全通过；`test-ui` 20 次点击数值一致；
  `make dist` 产出 22.9 MB `dist/TreeOfLife.exe`，在**空目录**（无 assets/、无 DLL）运行
  `--shot --ss 3` 正常导出 16 张 3840×2160，中文、星云、底片纹理均来自内存解码。
- **交付**：`deliverables/TreeOfLife.exe`（单文件版副本）；`README.md` 增"单文件版"小节。
- **未完成**：`_trash_20261126` 仍在等用户确认后整体删除；`dist/` 与 `deliverables/` 各存一份同一份 exe。

## 2026-11-27 · Agent · 突变图谱 + 星图间距 + 全中文

- **授权**：用户要求「一个区域能点开看当前获得的全部突变」「进化树图标太挤，要清晰展现每条分化路线」「突变里的英文全部转中文」，随后追加「还是做成星图形状，只把空间距离拉开」。
- **突变图谱**：新增 `UI_OVL_MUT_LIST` / `UI_BTN_OPEN_MUT_LIST`（`include/ui_internal.h`、`src/ui.c`），入口三个 —— 顶栏第三个入口「突变」、物种卡突变 chip 行右端的「全部 N >」（整行可点）、快捷键 `V`。绘制在 `src/screens.c:draw_mut_list_overlay()`：按物种分栏（1–2 个一行两栏、3–4 个 2×2），每栏列出该物种**全部**已获得突变（3 列格子，装不下标「还有 N 项」），底部「本轮共振」列出已触发共振与它的两个前置突变，页脚给本轮总数 / 图鉴收录 / 共振进度。适配层新增只读接口 `ua_synergy_count/name/desc/pair/active`（`include/ui_adapter.h`、`src/ui_adapter.c`），共振判定仍由 B 提供，UI 不自己比对。
- **星图进化树**：`draw_tree_overlay()` + `tree_layout()` 保持极坐标星图，重算几何拉开间距 —— 椭圆比 1.75（原 2.3）、角度按叶子数等分、每圈整体旋转 0.085 rad、内圈半径 26→58、节点半径 9.5；标签用八候选 + 两档距离避让、外扩 3px，挤不下就不画（当前/选中/路线上的一定画）；圈号去掉文字只留刻度点。另加节点点击（`UI_BTN_TREE_NODE`，`tree_sel` 存在 `UIState`）：点节点高亮它到原始细胞的整条路线，再点一次取消，底部详情条同步显示该物种的生态/阶段/分化自/在场种群。中途曾改成层状树（列=阶段、行=先序），用户要求回到星图后已回退，改动未留在最终代码里。
- **全中文**：`src/evolution.c` 的 10 条突变（34–43）、4 条共振（戴森云/光子菌席/热泉暖流/几丁质建筑）与 4 条规则层提示串；`src/species.c` 的 6 条侧支物种、6 条基石、6 条子升级、「未知物种」；`src/environment.c` 的「未知环境」。
- **验证**：`OS=Unix mingw32-make` 0 warning / 0 error；`test` 48,153 项规则 + 36 项接线 + 60 分钟回归全绿；`test-ui` 20 次点击一致；`python tools/gen_glyphs.py` 后字形 837 字；`--shot --ss 3` 导出 17 张 3840×2160（新增 `shot_05b_mutations.png`）并覆盖 `deliverables/screens/`；`make dist` 重建单文件版 22.87 MB，空目录运行导出 17 张截图正常，已覆盖 `deliverables/TreeOfLife.exe`。
- **文档**：`README.md` 补「界面补充（2026-11-27）」小节、操作说明加 `V` 与节点点击、截图数改 17；`docs/art/01-visual-identity.md` 增 v5 章节。

## 2026-11-27 · Agent · 合并 B 的 v8 规则到统一 A+B+C 布局

- **授权**：用户要求把本地 v7/v8 + 随机增益改动上传 GitHub；远程 `main` 已重构为根目录 A+B+C（UI 集成），用户选择「移植到新布局并合并 main」。
- **布局合并**：远程把 `TreeOfLife/{src,include,data,demo,tests}` 迁到根目录并集成 UI；本轮以 B 的新版规则为准覆盖 `src/evolution.c`、`src/species.c`、`src/environment.c`、`include/{game_types,evolution,buffs}.h`、`data/balance.h`，保留远程新增的 `ACTION_BUY_CAPACITY/RESERVE`、`LegacyState.achievement_flags`、`SAVE_VERSION 8` 与 `prestige_milestone_*`。旧 `TreeOfLife/` 目录删除，`build/` 台账与报告脚本迁到 `tools/reports/`。
- **本地化**：运行 `tools/localize_data.py` 把 186 物种 / 204 突变 / 14 环境的名称与描述统一为中文（234 处替换，0 缺失）。
- **接线修复**：`src/game.c` / `src/economy.c` 对无硬顶 `RUN_UPGRADE_MAX_LEVEL=-1` 增加 `>=0` 保护；`src/save.c` 对无上限储备与新增 buff/洞见字段做校验；`tests/test_regression.c` 更新为纯 ATP 容量、45% Meteor 与 v8 语义。
- **构建**：`Makefile` 把 `src/buffs.c` 加入规则层/核心对象，新增 `report` 目标；`tools/reports/run_report.py` 用 Python 捕获 demo 输出，避开 Windows shell 的 `>` 重定向问题；`make_report_table.py` 改为调用根目录 `build/test_b.exe`。
- **验证**：`mingw32-make -B test` → B 26 组 / 92,578 项 0 失败、A+B 接线 36 项 0 失败、60 分钟回归通过；`build.ps1 -SkipTests` 构建 `treeoflife.exe` 成功；`test-ui` 20 次点击一致；`make report` 重跑 1000 种子，33 项验收全达标（EP 45/60/61、阶段 8/10/11、Meteor 63.9/52.4/48.8%、容量 22 级、存档 v8 / 3,208 B）。
- **Git**：本地 v8 提交已先推到 `b-rules-v8` 备份；本条目随合并提交同步到 `main`。
- **未完成**：`b-rules-v8` 分支保留为备份，可在确认 `main` 稳定后删除；报告页 §34/§35 内容沿用旧布局生成，数值一致。


## 2026-11-27 · Agent · 落实 v9 优化方案（P0-18…P2 全部条目）

- **授权**：用户给出 `TreeOfLife-优化方案-v9.md`，要求「根据文件夹里的所有日志，对每一项进行优化」。
- **P0-18 阶段 12 永不到达**：根因不是 phase 11→13 的直连边，而是 `find_key_mutation` 的保底位取「子物种里 ID 最小的那个」——内容扩容后奇点的最小 ID 子物种是量子生物圈（42）而非心智场（44），保底一直在推量子认知，心智场的门票只能靠随机池命中（4/1000）。改为 `src/species.c` 的 `k_main_lineage[]` 显式主线表 + `species_main_child()`（`include/evolution.h` 导出），保底深度用 `MAINLINE_GUARANTEE_MAX_PHASE = 12` 封住；`Entropy Reversal` 稀有度 2→0（阶段 13 不走保底，靠权重）。纪元 3 阶段 12 抵达率 **0.4% → 100%**、阶段 13 **0.4% → 5.1%**。
- **P0-19 全清率 1.7/2.1/1.7%**：`environment.c` 的 `pick_next_environment` 按 `era_crisis_mask()` 只抽本纪元 4 类危机（1/2/3 纪元各有掩码，4 类全通过后不再重复抽危机）；危机伤害统一归一（变暖 0.0130 / 变冷 0.0122 / 微光 0.0115 / 氧气 0.0124 / 毒素 0.0148 / 陨石 0.0170 / 雾霾 0.0155），阶段放大改为 `CRISIS_EARLY_SCALE + 0.25×阶段`（0.80 起算）；通过线下调为 `0.672 / 0.70 / 0.75`。全清率 **→ 39.2 / 26.7 / 24.6%**，纪元 1 逐类型 70.1 / 88.3 / 85.3 / 89.5%。
- **P0-20 储备零收益等级**：`RESERVE_MAX_LEVEL = 26`（0.94^26 已触底 0.20），查询与购买两侧共用该上限，纪元 3 最大储备正好 26 级。
- **P0-21 容量抢开局（单变量对照）**：`CAPACITY_ATP_SECONDS` 5 / 12 / 20 三档各跑 1000 种子——首分化 p50 192.9 / 193.1 / 184.9，但纪元 2 容量中位从 17 级崩到 4 / 1 级（12 和 20 会把容量出口饿死），**取 5.0 不动**。首分化最终 165.9 s / p90 201.6 s（危机前期伤害被压掉两成后，突变支出提前，自然回落）。
- **P1-10 反拟合检查 v2**：`make_report_table.py` 的 `anti_fit()` 改为返回降级原因——等于上下界 →「目标贴边界」；区间内但离边界不足 25% 带宽 →「目标贴实测（需复核）」；单边指标（`lo == 0`）只在 ≥90% 上界时提示。`design.csv` 四条被放宽的区间同时回退到设计口径（首分化 p90 360→280、首突变 p50 60→45、储备 27→26、全清率 0–10% → 25–45 / 18–35 / 18–35%），`design.lock` 重新锁定。
- **P1-11 内容宽度**：覆盖口径统一到 `BSPEC1/2/3`（放弃 `COVER_S`，后者只在探索模式展开）；阶段 10 三个横向分支（Orbital Station / Lunar Colony / Fusion Economy）亲本改为「太空时代或奇点」、门票改为主线自有的 Rocketry / Rocketry / Planetary Industry，`best_child_under_cap` 改按「门票齐备 + 未发现」排序，避免选中门票没凑齐的侧支永久卡住。单局物种中位 11 → 13，随机策略覆盖 31/185 物种、128/204 突变；Orbital Station 在纪元 2 的 200 局里全部出现。
- **P2 台账一致**：`demo_b.c` 四处 ATP 出口（升级 / 突变 / 储备 / 容量）统一记 `before - after`，新增收入侧累加（产出、点击、瞬时增益）与闭合校验 `ATPSUM` 第 6 列残差（实测最大 0.004 ATP）；新增 `RAMPTIME,局,纪元,到顶耗时,首分化即时`，纪元 1 到顶中位 428.8 s、纪元 3 到顶 5.1%（中位 425.4 s），首次分化「纪元 3 记 0.1 s」的退化情形单独标出（89.8% 为第一步即时兑现）。
- **测试**：`tests/test_b.c` 新增 `test_main_lineage()`（13 条边、阶段严格递增、保底深度 < 阶段 13），危机用例改为按 `CRISIS_EARLY_SCALE` 计价、危机数上限 4；`tests/test_regression.c` 修掉 `species_children` 越界写（`if(n>8)n=8`）并改为沿 `species_main_child` 推进。**27 组 / 92,622 项 / 0 失败**，A+B 接线 36 项通过，60 分钟回归「阶段 8、143 突变、8 分化、30 升级、80 EP」。
- **验收**：`make report` 重跑 1000 种子三份 CSV，**39 项全部达标、0 不达标、0 降级**（新增 3 项台账口径行）；EP 45 / 58 / 63，阶段 8 / 10 / 12，首分化 p50 165.9 s、首突变 39.8 s，ATP 未花 3.1 / 2.0 / 3.3%，容量 23 级，存档 v8 / 3,208 B。
- **撤销的实验**：曾给规则表加「叶子物种回落到主线的兜底边」，结果文明(8)↔海洋生态城(7) 无限往复（1900 次分化、140 个突变），且破坏 `test_every_content_playable`（6 项失败），已完整回退，`species_children` / `species_has_parent` 保持纯规则表语义。
- **文档**：`docs/02-数据与计算规则.md` 补 §5.4 主线表说明、§6.4 保底口径、§7.2 危机池轮换表与新伤害值、§8.6 通过线重定标、§8.7 储备 26 级硬顶、§8.10 `RAMPTIME`、新增 §8.11 台账口径；`README.md` 更新自测规模与 60 分钟回归数据。
- **未完成**：`deliverables/` 与 `dist/` 未重建（本轮只动规则层与台账，未改 UI）；音效、sanitizer、全随机路线仍未验收。
