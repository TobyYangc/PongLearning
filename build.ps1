param([string]$Deps = (Join-Path $PSScriptRoot 'external'))

$ErrorActionPreference = 'Stop'

$ray = Join-Path $Deps 'raylib-5.5_win64_mingw-w64'

Push-Location $PSScriptRoot
try {

    
    & windres app.rc -O coff -o app.res
    
    if ($LASTEXITCODE -ne 0) { throw 'Resource build failed' }

    
    $buildArgs = @(

        '-std=c++20', '-Wall', '-Wextra', '-static', '-municode', '-mwindows'

        '-I', (Join-Path $ray 'include'), '-I', $Deps

        'main.cpp', 'menu.cpp', 'game.cpp', 'files.cpp', 'audio.cpp', 'app.res'

        '-L', (Join-Path $ray 'lib'), '-lraylib'
        '-lopengl32', '-lgdi32', '-lwinmm', '-luser32'
        '-o', 'Pong.exe'
    )
    
    & g++ @buildArgs
    
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

} finally { Pop-Location }
