@echo off
rem EXT-17 - build and run the tests that need no N8RO install.
rem Everything here links nothing: it tests the parts of the campaign runner whose correctness
rem is about our own output rather than about the platform, and - since M3 - the capture reader,
rem whose correctness is about somebody else's bytes and is the whole of CR-CAP-2.
setlocal enabledelayedexpansion
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "free" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\tools\find-vcvars.cmd" free
if errorlevel 1 exit /b 1

set ROOT=%~dp0..
set OUT=%ROOT%\build\tests
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\Json.cpp" ^
   "%~dp0json_writer_test.cpp" ^
   /Fe:"%OUT%\json_writer_test.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

"%OUT%\json_writer_test.exe" > "%OUT%\json_writer_test.out" 2>&1
set TESTRC=%errorlevel%
type "%OUT%\json_writer_test.out"
if not "%TESTRC%"=="0" (
  echo TESTS FAILED
  exit /b 1
)

echo.
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%~dp0capture_reader_test.cpp" ^
   /Fe:"%OUT%\capture_reader_test.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

rem The repo root is passed in so the suite can find contract/ and campaigns/ wherever it was
rem invoked from. Nothing it writes goes anywhere but build\tests\mutations - contract/ is
rem read-only, and a test that edited a vendored fixture in place would be the worst possible
rem way to discover that.
"%OUT%\capture_reader_test.exe" "%ROOT%" > "%OUT%\capture_reader_test.out" 2>&1
set TESTRC=%errorlevel%
type "%OUT%\capture_reader_test.out"
if not "%TESTRC%"=="0" (
  echo TESTS FAILED
  exit /b 1
)

rem CR-DET-1, CR-DET-2 and CR-DET-3. The comparison's own behaviour, and each of the three
rem hazards [B] names tested by running it rather than by reading it - the build-time searches in
rem tools\n8ro-compare\build.cmd cover the other half, and neither subsumes the other.
echo.
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%ROOT%\src\compare\Compare.cpp" ^
   "%~dp0determinism_test.cpp" ^
   /Fe:"%OUT%\determinism_test.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

"%OUT%\determinism_test.exe" "%ROOT%" > "%OUT%\determinism_test.out" 2>&1
set TESTRC=%errorlevel%
type "%OUT%\determinism_test.out"
if not "%TESTRC%"=="0" (
  echo TESTS FAILED
  exit /b 1
)

rem CR-PAR-1, and the half of CR-PAR-2 that needs no simulator. src\param\Axis links nothing but
rem the JSON parser, so the WHOLE of the axis's configuration surface - what parses, what is
rem refused and by what name, that a value's declared text survives, and that the sweep is
rem ordered by value rather than by spelling - is checkable here. The only part that needs a
rem simulator is whether the platform honours a swept value, and that is measured against real
rem runs in docs\m5-oq4.md rather than asserted anywhere.
echo.
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\param\Axis.cpp" ^
   "%~dp0parameter_test.cpp" ^
   /Fe:"%OUT%\parameter_test.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

"%OUT%\parameter_test.exe" > "%OUT%\parameter_test.out" 2>&1
set TESTRC=%errorlevel%
type "%OUT%\parameter_test.out"
if not "%TESTRC%"=="0" (
  echo TESTS FAILED
  exit /b 1
)

rem CR-AS-1 through CR-AS-4, and the half of CR-CAP-1 that is about the evaluator rather than
rem about a directory of captures. src\assert links nothing but the capture reader and the JSON
rem parser, so the WHOLE assertion surface is checkable here - what parses and what is refused by
rem what name, the geodesy contract/ did not carry (E-5), what a verdict says, and the four-row
rem absence classification exercised from BOTH sides over captures written by hand in the test
rem that asserts on them. The only thing needing a simulator is whether a real run produces the
rem shapes these captures imitate, and that is measured in docs\m6-assertions.md.
echo.
cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\Json.cpp" ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\assert\Geodesy.cpp" ^
   "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.cpp" ^
   "%~dp0assertion_test.cpp" ^
   /Fe:"%OUT%\assertion_test.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

"%OUT%\assertion_test.exe" "%ROOT%" > "%OUT%\assertion_test.out" 2>&1
set TESTRC=%errorlevel%
type "%OUT%\assertion_test.out"
if not "%TESTRC%"=="0" (
  echo TESTS FAILED
  exit /b 1
)

rem --- CHECK-COUNT AUTHORITY ----------------------------------------------------------------
rem The README quotes a total, and a number in prose is a number that rots: it said 466 for a
rem milestone after the determinism suite had grown to 105 (F-41). So the total is SUMMED here
rem from what the suites actually printed, and compared against checks.golden.txt - the same
rem mechanism and the same discipline as the four golden --help files. Growing the suite means
rem updating the golden file, deliberately, in the commit that grew it.
rem
rem json_writer_test prints no count line and contributes 0. That is why the figure is stated
rem as "across 5 suite(s)" rather than as a sum of five numbers.
set /a CHECKS=0
for %%S in (json_writer_test capture_reader_test determinism_test parameter_test assertion_test) do (
  for /f "tokens=1" %%N in ('findstr /r /c:"^[0-9][0-9]* check(s)" "%OUT%\%%S.out"') do set /a CHECKS+=%%N
)

rem THE GOLDEN PINS THE MANDATORY TOTAL, NOT WHAT THIS MACHINE HAPPENED TO RUN.
rem capture_reader_test's tier 4 reads the real producer-0.9.0 captures under
rem campaigns\m2-oq1\runs, which are untracked and 569 MB, so it runs on the development
rem machine and is skipped everywhere else. That made the check count a property of the machine:
rem 475 here, 469 on a clean windows-latest runner, zero failures both times. F-41 added this
rem golden to stop a total drifting silently, and a golden that only holds in one place is the
rem same defect it was built to prevent - which is exactly what the first CI run reported.
rem
rem So the suite prints its optional count on its own line and it is subtracted here. Those
rem checks still RUN wherever the captures are, and a failure in one still fails the suite; it
rem is only the COUNT that is held apart, so that the number in the golden file is one every
rem machine can reproduce.
set /a OPTIONAL=0
for /f "tokens=1" %%N in ('findstr /r /c:"^[0-9][0-9]* optional check(s)" "%OUT%\capture_reader_test.out"') do set /a OPTIONAL+=%%N
set /a MANDATORY=!CHECKS!-!OPTIONAL!

echo.
echo tests: !CHECKS! check(s) across 5 suite(s), 0 failure(s).
if not "!OPTIONAL!"=="0" (
  echo        of which !OPTIONAL! are optional ^(tier 4 - the untracked 0.9.0 captures are present^);
  echo        !MANDATORY! is the mandatory total, and the golden file pins that.
) else (
  echo        tier 4 was skipped ^(the untracked 0.9.0 captures are not present here^).
)
> "%OUT%\checks.actual.txt" echo !MANDATORY!
fc /n "%~dp0checks.golden.txt" "%OUT%\checks.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: the suite ran !MANDATORY! mandatory check^(s^); tests\checks.golden.txt says otherwise.
  echo Update the golden file deliberately, in the commit that changed the suite, and
  echo update the figure in README.md's "Building" section in the same breath.
  exit /b 1
)

echo tests: all passed, and the check count matches tests\checks.golden.txt.
exit /b 0
