@echo off
rem Cross-platform SDL3 Markdown app, built here with MSVC 2022 and CMake.
setlocal
set "ROOT=%~dp0"
set "OUTPUT=%ROOT%build\cyui_markdown_test.exe"
if not "%~1"=="" set "OUTPUT=%~f1"
for %%i in ("%OUTPUT%") do set "OUTPUT_DIR=%%~dpi"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
if not defined VSDIR set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not defined VCINSTALLDIR (
    if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" exit /b 1
    call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
set "TASK_CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%TASK_CMAKE%" set "TASK_CMAKE=cmake"
pushd "%ROOT%" || exit /b 1
"%TASK_CMAKE%" -S . -B build/markdown-sdl3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCYUI_BUILD_DEMO=OFF -DCYUI_BUILD_GALLERY=OFF -DCYUI_BUILD_AUDIO=OFF -DCYUI_BUILD_MARKDOWN_DEMO=ON || goto :failed
"%TASK_CMAKE%" --build build/markdown-sdl3 --config Release --target cyui_markdown_test --parallel 8 || goto :failed
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
copy /Y "build\markdown-sdl3\cyui_markdown_test.exe" "%OUTPUT%" >nul || goto :failed
if not exist "%OUTPUT_DIR%licenses" mkdir "%OUTPUT_DIR%licenses"
copy /Y "build\markdown-sdl3\licenses\*" "%OUTPUT_DIR%licenses\" >nul || goto :failed
popd
echo Built: %OUTPUT%
exit /b 0
:failed
popd
exit /b 1
