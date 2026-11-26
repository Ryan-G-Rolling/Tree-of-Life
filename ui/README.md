> 2026-11-26：A+B+C 已接通，统一构建请在仓库根目录运行 `./build.ps1`。本页以下保留该成员独立开发时的说明；整合状态以根目录 README.md 和 INTEGRATION_NOTES.md 为准。

# Tree of Life · 客户端（Member C）

Raylib 2D 生物进化增量游戏的**图形界面与交互层**。规则与数值属于 Member A / B，
本目录只负责「把 GameState 画出来」和「把点击变成 GameAction」。

```text
Raylib 输入 → [C] ui_* → GameAction → [A] game_apply_action → [B] 规则计算
                                                                   ↓
             [C] render ← 只读 GameState ← [A] 一次性提交状态 ←────┘
```

## 我负责什么 / 不负责什么

| 属于 C | 不属于 C |
| --- | --- |
| `src/main.c` 窗口、帧循环、固定步累加、读档存档时机 | 任何资源公式与平衡数值 |
| `src/ui.c` 页面状态机、按钮 → GameAction 转换、提示条 | 修改 `GameState` 任何字段 |
| `src/screens.c` 7 个页面/弹层的布局与绘制 | 决定 EP 怎么算 |
| `src/render.c` 文本、面板、进度条、按钮等绘制原语 | 决定环境倍率 |
| `src/input.c` 逻辑坐标换算、最上层命中判定 | 增加新资源或新机制 |
| `assets/` 字体与音效 | 改 `game_types.h` 的所有权 |

界面对 `GameState` 只有 `const` 权限。全目录 grep 不到 `run.xxx =`，只有 `game_apply_action` 一条写路径。

## 目录

```text
TreeOfLife/
├── include/
│   ├── game_types.h      M0 共享类型（提案稿，三人确认后冻结）
│   ├── ui.h              对外 4 个入口（01 文档 §6.4）
│   ├── ui_internal.h     C 私有：UIState、布局、绘制原语
│   ├── ui_adapter.h      C 需要的只读查询契约（见 03 提案）
│   └── ui_glyphs.h       自动生成的中文字形集
├── src/                  main / ui / screens / render / input
├── data/                 平衡表（A/B 所有，C 只读，目前为空）
├── tests/mock/           临时假 A + 假 B + 动作自检（集成时删除）
├── tools/                gen_glyphs.py / gen_sfx.py / crop.c
├── assets/fonts,assets/sfx
└── docs/screens/         7 张页面截图（验收用）
```

## 构建

需要 MinGW-w64 gcc 与 raylib 5.5。本机把这套装在 `D:/mingw64`。

```bash
cd TreeOfLife
mingw32-make            # 默认 MOCK=1：用 tests/mock 的假 A/B，界面独立可跑
./treeoflife.exe        # 必须在项目根目录运行（assets/ 是相对路径）
```

`libraylib.dll` 与 `glfw3.dll` 已经放在根目录，与 exe 同目录即可运行，无需配 PATH。

### 首次配置 raylib（其他同学照做一次）

```bash
cd /tmp
curl -O https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/mingw64/mingw-w64-x86_64-raylib-5.5-2-any.pkg.tar.zst
curl -O https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/mingw64/mingw-w64-x86_64-glfw-3.4-1-any.pkg.tar.zst
/c/Windows/System32/tar.exe -xf mingw-w64-x86_64-raylib-5.5-2-any.pkg.tar.zst   # Git Bash 自带 tar 缺 zstd
/c/Windows/System32/tar.exe -xf mingw-w64-x86_64-glfw-3.4-1-any.pkg.tar.zst
cp mingw64/include/raylib.h          /d/mingw64/x86_64-w64-mingw32/include/
cp mingw64/lib/libraylib.dll.a       /d/mingw64/x86_64-w64-mingw32/lib/
cp mingw64/bin/libraylib.dll         <项目根>/
cp mingw64/bin/glfw3.dll             <项目根>/   # 来自 glfw 包
```

## 操作

| 输入 | 行为 |
| --- | --- |
| 鼠标点击生命体 / `空格` | 点击获得 ATP |
| `1` `2` `3` | 购买代谢 / 繁殖 / 基因升级 |
| `M` | 打开突变面板（首次打开生成候选，之后不再刷新） |
| `E` | 打开 End of Era 预览 |
| `T` / `C` | 进化树 / 图鉴 |
| `Esc` | 关闭弹层 |

## 自检

```bash
mingw32-make test        # 33 项：按钮→GameAction 字段映射 + 八步流程 + 存档往返
./treeoflife.exe --shot  # 导出 9 张页面 png 到根目录，用于离线检查排版
```

`--selftest` 重点验证 A/B 最容易出错也最难肉眼发现的东西：`target_id` / `option` 有没有填错目标。

## 资源

- `assets/fonts/ui_zh.ttf`：等线（Windows 自带），**仅课程演示用**。要公开发布请换成 Noto Sans SC（OFL）。
- Raylib 默认字体不含中文，必须 `LoadFontEx` 显式给码点。新增中文文案后跑一次：

```bash
mingw32-make glyphs      # 扫描 src/ 里的中文串，重生成 include/ui_glyphs.h
```

  启动日志会打印 `UI font: N glyphs loaded`；若有缺字会告警 `missing U+XXXX`。

- `assets/sfx/*.wav`：`python tools/gen_sfx.py` 合成的三个提示音（无版权问题）。

## 布局规范

- 逻辑画布固定 **1280×800**，窗口任意缩放时整体缩放并留黑边（`ui.c` 的离屏纹理 + letterbox），
  所以布局代码永远用固定坐标，不需要关心窗口尺寸。
- 每帧先 `screens_build()` 生成按钮矩形表，再判命中，最后用同一份矩形绘制：
  弹层天然压住底层，不会出现“画在下层却被点到”。
- UI 自己的状态（悬停、动画、弹层、提示条）全在 `g_ui`，**不进 GameState、不进存档**。

## 集成到 A/B

1. 把 `tests/mock/mock_adapter.c` 换成一行转发到 A/B 真实函数的实现（函数清单见 `ui_adapter.h` 与 03 提案）。
2. `make MOCK=0 EXTRA_SRC="src/game.c src/economy.c src/save.c src/evolution.c src/species.c src/environment.c src/prestige.c src/legacy.c"`
3. `src/` 下的界面代码一行都不用改；`#ifdef UI_MOCK` 只出现在 `main.c` 的 include 与初始化处。

## 已知限制

- 只有课程最终版的物种/突变内容量（7 物种、12 突变、7 环境），`SPECIES_COUNT` 等上限留给 A/B 扩充。
- 截图模式用假状态，数字不代表真实平衡（平衡由 A/B 负责）。
- 未做窗口全屏切换、手柄支持；音效只有三个提示音，没有背景音乐。
