# GGSTNativeRes - Microsoft Store / Xbox app (Game Pass) version check
#
# Helps add support for the Microsoft Store / Xbox app version of Guilty Gear -Strive-.
# It READS the game's executable (nothing is changed, nothing is sent anywhere) and prints:
#   - the executable's name, size and version
#   - which DLLs it loads (this decides whether GGSTNativeRes can load there)
#   - which settings folder the game uses
# The report is also copied to your clipboard so you can paste it into a comment or GitHub issue.
#
# How to run (no admin rights or extra software needed):
#   1. Save this file, e.g. to your Downloads folder.
#   2. Open PowerShell in that folder (Shift + right-click in the folder > "Open PowerShell window here",
#      or type powershell into the File Explorer address bar and press Enter).
#   3. Run:   powershell -ExecutionPolicy Bypass -File .\xbox-check.ps1
#      If the game is installed somewhere unusual, add the path to its WinGDK folder:
#             powershell -ExecutionPolicy Bypass -File .\xbox-check.ps1 -Path "D:\Games\GUILTY GEAR STRIVE\Content\RED\Binaries\WinGDK"
#
# (-ExecutionPolicy Bypass only applies to this one run; it doesn't change any system setting.)
param([string]$Path = "")

$ErrorActionPreference = "Stop"
$out = New-Object System.Collections.Generic.List[string]
function Say([string]$s = "") { $out.Add($s); Write-Host $s }

# --- find the WinGDK folder ------------------------------------------------------------------
$candidates = @()
if ($Path) { $candidates += $Path }
else {
	foreach ($d in Get-PSDrive -PSProvider FileSystem) {
		foreach ($sub in "XboxGames\GUILTY GEAR STRIVE\Content\RED\Binaries\WinGDK",
		                 "XboxGames\GUILTY GEAR -STRIVE-\Content\RED\Binaries\WinGDK") {
			$candidates += Join-Path $d.Root $sub
		}
		$xg = Join-Path $d.Root "XboxGames"
		if (Test-Path $xg) {
			Get-ChildItem $xg -Directory -ErrorAction SilentlyContinue |
				Where-Object { $_.Name -match 'GUILTY|STRIVE|GGST' } |
				ForEach-Object { $candidates += Join-Path $_.FullName "Content\RED\Binaries\WinGDK" }
		}
	}
}
$folder = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

Say "GGSTNativeRes Xbox/Microsoft Store check"
Say ("Windows {0}, PowerShell {1}" -f [Environment]::OSVersion.Version, $PSVersionTable.PSVersion)
if (-not $folder) {
	Say "Could not find the game's WinGDK folder. Looked in:"
	$candidates | Select-Object -Unique | ForEach-Object { Say "  $_" }
	Say "Run again with -Path pointing at the folder that contains the game's .exe (see the top of this script)."
	try { ($out -join "`r`n") | Set-Clipboard } catch {}
	return
}
Say "Folder: $folder"
Say ""
Say "Files in this folder:"
Get-ChildItem $folder -File | Sort-Object Name | ForEach-Object { Say ("  {0,-45} {1,12:N0} bytes" -f $_.Name, $_.Length) }

