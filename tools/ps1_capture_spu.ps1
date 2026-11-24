# Mesure du pic SPU reel pendant le rendu et le streaming CD sur l'ISO PS1 livree.
# Le Lua lit g_ps1SpuDiag (table d'occupation instrumentee) sans ecrire dans le
# POD ; ce script prouve la provenance (SHA256) et la coherence des releves.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_spu.ps1 `
#     -Iso ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso `
#     -Elf ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2.elf `
#     -OutDir qa\ps1_logs\spu_v0822 `
#     -ExpectedSha256 <SHA256 de l'ISO livree>
#
# Sortie : "PS1_CAPTURE_SPU: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 300,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Iso,
	[string]$Elf,
	[string]$OutDir,
	[string]$ExpectedSha256
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_SPU: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

if (-not $Iso) { $Iso = 'ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso' }
if (-not $Elf) { $Elf = 'ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2.elf' }
$isoPath = if ([IO.Path]::IsPathRooted($Iso)) { $Iso } else { Join-Path $repo $Iso }
$elfPath = if ([IO.Path]::IsPathRooted($Elf)) { $Elf } else { Join-Path $repo $Elf }
$lua = Join-Path $repo 'docs\ps1\lua\capture_spu.lua'
if (-not $OutDir) { $OutDir = 'qa\ps1_logs\spu_v0822' }
$outdir = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elfPath, $isoPath)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}

$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $isoPath).Hash
$elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elfPath).Hash
if ($ExpectedSha256 -and $ExpectedSha256.ToUpperInvariant() -ne $isoHash) {
	Fail "ISO_SHA256 attendu=$($ExpectedSha256.ToUpperInvariant()) reel=$isoHash"
}

# --- Adresses via nm sur l'ELF LIVRE ----------------------------------------
$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elfPath
if ($LASTEXITCODE -ne 0) { Fail "echec nm sur $elfPath" }

function Get-Addr([string]$pattern, [string]$label) {
	$line = $nm | Where-Object { $_ -match $pattern } | Select-Object -Last 1
	if (-not $line) { Fail "symbole $label introuvable dans l'ELF livre" }
	# Certaines versions de nm emettent 16 hexades (ffffffff préfixe).
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S*$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}

$mainAddr = Get-Addr '\bmain$' 'main'
$spuAddr = Get-Addr '\bg_ps1SpuDiag$' 'g_ps1SpuDiag'
$introAddr = Get-Addr 'g_introE$' 'g_intro'
$menuAddr = Get-Addr 'g_menuE$' 'g_menu'
$invAddr = Get-Addr '\bg_invCapture$' 'g_invCapture'

# --- Lancement PCSX-Redux sur l'ISO exacte ----------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_SPU_ADDR = $spuAddr
$env:PS1_INTRO_ADDR = $introAddr
$env:PS1_MENU_ADDR = $menuAddr
$env:PS1_INV_ADDR = $invAddr
$stdoutFile = Join-Path $outdir 'spu_stdout.txt'
$stderrFile = Join-Path $outdir 'spu_stderr.txt'

$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $isoPath,
	'-fastboot',
	'-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
$cmdString = $PcsxExe + ' ' + ($argList -join ' ')

$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile

$timeoutMs = [Math]::Max($Seconds, 5) * 1000
if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill(); $p.WaitForExit(5000) | Out-Null
	Fail "timeout apres $Seconds secondes"
}
if ($p.ExitCode -ne $null -and $p.ExitCode -ne 0) { Fail "PCSX-Redux code de sortie $($p.ExitCode)" }

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

function Get-Field([string]$line, [string]$name) {
	$m = [regex]::Match($line, "(?:^|\s)${name}=([^\s]+)")
	if (-not $m.Success) { Fail "champ $name absent: $line" }
	return $m.Groups[1].Value
}

