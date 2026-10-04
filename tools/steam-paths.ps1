# Dot-source helper: finds Steam and a game's install folder from the registry and Steam's
# libraryfolders.vdf, so the tools don't depend on where anyone installed things.
#   . "$PSScriptRoot\steam-paths.ps1"
#   $steamExe = Get-SteamExe
#   $gameDir  = Get-SteamGameDir "GUILTY GEAR STRIVE"

function Get-SteamRoot {
	$p = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction SilentlyContinue).SteamPath
	if (-not $p) { $p = "${env:ProgramFiles(x86)}\Steam" }
	return ($p -replace '/', '\')
}

function Get-SteamExe { Join-Path (Get-SteamRoot) "steam.exe" }

function Get-SteamGameDir([string]$installDir) {
	$root = Get-SteamRoot
	$libs = @($root)
	$vdf = Join-Path $root "steamapps\libraryfolders.vdf"
	if (Test-Path $vdf) {
		$libs += Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' |
			ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' }
	}
	foreach ($l in ($libs | Select-Object -Unique)) {
		$d = Join-Path $l "steamapps\common\$installDir"
		if (Test-Path $d) { return $d }
	}
	throw "Couldn't find '$installDir' in any Steam library - pass the folder explicitly."
}
