@echo off
setlocal
set VSLANG=1033
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if not defined VSDIR exit /b 1
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S "%~dp0." -B "%~dp0build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
if "%2"=="--safe-compile" (
    cmake --build "%~dp0build\%CONFIG%" --target ShaderixFrame advanced_tests frame_tests editor_tests export_tests transition_tests mixer_tests production_tests || exit /b 1
    exit /b 0
)
cmake --build "%~dp0build\%CONFIG%" || exit /b 1
if "%2"=="--compile-only" exit /b 0
ctest --test-dir "%~dp0build\%CONFIG%" --output-on-failure || exit /b 1
echo Built: %~dp0build\%CONFIG%\ShaderixFrame.exe
