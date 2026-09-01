@echo off
rem EXT-17 M2 - build the R9/OQ-4 parameterisation-axis feasibility spike.
rem A spike, not a product: it answers OQ-4's feasibility question and is then evidence.
rem Links the N8RO SDK only, through the same src/ components the campaign runner uses.
rem THIS SCRIPT HAS NOW GONE STALE TWICE, THE SAME WAY, AND THE SECOND TIME IS THE FINDING.
rem The capture sources are here because M3 gave RunOnce a read-back of the capture it just
rem produced; this script had not been re-run since and no longer linked. Fixed at M5.
rem The assertion sources are here because M6 gave RunOnce verdicts to write, and the same
rem thing happened again: five unresolved externals, found only by building every script in
rem the tree during the handover pass. A spike is evidence rather than product, but a build
rem command committed to the repository is something a reader is invited to run, and this one
rem had not been run since the milestone before last. Nothing executes these two scripts, so
rem nothing notices - the same shape as F-38, one level down.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "sdk" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" sdk
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\spike-axis
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   /I "C:\N8RO\include\n8ro-sim" /I "C:\N8RO\include\n8ro-core" ^
   "%ROOT%\src\common\Log.cpp" ^
   "%ROOT%\src\common\Json.cpp" ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%ROOT%\src\assert\Geodesy.cpp" ^
   "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\proc\Process.cpp" ^
   "%ROOT%\src\control\EngineControl.cpp" ^
   "%ROOT%\src\run\StopPredicate.cpp" ^
   "%ROOT%\src\run\RunRecord.cpp" ^
   "%ROOT%\src\run\RunOnce.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\spike-axis.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
exit /b %errorlevel%
