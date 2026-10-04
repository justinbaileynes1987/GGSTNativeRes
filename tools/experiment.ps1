# Runs one launch of Strive with the DIAGNOSTIC build installed and prints its timeline:
# every resolution change the game requests (with the code address that asked) and every
# display-mode change. Used to prove what each fix does; see VERIFICATION.md.
#
#   .\experiment.ps1 -FixBootDefault 0 -FixFullscreenCap 0              # stock behaviour
#   .\experiment.ps1 -FixBootDefault 0 -FixFullscreenCap 0 -ExtraArgs -ForceRes
#
# Requires build\diag\xapofx1_5.dll installed in the game's RED\Binaries\Win64 folder.
# Closes any running copy of the game first.
param(
	[int]$FixBootDefault = 1,
	[int]$FixFullscreenCap = 1,
	[string]$ExtraArgs = "",
	[int]$Seconds = 30,
	[string]$GameDir = "",   # default: found automatically via Steam
	[string]$Steam = "",     # default: found automatically via the registry
	[switch]$KeepRunning  # leave the game open afterwards (e.g. to run dump.py)
)
. "$PSScriptRoot\steam-paths.ps1"
if (-not $GameDir) { $GameDir = Get-SteamGameDir "GUILTY GEAR STRIVE" }
if (-not $Steam) { $Steam = Get-SteamExe }
$bin = Join-Path $GameDir "RED\Binaries\Win64"
$ini = Join-Path $bin "GGSTNativeRes.ini"
Get-Process GGST-Win64-Shipping -ErrorAction SilentlyContinue | Stop-Process
Start-Sleep 4

Add-Type -Namespace W -Name Ini -MemberDefinition @'
[DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
public static extern bool WritePrivateProfileString(string section, string key, string value, string file);
'@
[void][W.Ini]::WritePrivateProfileString("Settings", "FixBootDefault", "$FixBootDefault", $ini)
[void][W.Ini]::WritePrivateProfileString("Settings", "FixFullscreenCap", "$FixFullscreenCap", $ini)

$launch = @("-applaunch", "1384160") + ($ExtraArgs -split ' ' | Where-Object { $_ })
Start-Process $Steam -ArgumentList $launch
Start-Sleep $Seconds
Write-Output "--- FixBootDefault=$FixBootDefault FixFullscreenCap=$FixFullscreenCap args='$ExtraArgs'"
Get-Content (Join-Path $bin "GGSTNativeRes.log") |
	Where-Object { $_ -match 'Target|disabled|Patched|RequestResolutionChange\(|DISPLAY' }
if (-not $KeepRunning) { Get-Process GGST-Win64-Shipping -ErrorAction SilentlyContinue | Stop-Process }
