# Changelog

All notable changes to FilterKeys Setter. The entry for the current version
is also the text of its GitHub release; keep the two in step when publishing
(see [docs/RELEASE.md](docs/RELEASE.md)).

## 1.11 -- 2026-10-07

FilterKeys Setter 1.11 — the first release since 1.10, and a large one.

**No FilterKeys or registry behaviour was changed.** The `SPI_*` calls, the `FKF_*` flag handling, the reads from `Control Panel\Accessibility\Keyboard Response` and the five presets work exactly as before. The theme preference lives in its own key, `HKCU\Software\FilterKeysSetter\Theme`.

### What is new

- **Dark theme** built from the application icon palette, switchable to the native light look; the choice is remembered. Windows high contrast switches it off entirely, live.
- **Sliders** for repeat delay and repeat rate, in sync with the numeric fields; the edit box stays authoritative outside the slider range.
- **A status line** showing what Windows is actually using, which is not necessarily what the fields contain.
- **Tool tips on 28 controls**, including the three read-outs whose meaning is least obvious.
- **Alt accelerators on all 22 interactive controls** — the dialog previously had none at all.
- **Segoe UI 9 pt** instead of the Windows XP era `MS Shell Dlg`, system DPI awareness, and a new application icon and logo.

### Bugs fixed

- Three Win32 reads whose failure was not checked. One of them followed its own error box by silently zeroing every field; another offered to "restore" FilterKeys off with all timings at 0; the third presented invented values as the Windows keyboard settings.
- `GetStringRegKey()` assumed `RegQueryValueEx` null terminates its result, which it does not promise, and read from an uninitialised buffer.
- The dialog members were never initialised — the first `UpdateData(FALSE)` ran on indeterminate memory.
- The manifest declared `processorArchitecture="X86"` and a duplicate common-controls dependency; it was also referenced by nothing.
- The installer was a parallel install rather than an upgrade, and packaged seven UCRT forwarder DLLs without the `ucrtbase.dll` they forward to.
- The project hard-coded platform toolset `v143`, which fails with `MSB8020` under Visual Studio 2026.

### For maintainers

- `tools\build.cmd [Win32|x86|x64] [Debug|Release] [toolset]` finds MSBuild
  through `vswhere`, warns when the MFC component is missing and writes
  `build-<configuration>-<platform>.log`. It builds the `.vcxproj`, not the
  solution: the solution calls the 32-bit platform `x86` while the project
  calls it `Win32`, so `msbuild FilterKeysSetter.sln /p:Platform=Win32`
  fails with `MSB4126`.
- `tools\release.cmd [tag]` reads the version out of the `.rc`, builds both
  platforms, copies them to `release\`, prints sizes and SHA-256 sums and,
  given a tag, uploads them. The procedure is in
  [docs/RELEASE.md](docs/RELEASE.md).
- Five static checkers run on any machine with Python -- dialog layout,
  message maps, resources, error handling and project files -- and
  `tools\selftest.py` verifies them by breaking the repository in 32 ways
  and insisting each break is caught.

### Building it yourself

```cmd
tools\build.cmd                 :: Release for Win32 and x64
tools\build.cmd x64 Release v143 :: pin a toolset
```

Requires Visual Studio 2022 or 2026 with the MFC component (*C++ MFC for latest v143/v145 build tools*), which is not part of the C++ workload by default.

## 1.10 -- 2024-01-28

No functional changes; rebuilt with Visual Studio 2022.

## 1.02 -- 2013-10-30

Soarer's release.
