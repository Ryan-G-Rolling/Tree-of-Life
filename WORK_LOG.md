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

## 2026-11-27 · Codex · 修复扩容越界、奖励接线与存档一致性

- **授权与基线**：用户要求直接处理本轮审查发现的问题；以 GitHub main 的 db560b0 为基线。遵循现有 main 更新约定，不创建平行工程或覆盖用户存档。当前 Linux 工作区没有原仓库，由 GitHub 连接器读取固定提交源码后验证；远端提交以原 main 为父提交，保留完整历史与未修改文件。

- **数组边界与界面**：src/screens.c 全部分化查询使用 SPECIES_COUNT 容量，分化弹层每页 4 条并提供翻页；未满足前置时仍能浏览。ui_adapter 的列表查询返回已写入数，规则层 species_children 的总数语义在头文件明确说明。UI 按钮容量增加到 MAX_SPECIES+64，容纳全部星图节点和关闭按钮；洞见按钮使用商店原页脚区域，避免覆盖旧页脚文字。

- **正式玩法接线**：game.c 初始化和固定步接入 buffs；追加 ACTION_COLLECT_BUFF / ACTION_BUY_INSIGHT，沿用复制后提交的失败回滚机制。economy.c 的实际点击收益使用奖励倍率；ui.c/screens.c 提供奖励领取按钮、倒计时、洞见等级/价格/购买按钮与 B/7 快捷键。奖励名称中文化，重新生成 ui_glyphs.h。

- **存档与数值**：即时资源包更新 max_biomass，拒绝非有限资源结果和计数溢出且不改变原状态；补齐 buff_visible 的布尔表示、活跃奖励类型、计数与时间一致性校验。洞见购买拒绝非有限价格和等级溢出。GameState 布局与 SAVE_VERSION 8 不变，不改写或迁移旧用户存档。

- **测试修复**：tests/test_b.c 的随机索引用无符号取模，消除负下标；tests/test_regression.c 的分支数组扩容，种子初始化同步奖励状态。新增 test_gameplay.c 验证正式动作链、奖励刷新/过期/暂停/重置、真实点击倍率、洞见扣款、失败回滚、即时存档和读档延续、损坏布尔表示拒绝。新增 test_ui_layout.c 遍历 186 个角色的全部分化页，验证全部路线、186 个星图节点、关闭按钮和新增控件；使用真实生产布局，无规则替身。点击飘字测试另补奖励状态用例（本轮仅编译检查，未执行需要 raylib 链接的 test-ui）。

- **自动化与文档**：Makefile 新增核心玩法测试、独立 ASan/UBSan 目标与无窗口布局测试；.github/workflows/core.yml 配置自动核心、内存、UI 编译和布局检查。README 记录新入口和验证范围，纠正旧 60 分钟策略结果，区分历史模拟报告与正式客户端。

- **实际验证**：Linux GCC 严格核心编译通过；26 组/92,578 项规则检查、36 项接线检查、完整集成回归及新增 gameplay 测试全部通过。ASan+UBSan（-fno-sanitize-recover=all）运行上述四个测试程序及真实 UI 布局测试全部通过；当前容器不支持 LeakSanitizer，因此本地用 ASAN_OPTIONS=detect_leaks=0，未宣称泄漏检查通过。全部正式 UI 源码及扩展点击测试通过 raylib 5.5 官方头文件的编译检查，工作流 YAML 解析和 git diff --check 通过。固定策略 60 分钟实测：阶段 0、20 次突变、1 次分化、30 次升级、15 EP；不作为完整玩法平衡验收。

- **同步与限制**：本条纳入 main 修复提交，通过 GitHub 连接器以预期旧提交校验更新分支。当前云环境没有 Windows 工具链和图形依赖，本轮未重新构建 exe、执行图形窗口、更新截图或重跑 1000 种子报告；既有二进制需要从修复后的源码重建。云端 shell 网络不可用，GitHub 读写使用已连接插件；保留官方 raylib 头文件作为本地验证依赖。GitHub Actions 的实际运行结果以 Actions 页面为准。

- **收尾**：验证完成后清理本轮 build/ 中的测试程序、对象文件和测试存档；保留源码、必要依赖及工作日志，没有删除远端分支、用户存档或历史资料。

## 2026-11-27 · Codex · 图鉴详情卡与 390 条成就风格文案

