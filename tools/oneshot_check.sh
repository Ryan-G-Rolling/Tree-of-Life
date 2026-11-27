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
if python tools/reports/gate_check.py build/batch3_oneshot.csv; then
    printf 'PASS: 6 硬门槛\n'
else
    printf 'FAIL: 6 硬门槛\n'
    FAILED="$FAILED / 6 硬门槛"
fi

step "7 布局检查" $MAKE test-layout
step "8 存档往返（新档写读 + 旧 v9 档拒绝）" build/test_gameplay.exe
step "9 星图冒烟（264 节点 / 边数上限 / 已发现可见）" build/test_ui_layout.exe

if [ -n "$FAILED" ]; then
    printf '\n失败：%s\n' "$FAILED"
    exit 1
fi
echo ""
echo "全部步骤通过；三条硬门槛的实测值见第 6 步输出。"
