@echo off
rem Builds the CyUI demo with MSVC, without CMake. Run from any prompt.
setlocal
set ROOT=%~dp0
if defined VCINSTALLDIR goto :compile
rem Any edition of Visual Studio 2022 or later with the x64 C++ tools, found by vswhere.
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
set VSDIR=
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
if not defined VSDIR set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
    echo MSVC x64 tools not found. Install Visual Studio 2022 with "Desktop development with C++". 1>&2
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
:compile
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
rem Use relative source names and a static runtime for the portable release build.
set IMGUI=..\third_party\imgui
cl /nologo /std:c++20 /O2 /MT /DNDEBUG /EHsc /W4 /utf-8 /MP /DUNICODE /D_UNICODE /DCYUI_HAS_AUDIO=1 /DCYUI_HAS_GALLERY=1 /DCYUI_HAS_CANVAS=1 /DCYUI_HAS_PLOTS=1 /DCYUI_HAS_CURVES=1 /DCYUI_HAS_NODES=1 /DCYUI_HAS_TIMELINE=1 /DCYUI_HAS_AUDIOVIEWS=1 /DCYUI_CODE_LANGUAGE_CATALOGUE=1 /DCYUI_HAS_CODE=1 /DCYUI_HAS_DIFF=1 /DCYUI_HAS_CODEEDITOR=1 /DCYUI_HAS_INSPECTOR=1 /DCYUI_HAS_ASSETS=1 /DCYUI_HAS_IMAGES=1 /DCYUI_HAS_VIEWPORT=1 /DCYUI_HAS_COMMANDS=1 /DCYUI_HAS_DIAGNOSTICS=1 /DCYUI_HAS_DOCUMENTVIEWS=1 /DCYUI_HAS_WEB=1 ^
   /I"..\third_party\lexilla\lexlib" /I"..\third_party\lexilla\include" /I"..\third_party\scintilla\include" /I"..\third_party\texteditor" /I"..\include" /I"%IMGUI%" /I"%IMGUI%\backends" ^
   "..\src\Theme.cpp" "..\src\Widgets.cpp" "..\src\Layout.cpp" "..\src\Filter.cpp" "..\src\Gallery.cpp" ^
   "..\src\modules\*.cpp" "..\src\modules\code\*.cpp" "..\third_party\lexilla\lexlib\*.cxx" "..\third_party\lexilla\lexers\*.cxx" "..\third_party\texteditor\TextEditor.cpp" ^
   "..\examples\demo_win32_dx11\main.cpp" ^
   "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" "%IMGUI%\imgui_widgets.cpp" "%IMGUI%\imgui_demo.cpp" ^
   "%IMGUI%\backends\imgui_impl_win32.cpp" "%IMGUI%\backends\imgui_impl_dx11.cpp" ^
   /Fe:cyui_demo.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib user32.lib gdi32.lib shell32.lib
set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
