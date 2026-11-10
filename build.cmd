@echo off
setlocal
cd /d "%~dp0"
set "MOD_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%MOD_VSWHERE%" (
    echo Visual Studio C++ build tools were not found. Install the Desktop development with C++ workload.
    exit /b 1
)
set "MOD_VS_ROOT="
for /f "usebackq tokens=*" %%i in (`"%MOD_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MOD_VS_ROOT=%%i"
if not defined MOD_VS_ROOT (
    echo No Visual Studio installation with the x64 C++ toolchain was found.
    exit /b 1
)
call "%MOD_VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /LD mod.cpp /Fo:build\mod.obj /Fe:build\XInput1_4.dll /link /DEF:xinput.def /IMPLIB:build\ShrekControllerGlyphs.lib /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc tests.cpp /Fo:build\tests.obj /Fe:build\tests.exe
if errorlevel 1 exit /b 1
build\tests.exe
