param(
	[string[]]$Lots = @(
		'v0.8.13-hud-guard',
		'v0.8.14-menu-selection-parite',
		'v0.8.15-remake-subtitle-orange',
		'v0.8.16-ps1-options-contrast',
		'v0.8.17-ps1-menu-hint-blue',
		'v0.8.18-ps1-intro-scene-sync',
		'v0.8.19-ps1-heap-stack-diag',
		'v0.8.20-ps1-zone-heap',
		'v0.8.21-ps1-vram-peak',
		'v0.8.22-ps1-spu-peak',
		'v0.8.23-ogre-progression',
		'v0.8.24-finrod-reunion',
        'v0.8.25-alexia-kindra'
	),
	[string]$LotRoot = '',
	[switch]$SkipPointers,
	[switch]$SkipBudgets
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrEmpty($LotRoot)) { $LotRoot = Join-Path $repo 'ps1\a_graver' }
$lotRoot = $LotRoot
$hashPattern = '^(\S+)\s+(\d+) octets\s+SHA256 ([0-9A-F]{64})$'
$notesText = Get-Content -LiteralPath (Join-Path $repo 'notes.md') -Raw
$notesHashes = @{}
foreach ($block in [regex]::Matches($notesText, '(?ms)^### BLOC AUTORITAIRE[^\r\n]*\((v0\.8\.(?:1[3-9]|2[0-5]))-[^)\r\n]+\)\s*$(.*?)(?=^### |\z)')) {
	$claim = [regex]::Match($block.Groups[2].Value, 'SHA256\s+`([0-9A-F]{64})`')
	if ($claim.Success) { $notesHashes[$block.Groups[1].Value] = $claim.Groups[1].Value }
}

$failures = New-Object System.Collections.Generic.List[string]
$totalChecked = 0
$notesChecked = 0

foreach ($lot in $Lots) {
	$shortLot = $lot.Substring(0, 7)
	$dir = Join-Path $lotRoot $lot
	if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
		$failures.Add("lot absent: $lot")
		continue
	}
	$manifest = Join-Path $dir 'SHA256.txt'
	$info = Join-Path $dir 'BUILD_INFO.txt'
	foreach ($required in @($manifest, $info)) {
		if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
			$failures.Add("fichier absent: $lot/$([System.IO.Path]::GetFileName($required))")
		}
	}
	if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { continue }

	$lotChecked = 0
	foreach ($line in (Get-Content -LiteralPath $manifest)) {
		if ($line -notmatch $hashPattern) { continue }
		$name = $matches[1]
		$expected = $matches[3]
		$target = Join-Path $dir $name
		if (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
			$failures.Add("artefact absent: $lot/$name")
			continue
		}
		$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash
		$lotChecked++
		$totalChecked++
		if ($actual -ne $expected) {
			$failures.Add("hash mismatch: $lot/$name attendu=$expected reel=$actual")
		}
	}
	if ($lotChecked -ne 7) {
		$failures.Add("manifest incomplet: $lot checked=$lotChecked attendu=7")
	}

	$manifestText = Get-Content -LiteralPath $manifest -Raw
	$manifestIso = [regex]::Match($manifestText, '(?m)^(?:SHA256|alundra2_ntsc\.iso[^\r\n]*SHA256) ([0-9A-F]{64})')
	if (-not $manifestIso.Success) {
		$failures.Add("ISO SHA256 absent du manifeste: $lot")
	} elseif ($notesHashes.ContainsKey($shortLot)) {
		$notesChecked++
		if ($manifestIso.Groups[1].Value -ne $notesHashes[$shortLot]) {
			$failures.Add("ISO SHA256 notes.md incoherent: $lot notes=$($notesHashes[$shortLot]) manifeste=$($manifestIso.Groups[1].Value)")
		}
	}

	Write-Output ("LOT {0,-32} artefacts_verifies={1}" -f $lot, $lotChecked)
}

# Pointeurs : DERNIERE_VERSION.txt designe le lot live et SHA256.txt porte son hash d'ISO.
$pointerOk = 0
if ($SkipPointers) { $pointerOk = 1 }
if (-not $SkipPointers) {
$pointerVersion = Join-Path $lotRoot 'DERNIERE_VERSION.txt'
$pointerHash = Join-Path $lotRoot 'SHA256.txt'
if ((Test-Path -LiteralPath $pointerVersion) -and (Test-Path -LiteralPath $pointerHash)) {
	$versionText = Get-Content -LiteralPath $pointerVersion -Raw
	$hashText = Get-Content -LiteralPath $pointerHash -Raw
	$pathMatch = [regex]::Match($versionText, '(?m)^Chemin\s*:\s*(.+)$')
	$shaMatch = [regex]::Match($hashText, '(?m)^SHA256 ([0-9A-F]{64})$')
	if (($pathMatch.Success -and $shaMatch.Success)) {
		$isoPath = $pathMatch.Groups[1].Value.Trim()
		if (Test-Path -LiteralPath $isoPath -PathType Leaf) {
			$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $isoPath).Hash
			if ($actual -eq $shaMatch.Groups[1].Value) {
				$pointerOk = 1
			} else {
				$failures.Add("pointeur SHA256 incoherent: attendu=$($shaMatch.Groups[1].Value) reel=$actual")
			}
		} else {
			$failures.Add("pointeur chemin absent: $isoPath")
		}
	} else {
		$failures.Add('pointeur: Chemin ou SHA256 absent')
	}
} else {
	$failures.Add('pointeurs absents')
}
}

# Un rapport de budgets par lot livre.
if (-not $SkipBudgets) {
foreach ($lot in $Lots) {
	$tag = $lot.Substring(0, 7) -replace '\.', ''
	$report = Join-Path $repo "docs\ps1\budget_$tag.md"
	if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
		$failures.Add("rapport budget absent: docs/ps1/budget_$tag.md ($lot)")
	}
}
}

Write-Output "ARTEFACTS_VERIFIES=$totalChecked"
Write-Output "NOTES_HASHES_VERIFIES=$notesChecked"
Write-Output "POINTEUR_OK=$pointerOk"
Write-Output "ECHECS=$($failures.Count)"
foreach ($failure in $failures) { Write-Output "FAIL $failure" }
if ($failures.Count -gt 0 -or $pointerOk -ne 1 -or $notesChecked -ne $Lots.Count -or $totalChecked -ne ($Lots.Count * 7)) {
	Write-Output 'PS1_LOT_CONFORMITY: FAIL'
	exit 1
}
Write-Output 'PS1_LOT_CONFORMITY: PASS'

