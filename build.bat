@echo off
rem Usage: build.bat          release build -> build\xapofx1_5.dll
rem        build.bat diag     diagnostic build (logs resolution requests) -> build\diag\xapofx1_5.dll
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0"
set "OUT=build" & set "DEFS="
if /i "%~1"=="diag" (set "OUT=build\diag" & set "DEFS=/DGGSTNR_DIAG")
if not exist %OUT% mkdir %OUT%
rc /nologo /fo %OUT%\version.res src\version.rc || exit /b 1
rem /Brepro: deterministic output (no timestamps), so anyone can rebuild and compare the hash
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 /LD /Brepro %DEFS% src\main.cpp %OUT%\version.res /Fo:%OUT%\ /Fe:%OUT%\xapofx1_5.dll ^
   /link /Brepro /DEF:src\exports.def user32.lib || exit /b 1
echo Built %OUT%\xapofx1_5.dll
