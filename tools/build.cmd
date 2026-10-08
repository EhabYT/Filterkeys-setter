@echo off
setlocal enabledelayedexpansion
rem ---------------------------------------------------------------------------
rem build.cmd -- find the installed Visual Studio, build FilterKeysSetter and
rem print a short diagnostics summary.
rem
rem Works with Visual Studio 2022 (toolset v143) and Visual Studio 2026 (v145);
rem the project picks the toolset up from whichever one this script finds.
rem
rem   tools\build.cmd              build Release for Win32 and x64
rem   tools\build.cmd x64          build Release for x64 only
rem   tools\build.cmd x64 Debug    build Debug for x64 only
rem   tools\build.cmd x64 Debug v143   pin a toolset when several are present
rem ---------------------------------------------------------------------------

set "PLATFORMS=Win32 x64"
if not "%~1"=="" set "PLATFORMS=%~1"
set "CONFIGURATION=Release"
if not "%~2"=="" set "CONFIGURATION=%~2"

rem The solution calls the 32-bit platform x86, the project calls it Win32.
rem This script builds the project, so accept both spellings.
if /i "%PLATFORMS%"=="x86" set "PLATFORMS=Win32"
if /i "%PLATFORMS%"=="win32" set "PLATFORMS=Win32"
if /i "%PLATFORMS%"=="x64" set "PLATFORMS=x64"

for %%p in (%PLATFORMS%) do (
    if /i not "%%p"=="Win32" if /i not "%%p"=="x64" (
        echo [!] Unknown platform "%%p". The project knows Win32 and x64.
        exit /b 1
    )
)
if /i not "%CONFIGURATION%"=="Debug" if /i not "%CONFIGURATION%"=="Release" (
    echo [!] Unknown configuration "%CONFIGURATION%". Use Debug or Release.
    exit /b 1
)

pushd "%~dp0.."

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] vswhere.exe not found. Is Visual Studio installed?
    echo     Expected at "%%ProgramFiles(x86)%%\Microsoft Visual Studio\Installer\vswhere.exe".
    goto :fail
)

set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath 2^>nul`) do (
    if not defined VSDIR set "VSDIR=%%i"
)
if not defined VSDIR (
    echo [!] No Visual Studio installation with MSBuild was found.
    goto :fail
)

set "VSNAME="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property displayName 2^>nul`) do (
    if not defined VSNAME set "VSNAME=%%i"
)

set "MSBUILD="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    echo [!] MSBuild.exe was not found inside "%VSDIR%".
    goto :fail
)

rem MFC is not part of the "Desktop development with C++" workload, and its
rem absence is the single most common failure here: MSBuild stops with
rem
rem   error MSB8041: MFC libraries are required for this project. Install them
rem   from the Visual Studio Installer (Individual components tab) for any
rem   toolsets and architectures being used.
rem
rem That check inside Microsoft.CppBuild.targets looks for exactly one file:
rem   $(VCToolsInstallDir)atlmfc\lib\$(PlatformShortName)\mfcs140.lib
rem so this script looks for the same file and says which architecture is
rem missing before MSBuild has to. Component ids are not probed for that:
rem a side-by-side MFC under a versioned id is just as usable, and the file
rem is the thing that actually decides.
set "VCTOOLSVER="
if exist "%VSDIR%\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt" (
    for /f "usebackq delims=" %%v in ("%VSDIR%\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt") do (
        if not defined VCTOOLSVER set "VCTOOLSVER=%%v"
    )
)
set "VCTOOLSDIR="
if defined VCTOOLSVER (
    set "VCTOOLSVER=!VCTOOLSVER: =!"
    set "VCTOOLSDIR=%VSDIR%\VC\Tools\MSVC\!VCTOOLSVER!"
)