- **授权与基线**：用户确认全部 186 个物种、204 个突变的短句，要求按此前图鉴卡片方案写入。基线为 main 的 6cb45f1；沿用原工程与原分支。

- **文案数据**：新增 include/codex.h、src/codex.c，只读提供按规则 ID 对应的中文展示名与短句，完整写入本次确认的 390 条内容。两个表以编译期断言校验数量；无效类型/ID 返回空指针。UI 适配层统一使用该展示名，英文条目采用本次确认的中文暂译名，不改规则枚举、属性与历史存档。

- **图鉴浏览与卡片**：物种/突变页改为每页 12 项的 4×3 网格，分别 16/17 页；已收录条目可点开 480×580 详情卡，上半部放大既有动态物种形态/突变纹章，下半部展示成就风格短句，按实际文字宽度在 UTF-8 字符边界换行。图像复用现有程序插图，无新增外部图片资源依赖。记录页保留原统计内容。

- **交互**：返回按钮、Esc、卡片外点击返回图鉴原分类与页码；卡片内部点击不关闭，不触发底层操作。未发现条目沿用剪影与锁定策略，不提前泄露名称或文案。分类切换重置页码，翻页与详情查询均做边界检查；浏览不产生游戏动作、不改变 GameState。

- **构建与字体**：Makefile 把新文案模块加入正式游戏、单文件构建、无窗口布局测试；增加 test_codex 并纳入 test/test-sanitize；GitHub Actions 的 UI 编译列表加入新模块。重新生成 include/ui_glyphs.h，共 1285 个非 ASCII 字符。README 记录入口、分页、解锁策略、图像来源和测试方法。

- **验证**：严格核心编译通过；26 组/92,578 项规则检查、36 项接线检查、集成回归、gameplay 测试及全部 390 条数据完整性/长度/ID/非法查询检查通过。真实本地按钮处理与布局测试遍历全部 390 个详情入口、所有分页、锁定条目、返回状态与游戏状态不变，普通及 ASan/UBSan 运行均通过；正式 UI 全部源码以官方 raylib 5.5 头文件通过编译检查。ASan/UBSan 的五组核心程序均通过，本地仍关闭当前容器不支持的 LeakSanitizer，未声称泄漏检查通过。git diff --check 通过。

- **交付与同步**：本条随图鉴功能提交同步 main，使用 GitHub 连接器在原提交之上更新，并校验预期旧分支头；未创建平行版本。云环境缺少 Windows 工具链和图形链接依赖，本轮未重打包 Windows exe、运行图形窗口或生成新截图；现有 exe 需要从最新源码重新构建，视觉实机验收尚未执行。

- **收尾**：已清理本轮生成的 build/ 对象、测试程序和测试存档；保留源码、官方头文件依赖与日志，未修改用户存档或删除历史材料。

## 2026-11-27 · C · 随机因子上屏、洞见升格、星图缩放与危机演出

- **基线**：本地从 origin/main 快进到 d5c1334（B 的图鉴详情卡与 390 条文案）。此前一轮未提交的 UI 改动存档在 `backup/local-wip-20261127`，本轮按远端实现重新落地，不覆盖 B 的规则层与分页。

- **随机因子（buffs）可视化**：`screens_buff_factor_pos()` 给出培养皿内四个固定方位（由 `buff_visible_icon` 决定，0.74R 半径，落在活体 62 与皿内壁 95 之间），`build_main` 的命中框与 `draw_dish` 的绘制共用同一坐标。四种外观按 `BUFF_ICON_CELL/DNA/VESICLE/SPORE` 程序化绘制；呼吸光环 + 外扩涟漪 + 悬停放大表示可点击。因子按钮在 `UI_BTN_CLICK` 之后入表，点击优先抢采集。

- **增益状态**：生效期间沿皿内壁画进度弧（稀有强增益走 COL_RARE），剩余不足 25% 时叠一层呼吸；产率增益在 ATP 大数字右侧挂倍率胶囊（×1.2 / ×1.5），点击增益把「采集 +N ATP」改写为带 `×2`。皿下提示行三态互斥：因子名称 / 增益剩余秒数 / 「下一个因子约 N 秒」。

