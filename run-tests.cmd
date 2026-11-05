@echo off
rem Host tests on Windows with Visual Studio 2022 Build Tools (MSVC + the CMake/Ninja bundled with it).
rem Run from any folder; set VSDEVCMD if your Build Tools live somewhere else.
rem On Linux/macOS: cmake -S . -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
setlocal
if not defined VSDEVCMD set "VSDEVCMD=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "CM=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%CM%\CMake\bin;%CM%\Ninja;%PATH%"
cd /d "%~dp0"
cmake -S . -B build-tests -G Ninja || exit /b 1
cmake --build build-tests || exit /b 1
ctest --test-dir build-tests --output-on-failure
exit /b %errorlevel%