function Get-Hex([string]$line, [string]$name) {
	$v = Get-Field $line $name
	if ($v -notmatch '^0x[0-9a-fA-F]+$') { Fail "champ $name n'est pas hexadécimal: $v" }
	return [Convert]::ToUInt64($v.Substring(2), 16)
}

$allLines = @($stdout -split "`n")
if (@($allLines | Where-Object { $_ -match '^CAPTURE_SPU: FAIL' }).Count -gt 0) {
	Fail "echec signale par le Lua: $(@($allLines | Where-Object { $_ -match '^CAPTURE_SPU: FAIL' }) -join ' | ')"
}

$phaseLines = @($allLines | Where-Object { $_ -match '^SPU phase=' })
$resultLines = @($allLines | Where-Object { $_ -match '^SPU_RESULT ' })
if ($phaseLines.Count -ne 3) { Fail "lignes SPU phase: $($phaseLines.Count) attendu 3" }
if ($resultLines.Count -ne 1) { Fail "lignes SPU_RESULT: $($resultLines.Count) attendu 1" }

$expectedPhases = @('menu', 'game', 'inventory')
$kMagic = [Convert]::ToUInt64('A2535044', 16)
$lastPeak = -1
$lastTransfers = 0
foreach ($i in 0..2) {
	$line = $phaseLines[$i]
	$phase = Get-Field $line 'phase'
	$magic = Get-Hex $line 'magic'
	$integrity = Get-Hex $line 'integrity'
	$ready = [int](Get-Field $line 'ready')
	$transfers = [int64](Get-Field $line 'transfers')
	$current = [int64](Get-Field $line 'currentBytes')
	$peak = [int64](Get-Field $line 'peakBytes')
	$peakFrame = [int64](Get-Field $line 'peakFrame')
	$overlap = [int64](Get-Field $line 'overlapBytes')
	$rejections = [int](Get-Field $line 'rejections')
	$spu = [int64](Get-Field $line 'spuBytes')
	$reserved = [int64](Get-Field $line 'reservedBytes')
	$bank = [int64](Get-Field $line 'bankBytes')
	$stream = [int64](Get-Field $line 'streamBytes')
	$chunks = [int64](Get-Field $line 'chunks')
	$underruns = [int64](Get-Field $line 'underruns')
	$pframe = [int64](Get-Field $line 'frame')

	if ($phase -ne $expectedPhases[$i]) { Fail "phase=$phase attendu $($expectedPhases[$i])" }
	if ($magic -ne $kMagic) { Fail "magic inattendu: 0x$('{0:x8}' -f $magic)" }
	if ($ready -ne 1) { Fail "ready=$ready a la phase $phase" }
	if ($spu -ne 524288) { Fail "spuBytes=$spu attendu 524288" }
	if ($reserved -ne 4096) { Fail "reservedBytes=$reserved attendu 4096" }
	if ($rejections -ne 0) { Fail "rejections=$rejections a la phase $phase" }
	if ($transfers -le 0) { Fail "transfers=$transfers a la phase $phase" }
	if ($current -ne $peak) { Fail "currentBytes=$current != peakBytes=$peak (table sans liberation)" }
	if ($peak -ne ($bank + $stream)) { Fail "peakBytes=$peak != bankBytes+streamBytes=$($bank + $stream)" }
	if ($peak -gt $spu) { Fail "peakBytes=$peak > spuBytes=$spu" }
	# peakFrame=0 est legitimate : le prefill CD synchrone du boot ecrit les deux
	# chunks avant la premiere image presentee. La preuve de streaming est
	# apportee par chunks/streamBytes/transfers/underruns ci-dessous.
	if ($peakFrame -lt 0 -or $peakFrame -gt $pframe) { Fail "peakFrame=$peakFrame hors [0,$pframe]" }
	if ($peak -lt $lastPeak) { Fail "pic non monotone: $peak < $lastPeak" }
	if ($transfers -lt $lastTransfers) { Fail "transfers non monotone: $transfers < $lastTransfers" }
	$lastPeak = $peak
	$lastTransfers = $transfers

	$computed = $kMagic -bxor [uint64]$transfers -bxor [uint64]$current -bxor [uint64]$peak -bxor [uint64]$overlap
	if ($integrity -ne $computed) {
		Fail "integrity=0x$('{0:x8}' -f $integrity) attendu 0x$('{0:x8}' -f $computed) a la phase $phase"
	}

	# Le double buffer musical est deja complet des le menu (prefill CD
	# synchrone) ; la preuve de streaming (chunks >= 2) n'est exigee qu'en
	# phase finale, apres que le gameplay a reellement exerce les IRQ SPU.
	if ($underruns -ne 0) { Fail "underruns=$underruns a la phase $phase" }
	if ($stream -ne 16384) { Fail "streamBytes=$stream attendu 16384 (double buffer complet)" }
}

