@echo off
rem EXT-17 M6 - build n8ro-judge, the condition evaluator over stored captures.
rem
rem LOOK AT THE COMPILE LINE. There is no /I, there is no /LIBPATH, and there is no .lib. This
rem binary links NOTHING: not EXT-08, not the N8RO SDK, not a third-party JSON library. That is
rem not a convention here, it is CR-CAP-1's third acceptance criterion made structural - a
rem binary that cannot reach the platform cannot start a host, load a scenario or subscribe to a
rem bus while pretending to re-judge a stored run.
rem
rem It needs no N8RO install to build and none to run.
setlocal
rem The toolchain is discovered, not hard-coded. See tools\find-vcvars.cmd for what
rem "free" selects here, and why it does not simply call C:\N8RO\dev\setup-dev.cmd.
call "%~dp0..\find-vcvars.cmd" free
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\n8ro-judge
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\Json.cpp" ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\assert\Geodesy.cpp" ^
   "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\n8ro-judge.exe" /Fo:"%OUT%\\" /Fd:"%OUT%\\"
if errorlevel 1 exit /b 1

rem --- The boundary, checked rather than asserted ---------------------------------------------
findstr /i /m /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
   "%ROOT%\src\assert\Conditions.h" "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\assert\Geodesy.h" "%ROOT%\src\assert\Geodesy.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: an SDK or EXT-08 name appears in the assertion's sources.
  findstr /i /n /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
     "%ROOT%\src\assert\*.h" "%ROOT%\src\assert\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

rem --- CR-CAP-1: nothing on the assertion path may reach a host, a bus or a process ------------
rem
rem "A re-judge mode takes a stored campaign directory and a condition file and produces
rem verdicts with NO HOST STARTED and NO BUS SUBSCRIPTION MADE." The compile line above already
rem makes that impossible, since none of those symbols could resolve. This search is the second
rem lock: it fails the build the moment somebody adds the include that would make it possible,
rem which is a milestone before the link would notice.
rem The tokens are call- and include-shaped on purpose. A bare "subscribe" also matches this
rem file's own prose explaining that it does not subscribe, and a check that fires on its own
rem documentation is a check somebody switches off.
findstr /i /m /c:"CreateProcess" /c:"ShellExecute" /c:"system(" /c:"_popen" /c:"popen(" ^
   /c:"subscribe(" /c:"publish(" /c:"EngineControl.h" /c:"proc/Process.h" /c:"Process.h" ^
   "%ROOT%\src\assert\Conditions.h" "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\assert\Geodesy.h" "%ROOT%\src\assert\Geodesy.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the assertion path names a process, a bus or the control path. Judging a
  echo stored capture reads a file and nothing else ^(CR-CAP-1^).
  findstr /i /n /c:"CreateProcess" /c:"ShellExecute" /c:"system(" /c:"_popen" /c:"popen(" ^
     /c:"subscribe(" /c:"publish(" /c:"EngineControl.h" /c:"Process.h" ^
     "%ROOT%\src\assert\*.h" "%ROOT%\src\assert\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

rem --- No global sort of a capture: the file's own record order is authoritative (format 5.2) --
findstr /m /c:"std::sort" /c:"std::stable_sort" /c:"qsort" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the evaluator sorts something. A capture's own record order is
  echo authoritative and a verdict's "first moment" must come from it, not from a reordering.
  exit /b 1
)

rem --- CR-DET-2's hazards, on a path that decides verdicts -------------------------------------
rem
rem A verdict is output, and CR-DET-2 says nothing of ours may vary between two identical runs.
rem A clock or a formatted time on this path would make a re-judgement differ from the live run
rem it is supposed to reproduce byte for byte - which is exactly the check --verify performs, so
rem this search protects the thing that proves CR-CAP-1.
findstr /i /m /c:"chrono" /c:"GetTickCount" /c:"GetSystemTime" /c:"QueryPerformanceCounter" ^
   /c:"time(" /c:"localtime" /c:"gmtime" /c:"strftime" /c:"GetLocalTime" ^
   "%ROOT%\src\assert\Conditions.h" "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\assert\Geodesy.h" "%ROOT%\src\assert\Geodesy.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the assertion path reads a clock or formats a time.
  findstr /i /n /c:"chrono" /c:"GetTickCount" /c:"GetSystemTime" /c:"time(" ^
     "%ROOT%\src\assert\*.h" "%ROOT%\src\assert\*.cpp" "%~dp0main.cpp"
  exit /b 1
)

findstr /m /c:"unordered_map" /c:"unordered_set" /c:"unordered_multimap" /c:"unordered_multiset" ^
   "%ROOT%\src\assert\Conditions.h" "%ROOT%\src\assert\Conditions.cpp" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the assertion path holds an unordered container. Its iteration order is
  echo unspecified, and a verdict file whose line order varied would fail its own identity check.
  exit /b 1
)

rem And the locale hazard. A verdict carries distances as TEXT, and that text is compared byte
rem for byte between a live judgement and a re-judgement. snprintf("%f") reads the ambient
rem decimal separator - a comma under German_Germany.1252 on this machine - so the evaluator
rem renders its own digits by arithmetic instead. src\common\Json.cpp is the writer and is
rem deliberately not in this list; nothing it formats reaches a verdict line.
findstr /m /c:"sprintf" /c:"snprintf" /c:"ostringstream" /c:"stringstream" ^
   /c:"strtod" /c:"atof" /c:"setprecision" /c:"to_string(static_cast<double" ^
   "%ROOT%\src\assert\Judge.h" "%ROOT%\src\assert\Judge.cpp" ^
   "%ROOT%\src\assert\Geodesy.h" "%ROOT%\src\assert\Geodesy.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the evaluator formats or parses a number through the current locale.
  echo A verdict's numbers are compared as text between a live run and a re-judgement.
  findstr /n /c:"sprintf" /c:"snprintf" /c:"ostringstream" /c:"strtod" /c:"atof" ^
     /c:"setprecision" "%ROOT%\src\assert\Judge.cpp" "%ROOT%\src\assert\Geodesy.cpp"
  exit /b 1
)

rem --- CLI authority check: --help must match the golden file, byte for byte -------------------
"%OUT%\n8ro-judge.exe" --help > "%OUT%\help.actual.txt"
if errorlevel 1 (
  echo BUILD FAILED: n8ro-judge --help did not run
  exit /b 1
)
fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: --help does not match tools\n8ro-judge\help.golden.txt
  fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt"
  exit /b 1
)
echo n8ro-judge: built, links nothing, cannot reach a host or a bus, carries none of CR-DET-2's
echo             hazards, and --help matches the golden file.
exit /b 0
