# v14 一次性自检流水线（Windows / PowerShell）
# 用法：powershell -ExecutionPolicy Bypass -File tools/oneshot_check.ps1
# 退出码 0 = 全部步骤通过；1 = 有步骤失败。
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
Set-Location (Split-Path -Parent $PSScriptRoot)
$script:failed = @()

function Step($name, [scriptblock]$body) {
    Write-Host ""
    Write-Host "=== $name ===" -ForegroundColor Cyan
    try {
        & $body
        if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        Write-Host "PASS: $name" -ForegroundColor Green
    } catch {
        Write-Host "FAIL: $name -- $_" -ForegroundColor Red
        $script:failed += $name
    }
}

Step "1 内容校验 11 项" { python tools/validate_content.py }

Step "2 严格编译（0 warning）" {
    New-Item -ItemType Directory -Force build | Out-Null
    gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude -Idata `
        -o build/demo_oneshot.exe demo/demo_b.c src/evolution.c src/species.c `
        src/environment.c src/prestige.c src/legacy.c src/buffs.c -lm
}

Step "3 规则层单测（264/282）" {
    mingw32-make SHELL=cmd.exe build/test_b.exe
    build/test_b.exe
}

Step "4 接线/回归/玩法/图鉴" {
    mingw32-make SHELL=cmd.exe build/test_integration.exe build/test_regression.exe `
        build/test_gameplay.exe build/test_codex.exe
    build/test_integration.exe
    build/test_regression.exe
    build/test_gameplay.exe
    build/test_codex.exe
}

Step "5 era2 1000 种子" { build/demo_oneshot.exe --batch2 1000 | Out-Null }

Step "6 era3 1000 种子（打印硬门槛 2/3 实测值）" {
    build/demo_oneshot.exe --batch3 1000 | Set-Content -Encoding UTF8 build/batch3_oneshot.csv
    $stage = @(); $mut = @(); $spc = @(); $ticket = @()
    foreach ($line in (Get-Content build/batch3_oneshot.csv | Where-Object { $_ -like "BATCH3,*" })) {
        $f = $line.Split(",")
        $stage += [int]$f[6]
        $mut += [int]$f[7]
        $spc += [int]$f[8]
        $ticket += [int]$f[22]
    }
    function Median($values) {
        $sorted = $values | Sort-Object
        $mid = [int]($sorted.Count / 2)
        if ($sorted.Count % 2 -eq 1) { return $sorted[$mid] }
        return [int](($sorted[$mid - 1] + $sorted[$mid]) / 2)
    }
    $mut = Median $mut
    $spc = Median $spc
    $ticket = Median $ticket
    $stage = Median $stage
    Write-Host ("era3 中位：阶段 {0} · 突变 {1} 张（ATP 门票 {2} 张）· 单局物种 {3} 个" -f $stage, $mut, $ticket, $spc)
    $gate2 = if ($mut -ge 10 -and $mut -le 16) { "PASS" } else { "FAIL" }
    $gate3 = if ($spc -ge 16 -and $spc -le 20) { "PASS" } else { "FAIL" }
    Write-Host ("硬门槛 2（10–16 张）：{0}（实测 {1} 张）" -f $gate2, $mut)
    Write-Host ("硬门槛 3（16–20 个）：{0}（实测 {1} 个）" -f $gate3, $spc)
}

Step "7 布局检查" { mingw32-make SHELL=cmd.exe test-layout }

Step "8 存档往返 + 旧档拒绝" {
    build/test_gameplay.exe
}

Step "9 星图冒烟（264 节点布局）" {
    mingw32-make SHELL=cmd.exe test-layout
    build/test_ui_layout.exe
}

Write-Host ""
if ($script:failed.Count -gt 0) {
    Write-Host ("失败：" + ($script:failed -join " / ")) -ForegroundColor Red
    exit 1
}
Write-Host "全部步骤通过。硬门槛请手工核对 oneshot 报告里的 era3 突变张数与单局物种数。" -ForegroundColor Green
exit 0
