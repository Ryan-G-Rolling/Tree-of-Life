# Tree of Life 项目文档

The group assignment for our C lesson 1

本目录是三人共同开发 Raylib 2D 生物进化增量游戏的设计基准。开发前先阅读以下三份文档。

1. [项目介绍与完整玩法](00-项目介绍与完整玩法.md)
2. [三人分工与接口约定](01-三人分工与接口约定.md)
3. [数据与计算规则](02-数据与计算规则.md)

## 当前状态

- 游戏方向、核心循环、初始数据和计算顺序已经冻结。
- Member A 的独立核心、经济和存档实现已完成一轮加固与控制台验收；见 [A 模块接入说明](INTEGRATION_NOTES.md)。突变、分化和部分环境提交仍待三人确认接口，B/C 的实现尚未接入。
- 数值可在测试后调整，资源种类、模块边界、状态流程和跨轮规则不应由个人或 Agent 擅自更改。
- 若确实需要改变核心规则，三人先更新本文档，再修改接口和代码。

## 已确定的核心

- 使用 C 语言和 Raylib 2D。
- 玩法由增量成长、主动选择突变、环境自然选择和跨轮永久成长组成。
- 单轮从原始生命开始，默认推进一条主谱系。
- `Diversification Lv.2` 允许维持一个额外物种分支，`Lv.3` 开启多物种协同。
- 玩家主动结束当前纪元并获得 Evolution Points，游戏不会强制结束本轮。
- 游戏逻辑不得依赖 Raylib，图形界面只读取状态和提交操作。

## 编译与测试 Member A 草稿

在本机 MinGW 环境运行 `mingw32-make test headers`。`tests/rules_stub.c` 仅为 Member B 规则的测试替身，不能加入正式游戏构建。

测试包括原有操作闭环、固定步进与失败回滚边界、损坏存档校验三组，以及公共头文件编译检查。`make all` 构建的是控制台测试程序，当前不包含可玩的 Raylib 游戏。

2026-11-25 已通过普通编译和 `-O2` 优化编译验收。可用以下命令强制完整重编：

```powershell
mingw32-make -B test headers
mingw32-make -B test headers "CFLAGS=-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude"
```
