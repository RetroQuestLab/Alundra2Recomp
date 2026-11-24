# Parcours ordres de Diaz -> epee de Jeehan -> Vallee de Mithra (v0.8.26) sur
# une ISO PS1 precise. La sonde ne place que le heros et appuie sur la manette :
# aucun champ QuestState ni inventaire n'est ecrit. Le passage 119->026 doit
# etre refuse sans l'epee, le coffre et l'epee ne se donnent qu'une fois, la
# carte 026 est lue sur le CD et la reprise (SELECT -> Continuer) la recharge.
# La carte memoire de l'utilisateur (memcard1.mcd) est copiee puis restauree ;
# la session utilise une carte de test vierge issue de memcard2.mcd.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_jeehan_vallee.ps1 -SkipBuild `
#     -Cue "ps1\a_graver\v0.8.26-jeehan-vallee\alundra2_ntsc.iso" -ExpectedSha256 "<hash>"
#
# Sortie : "PS1_CAPTURE_JEEHAN_VALLEE: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 600,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$OutDir,
	[string]$ExpectedSha256,
	[string]$Elf
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_JEEHAN_VALLEE: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = if ($Elf) { $Elf } else { Join-Path $ps1Dir 'build\alundra2.elf' }
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$lua = Join-Path $repo 'docs\ps1\lua\capture_jeehan_vallee.lua'
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\jeehan_vallee_v0826' }
if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elf, $Cue)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $SkipBuild) {
	& powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build_ps1.ps1')
	if ($LASTEXITCODE -ne 0) { Fail 'build PS1' }
}

$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elf

function Get-Symbol([string]$suffix, [string]$label) {
	$line = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $line) { Fail "$label absent de l ELF" }
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}

function Get-SymbolSuffix([string]$suffix, [string]$label) {
	$line = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $line) { Fail "$label absent de l ELF" }
	return '0x' + [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$').Groups[1].Value.ToLowerInvariant()
}

$mainAddr = Get-Symbol '\bmain$' 'main'
$enemyAddr = Get-Symbol '\bg_ps1EnemyAnim$' 'g_ps1EnemyAnim'
$duelAddr = Get-Symbol '\bg_ps1OgreDuel$' 'g_ps1OgreDuel'
$env:SESSION_OUT = $OutDir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_ENEMY_ADDR = $enemyAddr
$env:PS1_DUEL_ADDR = $duelAddr
$env:PS1_INTRO_ADDR = Get-Symbol '7g_introE$' 'g_intro'
$env:PS1_HERO_X_ADDR = Get-Symbol '3g_xE$' 'g_x'
$env:PS1_HERO_Y_ADDR = Get-Symbol '3g_yE$' 'g_y'
$env:PS1_DIR_ADDR = Get-Symbol '5g_dirE$' 'g_dir'
$env:PS1_QUEST_ADDR = Get-Symbol '7g_questE$' 'g_quest'
$env:PS1_DLG_SHOWN_ADDR = Get-Symbol '7g_shownE$' 'g_shown'
$env:PS1_DLG_PANEL_ADDR = Get-Symbol '7g_panelE$' 'g_panel'
$env:PS1_DLG_ACTIVE_ADDR = Get-Symbol '8g_activeE$' 'g_active'
$env:PS1_MENU_DRAW_ADDR = Get-SymbolSuffix 'menuDrawPS1v$' 'menuDrawPS1'
$env:PS1_INTRO_DRAW_ADDR = Get-SymbolSuffix 'introDrawPS1v$' 'introDrawPS1'
$env:PS1_MENU_ADDR = Get-SymbolSuffix '6g_menuE$' 'g_menu'
$env:PS1_DUEL_ADDR = Get-Symbol '\bg_ps1OgreDuel$' 'g_ps1OgreDuel'
$env:PS1_INV_ADDR = Get-Symbol '5g_invE$' 'g_inv'
$env:PS1_ZONE_TILES_ADDR = Get-Symbol '_18g_nTilesE$' 'g_nTiles'
$env:PS1_PFDRAW_ADDR = Get-Symbol '\bpfDrawRect$' 'pfDrawRect'
$audioAddr = Get-Symbol '\bg_ps1AudioDiag$' 'g_ps1AudioDiag'
$env:PS1_AUDIO_DIAG_ADDR = $audioAddr

