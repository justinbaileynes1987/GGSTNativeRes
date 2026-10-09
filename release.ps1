# Builds both release DLLs and packages them:
#   dist\GGSTNativeRes-<version>-steam.zip  (xapofx1_5.dll, README.txt, LICENSE.txt)
#   dist\GGSTNativeRes-<version>-xbox.zip   (Content/RED/Binaries/WinGDK/dsound.dll, README.txt, LICENSE.txt)
#   dist\SHA256SUMS.txt                     (hashes of the zips and of the DLLs)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$version = (Select-String -Path "$root\src\version.h" -Pattern 'GGSTNR_VERSION_STRING "([^"]+)"').Matches[0].Groups[1].Value
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$dist = "$root\dist"
if (Test-Path "$dist\GGSTNativeRes-*") { Remove-Item "$dist\GGSTNativeRes-*", "$dist\SHA256SUMS.txt" -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory $dist -Force | Out-Null

# $files: zip entry name (forward slashes, so every unzip tool recreates the folders) -> source file
function New-Zip([string]$zip, [System.Collections.Specialized.OrderedDictionary]$files) {
	$archive = [System.IO.Compression.ZipFile]::Open($zip, "Create")
	try {
		foreach ($e in $files.GetEnumerator()) {
			[void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $e.Value, $e.Key, "Optimal")
		}
	} finally { $archive.Dispose() }
}

$sums = @()
foreach ($variant in "steam", "xbox") {
	$xbox = $variant -eq "xbox"
	cmd /c "`"$root\build.bat`" $(if ($xbox) { 'xbox' })"
	if ($LASTEXITCODE) { throw "$variant build failed" }
	$dll = if ($xbox) { "$root\build\xbox\dsound.dll" } else { "$root\build\xapofx1_5.dll" }
	$fileVersion = (Get-Item $dll).VersionInfo.FileVersion
	if ($fileVersion -ne $version) { throw "DLL version $fileVersion doesn't match version.h $version" }

	$readme = if ($xbox) { "$root\release\README-xbox.txt" } else { "$root\release\README.txt" }
	if ((Get-Content $readme -TotalCount 1) -ne "GGSTNativeRes $version") { throw "$readme doesn't start with 'GGSTNativeRes $version'" }

	$files = [ordered]@{}
	$files[$(if ($xbox) { "Content/RED/Binaries/WinGDK/dsound.dll" } else { "xapofx1_5.dll" })] = $dll
	$files["README.txt"] = $readme
	$files["LICENSE.txt"] = "$root\LICENSE"
	$zip = "$dist\GGSTNativeRes-$version-$variant.zip"
	New-Zip $zip $files

	foreach ($f in $zip, $dll) {
		$sums += "{0}  {1}" -f (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(), (Split-Path $f -Leaf)
	}
}
$sums | Set-Content "$dist\SHA256SUMS.txt" -Encoding ascii
Write-Output "GGSTNativeRes $version packaged:"
Get-ChildItem $dist -File | ForEach-Object { "  {0,-34} {1,8} bytes" -f $_.Name, $_.Length }
$sums