# --- read the PE import tables ---------------------------------------------------------------
function Get-PeImports([string]$file) {
	$b = [IO.File]::ReadAllBytes($file)
	if ([BitConverter]::ToUInt16($b, 0) -ne 0x5A4D) { throw "not an executable (no MZ header)" }
	$pe = [BitConverter]::ToInt32($b, 0x3C)
	if ([BitConverter]::ToUInt32($b, $pe) -ne 0x4550) { throw "no PE header (file may be encrypted)" }
	$machine = [BitConverter]::ToUInt16($b, $pe + 4)
	$nsec = [BitConverter]::ToUInt16($b, $pe + 6)
	$optSize = [BitConverter]::ToUInt16($b, $pe + 20)
	$opt = $pe + 24
	$is64 = [BitConverter]::ToUInt16($b, $opt) -eq 0x20B
	$imageBase = if ($is64) { [BitConverter]::ToUInt64($b, $opt + 24) } else { [uint64][BitConverter]::ToUInt32($b, $opt + 28) }
	$dd = $opt + $(if ($is64) { 112 } else { 96 })
	$secs = @()
	for ($i = 0; $i -lt $nsec; $i++) {
		$s = $opt + $optSize + 40 * $i
		$secs += [pscustomobject]@{
			Name = [Text.Encoding]::ASCII.GetString($b, $s, 8).TrimEnd([char]0)
			VA = [BitConverter]::ToUInt32($b, $s + 12); VSize = [BitConverter]::ToUInt32($b, $s + 8)
			Raw = [BitConverter]::ToUInt32($b, $s + 20); RawSize = [BitConverter]::ToUInt32($b, $s + 16) }
	}
	$toOff = {
		param([uint32]$rva)
		foreach ($s in $secs) { if ($rva -ge $s.VA -and $rva -lt $s.VA + [Math]::Max($s.VSize, $s.RawSize)) { return [int]($rva - $s.VA + $s.Raw) } }
		return -1
	}
	$cstr = { param([int]$o) $e = $o; while ($e -lt $b.Length -and $b[$e] -ne 0) { $e++ }; [Text.Encoding]::ASCII.GetString($b, $o, $e - $o) }

	$static = @(); $delay = @()
	$impRva = [BitConverter]::ToUInt32($b, $dd + 8)          # directory 1: imports
	if ($impRva) {
		$o = & $toOff $impRva
		while ($o -ge 0 -and [BitConverter]::ToUInt32($b, $o + 12) -ne 0) {
			$n = & $toOff ([BitConverter]::ToUInt32($b, $o + 12)); if ($n -ge 0) { $static += & $cstr $n }
			$o += 20
		}
	}
	$dlyRva = [BitConverter]::ToUInt32($b, $dd + 13 * 8)     # directory 13: delay-load imports
	if ($dlyRva) {
		$o = & $toOff $dlyRva
		while ($o -ge 0 -and [BitConverter]::ToUInt32($b, $o + 4) -ne 0) {
			$attr = [BitConverter]::ToUInt32($b, $o); $nameRef = [uint64][BitConverter]::ToUInt32($b, $o + 4)
			$nameRva = if ($attr -band 1) { $nameRef } else { $nameRef - $imageBase }
			$n = & $toOff ([uint32]$nameRva); if ($n -ge 0) { $delay += & $cstr $n }
			$o += 32
		}
	}
	[pscustomobject]@{ Machine = ('0x{0:X4}' -f $machine); Is64 = $is64; Sections = ($secs.Name -join ','); Static = $static; Delay = $delay }
}

$exes = Get-ChildItem $folder -Filter *.exe -File | Sort-Object Length -Descending
if (-not $exes) { Say ""; Say "No .exe found in this folder." }
foreach ($exe in $exes) {
	Say ""
	Say ("=== {0}  ({1:N0} bytes, version {2})" -f $exe.Name, $exe.Length, $exe.VersionInfo.FileVersion)
	try {
		$imp = Get-PeImports $exe.FullName
		Say ("  64-bit: {0}   sections: {1}" -f $imp.Is64, $imp.Sections)
		Say ("  Loads at startup ({0}): {1}" -f $imp.Static.Count, (($imp.Static | Sort-Object) -join ', '))
		Say ("  Loads on demand  ({0}): {1}" -f $imp.Delay.Count, (($imp.Delay | Sort-Object) -join ', '))
		$all = @($imp.Static) + @($imp.Delay) | ForEach-Object { $_.ToLower() }
		Say "  DLL names relevant to mod loaders:"
		foreach ($n in "xapofx1_5.dll", "x3daudio1_7.dll", "sensapi.dll", "xinput1_3.dll", "dwmapi.dll", "winmm.dll", "version.dll", "dinput8.dll", "dxgi.dll", "d3d11.dll") {
			$where = if ($imp.Static -contains $n -or ($imp.Static | Where-Object { $_.ToLower() -eq $n })) { "loaded at startup" }
			         elseif ($all -contains $n) { "loaded on demand" } else { "not imported" }
			Say ("    {0,-18} {1}" -f $n, $where)
		}
	} catch {
		Say "  Could not read imports: $($_.Exception.Message)"
		Say "  (The Xbox app protects the game's .exe. Start the game, wait for the title screen and run this"
		Say "   script again: the 'running game' section below then shows the same information.)"
	}
}