$stdoutFile = Join-Path $OutDir 'capture_stdout.txt'
$stderrFile = Join-Path $OutDir 'capture_stderr.txt'
$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $Cue,
	'-fastboot',
	'-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Cue).Hash
$shaMatch = 'N/A'
$want = ''
if ($ExpectedSha256) {
	$want = ($ExpectedSha256 -replace '\s', '').ToUpperInvariant()
	if ($isoHash.ToUpperInvariant() -ne $want) {
		Fail "hash ISO inattendu: $isoHash (attendu $want)"
	}
	$shaMatch = '1'
}
@"
CMD=$PcsxExe $($argList -join ' ')
MAIN_ADDR=$mainAddr
ENEMY_ADDR=$enemyAddr
DUEL_ADDR=$duelAddr
DLG_ACTIVE_ADDR=$env:PS1_DLG_ACTIVE_ADDR
AUDIO_ADDR=$audioAddr
CUE=$Cue
SHA256=$isoHash
EXPECTED_SHA256=$want
SHA256_MATCH=$shaMatch
OUTDIR=$OutDir
"@ | Set-Content -LiteralPath (Join-Path $OutDir 'capture_meta.txt') -NoNewline

$card = Join-Path $repo 'memcard1.mcd'
$template = Join-Path $repo 'memcard2.mcd'
if (-not (Test-Path $template)) { Fail "gabarit de carte absent: $template" }
$userCard = Join-Path $OutDir 'memcard1_utilisateur.bak'
if (Test-Path $card) { Copy-Item -LiteralPath $card -Destination $userCard -Force }
Copy-Item -LiteralPath $template -Destination $card -Force
try {
$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
$timeoutMs = [Math]::Max($Seconds, 5) * 1000
if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill()
	$p.WaitForExit(5000) | Out-Null
	Fail "timeout apres $Seconds secondes"
}
} finally {
	if (Test-Path $card) { Copy-Item -LiteralPath $card -Destination (Join-Path $OutDir 'memcard_test_apres.mcd') -Force }
	if (Test-Path $userCard) { Copy-Item -LiteralPath $userCard -Destination $card -Force }
}

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load BIOS|Could not load CD' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

$ppmFiles = @(Get-ChildItem -LiteralPath $OutDir -Filter '*.ppm' -File)
if ($ppmFiles.Count -eq 0) { Fail 'aucune capture PPM produite' }
foreach ($ppm in $ppmFiles) {
	$png = [IO.Path]::ChangeExtension($ppm.FullName, '.png')
	& py -3.12 (Join-Path $repo 'tools\ppm_to_png.py') $ppm.FullName $png
	if ($LASTEXITCODE -ne 0) { Fail "conversion PNG de $($ppm.Name)" }
}

$stdout -split "`n" | Where-Object { $_ -match 'shot |JEEHAN|PASSAGE_|VALLEE|ORDRES|COFFRE|EPEE|REPRISE|DEPART|RELOAD|PS1_SAVE' } | ForEach-Object { Write-Host $_ }
foreach ($m in @('VALLEE_REFUSEE_SANS_EPEE', 'ORDRES_TROUVES', 'COFFRE_VIDE', 'JEEHAN_ORDRE_VU', 'EPEE_SUPREME_RECUE', 'JEEHAN_SANS_DOUBLE', 'VALLEE_ARRIVEE', 'GUIDE_VALLEE', 'GOLEM_CROIX_SANS_EFFET', 'GOLEM_VAINCU', 'LAMPE_OBTENUE', 'LAMPE_UNIQUE', 'EDEN_ARRIVEE', 'EDEN_CARTE', 'ROSWELL_LAMPE', 'OMEGA', 'GOLEM_EDEN_VAINCU', 'PRISME_OBTENU', 'ROSWELL_PRISME', 'MEDUSA_ARRIVEE', 'RUBY', 'ALBERT', 'ZACH', 'CAPITAINE_VAINCU', 'CRISTAL_OBTENU', 'REPRISE', 'JEEHAN_VALLEE_CAPTURE: PASS')) {
	if ($stdout -notmatch [regex]::Escape($m)) { Fail "etape absente: $m" }
}
if ($stdout -notmatch 'PS1_SAVE_READ OK seq=\d+ zone=50 ') { Fail 'reprise non lue depuis la carte en zone 50' }
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $OutDir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host 'PS1_CAPTURE_JEEHAN_VALLEE: PASS'
exit 0