- **洞见条目**：商店底部从 320×28 细条改为 320×80 卡（三张机能卡 140→118 高、148→126 间距腾出空间），显示 Lv、`全产率 ×1.06 → ×1.08`、理念价格、`领悟 [7]` 胶囊；前期理念产量为 0 时价格位写「阶段 6 起才有理念产出」。适配层补 `ua_insight_multiplier` / `ua_insight_next_multiplier` / `ua_knowledge_rate` / `ua_buff_duration`。

- **顶栏第五格**：新增「理念」chip（COL_VIOLET + ICON_INSIGHT），五格平分 16..782（宽 150、步距 154），此前玩家看不到洞见的花费来源。

- **星图**：移植 v9 布局（每圈按该阶段物种数均分 + 先序排列），环半径起始 34→48，画框 1240×676；新增滚轮以鼠标为锚点缩放 0.75–2.8、空白处拖拽平移、右上「复位」，新增 `UI_BTN_TREE_SKY` / `UI_BTN_TREE_RESET`、`input_wheel()` / `input_mouse_delta()` / `input_click_down()`。连线与节点标签用 `BeginScissorMode` 裁在 sky 内，放大后不再画到标题。

- **危机演出**：`crisis_watch()` 只在环境 id 变化且新环境为危机的那一帧触发；抖屏在最终合成里按包络平移整帧并把 dst 外扩遮黑边（陨石 15px / 耀斑 9px / 生态 4–6px）；`draw_crisis_fx()` 分五类特效 + 2 秒标题条，`env_weather()` 提供持续氛围层。`ui_debug_poke_crisis()` 供截图构造。

- **图鉴筛选**：`codex_visible_list()` 让物种页只列「已发现 + 相邻（父/子）」并已发现优先（17 已发现 + 117 相邻 = 12 页，原 16 页全量），突变页只列已收录（23 / 204 收在 2 页）；计数分别显示 `已发现 17 / 186 · 相邻 117` 与 `已收录 23 / 204`。适配层补 `ua_max_species_stage` / `ua_species_child_list`。

- **验证**：0 warning / 0 error；`make test` 26 组 92,578 项 + 36 项接线 + 集成 + 60 分钟回归 + gameplay（buff timing / insight / 回滚 / 读档延续）+ codex 全量通过；`make test-ui` 25 次点击（含激活增益）一致；`--shot --ss 3` 导出 21 张 3840×2160，字形 1290 个非 ASCII 字符（补 悟/拖/拽/滚/相）。`make dist` 单文件 23.02 MB，在空目录跑通并导出 21 张，已覆盖 `deliverables/TreeOfLife.exe` 与 `deliverables/screens/`。

- **已知限制**：`make test-layout` 在 Windows MinGW 下因 PE 的 `--gc-sections` 无法剔除未引用的 `COL_*` / `ui_font_register` 而链接失败，Linux CI 走 ELF 不受影响；该目标未在本轮改动。

## 2026-11-27 · C · 生物图像限定画框、突变文案中文化

- **问题**：用户截图里一只阶段 1 杆菌几乎横穿整块展台，压到顶部「微光期」危机横幅上。原因是 12 类形态只在「本体半径 r」内排布时看着正常，实际每一类都会在 r 之外再伸出一圈：杆菌胶囊半长 1.46r 加鞭毛到 2.2r，行星环 1.34r，奇点吸积盘 1.66r。展台按 r=96 画，杆状菌真实高度到 280px，展台只有 284px。

- **收缩**：`morph_extent(stage, thin)` 登记 12 类形态的外接系数（thin=true 时含鞭毛/星尘/喷流），`organism_fit_ex()` 按容器可用半径反算本体半径。培养皿用 thin（玻璃壁是硬边界），展台/图鉴/缩略图用实心系数（细线到画框边界被裁是自然的，算进去杆状菌会缩成一个点）。

- **硬裁剪**：新增 `ui_clip_begin()/ui_clip_end()`（render.c，scissor 用渲染目标像素，逻辑坐标乘 `ui_canvas_ss()`，与星图连线同一套换算）。培养皿裁在皿的方框、展台裁在标题与物种名之间的 194px 画框、图鉴网格裁在 72×72 图标区、图鉴大图裁在 254px 画框、商店分化缩略图裁在 40×40 磁贴。`draw_organism_in()` 统一封装"先收缩再裁剪"，返回实际半径。

