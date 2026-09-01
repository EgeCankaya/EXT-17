@echo off
rem EXT-17 M1 - build the by-hand run driver. Release, x64, C++17.
rem Links the N8RO SDK only: n8ro-sim + n8ro-core. No EXT-08 anything.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "sdk" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" sdk
if errorlevel 1 exit /b 1
set OUT=%~dp0..\..\build\m1-run
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   /I "C:\N8RO\include\n8ro-sim" /I "C:\N8RO\include\n8ro-core" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\m1-run.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
exit /b %errorlevel%
