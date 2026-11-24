# Round-trip de sauvegarde PS1 natif : ecriture, arret PCSX, redemarrage a froid,
# puis relecture depuis la carte memoire persistee sur disque.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_save_roundtrip.ps1 [-SkipBuild]
#   powershell -ExecutionPolicy Bypass -File tools\ps1_save_roundtrip.ps1 -ResetCard -Cue "ps1\a_graver\v0.8.0-sauvegarde\alundra2_ntsc.iso"
#   powershell -ExecutionPolicy Bypass -File tools\ps1_save_roundtrip.ps1 -SkipBuild -ResetCard `
#     -Cue "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ps1.cue" `
#     -Iso "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso" `
#     -ExpectedSha256 26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54 `
#     -ExpectedElfSha256 2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7 `
#     -ExpectedDigest 46f25a9f
#
# Sortie : "PS1_SAVE_ROUNDTRIP: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 180,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$OutDir,
	[switch]$NoFastBoot,
	[switch]$ResetCard,
	[string]$Cue,
	[string]$Iso,
	[string]$ExpectedSha256,
	[string]$ExpectedElfSha256,
	[int]$ExpectedZone = 11,
	[int]$ExpectedX = 660,
	[int]$ExpectedY = 760,
	[string]$ExpectedDigest,
	[int]$ExpectedSeq = 1,
	[int]$ExpectedSlot = -1,
	[string]$ExpectedCardSha256,
	[switch]$WriteOnly,
	[switch]$LoadOnly
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_SAVE_ROUNDTRIP: FAIL - $message"
	exit 1
}

if ($WriteOnly -and $LoadOnly) { Fail 'WriteOnly et LoadOnly sont mutuellement exclusifs' }

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$cue = $Cue
if (-not $Iso) { $Iso = Join-Path (Split-Path -Parent $cue) 'alundra2_ntsc.iso' }
$iso = $Iso
$lua = Join-Path $repo 'docs\ps1\lua\save_roundtrip.lua'
$card = Join-Path $repo 'memcard1.mcd'  # port 1 (kCardPort=0) = memcard1.mcd
if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\save_roundtrip' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elf, $cue, $iso, $card)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $SkipBuild) {
	& powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build_ps1.ps1')
	if ($LASTEXITCODE -ne 0) { Fail 'build PS1' }
}

$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $iso).Hash
$elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash
$cueHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $cue).Hash
Write-Host "ISO_SHA256=$isoHash"
Write-Host "ELF_SHA256=$elfHash"
Write-Host "CUE_SHA256=$cueHash"
if ($ExpectedSha256 -and $ExpectedSha256.ToUpperInvariant() -ne $isoHash) {
	Fail "ISO_SHA256 attendu=$($ExpectedSha256.ToUpperInvariant()) reel=$isoHash"
}
if ($ExpectedElfSha256 -and $ExpectedElfSha256.ToUpperInvariant() -ne $elfHash) {
	Fail "ELF_SHA256 attendu=$($ExpectedElfSha256.ToUpperInvariant()) reel=$elfHash"
}

$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elf
$line = $nm | Where-Object { $_ -match '\bmain$' } | Select-Object -Last 1
if (-not $line) { Fail 'symbole main absent de l ELF' }
$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
if (-not $m.Success) { Fail "adresse main illisible: $line" }
$mainAddr = '0x' + $m.Groups[1].Value.ToLowerInvariant()

function Get-Symbol([string]$suffix, [string]$label) {
	$hit = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $hit) { Fail "$label absent de l ELF" }
	$hm = [regex]::Match($hit, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
	if (-not $hm.Success) { Fail "adresse $label illisible: $hit" }
	return '0x' + $hm.Groups[1].Value.ToLowerInvariant()
}
$env:PS1_MENU_DRAW_ADDR  = Get-Symbol 'menuDrawPS1v$' 'menuDrawPS1'
$env:PS1_INTRO_DRAW_ADDR = Get-Symbol 'introDrawPS1v$' 'introDrawPS1'
$env:PS1_MENU_ADDR       = Get-Symbol '6g_menuE$' 'g_menu'
$env:PS1_QUEST_ADDR      = Get-Symbol '7g_questE$' 'g_quest'
$env:PS1_INV_STATE_ADDR  = Get-Symbol '5g_invE$' 'g_inv'

