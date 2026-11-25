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

# Licence du disque (secteurs 0-15 : texte "Licensed by Sony Computer
# Entertainment ..." et logo). Une vraie PS1 se bloque sur le logo sans elles
# (essai materiel v0.8.40). Elles sont lues sur le disque ORIGINAL de
# l'utilisateur (image brute 2352), jamais telechargees ni publiees.
$licenseOut = Join-Path $src 'assets\license_scee.dat'
$licenseSrc = $env:ALUNDRA_LICENSE_DISC
if (-not $licenseSrc) {
	$cand = Get-ChildItem -LiteralPath $repo -Filter '*SLES*.bin' -File -ErrorAction SilentlyContinue | Select-Object -First 1
	if ($cand) { $licenseSrc = $cand.FullName }
}
if ($licenseSrc -and (Test-Path -LiteralPath $licenseSrc)) {
	$fs = [IO.File]::OpenRead($licenseSrc)
	try { $buf = New-Object byte[] (16 * 2352); [void]$fs.Read($buf, 0, $buf.Length) } finally { $fs.Close() }
	$lic = [Text.Encoding]::ASCII.GetString($buf, 4 * 2352 + 24, 60)
	if ($lic -notmatch 'Licensed\s+by') { throw "secteur de licence introuvable dans $licenseSrc" }
	if (-not (Test-Path (Split-Path $licenseOut))) { New-Item -ItemType Directory -Path (Split-Path $licenseOut) | Out-Null }
	[IO.File]::WriteAllBytes($licenseOut, $buf)
	Write-Host ('Licence du disque : ' + ($lic -replace '\s+', ' ').Trim())
} elseif (-not (Test-Path $licenseOut)) {
	throw 'licence de disque absente : definir ALUNDRA_LICENSE_DISC (image brute 2352 de votre disque)'
}

# Les blobs 4bpp sont generes depuis assets/ (non publie) avant chaque build.
$assetsDir = Join-Path $src 'assets'
if (-not (Test-Path $assetsDir)) { New-Item -ItemType Directory -Path $assetsDir | Out-Null }
$zoneMap = Join-Path $repo 'assets\maps\011.a1m'
$heroSprite = Join-Path $repo 'assets\sprites\hero.a1s'
if ((Test-Path $zoneMap) -and (Test-Path $heroSprite)) {
	# Une seule zone active en RAM : un blob P1ZM par zone de la suite A→Z.
	# 026 sert uniquement au harnais de mesure du pic heap (jamais de warp de jeu).
	foreach ($zoneId in @('011', '013', '019', '119', '120', '026')) {
		$map = Join-Path $repo "assets\maps\$zoneId.a1m"
		if (-not (Test-Path $map)) { throw "carte A1 absente : $map" }
		& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') $map $heroSprite $assetsDir
		if ($LASTEXITCODE -ne 0) { throw "generation du blob P1ZM zone $zoneId" }
	}
	# Ruines d'Eden : carte A1 028 (data/world.txt `map 042 028`), lue sur le CD.
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') (Join-Path $repo 'assets\maps\028.a1m') $heroSprite $assetsDir '042'
	if ($LASTEXITCODE -ne 0) { throw 'generation du blob P1ZM zone 042' }
	# Puerto Medusa : carte A1 029 (data/world.txt `map 050 029`).
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') (Join-Path $repo 'assets\maps\029.a1m') $heroSprite $assetsDir '050'
	if ($LASTEXITCODE -ne 0) { throw 'generation du blob P1ZM zone 050' }
	# Manoir de Milena : carte A1 316 (data/world.txt `map 086 316`).
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') (Join-Path $repo 'assets\maps\316.a1m') $heroSprite $assetsDir '086'
	if ($LASTEXITCODE -ne 0) { throw 'generation du blob P1ZM zone 086' }
	# Fonds du prologue (src/title.cpp::useZone : carte A1 brute du meme nom).
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') (Join-Path $repo 'assets\maps\042.a1m') $heroSprite $assetsDir 'intr042'
	if ($LASTEXITCODE -ne 0) { throw 'generation du fond d intro 042' }
	& py -3.12 (Join-Path $repo 'tools\ps1_export_zone.py') (Join-Path $repo 'assets\maps\018.a1m') $heroSprite $assetsDir
	if ($LASTEXITCODE -ne 0) { throw 'generation du fond d intro 018' }
} else {
	Write-Warning 'Sources A1 absentes : utilisation des blobs deja present dans ps1/assets.'
}
# Regression VRAM / exporteurs (inclut les sprites PNJ de la premiere interaction).
& py -3.12 (Join-Path $repo 'tools\ps1_export_zone_test.py')
if ($LASTEXITCODE -ne 0) { throw 'tests exportateurs PS1' }
foreach ($blob in @('zone011.dat', 'zone013.dat', 'zone019.dat', 'zone119.dat', 'zone120.dat', 'zone026.dat', 'zone042.dat', 'zone050.dat', 'zone086.dat', 'intr042.dat', 'zone018.dat', 'hero.dat')) {
	if (-not (Test-Path (Join-Path $assetsDir $blob))) { throw "Blob PS1 absent : $blob" }
}

