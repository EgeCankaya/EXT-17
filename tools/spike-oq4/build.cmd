@echo off
rem EXT-17 M5 - build the OQ-4 fidelity spike.
rem A spike, not a product: it measures the one OQ-4 criterion M2 deliberately did not -
rem whether a swept range still produces a scenario that makes sense - and is then evidence.
rem Links the N8RO SDK only, through the same src/ components the campaign runner uses.
rem
rem THE ASSERTION SOURCES ARE HERE BECAUSE THIS SCRIPT WENT STALE AND NOTHING NOTICED. M6 gave
rem RunOnce verdicts to write, this spike was not rebuilt afterwards, and it had been failing
rem with five unresolved externals ever since - found only by building every script in the tree
rem during the handover pass. spike-axis went stale the same way at M3 and says so in its own
rem header, so this is the second occurrence of one pattern: a spike is evidence rather than
rem product, but a build command committed to the repository is one a reader is invited to run,
rem and nothing in this project executes these two. The same shape as F-38, one level down.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "sdk" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" sdk
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\spike-oq4
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
   /Fe:"%OUT%\spike-oq4.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
exit /b %errorlevel%