set "MFCMISSING="
set "MFCFOUND="
if defined VCTOOLSDIR (
    for %%p in (%PLATFORMS%) do (
        set "SHORT=x86"
        if /i "%%p"=="x64" set "SHORT=x64"
        if exist "!VCTOOLSDIR!\atlmfc\lib\!SHORT!\mfcs140.lib" (
            set "MFCFOUND=!MFCFOUND! !SHORT!"
        ) else (
            set "MFCMISSING=!MFCMISSING! !SHORT!"
        )
    )
)

echo.
echo Visual Studio : %VSNAME%
echo Path          : %VSDIR%
echo MSBuild       : %MSBUILD%
if not defined VCTOOLSDIR (
    echo MFC           : not checked ^(no MSVC toolset folder under "%VSDIR%"^)
) else if defined MFCMISSING (
    echo MSVC toolset  : !VCTOOLSVER!
    echo MFC           : MISSING for!MFCMISSING!
    echo.
    echo     MSBuild would stop with error MSB8041. It looks for
    echo         !VCTOOLSDIR!\atlmfc\lib\^<arch^>\mfcs140.lib
    echo     and that file is not there for the architecture^(s^) above.
    echo.
    echo     Open the Visual Studio Installer, press Modify, open the
    echo     "Individual components" tab, search for MFC and tick
    echo         "C++ MFC for latest v143 build tools (x86 & x64)"   ^(VS 2022^)
    echo         "C++ MFC for latest v145 build tools (x86 & x64)"   ^(VS 2026^)
    echo     One component covers both x86 and x64. If the project is built
    echo     with Spectre mitigations, tick the matching Spectre variant too.
    echo.
    echo     Or from an elevated command prompt, in one go:
    echo         "%%ProgramFiles(x86)%%\Microsoft Visual Studio\Installer\setup.exe" ^^
    echo             modify --installPath "%VSDIR%" ^^
    echo             --add Microsoft.VisualStudio.Component.VC.ATLMFC ^^
    echo             --quiet --norestart
    echo.
    echo     The repository also carries a .vsconfig listing this component;
    echo     Visual Studio offers to install what is missing when the solution
    echo     is opened. See docs\MFC.md.
    echo.
    goto :fail
) else (
    echo MSVC toolset  : !VCTOOLSVER!
    echo MFC           : installed for!MFCFOUND!
)
echo.

rem The project is built directly instead of the solution: FilterKeysSetter.Setup
rem is a .vdproj and needs the Visual Studio Installer Projects extension, which
rem has nothing to do with whether the application itself compiles.
set "TOOLSET="
if not "%~3"=="" set "TOOLSET=/p:PlatformToolset=%~3"
if defined TOOLSET echo Toolset       : %~3

set "FAILED="
for %%p in (%PLATFORMS%) do (
    echo === %CONFIGURATION% ^| %%p ===========================================
    "%MSBUILD%" FilterKeysSetter.vcxproj /nologo /m /t:Rebuild /p:Configuration=%CONFIGURATION% /p:Platform=%%p !TOOLSET! /verbosity:minimal /fileLogger "/fileLoggerParameters:LogFile=build-%CONFIGURATION%-%%p.log;Verbosity=normal"
    if errorlevel 1 (
        set "FAILED=1"
        echo [!] %CONFIGURATION% ^| %%p FAILED
    ) else (
        echo [ok] %CONFIGURATION% ^| %%p
    )
    echo.
)

echo === Diagnostics ==============================================
set "FOUND="
for %%p in (%PLATFORMS%) do (
    if exist "build-%CONFIGURATION%-%%p.log" (
        for /f "usebackq delims=" %%l in (`findstr /i /r /c:" error [A-Z]" /c:" warning [A-Z]" "build-%CONFIGURATION%-%%p.log"`) do (
            set "FOUND=1"
            echo %%p: %%l
        )
    )
)
if not defined FOUND echo No errors or warnings.
echo.
echo Full logs: build-%CONFIGURATION%-*.log
echo.

popd
if defined FAILED exit /b 1
exit /b 0

:fail
popd
exit /b 1