# --- DLLs loaded by the running game (works even when the .exe itself can't be read) ----------
Say ""
# Match by process NAME, not by path: Xbox app games run from a protected location that is mapped
# into C:\XboxGames\...\Content, so the running process reports a different path (or none at all).
$exeNames = @(Get-ChildItem $folder -Filter *.exe -File | ForEach-Object { $_.BaseName })
$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $exeNames -contains $_.ProcessName })
if (-not $running) {
	Say ("Running game: no process named {0} found. (Start the game, wait for the title screen and run" -f (($exeNames | ForEach-Object { "$_.exe" }) -join ' / '))
	Say "this script again for the most useful report.)"
	$similar = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^RED-|GGST|STRIVE|GUILTY' })
	if ($similar) { Say ("  Similar running processes: {0}" -f (($similar | ForEach-Object { "$($_.ProcessName) ($($_.Id))" }) -join ', ')) }
}
foreach ($p in $running) {
	Say ("=== Running game: {0}.exe (process {1})" -f $p.ProcessName, $p.Id)
	$reported = try { $p.Path } catch { $null }
	Say ("  Runs from: {0}" -f $(if ($reported) { $reported } else { "(Windows didn't report a path)" }))
	try {
		$mods = @($p.Modules)
		Say ("  {0} DLLs loaded" -f ($mods.Count - 1))
		Say "  DLL names relevant to mod loaders (and where they were loaded from):"
		foreach ($n in "xapofx1_5.dll", "x3daudio1_7.dll", "sensapi.dll", "xinput1_3.dll", "dwmapi.dll", "winmm.dll", "version.dll", "dinput8.dll", "dxgi.dll", "d3d11.dll") {
			$m = $mods | Where-Object { $_.ModuleName -ieq $n } | Select-Object -First 1
			Say ("    {0,-18} {1}" -f $n, $(if ($m) { "loaded from $($m.FileName)" } else { "not loaded" }))
		}
		$names = $mods | Select-Object -Skip 1 | ForEach-Object { $_.ModuleName } | Sort-Object -Unique
		Say ("  All loaded DLLs: {0}" -f ($names -join ', '))
	} catch {
		Say "  Could not list the game's DLLs: $($_.Exception.Message)"
		# Second opinion via tasklist, which uses a different Windows API path.
		try {
			$t = & tasklist.exe /m /fo csv /fi "PID eq $($p.Id)" 2>&1 | ConvertFrom-Csv
			if ($t -and $t.Modules -and $t.Modules -ne 'N/A') { Say ("  tasklist reports these DLLs: {0}" -f $t.Modules) }
			else { Say "  tasklist couldn't list them either." }
		} catch { Say "  tasklist couldn't list them either." }
	}
}

# --- settings folder -------------------------------------------------------------------------
Say ""
$cfg = Join-Path $env:LOCALAPPDATA "GGST\Saved\Config"
if (Test-Path $cfg) {
	Say "Settings folders in %LOCALAPPDATA%\GGST\Saved\Config:"
	Get-ChildItem $cfg -Directory | ForEach-Object {
		$g = Join-Path $_.FullName "GameUserSettings.ini"
		Say ("  {0,-20} GameUserSettings.ini: {1}" -f $_.Name, $(if (Test-Path $g) { "yes" } else { "no" }))
	}
} else { Say "No %LOCALAPPDATA%\GGST\Saved\Config folder found (the game may store settings elsewhere)." }

Say ""
try { ($out -join "`r`n") | Set-Clipboard; Write-Host "The report above has been copied to your clipboard - paste it into your comment or GitHub issue." -ForegroundColor Green }
catch { Write-Host "Copy the report above and paste it into your comment or GitHub issue." -ForegroundColor Green }