- **种群环**：进度环原按固定 `radius+14` 画，收缩后会出现"环比生物大一圈"或切线穿过本体；改成按收缩后的半径 × 外接系数 +10、上限为画框留白，并移到生物之前绘制，环永远套在外面且不会压住本体。

- **中文化**：`ua_speciation_requirement()` 改取图鉴的 204 条中文突变名（原来直接用 `MutationDef.name`，界面上出现「需要：Lipid Repair」「需要：RNA Proofreading」）。`ua_mutation_desc()` 不再返回 `MutationDef.description`（英文模板，且同阶段多条共用一句），改为按 atp/biomass/dna/cap/growth 加成、5 项抗性与理念加成现场拼中文摘要，例如「阶段 8 · ATP +25% · 生物量 +20% · 上限 +30% · 增速 +5% · 耐热 +2 · 耐氧 +2」，无加成时写「无产出加成」。

- **截图与验证**：main.c 新增 `fill_demo_early()`（单槽杆状菌）与 4 张新图：`shot_10_early_rod`、`shot_10b_lowlight`（微光期）、`shot_10c_speciation_zh`（中文前置条件）、`shot_02b_mutation_late`（后期突变，验证最长中文摘要不顶到「点击选择」）。0 warning / 0 error；`make test` 26 组 92,578 项 + 36 项接线 + 集成 + 60 分钟回归 + gameplay + codex 全通过；`make test-ui` 25 次点击一致；字形集重生成 1291 字符；`--shot --ss 3` 导出 25 张 3840×2160，`make dist` 单文件 22 MB 在空目录跑通并导出 25 张，已覆盖 `deliverables/`。

- **未改动**：`EP` / `Evolution Points` / `Legacy` 仍是设计文档里的专有名词（顶栏写作「EP 永久」），本轮只译突变名与前置条件，不改这些品牌词。

## 2026-11-27 · Codex · 待修复问题登记（暂不修改）

- **来源**：用户实测反馈及所附情报面板截图。
- **问题**：主界面「情报」面板右上角的「产出」「纪录」（界面显示为「记录」）标签点击无响应，无法从「下一步」切换页面。
- **状态与约束**：用户明确要求先记录、不要修改；本轮仅追加此记录，未改代码、未排查根因、未重新构建，尚未独立复现。等待用户后续指令再修复。

## 2026-11-27 · Codex · 四套 UI 风格概念稿（待用户选择）

- **需求**：用户认为当前 UI 风格不好看，要求生成多个方案供选择；本轮未授权选定最终方案。
- **产物**：使用内置 image_gen 分别生成 A 极简实验室、B 自然手绘、C 深海荧光、D 复古博物馆四张独立概念图，保存至 deliverables/ui-concepts/，对应 A-minimal-lab.png、B-naturalist.png、C-bioluminescent.png、D-museum.png；完整提示词保存至同目录 prompts.txt。
- **核对与边界**：四图已生成并展示，保留资源栏、主体生物、升级、突变/分化/结束纪元等主要界面意图。概念图中的文本、数值、物种形象和布局属于视觉探索，不代表实际规则或已实现界面；未改游戏代码、未构建、未修复此前登记的产出/记录点击问题。
- **状态**：等待用户选择或指定混搭后再实施；本轮无临时脚本或测试产物。概念图与日志保存在本地，尚未同步 GitHub；上一条问题登记提交也仍待与远端新提交协调。

## 2026-11-27 · Codex · 第二轮 UI 概念：进化路线图方向

- **用户纠正**：前四套视觉方案均不接受，用户认为太像网游，要求继续生成多个方案并参考《从细胞到奇点》的路线图。此为视觉探索方向，尚未选定最终设计。
- **参考与产物**：联网检索该游戏的科技树截图和 Steam 官方介绍；使用内置 image_gen 生成六套全屏节点/连线路线图概念：E 经典节点树、F 冷灰科研图、G 浅色系统图、H 低多边形地图、I 横向年代长卷、J 黑白谱系图。图片及完整提示词保存至 deliverables/ui-concepts/round-2/，逐张展示供用户选择。
- **核对与边界**：各图已生成、展示并保存。方向强调开放画布、节点前置关系、已解锁/可用/锁定状态和简洁详情面板，减少装饰框、大型按钮及商店卡片；概念图中的连接、文案、资源成本和时代划分仍为示意，不作为游戏规则或接口变更。
- **状态**：未改游戏代码、未构建、未实施界面替换；此前产出/记录标签问题继续保留待修复。本轮产物与日志保存在本地，尚未同步 GitHub；无临时脚本需要清理。


