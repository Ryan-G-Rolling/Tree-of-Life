param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
function Run-Native([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed ($LASTEXITCODE)" }
}
$dependency = '.local/deps/raylib-5.5_win64_mingw-w64'
if (!(Test-Path -LiteralPath "$dependency/include/raylib.h")) {
    New-Item -ItemType Directory -Force .local/deps | Out-Null
    Run-Native curl.exe @('-L','--fail','--output','.local/deps/raylib.zip','https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip')
    Expand-Archive -LiteralPath .local/deps/raylib.zip -DestinationPath .local/deps -Force
    Remove-Item -LiteralPath .local/deps/raylib.zip
}
if (!(Test-Path -LiteralPath 'assets/fonts/ui_zh.ttf')) {
    New-Item -ItemType Directory -Force assets/fonts | Out-Null
    Copy-Item -LiteralPath "$env:WINDIR/Fonts/Deng.ttf" -Destination assets/fonts/ui_zh.ttf
}
Run-Native python @('tools/gen_glyphs.py')
if (!$SkipTests) { Run-Native mingw32-make @('-B','test','WARN=-Wall -Wextra -Wpedantic -Werror') }
Run-Native mingw32-make @('all')
if (!$SkipTests) { Run-Native mingw32-make @('test-ui') }
Write-Host 'Ready: .\treeoflife.exe (unified A+B+C)'
