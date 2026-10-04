# Builds the release DLL and packages it:
#   dist\GGSTNativeRes-<version>.zip   (xapofx1_5.dll, README.txt, LICENSE.txt)
#   dist\SHA256SUMS.txt                (hashes of the zip and of the DLL)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$version = (Select-String -Path "$root\src\version.h" -Pattern 'GGSTNR_VERSION_STRING "([^"]+)"').Matches[0].Groups[1].Value

cmd /c "`"$root\build.bat`""
if ($LASTEXITCODE) { throw "build failed" }

$dll = "$root\build\xapofx1_5.dll"
$fileVersion = (Get-Item $dll).VersionInfo.FileVersion
if ($fileVersion -ne $version) { throw "DLL version $fileVersion doesn't match version.h $version" }

$dist = "$root\dist"
$stage = "$dist\GGSTNativeRes-$version"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null
Copy-Item $dll $stage
Copy-Item "$root\release\README.txt" "$stage\README.txt"
Copy-Item "$root\LICENSE" "$stage\LICENSE.txt"

$zip = "$dist\GGSTNativeRes-$version.zip"
Compress-Archive -Path "$stage\*" -DestinationPath $zip
$sums = foreach ($f in $zip, "$stage\xapofx1_5.dll") {
	"{0}  {1}" -f (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(), (Split-Path $f -Leaf)
}
$sums | Set-Content "$dist\SHA256SUMS.txt" -Encoding ascii
Write-Output "GGSTNativeRes $version packaged:"
Get-ChildItem $dist -File | ForEach-Object { "  {0,-28} {1,8} bytes" -f $_.Name, $_.Length }
$sums
