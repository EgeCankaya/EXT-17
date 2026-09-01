@echo off
rem EXT-17 - locate a Visual Studio x64 build environment and enter it.
rem
rem WHY THIS FILE EXISTS (F-43). Eight build scripts each opened with the same hard-coded absolute
rem path to one Visual Studio *Insiders* install on one machine:
rem
rem     call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
rem
rem Every one of them therefore failed on any machine but the development machine, at the FIRST
rem command the README asks a reader to run. Nothing caught it because nothing here had ever been
rem built anywhere else - the same shape as the CRLF defect in .gitattributes, and found the same
rem way: by asking what a fresh clone on a fresh machine actually does.
rem
rem WHY IT DOES NOT JUST CALL C:\N8RO\dev\setup-dev.cmd, WHICH ALREADY DOES THIS. That would be
rem one line, and it would be wrong. That script requires the N8RO install by construction, so
rem inheriting it would make `tests\build.cmd` - which links nothing, needs no install, and is the
rem whole of this project's zero-install claim - depend on C:\N8RO being present. The three
rem SDK-free tools and the test suite must build on a machine that has never seen the SDK. That is
rem not a convenience; it is the property those four build scripts exist to demonstrate. The
rem vswhere query shape below is deliberately the same as setup-dev.cmd's, so the discovery agrees
rem with the platform's own even though the dependency does not.
rem
rem TWO TIERS, AND THE SPLIT IS THE POINT
rem
rem   call find-vcvars.cmd sdk    Require Visual Studio 18.x, matching the toolset C:\N8RO 2.1.328
rem                               was built with. Used by the four builds that link n8ro-sim.lib
rem                               and n8ro-core.lib: m1-run, n8ro-campaign, spike-axis, spike-oq4.
rem                               Narrow on purpose - mixing toolsets across an import library is
rem                               a class of bug this project should not be discovering at 2am.
rem
rem   call find-vcvars.cmd free   Accept 17.x or 18.x. Used by n8ro-capture, n8ro-compare,
rem                               n8ro-judge and tests\build.cmd. They link nothing and need only
rem                               a C++17 compiler, so there is no toolset to match. This is
rem                               insurance rather than a requirement, and the distinction is
rem                               worth keeping straight: the windows-latest image the CI job
rem                               uses carries 18.x, so the job would pass without the widening.
rem                               What it buys is every machine that has 17.x and not 18.x -
rem                               an older runner image, or an evaluator on stock VS 2022, who
rem                               would otherwise be told to install a second Visual Studio to
rem                               build four targets that need no toolset in particular.
rem                               VERIFIED, not assumed: EXT17_VCVARS pinned at VS 2022 17.14
rem                               builds the free tier with cl 19.44.
rem
rem Set EXT17_VCVARS to a vcvars64.bat path to override the scan entirely. That is how the 17.x
rem path was verified locally on a machine that also has 18.x, and how CI pins a toolchain.
rem
rem NOT `setlocal`, on purpose: the whole reason to call this is the environment vcvars64.bat sets,
rem and a `setlocal` here would discard it on the way out.
rem
rem EXT17_VS_WANT IS ECHOED INSIDE AN `if (...)` BLOCK, SO IT MUST NEVER CONTAIN A PARENTHESIS.
rem cmd expands it while reading the block, so a `)` in the value closes the block early: the
rem diagnostic then prints its last three lines unconditionally and swallows its first two, which
rem is precisely as confusing as it sounds. It cost a debugging round here; the dash in the
rem "free" description below is that, and not a style choice.

if not "%EXT17_VCVARS%"=="" (
  if not exist "%EXT17_VCVARS%" (
    echo BUILD FAILED: EXT17_VCVARS is set to
    echo   "%EXT17_VCVARS%"
    echo which does not exist. Unset it to fall back to the vswhere scan.
    exit /b 1
  )
  call "%EXT17_VCVARS%" >nul
  if errorlevel 1 (
    echo BUILD FAILED: vcvars64.bat from EXT17_VCVARS reported an error.
    exit /b 1
  )
  exit /b 0
)

