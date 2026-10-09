@echo off
rem Usage: build.bat [xbox] [diag]
rem   (none)     Steam release build                       -> build\xapofx1_5.dll
rem   xbox       Microsoft Store / Xbox app release build  -> build\xbox\dsound.dll
rem   diag       diagnostic build (logs resolution requests), combinable with xbox
rem              -> build\diag\xapofx1_5.dll or build\xbox-diag\dsound.dll
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0"

set "XBOX=" & set "DIAG="
for %%a in (%*) do (
	if /i "%%a"=="xbox" set "XBOX=1"
	if /i "%%a"=="diag" set "DIAG=1"
)
set "DLL=xapofx1_5.dll" & set "DEF=src\exports.def" & set "DEFS=" & set "OUT=build"
if defined XBOX (set "DLL=dsound.dll" & set "DEF=src\exports_dsound.def" & set "DEFS=/DGGSTNR_PROXY_DSOUND" & set "OUT=build\xbox")
if defined DIAG (set "DEFS=%DEFS% /DGGSTNR_DIAG" & if defined XBOX (set "OUT=build\xbox-diag") else (set "OUT=build\diag"))

if not exist %OUT% mkdir %OUT%
rc /nologo %DEFS% /fo %OUT%\version.res src\version.rc || exit /b 1
rem /Brepro: deterministic output (no timestamps), so anyone can rebuild and compare the hash
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 /LD /Brepro %DEFS% src\main.cpp %OUT%\version.res /Fo:%OUT%\ /Fe:%OUT%\%DLL% ^
   /link /Brepro /DEF:%DEF% user32.lib || exit /b 1
echo Built %OUT%\%DLL%