# Preuve que le streaming CD a ete exerce (revue du design), evaluee sur la
# phase finale apres gameplay et non sur le menu ou le prefill domine.
$finalChunks = [int64](Get-Field $phaseLines[2] 'chunks')
$finalTransfers = [int64](Get-Field $phaseLines[2] 'transfers')
$finalUnderruns = [int](Get-Field $phaseLines[2] 'underruns')
$finalStream = [int64](Get-Field $phaseLines[2] 'streamBytes')
if ($finalChunks -lt 2) { Fail "chunks=$finalChunks < 2 en phase inventory" }
if ($finalTransfers -lt 3) { Fail "transfers=$finalTransfers < 3 (banque + 2 chunks) en phase inventory" }
if ($finalUnderruns -ne 0) { Fail "underruns=$finalUnderruns en phase inventory" }
if ($finalStream -ne 16384) { Fail "streamBytes=$finalStream attendu 16384 en phase inventory" }

$result = $resultLines[0]
$resultPeak = [int64](Get-Field $result 'peakBytes')
$inventoryPeak = [int64](Get-Field $phaseLines[2] 'peakBytes')
if ($resultPeak -ne $inventoryPeak) {
	Fail "SPU_RESULT peakBytes=$resultPeak != phase inventory peakBytes=$inventoryPeak"
}
foreach ($name in @('peakFrame', 'currentBytes', 'bankBytes', 'streamBytes', 'transfers', 'overlapBytes', 'rejections', 'chunks', 'underruns', 'integrity')) {
	if ((Get-Field $result $name) -ne (Get-Field $phaseLines[2] $name)) {
		Fail "SPU_RESULT $name divergent de la phase inventory"
	}
}

$spuLog = Join-Path $outdir 'spu_capture.txt'
@"
PS1_CAPTURE_SPU: PASS
CMD=$cmdString
ISO=$isoPath
ISO_SHA256=$isoHash
ELF=$elfPath
ELF_SHA256=$elfHash
MAIN_ADDR=$mainAddr
SPU_ADDR=$spuAddr
INTRO_ADDR=$introAddr
MENU_ADDR=$menuAddr
INV_ADDR=$invAddr
LUA=$lua
$($phaseLines | Out-String)
$($resultLines | Out-String)
VERDIT: pic SPU mesure=$inventoryPeak octets, banque=$bank, stream=$stream, transfers=$transfers, chunks=$chunks, underruns=$underruns, overlap=$overlap, rejets=0.
"@ | Set-Content -LiteralPath $spuLog -Encoding UTF8

Write-Host '--- extrait stdout ---'
$phaseLines | ForEach-Object { Write-Host $_ }
$resultLines | ForEach-Object { Write-Host $_ }
Write-Host '----------------------'
Write-Host "Adresses : main=$mainAddr g_ps1SpuDiag=$spuAddr"
Write-Host "ISO : $isoPath ($isoHash)"
Write-Host "Log : $spuLog"
Write-Host 'PS1_CAPTURE_SPU: PASS'
exit 0
