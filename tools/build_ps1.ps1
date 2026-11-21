# Build canonique de la cible PS1 native (separée du build PC).
#
# Usage : powershell -ExecutionPolicy Bypass -File tools\build_ps1.ps1
# Sortie : ps1\build\alundra2.exe (PS-X EXE) et alundra2_ps1.bin/.cue
# Ne touche PAS a build.ps1 ni au dossier build/ du PC.

[CmdletBinding()]
param(
	[string]$SdkRoot,
	[switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

if (-not $SdkRoot) {
	$SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32'
}
$sdkBin = Join-Path $SdkRoot 'bin'
if (-not (Test-Path (Join-Path $sdkBin 'mipsel-none-elf-gcc.exe'))) {
	throw "Toolchain MIPS absente: $sdkBin"
}

$env:PSN00BSDK_LIBS = Join-Path $SdkRoot 'lib\libpsn00b'
$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path

$src = Join-Path $repo 'ps1'
$build = Join-Path $src 'build'
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

# Les blobs 4bpp sont generes depuis assets/ (non publie) avant chaque build.
$assetsDir = Join-Path $src 'assets'
if (-not (Test-Path $assetsDir)) { New-Item -ItemType Directory -Path $assetsDir | Out-Null }
$zoneMap = Join-Path $repo 'assets\maps\011.a1m'
$heroSprite = Join-Path $repo 'assets\sprites\hero.a1s'
if ((Test-Path $zoneMap) -and (Test-Path $heroSprite)) {
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') $zoneMap $heroSprite $assetsDir
	if ($LASTEXITCODE -ne 0) { throw 'generation des blobs P1ZM/P1SP' }
} else {
	Write-Warning 'Sources A1 absentes : utilisation des blobs deja present dans ps1/assets.'
}
if (-not (Test-Path (Join-Path $assetsDir 'zone011.dat')) -or
    -not (Test-Path (Join-Path $assetsDir 'hero.dat'))) {
	throw 'Blobs PS1 absents : generation impossible.'
}

# L'atlas de police A1 est genere depuis src/a1_font.h (FONT3.TIM non publie).
& py -3.12 (Join-Path $repo 'tools\ps1_export_font.py')
if ($LASTEXITCODE -ne 0) { throw 'generation de l atlas de police PS1' }

Push-Location $src
try {
	cmake --preset default .
	if ($LASTEXITCODE -ne 0) { throw 'configuration CMake PS1' }
	cmake --build build
	if ($LASTEXITCODE -ne 0) { throw 'build PS-X EXE / BIN-CUE' }
}
finally { Pop-Location }

Get-ChildItem $build -File | Where-Object { $_.Name -match 'alundra2.*\.(exe|elf|bin|cue|map)' } |
	Select-Object Name, Length | Format-Table -AutoSize
Write-Host 'PS1_BUILD: PASS'