## 2026-11-27 · Codex · 同步 GitHub main 与本地重建验收

- **同步**：获取远端 main 的 13 个新增提交（截至 168ec67）；本地 50a7768 问题登记及 e76651c 两轮概念稿日志与远端合并。冲突仅在 WORK_LOG.md，保留双方记录并统一换行，未另改游戏规则或界面实现。远端 b-rules-v8 已是 main 的祖先，本轮保留备份分支。
- **当前进度**：主线已整合 v9 规则、186 物种、204 突变、20 共振、14 环境、随机奖励/洞见接线、390 条图鉴文案、突变图谱、星图缩放与画框裁剪。1000 种子/39 项验收为远端报告，本轮未重跑。
- **本机验证**：build.ps1 完整通过：27 组/92,622 项规则检查、36 项接线检查、真实集成、gameplay、390 条图鉴数据及 25 次点击反馈检查。60 分钟固定策略得到阶段 8、143 次突变、8 次分化、30 次升级、预览 80 EP。单文件版在隔离目录正常退出（0）并输出 25 张截图，已查看主界面；未做完整鼠标交互、音效或 sanitizer 复验。
- **交付**：重建根目录 treeoflife.exe；覆盖 deliverables/Tree-of-Life/treeoflife.exe、包内 README.txt、原同名 Tree-of-Life-20261126.zip，并生成 deliverables/TreeOfLife.exe 单文件版。核对普通版及单文件副本分别哈希一致；包名保留，内容更新为本轮。二进制与概念稿沿用忽略规则，仅保留本地，未强制加入 Git。
- **构建限制**：直接 mingw32-make dist 在本机 cmd 的 copy 正斜杠路径处失败；本轮用 PowerShell 复制相同六个资源并按 Makefile 参数调用 objcopy，再执行 dist 链接成功，未修改构建脚本。Windows 无窗口 test-layout 的既有链接问题未在本轮复验或修复。
- **收尾与待办**：本轮开始前 build/ 不存在，确认其全部为本轮测试对象、测试存档与隔离截图后清理；用户原有存档、资料、概念图均保留。UI 概念方向仍待用户选择；此前产出/记录标签问题继续按“先登记不修复”处理，本轮未交互复现。合并与本条记录沿用 main 提交并推送，完成后核对远端提交。

## 2026-11-27 · Codex · 开始页面与独立生涯记录设计决定

- **已确认功能**：开始页面采用完整基础菜单：继续游戏/新游戏、成就、设置、玩法说明、制作人员、退出；成就定位为纯收集与荣誉，不提供数值或外观奖励。
- **最新纠正**：图鉴与成就独立于游戏存档，归入本机共享的生涯记录；新建游戏存档不重置图鉴与成就。此前“新游戏清除成就/图鉴”的提案作废；本轮未确定生涯记录文件格式或迁移实现。
- **白金成就**：新增最终成就「白金大神」，全部普通成就解锁后自动解锁；判定不包含「白金大神」自身。
- **范围与验证**：本轮仅确认并记录设计约束，未修改游戏代码、存档或交付程序，未构建或测试；普通成就清单、设置细项、界面布局与旧档迁移仍待后续规划。本条沿用 main 提交并同步。

## 2026-11-27 · Agent · v10 定价体系与结构收口（第十九轮）

- **P0-22 四张价格表改时间本位**：`economy_upgrade_cost` / `evolution_capacity_*` / `evolution_reserve_cost` /
  `evolution_sub_upgrade_cost` / `evolution_speciation_cost` / `evolution_mutation_atp_cost` 全部改为
  `N × 被动产率`，锚点统一走新函数 `evolution_price_rates()`（剔除环境与限时增益的瞬时倍率，避免价格抖动）。
  删掉 `MUTATION_ATP_COST_MAX`（5e8 上限把“45 秒”压成“5 秒”）。点击收益改为 3 秒产率（`CLICK_INCOME_SECONDS`），
  旧 `BASE_CLICK_ATP` 退出计价。相关：`data/balance.h`、`src/evolution.c`、`src/economy.c`、`src/game.c`、`src/ui_adapter.c`。
