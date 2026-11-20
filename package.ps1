param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & ./build.ps1
    & $Python -m PyInstaller --noconfirm --clean --windowed --onedir --name Pong --add-binary 'pong_core.dll;.' --manifest app.manifest launcher.py
    if ($LASTEXITCODE) { throw 'Python packaging failed' }
    $licenseDir = Join-Path $PSScriptRoot 'dist/Pong/_internal/licenses'
    New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'external/raylib-5.5_win64_mingw-w64/LICENSE') -Destination (Join-Path $licenseDir 'raylib-LICENSE') -Force
} finally { Pop-Location }
