@echo off
rem EXT-17 - build and run the tests that need no N8RO install.
rem Everything here links nothing: it tests the parts of the campaign runner whose correctness
rem is about our own output rather than about the platform, and - since M3 - the capture reader,
rem whose correctness is about somebody else's bytes and is the whole of CR-CAP-2.
setlocal enabledelayedexpansion
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set ROOT=%~dp0..
set OUT=%ROOT%\build\tests
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\Json.cpp" ^
   "%~dp0json_writer_test.cpp" ^
   /Fe:"%OUT%\json_writer_test.exe" /Fo:"%OUT%\\"
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
   /Fe:"%OUT%\capture_reader_test.exe" /Fo:"%OUT%\\"
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
   /Fe:"%OUT%\determinism_test.exe" /Fo:"%OUT%\\"
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
   /Fe:"%OUT%\parameter_test.exe" /Fo:"%OUT%\\"
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
   /Fe:"%OUT%\assertion_test.exe" /Fo:"%OUT%\\"
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
echo.
echo tests: !CHECKS! check(s) across 5 suite(s), 0 failure(s).
> "%OUT%\checks.actual.txt" echo !CHECKS!
fc /n "%~dp0checks.golden.txt" "%OUT%\checks.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: the suite ran !CHECKS! check^(s^); tests\checks.golden.txt says otherwise.
  echo Update the golden file deliberately, in the commit that changed the suite, and
  echo update the figure in README.md's "Building" section in the same breath.
  exit /b 1
)

echo tests: all passed, and the check count matches tests\checks.golden.txt.
exit /b 0
