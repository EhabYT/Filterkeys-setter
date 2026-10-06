<p align="center">
  <img src="res/logo.png" alt="FilterKeys Setter logo" width="160" height="160">
</p>

<h1 align="center">Filterkeys setter</h1>

<p align="center">
  <a href="https://github.com/EhabYT/Filterkeys-setter/actions/workflows/build.yml"><img src="https://github.com/EhabYT/Filterkeys-setter/actions/workflows/build.yml/badge.svg" alt="Build status"></a>
</p>

Original version 1.02 from Soarer's article [FilterKeys Setter... for a faster key repeat (in Windows)](https://geekhack.org/index.php?topic=41881.0).

# Version history
* 1.11 2026-10-06 Dark theme with repeat sliders, tooltips for every setting,
  Segoe UI dialog font, system DPI awareness, new application icon and logo,
  CI build workflow for Win32 and x64
* 1.10 2024-01-28 No changes - Visual Studio 2022 build
* 1.02 2013-10-30 Soarer's release

# Appearance

The dialog ships with a dark theme built from the palette of the application
icon (`#0A2342` background, `#1E5A9E` gradient, `#4B9BEE` accent). Clear the
*Dark theme* check box in the *Appearance* group to fall back to the native
light look; the choice is remembered in
`HKEY_CURRENT_USER\Software\FilterKeysSetter\Theme`.

The repeat delay and repeat rate can be dialled in with sliders as well as
typed as exact millisecond values -- the two stay in sync, and the edit box
remains authoritative for values outside the slider range.

# Building

The project is an MFC desktop application built with the Visual Studio 2022 toolset (`v143`).

* **Visual Studio:** open `FilterKeysSetter.sln` and build the `Release` configuration for `x86` or `x64`.
  Requires the *Desktop development with C++* workload including *MFC for latest v143 build tools*.
* **Command line:**

  ```cmd
  msbuild FilterKeysSetter.sln /p:Configuration=Release /p:Platform=x64
  ```

Every push and pull request is built for both `Win32` and `x64` by the
[build workflow](.github/workflows/build.yml), which publishes the resulting
`FilterKeysSetter.exe` as a downloadable artifact.

# Usage:
![Sample usage screenshot](https://geekhack.org/index.php?action=dlattach;topic=41881.0;attach=17471;image)
