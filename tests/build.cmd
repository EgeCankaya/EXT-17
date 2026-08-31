@echo off
rem EXT-17 - build and run the tests that need no N8RO install.
rem Everything here links nothing: it tests the parts of the campaign runner whose correctness
rem is about our own output rather than about the platform.
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
echo tests: all passed.
exit /b 0
