#!/usr/bin/env sh
# v14 一次性自检流水线（Linux / CI）
# 用法：sh tools/oneshot_check.sh；退出码 0 = 全部通过。
set -u
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT" || exit 1
FAILED=""

step() {
    name=$1
    shift
    printf '\n=== %s ===\n' "$name"
    if "$@"; then
        printf 'PASS: %s\n' "$name"
    else
        printf 'FAIL: %s\n' "$name"
        FAILED="$FAILED / $name"
    fi
}

MAKE=${MAKE:-make}

printf '\n=== 1 内容校验（11 项） ===\n'
python tools/validate_content.py || { echo "FAIL: 内容校验"; exit 1; }
printf 'PASS: 1 内容校验\n'

printf '\n=== 2 严格编译（-Werror） ===\n'
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude -Idata \
    -o build/demo_oneshot.exe demo/demo_b.c src/evolution.c src/species.c \
    src/environment.c src/prestige.c src/legacy.c src/buffs.c -lm || { echo "FAIL: 编译"; exit 1; }
printf 'PASS: 2 严格编译\n'

$MAKE build/test_b.exe || exit 1
step "3 规则层单测（264/282 断言）" build/test_b.exe
$MAKE build/test_integration.exe build/test_regression.exe build/test_gameplay.exe build/test_codex.exe || exit 1
step "4a 接线" build/test_integration.exe
step "4b 回归" build/test_regression.exe
step "4c 玩法（含存档往返与旧档拒绝）" build/test_gameplay.exe
step "4d 图鉴" build/test_codex.exe
step "5 era2 1000 种子" sh -c 'build/demo_oneshot.exe --batch2 1000 > /dev/null'

printf '\n=== 6 era3 1000 种子 ===\n'
build/demo_oneshot.exe --batch3 1000 > build/batch3_oneshot.csv || {
    echo "FAIL: era3 批次"
    exit 1
}
python - <<'PY'
import statistics
rows = [l.strip().split(",") for l in open("build/batch3_oneshot.csv", encoding="utf-8")
        if l.startswith("BATCH3,")]
spc = sorted(int(r[8]) for r in rows)
mut = sorted(int(r[7]) for r in rows)
ticket = statistics.median(int(r[22]) for r in rows)
stage = statistics.median(int(r[6]) for r in rows)


def q(values, p):
    return values[min(int(p * len(values)), len(values) - 1)]


s_med, s_p10, s_p90 = statistics.median(spc), q(spc, 0.10), q(spc, 0.90)
m_med, m_p90 = statistics.median(mut), q(mut, 0.90)
print("era3 中位：阶段 %d · 突变 %d 张（ATP 门票 %d 张）· 单局物种 %d 个"
      % (stage, m_med, ticket, s_med))
print("物种分位：P10 %d / 中位 %d / P90 %d" % (s_p10, s_med, s_p90))
print("突变分位：P90 %d 张" % m_p90)
print("硬门槛 2（突变中位 12–14 / P90 ≤16）：%s"
      % ("PASS" if 12 <= m_med <= 14 and m_p90 <= 16 else "FAIL"))
print("硬门槛 3（物种中位 16–18 / P10 ≥14 / P90 ≤20）：%s"
      % ("PASS" if 16 <= s_med <= 18 and s_p10 >= 14 and s_p90 <= 20 else "FAIL"))
PY

step "7 布局检查" $MAKE test-layout
step "8 存档往返（新档写读 + 旧 v9 档拒绝）" build/test_gameplay.exe
step "9 星图冒烟（264 节点 / 边数上限 / 已发现可见）" build/test_ui_layout.exe

if [ -n "$FAILED" ]; then
    printf '\n失败：%s\n' "$FAILED"
    exit 1
fi
echo ""
echo "全部步骤通过；三条硬门槛的实测值见第 6 步输出。"