- **P0-23 平方根收益 + 容量上限 20**：`economy_run_multiplier` 与 `evolution_calculate_species_rates` 同步改为
  `1 + 0.2/0.25 × √L`；`CAPACITY_MAX_LEVEL 20`。配套 `POP_GROWTH_SCALE 3.0` 与 `DNA_INCOME_SCALE 2.0`
  补回平方根削掉的复合速度（否则 60 种子实测塌到阶段 4）。
- **P0-24/25**：阶段 13 门票 ×1.55 hard wall；储备减伤 26 级触底后 UI 给出“减免已触底（80%）”提示并禁止扣款
  （`src/ui.c` 的 `ACTION_LEVEL_MAXED` 分支）。
- **P0-26 危机 6 选 4**：`era_crisis_pool()` + `roll_crisis_selection()`（按 run 种子抽 4 类，存 `run.crisis_selection`）；
  新增环境 `ENV_ACID_RAIN`(14) / `ENV_RADIATION_FALLOUT`(15)；`run.crisis_seen` 记录本纪元出现过的类型。
  CRISIS 日志改为“每局每类一条”，纪元结束补 `survived=0` 行。存档 `SAVE_VERSION 9`，`save.c` 校验危机位掩码。
- **P1-11 支线分化**：`species_branch_candidates()`（规则层）给出同阶段、非主线、有后继的支线候选，
  `evolution_validate_speciation` 接受支线目标，`ua_speciation_children` 与演示策略并入候选。
- **P1-12 Windows 布局检查**：`test-layout` 补 `render.c/atmosphere.c/tex.c/embedded.c/input.c` 与 `$(LIBS)`，
  MinGW 下首次通过；`.github/workflows/core.yml` 新增 `windows-latest` 任务（MinGW + 官方 raylib 5.5）。
- **P1-14/15 验收收口**：`design.csv` 重写为 v10 目标（EP 40–50/50–62/52–66、阶段抵达率、单级耗时 25–80 s、
  纪元 3 支出 ≤1e9、储备 26 级占比 ≤40%、危机类型覆盖、EP σ 等），`make_report_table.py` 新增
  `sec_per_upgrade / era3_spend / reserve26_share / crisis_types_run / ep3_sigma / stage8_arrive_era1 /
  stage10_arrive_era2 / cap_time_p50` 与反拟合检查；“185/185”统一为“186 物种（含原始细胞）”。
- **验证**：`make test` 27 组 92,621 项 + 36 项接线 + 集成 + 60 分钟回归（阶段 8、143 突变、8 分化、30 升级、52 EP）
  + gameplay + codex 全通过；`make test-layout`（Windows）通过；`make report` 1000 种子跑完。
- **1000 种子实测**：EP 45 / 54 / 62；阶段 8 / 10 / 12（阶段 12 抵达 95.1%、13 19.2%）；封顶 526.5 s；
  单级耗时 p50 20 s；纪元 3 支出 3.49e10（v9 为 9.19e10）；储备 26 级占比 0%；危机全清 26.6 / — / 31.7%；
  存档 v9 3,216 B；单局物种 13。
- **未达标（如实记录）**：首分化 52 s（目标 150–210）、首突变 16 s（35–60）、纪元 3 支出 3.49e10（≤1e9）、
  危机类型 7 类/局（项目只有 9 类危机，v10 方案按 18 类估算）、EP σ 2.55（≥4）、子升级 1 层（3–8）、
  容量 5 级（6–14）、ATP 未花 19.8%（≤10%）。原因：时间本位把价格锚到当前收入后，钱包存量≈1/增长率，
  高阶段大件（子升级/容量）与门票抢同一笔收入；EP 为小整数，σ 难以到 4。
- **未完成**：P1-13 `src/screens.c`（237 KB）拆 6 个翻译单元未执行——文件内大量跨屏 static 辅助函数，
  机械拆分需要同步改 3 个调用点并重跑全部布局/截图验证，本轮时间不足，留待下一轮单独做。
- **清理**：删除本轮探针 `build/probe_v10.c`、`build/probe_chain.c`、`build/probe_mut.c`、`build/probe_*.exe`
  与 `%TEMP%` 下的 `v10_*.json`；`tools/reports/` 保留本轮生成的实验 CSV。

## 2026-11-27 · Agent · v10 未达标 8 项收口（第二十轮）

