@echo off
setlocal enabledelayedexpansion
rem ---------------------------------------------------------------------------
rem release.cmd -- build both Release binaries, collect them under release\,
rem print their sizes and SHA-256 hashes, and optionally upload them to a
rem GitHub release.
rem
rem   tools\release.cmd            build and collect only
rem   tools\release.cmd v1.11      ... and upload the files to that release
rem
rem Uploading needs the GitHub CLI (gh) and an account with write access. The
rem release may be a draft; gh uploads to drafts as well.
rem ---------------------------------------------------------------------------

set "TAG=%~1"

pushd "%~dp0.."

rem -- version, straight out of the resource script -----------------------
rem The line reads:  VALUE "FileVersion", "1.0.11.0"
rem Take everything after the comma, drop the blanks, then strip the quotes by
rem position -- substituting a quote inside a quoted SET is a known trap.
set "RAW="
for /f "tokens=2 delims=," %%v in ('findstr /c:"VALUE \"FileVersion\"" FilterKeysSetter.rc') do (
    if not defined RAW set "RAW=%%v"
)
set "RAW=%RAW: =%"
set "VERSION=%RAW:~1,-1%"
if not defined VERSION set "VERSION=unknown"
echo Version       : %VERSION%
echo.

rem -- build ---------------------------------------------------------------
call "%~dp0build.cmd" Win32 Release
if errorlevel 1 goto :buildfailed
call "%~dp0build.cmd" x64 Release
if errorlevel 1 goto :buildfailed

rem -- collect -------------------------------------------------------------
set "WIN32EXE=Release\FilterKeysSetter.exe"
set "X64EXE=x64\Release\FilterKeysSetter.exe"

if not exist "%WIN32EXE%" (
    echo [!] %WIN32EXE% was not produced.
    goto :fail
)
if not exist "%X64EXE%" (
    echo [!] %X64EXE% was not produced.
    goto :fail
)

if not exist "release" mkdir "release"
copy /y "%WIN32EXE%" "release\FilterKeysSetter-%VERSION%-win32.exe" >nul
copy /y "%X64EXE%" "release\FilterKeysSetter-%VERSION%-x64.exe" >nul

echo.
echo === Release files ============================================
for %%f in ("release\FilterKeysSetter-%VERSION%-*.exe") do (
    echo %%~nxf  %%~zf bytes
    certutil -hashfile "%%f" SHA256 | findstr /v ":" | findstr /r "[0-9a-f]"
)
echo.

rem -- the installer, if it was built --------------------------------------
if exist "FilterKeysSetter.Setup\Release\FilterKeysSetter.msi" (
    copy /y "FilterKeysSetter.Setup\Release\FilterKeysSetter.msi" "release\FilterKeysSetter-%VERSION%.msi" >nul
    echo Installer     : release\FilterKeysSetter-%VERSION%.msi
) else (
    echo Installer     : not built -- open the solution in Visual Studio with the
    echo                 Installer Projects extension and build FilterKeysSetter.Setup.
)
echo.

rem -- upload --------------------------------------------------------------
if not defined TAG (
    echo No tag given, nothing uploaded. Pass one to upload, for example:
    echo     tools\release.cmd v1.11
    goto :done
)

where gh >nul 2>&1
if errorlevel 1 (
    echo [!] The GitHub CLI ^(gh^) was not found; upload skipped.
    echo     Files are ready in release\.
    goto :done
)

echo Uploading to release %TAG% ...
gh release upload %TAG% "release\FilterKeysSetter-%VERSION%-win32.exe" "release\FilterKeysSetter-%VERSION%-x64.exe" --clobber
if errorlevel 1 (
    echo [!] Upload failed. The files are still in release\.
    goto :fail
)
echo Uploaded.

:done
popd
exit /b 0

:buildfailed
echo [!] The build failed; see build-Release-*.log.
popd
exit /b 1

:fail
popd
exit /b 1
