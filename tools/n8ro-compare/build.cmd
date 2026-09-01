@echo off
rem EXT-17 M4 - build n8ro-compare, the determinism comparison for two captures.
rem
rem LOOK AT THE COMPILE LINE. There is no /I, there is no /LIBPATH, and there is no .lib. This
rem binary links NOTHING: not EXT-08, not the N8RO SDK, not a third-party JSON library. The gate
rem the whole project rests on is decided by code that could not reach the platform if it wanted
rem to, and a build script anyone can read in ten seconds is a better proof of that than an
rem argument about translation units.
rem
rem It needs no N8RO install to build and none to run.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "free" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" free
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\n8ro-compare
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%ROOT%\src\compare\Compare.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\n8ro-compare.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

rem --- The boundary, checked rather than asserted ---------------------------------------------
findstr /i /m /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
   "%ROOT%\src\compare\Compare.h" "%ROOT%\src\compare\Compare.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: an SDK or EXT-08 name appears in the comparison's sources.
  findstr /i /n /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
     "%ROOT%\src\compare\*.h" "%ROOT%\src\compare\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

rem CR-CAP-4's fourth acceptance criterion, extended to the comparison: no code path sorts a
rem capture. The file's own record order is authoritative (format 5.2), and the comparison walks
rem two already-ordered sequences rather than reordering either.
findstr /m /c:"std::sort" /c:"std::stable_sort" /c:"qsort" ^
   "%ROOT%\src\compare\Compare.h" "%ROOT%\src\compare\Compare.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the comparison sorts something. A capture's own record order is
  echo authoritative and must not be reordered to make an alignment work.
  exit /b 1
)

rem --- CR-DET-2: the three hazards [B] names, none of which may appear on this path ------------
rem
rem "Anything of yours that varies between runs - a timestamp in the compared output, an
rem unordered container iterated, a value read from a clock."
rem
rem These are searched for by name here AND tested behaviourally in tests\determinism_test.cpp.
rem Neither check subsumes the other: the search catches a reintroduction on a path no test
rem happens to exercise, and the test catches one this search does not know the spelling of.
rem
rem (1) a value read from a clock, and (2) a timestamp in compared output.
findstr /i /m /c:"chrono" /c:"GetTickCount" /c:"GetSystemTime" /c:"QueryPerformanceCounter" ^
   /c:"time(" /c:"localtime" /c:"gmtime" /c:"strftime" /c:"GetLocalTime" ^
   "%ROOT%\src\compare\Compare.h" "%ROOT%\src\compare\Compare.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the comparison path reads a clock or formats a time. CR-DET-2: nothing
  echo of ours may vary between two identical runs, and a clock is the first hazard [B] names.
  findstr /i /n /c:"chrono" /c:"GetTickCount" /c:"GetSystemTime" /c:"QueryPerformanceCounter" ^
     /c:"time(" /c:"localtime" /c:"gmtime" /c:"strftime" /c:"GetLocalTime" ^
     "%ROOT%\src\compare\*.h" "%ROOT%\src\compare\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

rem (3) an unordered container iterated on a path that produces compared output.
findstr /m /c:"unordered_map" /c:"unordered_set" /c:"unordered_multimap" /c:"unordered_multiset" ^
   "%ROOT%\src\compare\Compare.h" "%ROOT%\src\compare\Compare.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the comparison path holds an unordered container. Its iteration order is
  echo unspecified, which is the third hazard [B] names. Use std::map.
  findstr /n /c:"unordered_map" /c:"unordered_set" ^
     "%ROOT%\src\compare\*.h" "%ROOT%\src\compare\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

rem And the locale hazard, which is this project's own addition to [B]'s three: a double
rem converted to or from text through the current locale reads a comma for a decimal point on
rem this machine. The comparison never converts a number for comparison at all - it matches on
rem the verbatim text the file carried - so none of these may appear.
findstr /m /c:"sprintf" /c:"snprintf" /c:"ostringstream" /c:"to_string(static_cast<double" ^
   /c:"strtod" /c:"atof" /c:"setprecision" ^
   "%ROOT%\src\compare\Compare.h" "%ROOT%\src\compare\Compare.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the comparison formats or parses a number through the current locale.
  echo Compare the verbatim text the capture carried instead ^(format 8.3^).
  findstr /n /c:"sprintf" /c:"snprintf" /c:"ostringstream" /c:"strtod" /c:"atof" ^
     /c:"setprecision" "%ROOT%\src\compare\*.h" "%ROOT%\src\compare\*.cpp"
  exit /b 1
)

rem --- CLI authority check: --help must match the golden file, byte for byte -------------------
"%OUT%\n8ro-compare.exe" --help > "%OUT%\help.actual.txt"
if errorlevel 1 (
  echo BUILD FAILED: n8ro-compare --help did not run
  exit /b 1
)
fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: --help does not match tools\n8ro-compare\help.golden.txt
  fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt"
  exit /b 1
)
echo n8ro-compare: built, links nothing, carries none of CR-DET-2's four hazards, and --help
echo               matches the golden file.
exit /b 0
