# Tree of Life · 最终集成版

三人协作的 Raylib 2D 生物进化增量游戏，把 A 的控制层、B 的规则层、C 的界面层合并为一个可编译、可运行、可测试的工程。

## 三方的边界

| 成员 | 目录 | 职责 |
|---|---|---|
| A | `src/game.c` `economy.c` `save.c`，`include/game.h` `economy.h` `save.h` | 时钟、固定步、动作分发、原子提交、存档 |
| B | `src/evolution.c` `species.c` `environment.c` `prestige.c` `legacy.c`，`include/evolution.h` `environment.h` `legacy.h`，`data/balance.h` | 物种、突变、环境、危机、EP 结算、永久树的全部规则与数值 |
| C | `src/main.c` `ui.c` `render.c` `screens.c` `input.c` `atmosphere.c` `ui_adapter.c`，`include/ui*.h` `atmosphere.h` | 窗口、输入映射、布局、绘制、氛围层、截图导出 |

`ui_adapter.c` 是 C 的只读适配层：每个查询一行，转发到 A 的 `economy.h` 或 B 的 `evolution.h` / `environment.h` / `legacy.h`。UI 不复制任何公式。

## 界面（深空 · 放置类）

视觉方案整体向《Cookie Clicker》的“大数字 + 金色点击反馈”与《从细胞到奇点》的“生物荧光 + 星图叙事”靠：深空藏青底、金色 HUD、青绿/紫色生物光，主体永远是一颗在皿里发光的活物。

| 能力 | 位置 | 说明 |
|---|---|---|
| 深空调色板 | `render.c` | `BG_DEEP 3,5,11` / 面板三层渐变 / `ACCENT 255,197,102` 金 / `ATP / BIOMASS / DNA / EP` 四色货币 / `TEAL·VIOLET` 生物光；蓝图式分层（底图 → 面板 → 内容 → 光效）不变 |
| 16:9 逻辑画布 + 4K 导出 | `ui.c` `compute_ss()` | 逻辑 1280×720，超采样整数 1–3 倍，`1280×720×3 = 3840×2160` 真 UHD；窗口默认 1600×900，最小 1280×720 |
| 生物形态引擎 12 类 | `screens.c` `draw_organism()` | 阶段决定形态类别：球菌 / 杆菌 / 真核 / 群体 / 组织 / 共生 / 神经 / 技术圈 / 文明 / 太空 / 奇点 / 超越；同物种在皿、展台、图鉴、星图完全一致；未发现物种只留空壳剪影，不泄露形态 |
| 细胞结构 | `screens.c` `sphere()` `membrane()` `capsule()` 与 `morph_*` | 细胞质比膜暗（细胞器才有对比度）+ 双层膜 + 高光弧；真核有核仁/染色质/内质网/线粒体嵴/高尔基/液泡；杆菌有荚膜、拟核、菌毛、贝塞尔鞭毛与分裂沟 |
| 行星渲染 | `screens.c` `planet_body()` | 固定冷色海洋 + 逐列朗伯明暗 + 4 块带海岸线的大陆 + 极冠 + 3 条云带 + 双层大气与受光侧轮廓光，技术圈/文明/太空是“真的球” |
| 培养皿主视觉 | `screens.c` `draw_dish()` | R≈104 的皿（72 格刻度、呼吸环、内圈微尘、冷调液体内辉），整块皿是点击区，点下去有 punch + 闪光 + 星火 |
| 采集反馈 | `ui.c` `ui_fx_burst/tick/draw` + `screens.c` `draw_gain_floats()` | 96 粒粒子池（3 种形状、重力、拖尾）+ 皿心上方 `+x ATP` 浮字三槽轮换，0.02s 内完成一次爽感闭环 |
| 大数字排版 | `render.c` `ui_fmt_num()` | 中文单位 万 / 亿 / 万亿，顶栏胶囊“大数字 + 小单位”，与放置类的数感一致 |
| 商店列 | `screens.c` `draw_shop()` | 页签「机能 / 分化」；机能卖三条永久线，分化卖分支；空档画成“未解锁的分支”占位，不留黑块 |
| 星图进化树 | `screens.c` `draw_tree_overlay()` | 径向谱系布局：父→子按子树叶子数分扇区、独生子给一点“拧劲”弯成螺旋；环=阶段（越外越晚），节点/标签两级避让 + 未发现节点只留空环，20 个物种全部有名字 |
| 情报面板 | `screens.c` `draw_info_panel()` | 中列三页签：下一步（分化/环境/突变/图鉴四行情报 + EP 公式）、产出（ATP/生物量/DNA 存量+产速+72 点 sparkline）、纪录（峰值/总种群/已点亮/预计 EP + 成就徽章） |
| 进度可视化 | `screens.c` `draw_top_bar()` `draw_shop()` | 顶栏纪元牌底部 3px 演化进度条；商店 footer 按 `E/M/S/C/B` 真实得分画五段彩条 |
| 新闻条 | `screens.c` `draw_ticker()` | 底栏 Cookie Clicker 式文案轮播（7 条按当前状态生成，含怀旧旁白），toast 出现时让位 |
| 星云底图 | `atmosphere.c` + `assets/tex/nebula.png` | Seedream 5.5 生成的 2048×1152 深空星云烘焙为底图（缺文件自动回退程序化星云），再叠 260 颗星 + 导航星光环 + 行星弧 + 颗粒 |
| 结算 / Legacy | `screens.c` `draw_end_era()` `draw_legacy()` | 结算把 EP 拆成 `E/M/S/C/B = 计数` 五行，主 CTA 金色实心；Legacy 卡片加大号幽灵等级 + 右下「生涯档案」四项跨轮数据 |
| 提示条 | `ui.c` `draw_toast()` | 底栏右段右对齐，在场时键位提示自动让位 |
| 氛围层 | `atmosphere.c` | 烘焙星云 + 银河带 + 星场 + 右下巨大行星弧，`image_grain` 抖动去色带；轻量 vignette 不压顶栏 |