function Run-Session([string]$phase, [string]$name) {
	$sessionDir = Join-Path $OutDir $name
	New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null
	$stdoutFile = Join-Path $sessionDir 'stdout.txt'
	$stderrFile = Join-Path $sessionDir 'stderr.txt'
	$env:SESSION_OUT = $sessionDir
	$env:PS1_MAIN_ADDR = $mainAddr
	$env:PS1_SAVE_PHASE = $phase
	$argList = @(
		'-testmode', '-stdout', '-lua_stdout',
		'-bios', $BiosFile,
		'-iso', $cue
	)
	# Sous -fastboot le breakpoint d'execution de main est parfois installe
	# apres le saut initial : la session ne recoit alors aucune commande.
	# Le boot BIOS complet rend le breakpoint deterministe.
	if (-not $NoFastBoot) { $argList += '-fastboot' }
	$argList += @(
		'-run', '-debugger', '-interpreter',
		'-portable', '-noupdate',
		'-dofile', $lua
	)
	"PHASE=$phase MAIN_ADDR=$mainAddr IMAGE=$cue ISO_SHA256=$isoHash ELF_SHA256=$elfHash CUE_SHA256=$cueHash" |
		Set-Content -LiteralPath (Join-Path $sessionDir 'meta.txt') -NoNewline
	$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
		-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
	if (-not $p.WaitForExit([Math]::Max($Seconds, 5) * 1000)) {
		$p.Kill()
		$p.WaitForExit(5000) | Out-Null
		Fail "timeout $name apres $Seconds secondes"
	}
	# PCSX-Redux quitte depuis Lua sans code de sortie Win32 exploitable :
	# ExitCode peut etre null. Les marqueurs du log font foi (voir
	# ps1_capture_inventory.ps1), completes par detection explicite d'un FAIL.
	$log = Get-Content -LiteralPath $stdoutFile -Raw
	if ($log -match 'SAVE_ROUNDTRIP: FAIL') { Fail "$name a echoue: $($Matches[0])" }
	return $log
}

$before = (Get-FileHash -LiteralPath $card -Algorithm SHA256).Hash
if ($ExpectedCardSha256 -and $ExpectedCardSha256.ToUpperInvariant() -ne $before) {
	Fail "carte SHA256 attendu=$($ExpectedCardSha256.ToUpperInvariant()) reel=$before"
}
$writeDigest = $null
$seq = $ExpectedSeq