if /i "%~1"=="sdk"  goto :scan_sdk
if /i "%~1"=="free" goto :scan_free
echo BUILD FAILED: find-vcvars.cmd expects "sdk" or "free" as its first argument, got "%~1".
echo This is a defect in the build script that called it, not in the caller's environment.
exit /b 1

rem --- The scan ---------------------------------------------------------------------------------
rem -prerelease so an Insiders install still counts; -latest so a machine carrying several picks
rem one deterministically rather than by enumeration order.
rem
rem THE TWO LOOPS BELOW ARE NEARLY IDENTICAL AND THAT IS NOT AN OVERSIGHT. The version range has
rem to appear INLINE, with its comma escaped as ^, and with the vswhere path as the only quoted
rem token on the line. Both constraints were measured rather than assumed:
rem
rem   -version "%RANGE%"   fails - a second quoted token defeats cmd /c's first/last-quote
rem                        stripping, and the line dies with 'C:\Program' is not recognized
rem   -version %~1         fails - inside a `for /f` backquote the comma is re-parsed as an
rem                        argument separator, so vswhere sees [17.0 and 18.99] and answers
rem                        "Error 0x57: Argument expected"
rem
rem So the range cannot be passed in as a parameter or held in a variable, and factoring these
rem two lines into one would silently search a nonsense range rather than fail loudly.
rem C:\N8RO\dev\setup-dev.cmd duplicates its query for the same reason and says so in one line;
rem this says it in ten because the failure is silent and the next person to tidy it will not
rem otherwise know why it is written this way.

:scan_sdk
set "EXT17_VS_WANT=18.x, to match the toolset the N8RO SDK at C:\N8RO was built with"
call :vswhere_present
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%I in (`"%EXT17_VSWHERE%" -latest -prerelease -products * -version [18.0^,18.99] -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "EXT17_VS_DIR=%%I"
goto :have_scan

:scan_free
set "EXT17_VS_WANT=17.x or 18.x - this target links nothing and needs only a C++17 compiler"
call :vswhere_present
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%I in (`"%EXT17_VSWHERE%" -latest -prerelease -products * -version [17.0^,18.99] -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "EXT17_VS_DIR=%%I"
goto :have_scan

:have_scan
if "%EXT17_VS_DIR%"=="" (
  echo BUILD FAILED: no Visual Studio with the C++ x64 toolset was found.
  echo   Wanted:  %EXT17_VS_WANT%
  echo   Needs:   the "Desktop development with C++" workload
  echo            ^(component Microsoft.VisualStudio.Component.VC.Tools.x86.x64^)
  echo   Or:      set EXT17_VCVARS to a vcvars64.bat path to skip this scan entirely.
  goto :vsfail
)
if not exist "%EXT17_VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
  echo BUILD FAILED: found a Visual Studio install at
  echo   "%EXT17_VS_DIR%"
  echo but it has no VC\Auxiliary\Build\vcvars64.bat. The C++ x64 tools are not installed there.
  goto :vsfail
)
call "%EXT17_VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
  echo BUILD FAILED: vcvars64.bat reported an error for
  echo   "%EXT17_VS_DIR%"
  goto :vsfail
)
call :cleanup
exit /b 0

:vsfail
call :cleanup
exit /b 1

:cleanup
set "EXT17_VS_DIR="
set "EXT17_VS_WANT="
set "EXT17_VSWHERE="
exit /b 0

rem --- Locating vswhere ---------------------------------------------------------------------
:vswhere_present
set "EXT17_VS_DIR="
set "EXT17_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%EXT17_VSWHERE%" exit /b 0
echo BUILD FAILED: vswhere.exe not found at
echo   "%EXT17_VSWHERE%"
echo Every Visual Studio installer since 2017 provides it. Install Visual Studio with the
echo "Desktop development with C++" workload, or set EXT17_VCVARS to a vcvars64.bat path.
exit /b 1
