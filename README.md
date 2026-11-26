# Tree of Life

三人 C 语言课程项目，使用 Raylib 2D。点击与自动产出积累 ATP、生物量和 DNA，选择突变、推进物种分化，经历环境事件，再通过结束纪元获得 EP 和永久强化。111

## 运行整合版

Windows + MinGW-w64 GCC + Python + PowerShell：

```powershell
.\build.ps1
.\treeoflife.exe
```

构建脚本运行 A 的三组测试、B 的规则测试及真实 A+B 集成测试，然后编译 C 的图形界面。首次构建从 [raylib 官方 5.5 发布页](https://github.com/raysan5/raylib/releases/tag/5.5) 下载 Windows 开发库到 `.local/deps`。字体从本机 Windows 等线复制到 `assets/fonts`，仅供课程本地演示。公开发布前须改用允许再分发的字体。

已构建过时可直接打开根目录 `treeoflife.exe`；程序始终从可执行文件旁读取资源、写入存档。`mingw32-make game` 只构建游戏，`mingw32-make integration` 执行完整构建与检查。

## 整合状态（2026-11-26）

已合入 `codex/member-b` 和 `codex/member-c`，正式游戏链接真实 A/B/C，未链接 `tests/rules_stub.c` 或 `ui/tests/mock`。保留队友原分支及提交历史。

- A：统一动作入口、固定步推进、经济与原子存读档。
- B：20 个物种、34 个突变、14 个环境、三物种槽、8 级永久强化、理念与子升级。
- C：主界面、突变与分化、图鉴、进化树、结算和永久强化页面。
- 已补齐随机状态提交、环境即时损失、危机最低人口、跨轮奖励，以及容量/储备/科技操作；图鉴与进化树支持滚轮翻页。

本版采用 **B 上传分支的扩充内容与数值**，与最初规格中的内容量不同。这是可运行的整合候选版，长期平衡和最终课程范围仍需团队试玩决定。集成细节和验收限制见 [INTEGRATION_NOTES.md](INTEGRATION_NOTES.md)。

## 操作

鼠标可完成全部主要动作。空格点击 ATP；1/2/3 购买单轮升级；M 突变、E 结算、T 进化树、C 图鉴、Esc 关闭弹层、F11 全屏。字母快捷键建议在英文输入法下使用。存在第三物种时，Tab 切换右侧分支卡。图鉴与进化树用鼠标滚轮翻页；底栏可购买容量扩张、应急储备和主谱系科技。

每 15 秒、成功操作后和退出时保存到 `saves/treeoflife-v7.sav`。结构已统一升级为 v7，旧的 A v1、B 演示档及 C 模拟档不兼容；旧路径文件保留。

## 验证与截图

```powershell
# A 的隔离边界测试（此目标才使用 rules_stub）
mingw32-make -B test headers
# 完整优化编译、A/B/真实集成测试、游戏编译
.\build.ps1
# 完整构建会重新生成 build/playthrough.sav；随后可导出截图，不修改试玩存档
.\treeoflife.exe --shot --load build/playthrough.sav --ss 2
```

`tests/test_integration.c` 覆盖真实突变与重复开候选、失败回滚、三谱系、双币容量、理念科技、Lv.8、陨石只扣一次、事件循环、固定步一致性、结算防重领、跨轮和存档往返，并从零资源自动模拟 60 分钟。它是固定种子与固定购买策略的回归验证，不能代表所有玩家的平衡体验。

原始设计：[玩法](00-项目介绍与完整玩法.md) · [分工与接口](01-三人分工与接口约定.md) · [数据规则](02-数据与计算规则.md)。B 的扩充说明见 `b/docs/`，C 的视觉说明见 `ui/docs/art/`。

## 工作日志与工作区

所有后续项目对话均按 [AGENTS.md](AGENTS.md) 记录改动，日志见 [WORK_LOG.md](WORK_LOG.md)。工作结束后清理临时记录，保留最终结果。

- `deliverables/Tree-of-Life-ABC/`：可直接试玩的目录。
- `deliverables/Tree-of-Life-ABC-20261126.zip`：最终试玩压缩包。
- `deliverables/screens/`：整合版结果截图。
- `assets/`：根目录游戏需要的字体与音效；`.local/deps/`：构建依赖。
- `build/`：构建与测试时重新生成的临时产物，收尾后可清理。