if (-not $LoadOnly) {
	if ($ResetCard) {
		# Carte propre reproductible : memcard2.mcd sert de gabarit (ses blocs 1/2
		# sont vides). Sans cet option, la carte courante doit deja avoir une partie.
		$template = Join-Path $repo 'memcard2.mcd'
		if (-not (Test-Path $template)) { Fail "gabarit de carte absent: $template" }
		Copy-Item -LiteralPath $template -Destination $card -Force
		$before = (Get-FileHash -LiteralPath $card -Algorithm SHA256).Hash
	}
	$writeLog = Run-Session 'write' '01_write'
	$expectedHasGame = 1
	if ($ResetCard) { $expectedHasGame = 0 }
	if ($writeLog -notmatch "PS1_SAVE_INIT READY hasGame=$expectedHasGame") {
		Fail "etat carte initial inattendu: hasGame=$expectedHasGame"
	}
	if ($writeLog -notmatch 'PS1_SAVE_WRITE OK seq=(\d+) slot=(\d+)') { Fail 'ecriture carte non confirmee' }
	$seq = [int]$Matches[1]
	$slot = [int]$Matches[2]
	Write-Host "ECRITURE seq=$seq slot=$slot"
	if ($seq -ne $ExpectedSeq) { Fail "sequence attendue=$ExpectedSeq reelle=$seq" }
	if ($ExpectedSlot -ge 0 -and $slot -ne $ExpectedSlot) {
		Fail "slot attendu=$ExpectedSlot reel=$slot"
	}
	if ($writeLog -notmatch 'SAVE_STATE phase=write digest=([0-9a-fA-F]{8}) mark=(-?\d+) stage=(-?\d+) zone=(-?\d+)') {
		Fail 'empreinte d etat absente avant ecriture'
	}
	$writeDigest = $Matches[1].ToLowerInvariant()
	$markSlot = [int]$Matches[2]
	if ($markSlot -lt 0) { Fail 'aucun objet possede a marquer avant ecriture' }
	Write-Host "ETAT_ECRIT digest=$writeDigest mark=$markSlot"
	if ($ExpectedDigest -and $ExpectedDigest.ToLowerInvariant() -ne $writeDigest) {
		Fail "digest attendu=$($ExpectedDigest.ToLowerInvariant()) reel=$writeDigest"
	}

	$after = (Get-FileHash -LiteralPath $card -Algorithm SHA256).Hash
	if ($before -eq $after) { Fail 'le fichier carte n a pas change apres ecriture' }
	Write-Host "CARTE_AVANT $before"
	Write-Host "CARTE_APRES $after"
	Copy-Item -LiteralPath $card -Destination (Join-Path $OutDir 'memcard1_after_write.mcd') -Force

	if ($WriteOnly) {
		"WRITE_SEQ=$seq`nWRITE_SLOT=$slot`nWRITE_DIGEST=$writeDigest`nCARD_SHA256=$after" |
			Set-Content -LiteralPath (Join-Path $OutDir 'write_state.txt') -NoNewline
		Write-Host 'PS1_SAVE_WRITE_ONLY: PASS'
		exit 0
	}
}

$loadLog = Run-Session 'load' '02_load_after_poweroff'
if ($loadLog -notmatch 'PS1_SAVE_INIT READY hasGame=1') { Fail 'sauvegarde non detectee apres redemarrage' }
if ($loadLog -notmatch "PS1_SAVE_INIT READY hasGame=1 seq=$seq") { Fail "sequence $seq non detectee au demarrage" }
if ($loadLog -notmatch "PS1_SAVE_READ OK seq=$seq zone=$ExpectedZone x=$ExpectedX y=$ExpectedY") {
	Fail "relecture seq=$seq zone/x/y attendus $ExpectedZone/$ExpectedX/$ExpectedY absente apres redemarrage"
}
if ($loadLog -notmatch 'SAVE_ROUNDTRIP: LOAD SENT') { Fail 'sequence de commandes load incomplete' }
if ($loadLog -notmatch 'SAVE_STATE phase=load digest=([0-9a-fA-F]{8})') {
	Fail 'empreinte d etat absente apres relecture'
}
$loadDigest = $Matches[1].ToLowerInvariant()
$referenceDigest = $writeDigest
if (-not $referenceDigest) { $referenceDigest = $ExpectedDigest }
if (-not $referenceDigest) { Fail 'digest de reference absent' }
if ($loadDigest -ne $referenceDigest.ToLowerInvariant()) {
	Fail "etat restaure divergent: reference=$referenceDigest relecture=$loadDigest"
}

Write-Host "RELECTURE seq=$seq zone=$ExpectedZone x=$ExpectedX y=$ExpectedY apres arret complet de l emulateur"
Write-Host "ETAT_RESTAURE digest=$loadDigest egal a l etat ecrit"
Write-Host 'PS1_SAVE_ROUNDTRIP: PASS'
exit 0