截图：`docs/screens/final/` 下 16 张 **3840×2160**（主界面 / 商店分化 / 点击 / 提示条 / 情报产出 / 情报纪录 / 突变 / 分化 / 图鉴三页 / 星图 / 结算 / Legacy / 危机 / 终局奇点），工程根同名 `shot_*.png` 即原始导出。

## 构建与运行

```bat
build.bat                :: 一键构建（自动把 D:\mingw64\bin 加进 PATH）
build.bat shot           :: 构建并导出 16 张 3840×2160 截图
treeoflife.exe           :: 在工程根目录运行（assets/ 是相对路径）
mingw32-make             :: 等价构建（Makefile 已改成 Windows 兼容写法）
mingw32-make test        :: 规则层 + 集成接线测试
```

依赖：MinGW-w64（`D:\mingw64`）、raylib 5.5（`libraylib.dll`、`glfw3.dll` 放在根目录）。

若 `mingw32-make` 因为环境缺少 `sh.exe` 报错，直接用一行 gcc：

```powershell
$env:Path='D:\mingw64\bin;'+$env:Path
gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -Iinclude -Idata src/*.c -o treeoflife.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lm
```

## 测试

| 可执行文件 | 覆盖 |
|---|---|
| `build/test_b.exe` | 规则层 21 组、36,498 项断言：物种、突变、环境、危机、EP、永久树 |
| `build/test_integration.exe` | 三方接线 36 项：A 的动作入口真的驱动了 B 的实现，存档在真实结构上往返 |

## 集成时做的四个决定

1. **共享类型以 B 的第十二轮版本为基准**
   B 的 `game_types.h` 是超集（20 物种、34 突变、14 环境、`knowledge` 资源、`capacity_levels` / `tech_levels`）。A 与 C 的版本落后两轮（7 物种、无理念资源）。只补了 C 需要的 `achievement_flags`。

2. **平衡数值单一来源**
   A 原来自带一套 `METABOLISM_COST_GROWTH 1.65`、5 档 Biomass 里程碑；B 标定的是 `1.25` 与 11 档。合并后 `economy.c` 全部改用 `data/balance.h` 的 `RUN_UPGRADE_*` 宏，里程碑直接调 `prestige_biomass_milestone_level()`，删掉了 `RunState.highest_biomass_milestone` 缓存字段。

3. **扣款只在 B 里发生一次**
   `evolution_apply_mutation` / `evolution_apply_speciation` / `evolution_apply_sub_upgrade` / `prestige_commit_end_era` / `legacy_apply_upgrade` 都自己校验并扣资源。A 的 `game_apply_action` 只负责调用与失败回滚，不再重复扣款。

4. **两套测试替身全部移除**
   A 的 `tests/rules_stub.c` 和 C 的 `tests/mock/` 都不进最终构建。UI 直接查询真实 A/B；规则只保留一份实现。

## 布局适配

C 单独开发时数据表是 7 物种 / 12 突变 / 5 级永久树。接入真实数据后重做了三处布局：

- 图鉴：物种 5 列 × 4 行、突变 5 列 × 7 行，面板高度按真实数量计算
- 进化树：12 个阶段改成极坐标星图，每阶段一环，环内整圈均分、逐环旋转偏移，亲缘线走贝塞尔弧
- 永久树：等级上限从 5 改为 B 的 `LEGACY_MAX_LEVEL`（8），进度点同步为 8 格

## 数据中文化

B 的数据表原本是英文。最终工程用 `tools/localize_data.py` 把 117 条名称与描述就地替换为中文（物种、基石、子升级、突变、共振、环境），与 `02-数据与计算规则` 的用词一致。报告页的数据在独立 JS 中，不受影响。

## 已知限制

- 环境与物种的显示名走数据表中的中文；若后续 B 增加内容，需要同步补翻译并重跑 `python tools/gen_glyphs.py`。
- 截图模式（`--shot`）会依次导出中期、晚期、终局三套演示状态；正常游戏仍从原始细胞、0 资源开始。
- A 原有的边界测试针对 `rules_stub` 编写，未随集成迁移；边界行为现由 `test_integration.exe` 与 B 的 36k 断言共同覆盖。


