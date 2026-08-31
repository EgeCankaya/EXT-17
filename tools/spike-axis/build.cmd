@echo off
rem EXT-17 M2 - build the R9/OQ-4 parameterisation-axis feasibility spike.
rem A spike, not a product: it answers OQ-4's feasibility question and is then evidence.
rem Links the N8RO SDK only, through the same src/ components the campaign runner uses.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\spike-axis
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   /I "C:\N8RO\include\n8ro-sim" /I "C:\N8RO\include\n8ro-core" ^
   "%ROOT%\src\common\Log.cpp" ^
   "%ROOT%\src\common\Json.cpp" ^
   "%ROOT%\src\proc\Process.cpp" ^
   "%ROOT%\src\control\EngineControl.cpp" ^
   "%ROOT%\src\run\StopPredicate.cpp" ^
   "%ROOT%\src\run\RunRecord.cpp" ^
   "%ROOT%\src\run\RunOnce.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\spike-axis.exe" /Fo:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
exit /b %errorlevel%
