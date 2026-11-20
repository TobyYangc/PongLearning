param([string]$Deps = (Join-Path $PSScriptRoot 'external'))
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $ray = Join-Path $Deps 'raylib-5.5_win64_mingw-w64'
    $flags = @('-std=c++20', '-Wall', '-Wextra', '-O2', '-static',
               '-I', (Join-Path $ray 'include'))
    $libraries = @('-L', (Join-Path $ray 'lib'), '-lraylib',
                   '-lopengl32', '-lgdi32', '-lwinmm')
    # 靜態使用 raylib 的碰撞函式；視窗與繪圖仍由 Tkinter 負責。
    & g++ @flags -shared bridge.cpp game.cpp @libraries -o pong_core.dll
    if ($LASTEXITCODE) { throw 'DLL build failed. Close the running game before rebuilding.' }
    foreach ($test in @('test_game', 'test_collision')) {
        & g++ @flags "$test.cpp" game.cpp @libraries -o "$test.exe"
        if ($LASTEXITCODE) { throw "$test compilation failed" }
        & "./$test.exe"
        if ($LASTEXITCODE) { throw "$test failed" }
    }
} finally { Pop-Location }
