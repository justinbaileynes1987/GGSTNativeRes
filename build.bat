@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /O2 /MT /W4 /EHsc /std:c++17 /LD src\main.cpp /Fo:build\ /Fe:build\sensapi.dll ^
   /link /DEF:src\exports.def user32.lib || exit /b 1
echo Built build\sensapi.dll
