@echo off
rem EXT-17 - build and run the tests that need no N8RO install.
rem Everything here links nothing: it tests the parts of the campaign runner whose correctness
rem is about our own output rather than about the platform, and - since M3 - the capture reader,
rem whose correctness is about somebody else's bytes and is the whole of CR-CAP-2.
setlocal
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

"%OUT%\json_writer_test.exe"
if errorlevel 1 (
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
"%OUT%\capture_reader_test.exe" "%ROOT%"
if errorlevel 1 (
  echo TESTS FAILED
  exit /b 1
)

echo.
echo tests: all passed.
exit /b 0