# L'atlas de police A1 est genere depuis src/a1_font.h (FONT3.TIM non publie).
& py -3.12 (Join-Path $repo 'tools\ps1_export_font.py')
if ($LASTEXITCODE -ne 0) { throw 'generation de l atlas de police PS1' }

# Les PNJ des quatre zones viennent des vrais sprites A1 (aucune couleur
# inventee) : ancien/villageois/alexia/belgar/natasha/jeehan -> npc.dat.
& py -3.12 (Join-Path $repo 'tools\ps1_export_npc.py') $assetsDir
if ($LASTEXITCODE -ne 0) { throw 'generation des sprites PNJ PS1' }

# Jalon boss : vrai sprite ogre A1 et deux coffres A1 dans actor.dat.
& py -3.12 (Join-Path $repo 'tools\ps1_export_actor.py') $assetsDir
if ($LASTEXITCODE -ne 0) { throw 'generation des acteurs PS1' }
if (-not (Test-Path (Join-Path $assetsDir 'enemy_anim.dat'))) { throw 'Blob PS1 absent : enemy_anim.dat' }
& py -3.12 (Join-Path $repo 'tools\ps1_export_enemy_test.py')
if ($LASTEXITCODE -ne 0) { throw 'tests export animation ogre PS1' }

# Le HUD utilise les vraies gemmes A1 (a1_hud.a1s) au lieu de rectangles rouges.
& py -3.12 (Join-Path $repo 'tools\ps1_export_hud.py') $assetsDir
if ($LASTEXITCODE -ne 0) { throw 'generation des gemmes du HUD PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_export_portrait.py') $assetsDir
if ($LASTEXITCODE -ne 0) { throw 'generation des bustes PS1' }

# Les panneaux de dialogue A2 sont generes depuis data/dialogues.txt (pagination A1).
& py -3.12 (Join-Path $repo 'tools\ps1_export_dialogue.py')
if ($LASTEXITCODE -ne 0) { throw 'generation des panneaux de dialogue PS1' }

# Audio natif : encodeur PS-ADPCM teste, puis banque VAG < 512 Ko pour le SPU.
& py -3.12 (Join-Path $repo 'tools\ps1_vag_tools_test.py')
if ($LASTEXITCODE -ne 0) { throw 'test encodeur VAG PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_audio_bank_test.py')
if ($LASTEXITCODE -ne 0) { throw 'generation/test banque audio PS1' }

# Musique CD : 40 themes VAGi generes une fois puis masterises dans l'ISO.
& py -3.12 (Join-Path $repo 'tools\ps1_export_music.py')
if ($LASTEXITCODE -ne 0) { throw 'generation des themes VAGi PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_music_bank_test.py')
if ($LASTEXITCODE -ne 0) { throw 'test banque musicale CD PS1' }

# L'inventaire A2 (59 objets) et les icones A1 natives du menu sont generes en
# tables POD avant la compilation MIPS ; les tests hotes verifient l'export.
& py -3.12 (Join-Path $repo 'tools\ps1_export_items_test.py')
if ($LASTEXITCODE -ne 0) { throw 'test exporteur objets PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_export_items.py')
if ($LASTEXITCODE -ne 0) { throw 'generation de la table objets PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_export_icons_test.py')
if ($LASTEXITCODE -ne 0) { throw 'test exporteur icones PS1' }
& py -3.12 (Join-Path $repo 'tools\ps1_export_icons.py')
if ($LASTEXITCODE -ne 0) { throw 'generation de l atlas d icones PS1' }

# L'introduction reutilise la meme pagination A1 avec les textes [prologue] (A2).
& py -3.12 (Join-Path $repo 'tools\ps1_export_intro.py')
if ($LASTEXITCODE -ne 0) { throw 'generation des panneaux d intro PS1' }

# Tests hotes TDD : ils compilent les vrais headers partages avec le build MIPS.
if (-not (Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }
$clangHost = Join-Path $repo 'external\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe'
if (-not (Test-Path $clangHost)) { throw 'clang++ hote absent pour les tests PS1' }
foreach ($t in @('menu_logic_test', 'menu_parity_test', 'intro_logic_test', 'diag_logic_test', 'window_layout_test', 'interaction_logic_test', 'world_logic_test', 'inventory_logic_test', 'inventory_data_test', 'save_logic_test', 'mixer_logic_test', 'enemy_logic_test', 'ogre_duel_logic_test', 'hero_vitals_logic_test', 'hud_logic_test', 'zone_heap_diag_test', 'vram_diag_test', 'vram_diag_host_test', 'spu_diag_test', 'spu_diag_host_test')) {
	$exe = Join-Path $build ($t + '.exe')
	$cpp = Join-Path $repo ('tools\ps1_' + $t + '.cpp')
	& $clangHost -std=c++20 -o $exe $cpp
	if ($LASTEXITCODE -ne 0) { throw "compilation du test hote $t" }
	& $exe
	if ($LASTEXITCODE -ne 0) { throw "test hote $t" }
}

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
