@echo off
rem EXT-17 M3 - build n8ro-capture, the conformant reader for n8ro-capture/1.
rem
rem LOOK AT THE COMPILE LINE. There is no /I, there is no /LIBPATH, and there is no .lib. This
rem binary links NOTHING: not EXT-08, not the N8RO SDK, not a third-party JSON library. That is
rem CR-CAP-2's first acceptance criterion and ADR-2's M3 gate, and a build script anyone can read
rem in ten seconds is a better proof of it than an argument about translation units.
rem
rem It needs no N8RO install to build and none to run.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set ROOT=%~dp0..\..
set OUT=%ROOT%\build\n8ro-capture
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++17 /EHsc /O2 /MD /W3 ^
   "%ROOT%\src\common\JsonParse.cpp" ^
   "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%~dp0main.cpp" ^
   /Fe:"%OUT%\n8ro-capture.exe" /Fo:"%OUT%\\"
if errorlevel 1 exit /b 1

rem --- The boundary, checked rather than asserted ---------------------------------------------
rem ADR-2's M3 gate is "no EXT-08 source has been read, and no EXT-08 identifier appears in
rem EXT-17's source". The reader's own sources are searched for the SDK's and the producer's
rem names. A hit fails the build, which is the only way a gate like this stays true.
findstr /i /m /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
   "%ROOT%\src\capture\Capture.h" "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.h" "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.h" "%ROOT%\src\capture\CaptureSet.cpp" ^
   "%ROOT%\src\common\JsonParse.h" "%ROOT%\src\common\JsonParse.cpp" ^
   "%~dp0main.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: an SDK or EXT-08 name appears in the reader's sources.
  findstr /i /n /c:"n8ro-sim" /c:"n8ro-core" /c:"n8ro-bridge" /c:"N8RO\include" ^
     "%ROOT%\src\capture\*.h" "%ROOT%\src\capture\*.cpp" ^
     "%ROOT%\src\common\JsonParse.*" "%~dp0main.cpp"
  exit /b 1
)

rem CR-CAP-4's fourth acceptance criterion: no code path sorts a capture by sim_time_s globally.
rem The sorts in the reader's sources are searched for and must be none - the only sorting this
rem program does is of directory entries, in main.cpp, which is not a capture.
findstr /m /c:"std::sort" /c:"std::stable_sort" /c:"qsort" ^
   "%ROOT%\src\capture\Capture.h" "%ROOT%\src\capture\Capture.cpp" ^
   "%ROOT%\src\capture\CaptureReader.h" "%ROOT%\src\capture\CaptureReader.cpp" ^
   "%ROOT%\src\capture\CaptureSet.h" "%ROOT%\src\capture\CaptureSet.cpp" >nul
if not errorlevel 1 (
  echo BUILD FAILED: the reader sorts something. A capture's own record order is authoritative
  echo and must not be reordered to make times monotonic ^(format 5.2^).
  exit /b 1
)

rem --- CLI authority check: --help must match the golden file, byte for byte -------------------
"%OUT%\n8ro-capture.exe" --help > "%OUT%\help.actual.txt"
if errorlevel 1 (
  echo BUILD FAILED: n8ro-capture --help did not run
  exit /b 1
)
fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt" >nul
if errorlevel 1 (
  echo BUILD FAILED: --help does not match tools\n8ro-capture\help.golden.txt
  fc /n "%~dp0help.golden.txt" "%OUT%\help.actual.txt"
  exit /b 1
)
echo n8ro-capture: built, links nothing, and --help matches the golden file.
exit /b 0
