> 2026-11-26：A+B+C 已接通，统一构建请在仓库根目录运行 `./build.ps1`。本页以下保留该成员独立开发时的说明；整合状态以根目录 README.md 和 INTEGRATION_NOTES.md 为准。

# Tree of Life · Member B 规则层

本目录是 B 负责的规则实现、自测与演示，不包含 Raylib，也不包含 A/C 的经济与界面代码。

## 三条命令

```powershell
# 构建 + 自测（Windows PowerShell）
./build.ps1

# 构建 + 自测 + 重新生成第四轮报告数据
./build.ps1 -Report

# 用 MSYS / WSL 时也可用 Make
make && make test
```

## 目录

| 路径 | 内容 |
| --- | --- |
| `include/` | 共享结构与 B 的公共头文件 |
| `src/` | evolution / species / environment / prestige / legacy |
| `data/balance.h` | 唯一平衡数据源 |
| `tests/test_b.c` | 规则自测（当前 21 组、36,498 项断言；支持 `--json`） |
| `demo/demo_b.c` | 控制台演示与经济桩，支持 `--csv / --batch / --batch2 / --batch3 / --explore / --sensitivity` |
| `build/` | 构建产物、实验 CSV 与报告生成脚本（不要整体删除） |

## 当前状态

- 规则层可编译、零警告（`-Werror`），自测全绿。
- 内容：20 个物种（阶段 0–11）、34 个突变、14 个环境事件、18 层子升级；EP 上限 58。
- 经济：DNA 按人口 0.25 次幂产出（阶段 0–1 ×1.5），理念按人口 0.5 次幂产出；
  突变 `1.48^已拥有必需突变数 × (0.35 + 0.12×阶段)`，首突变 6 折，可选突变不进链；
  容量无上限、4 级起 70% ATP + 30% Biomass；ATP 应急储备保留。
- 危机：按事件期间最低人口判定，纪元 1 通过线 0.63、纪元 2+ 0.70；
  伤害按类型标定，劣势产率 ×0.4，陨石即时损失 `0.35 + 0.05×阶段`。
- 永久树为 8 级、单线成本 36 EP、四线共 144 EP。
- A 的 `game/economy/save` 与 C 的界面尚未实现，当前只能通过 `demo_b` 验证规则。
- 存档版本为 6（824 B）。

## 已知缺口

- `build/` 同时存放产物与报告数据源，清理时请勿删除其中的 CSV 与 `gen_*.py`。
- MinGW 不支持 ASan；需要 sanitizer 时用 WSL/LLVM 单独跑一次。
- 报告页的历史小节是各轮快照，默认值以最新一节为准。
