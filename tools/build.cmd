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
rem ---------------------------------------------------------------------------

set "PLATFORMS=Win32 x64"
if not "%~1"=="" set "PLATFORMS=%~1"
set "CONFIGURATION=Release"
if not "%~2"=="" set "CONFIGURATION=%~2"

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

rem MFC is not part of the "Desktop development with C++" workload. Without it
rem the build dies at afxwin.h, which is the single most common failure here.
set "MFCDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath 2^>nul`) do (
    if not defined MFCDIR set "MFCDIR=%%i"
)

echo.
echo Visual Studio : %VSNAME%
echo Path          : %VSDIR%
echo MSBuild       : %MSBUILD%
if defined MFCDIR (
    echo MFC           : installed
) else (
    echo MFC           : NOT DETECTED
    echo.
    echo     The MFC component is missing or vswhere cannot see it. If the build
    echo     fails with "Cannot open include file: 'afxwin.h'", open the Visual
    echo     Studio Installer, pick Modify, Individual components, and add
    echo     "C++ MFC for latest v145 build tools" ^(VS 2026^) or
    echo     "C++ MFC for latest v143 build tools" ^(VS 2022^).
)
echo.

rem The project is built directly instead of the solution: FilterKeysSetter.Setup
rem is a .vdproj and needs the Visual Studio Installer Projects extension, which
rem has nothing to do with whether the application itself compiles.
set "FAILED="
for %%p in (%PLATFORMS%) do (
    echo === %CONFIGURATION% ^| %%p ===========================================
    "%MSBUILD%" FilterKeysSetter.vcxproj /nologo /m /t:Rebuild /p:Configuration=%CONFIGURATION% /p:Platform=%%p /verbosity:minimal /fileLogger "/fileLoggerParameters:LogFile=build-%CONFIGURATION%-%%p.log;Verbosity=normal"
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
