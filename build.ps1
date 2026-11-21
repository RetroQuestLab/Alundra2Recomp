# Compile le prototype : powershell -ExecutionPolicy Bypass -File build.ps1
# Prerequis : external/ (llvm-mingw + SDL3) et ninja.exe dans le PATH (ou le module Python "ninja").
$ErrorActionPreference = 'Stop'
$bin = "$PSScriptRoot\external\llvm-mingw-20261020-ucrt-x86_64\bin"
$env:Path = "$bin;" + $env:Path

$ninjaCmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
if ($ninjaCmd) {
    $ninja = $ninjaCmd.Source
} else {
    $ninja = python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))" 2>$null
}
if (-not $ninja -or -not (Test-Path $ninja)) {
    throw "ninja.exe introuvable : installe-le ou ajoute le module Python 'ninja'."
}

$ninjaFile = "$PSScriptRoot\build\build.ninja"
$needConfigure = -not (Test-Path $ninjaFile)
if (-not $needConfigure) {
    $cmakeFile = Get-Item "$PSScriptRoot\CMakeLists.txt"
    if ($cmakeFile.LastWriteTime -gt (Get-Item $ninjaFile).LastWriteTime) { $needConfigure = $true }
}
if ($needConfigure) {
    cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -G Ninja `
        -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_CXX_COMPILER="$bin\clang++.exe" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
cmake --build "$PSScriptRoot\build"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "OK : build\alundra2.exe"