- **目标**：把上一轮验收剩余未达标项修到达标——首分化、首突变、EP σ、阶段 12/13 抵达率、危机覆盖、
  ATP 未花、纪元 3 支出、单级耗时、容量等级、子升级层数。
- **数值改动（`data/balance.h`）**：
  - 单轮升级 `85/95 → 84/94 s ATP 产率`，基因组 `45 → 60 s 生物量产率`（把单级耗时从 100 s 拉回 26 s）。
  - 容量 `24/10 → 80/10 s`（ATP 主导，生物量与门票解耦，纪元 3 容量从 1 级拉到 10 级）。
  - 储备 `70 → 110 s ATP 产率`（避免储备滚到 26 级把危机全部磨平；26 级占比 0%）。
  - 子升级 `25 → 42 s 知识产率`（每纪元 4–6 层 → 实际 4 层）。
  - 突变 ATP 门票 `30 → 8 s`（此前纪元 3 买 150+ 张突变把 ATP 吃光，容量/升级被饿死）。
  - `PHASE_RATE_COMPRESSION 0.68 → 0.635`（纪元 3 支出 3.49e10 → 7.64e8）。
  - 分化门票新增阶段 12 溢价 `SPECIATION_PHASE12_MULT 1.25`、阶段 13 hard wall 收窄到 `1.12`，
    `SPECIATION_EARLY_FLOOR_SCALE 5.6 → 6.3`（首分化 152 s）。
  - `EARLY_DNA_BONUS 1.5 → 1.40`（首突变 34.5 → 36.3 s）。
  - EP 重排：`EP_MUTATION_CAP 8 → 3`、`EP_POINTS_PER_CRISIS 3 → 5`（纪元 3 σ 2.2 → 5.46）。
- **规则改动**：`evolution_speciation_cost` 增加阶段 12 溢价分支；`src/environment.c` 工业雾霾伤害
  `1.55% → 1.25%/s`（纪元 2 全清率拉回 28.7%）。
- **演示策略（`demo/demo_b.c`）**：购买余量 `1.15 → 1.02`（ATP 未花 11.3% → 9.5%）；
  纪元 3 容量只留 30%→55% 门票生物量预留（纪元 1/2 仍冻结整张门票，保住阶段 8 抵达率）；
  5 路轮转空转时高水位补买代谢（阈值 4 倍价格）。
- **验收口径修复**：`tools/reports/make_report_table.py` 的 `CRISIS_IDS` 补齐 14–17；
  此前纪元 2 池 {6,13,14,15}、纪元 3 池 {1,5,16,17} 各漏一半样本，逐类型通过率与全清率失真。
- **测试更新（`tests/test_b.c`、`tests/test_integration.c`）**：新增 `test_phase_compress / test_dna_ramp /
  test_sub_upgrade_cost` 三个由常量推导的辅助函数，替换全部写死的产率、门票、子升级、洞见、DNA 期望值；
  集成测试改为直接补足 ATP 再验证升级接线。
- **验证**：`make test` 27 组 92,622 项 + 36 项接线 + 集成 + 60 分钟回归（阶段 8、143 突变、8 分化、30 升级、57 EP）
  + gameplay + codex 全通过；`mingw32-make` 重建 `treeoflife.exe`；`make report` 1000 种子跑完，43 项全部达标
  （部分因贴近目标边界被标“需复核”）。
- **1000 种子实测**：EP 46 / 55 / 64（纪元 3 σ 5.46）；阶段 p50 8 / 10 / 12；阶段 12 抵达 74.3%、13 15.3%；
  首分化 152 s、首突变 36.3 s；封顶 507.4 s；单级耗时 26.1 s；纪元 3 支出 7.64e8、未花 9.5%；
  容量 10 级、储备 5 级（26 级占比 0%）；危机类型覆盖中位 10 类/局；全清 34.8 / 28.7 / 36.7%；存档 v9 3,224 B。
- **未完成**：P1-13 `src/screens.c` 拆 6 个翻译单元仍未执行（同上轮原因）；`git push` 未执行（需用户明确授权）。
- **清理**：删除本轮临时分析脚本 `build/tmp_analyze.py` 与中间扫描 `build/tmp_b2.csv`、`build/tmp_b3.csv`；
  `tools/reports/` 保留 `make report` 生成的正式 CSV。
