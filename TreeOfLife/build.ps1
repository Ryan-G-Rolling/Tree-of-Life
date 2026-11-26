param(
    [switch]$Report
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$src = @(
    'src/evolution.c',
    'src/species.c',
    'src/environment.c',
    'src/prestige.c',
    'src/legacy.c'
)
$cflags = @('-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2', '-Iinclude', '-Idata')

New-Item -ItemType Directory -Force -Path build | Out-Null

Write-Host '[1/3] build test_b' -ForegroundColor Cyan
gcc @cflags 'tests/test_b.c' @src -lm -o 'build/test_b.exe'
if ($LASTEXITCODE -ne 0) { throw 'test_b build failed' }

Write-Host '[2/3] build demo_b' -ForegroundColor Cyan
gcc @cflags 'demo/demo_b.c' @src -lm -o 'build/demo_b.exe'
if ($LASTEXITCODE -ne 0) { throw 'demo_b build failed' }

Write-Host '[3/3] run self tests' -ForegroundColor Cyan
& ./build/test_b.exe
if ($LASTEXITCODE -ne 0) { throw 'self tests failed' }

Write-Host '[check] docs constants vs balance.h' -ForegroundColor Cyan
python 'build/gen_constants.py' '--check'
if ($LASTEXITCODE -ne 0) { throw 'docs constants out of sync' }

if ($Report) {
    Write-Host '[report] regenerate round-12 regression data' -ForegroundColor Cyan
    & ./build/demo_b.exe --batch2 1000 | Set-Content -Path 'build/batch2_scan_round12.csv' -Encoding ascii
    & ./build/demo_b.exe --batch3 1000 | Set-Content -Path 'build/batch3_scan_round12.csv' -Encoding ascii
    & ./build/demo_b.exe --batch3 1000 --explore |
        Set-Content -Path 'build/batch3_explore_round12.csv' -Encoding ascii

    python 'build/gen_round12.py'
    if ($LASTEXITCODE -ne 0) { throw 'report data generation failed' }
    python 'build/make_report_table.py'
    if ($LASTEXITCODE -ne 0) { throw 'acceptance table generation failed' }
    Write-Host 'report data updated: ../tree-of-life-b/assets/round12.js + acceptance.js' -ForegroundColor Green
}

Write-Host 'OK' -ForegroundColor Green
