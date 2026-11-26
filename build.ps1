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
}
New-Item -ItemType Directory -Force build,assets/fonts,assets/sfx | Out-Null
if (!(Test-Path -LiteralPath assets/fonts/ui_zh.ttf)) {
    Copy-Item -LiteralPath "$env:WINDIR/Fonts/Deng.ttf" -Destination assets/fonts/ui_zh.ttf
}
Run-Native python @('ui/tools/gen_sfx.py')
Copy-Item ui/assets/sfx/*.wav assets/sfx/
Run-Native python @('ui/tools/gen_glyphs.py')
$rules = @('b/src/evolution.c','b/src/species.c','b/src/environment.c','b/src/prestige.c','b/src/legacy.c')
$core = @('src/game.c','src/economy.c','src/save.c')
$strict = @('-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-Iinclude','-Idata')
if (!$SkipTests) {
    Run-Native mingw32-make @('-B','test','headers','CFLAGS=-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude')
    Run-Native gcc ($strict + @('b/tests/test_b.c') + $rules + @('-o','build/test_b.exe','-lm'))
    Run-Native ./build/test_b.exe @()
    Run-Native gcc ($strict + @('tests/test_integration.c') + $core + $rules + @('-o','build/test_integration.exe','-lm'))
    Run-Native ./build/test_integration.exe @()
}
$ui = @('ui/src/main.c','ui/src/ui.c','ui/src/render.c','ui/src/screens.c','ui/src/input.c','ui/src/atmosphere.c','src/ui_adapter.c')
Run-Native gcc (@('-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Iinclude','-Idata','-Iui/include',"-I$dependency/include") + $ui + $core + $rules + @('-o','treeoflife.exe',"$dependency/lib/libraylib.a",'-lopengl32','-lgdi32','-lwinmm','-lm'))
Write-Host 'Ready: .\treeoflife.exe (real A+B+C, no mock rules)'
