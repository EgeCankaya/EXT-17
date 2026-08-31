@echo off
rem EXT-17 M1 - build the by-hand run driver. Release, x64, C++17.
rem Links the N8RO SDK only: n8ro-sim + n8ro-core. No EXT-08 anything.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set OUT=%~dp0..\..\build\m1-run
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   /I "C:\N8RO\include\n8ro-sim" /I "C:\N8RO\include\n8ro-core" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\m1-run.exe" /Fo:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
exit /b %errorlevel%
