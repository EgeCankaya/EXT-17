@echo off
rem EXT-17 M4 - build n8ro-campaign. Release, x64, C++17.
rem Links the N8RO SDK only: n8ro-sim + n8ro-core. No EXT-08 anything.
rem
rem Since M3 it also links src/capture, the conformant reader, so that a run can read back the
rem capture it just produced. That direction is fine and the reverse is not: the requirement is
rem that the READER links no SDK, which tools\n8ro-capture\build.cmd is the proof of.
rem
rem Since M6 it also links src/assert, the condition evaluator, in the same direction again -
rem and that is what makes CR-CAP-1's identity structural rather than promised: the campaign's
rem "live" verdicts are produced by the same code, over the same stored capture, that
rem n8ro-judge re-judges later. There is no second evaluator for the two to disagree about.
rem tools\n8ro-judge\build.cmd is that one's proof, and it carries the searches that keep the
rem assertion path unable to reach a host or a bus.
rem
rem Since M4 it also links src/compare, the determinism comparison, for the same reason and in
rem the same direction. tools\n8ro-compare\build.cmd is that one's proof, and it is where the
rem searches for CR-DET-2's hazards live - a clock, a timestamp, an unordered container, a
rem locale-dependent number conversion - because those are properties of the comparison's own
rem sources rather than of whatever happens to link them.
rem
rem The build ends by comparing the binary's own --help against help.golden.txt. That comparison
rem is the mechanism the PRD names as keeping the CLI authority table true: the document does not
rem enumerate the options, the golden file does, and a drift fails the build rather than an audit.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "sdk" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" sdk
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\n8ro-campaign
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   /I "C:\N8RO\include\n8ro-sim" /I "C:\N8RO\include\n8ro-core" ^
   "%ROOT%\src\common\Log.cpp" ^
   "%ROOT%\src\common\Json.cpp" ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%ROOT%\src\compare\Compare.cpp" ^
   "%ROOT%\src\param\Axis.cpp" ^
   "%ROOT%\src\assert\Geodesy.cpp" ^
   "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\proc\Process.cpp" ^
   "%ROOT%\src\control\EngineControl.cpp" ^
   "%ROOT%\src\run\StopPredicate.cpp" ^
   "%ROOT%\src\run\RunRecord.cpp" ^
   "%ROOT%\src\run\RunOnce.cpp" ^
   "%ROOT%\src\run\SelfTest.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\n8ro-campaign.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\" ^
   /link /LIBPATH:"C:\N8RO\lib" n8ro-sim.lib n8ro-core.lib
if errorlevel 1 exit /b 1

rem --- CLI authority check: --help must match the golden file, byte for byte -----------------
set PATH=C:\N8RO\bin;%PATH%
"%OUT%\n8ro-campaign.exe" --help > "%OUT%\help.actual.txt"
if errorlevel 1 (
  echo BUILD FAILED: n8ro-campaign --help did not run
  exit /b 1
)
fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: --help does not match tools\n8ro-campaign\help.golden.txt
  echo Update the golden file deliberately, or fix the option list.
  fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt"
  exit /b 1
)
echo n8ro-campaign: built, and --help matches the golden file.
exit /b 0
