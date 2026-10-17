# MSB8041: MFC libraries are required for this project

```
error MSB8041: MFC libraries are required for this project. Install them from
the Visual Studio Installer (Individual components tab) for any toolsets and
architectures being used.
```

This is the most common way a build of FilterKeys Setter fails, and it is
not a fault in the project. The program *is* an MFC application
(`UseOfMfc` is `Dynamic`), and **MFC is not part of the "Desktop development
with C++" workload** -- it has to be ticked separately. The error says
nothing about which architecture is missing, which is what makes it
annoying.

## What MSBuild actually checks

`Microsoft.CppBuild.targets` looks for a single file:

```
$(VCToolsInstallDir)atlmfc\lib\$(_SpectreLibsDir)$(PlatformShortName)\mfcs140.lib
```

- `$(VCToolsInstallDir)` is something like
  `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\`
- `$(PlatformShortName)` is `x86`, `x64`, `arm`, `arm64` or `arm64EC` --
  note that the `Win32` platform looks in **`x86`**
- `$(_SpectreLibsDir)` is empty, or `spectre\` when the project is built
  with Spectre mitigations

If that file is absent, the build stops. `tools\build.cmd` now looks for the
very same file before calling MSBuild and names the missing architecture, so
the message says what to do instead of what went wrong.

## The fix

### In the installer, by hand

1. Close Visual Studio and open the **Visual Studio Installer**.
2. **Modify** next to your installation, then the **Individual components**
   tab.
3. Type `MFC` into the search box.
4. Tick the component matching your toolset:
   - Visual Studio 2022 (v143): *C++ MFC for latest v143 build tools
     (x86 & x64)*
   - Visual Studio 2026 (v145): *C++ MFC for latest v145 build tools
     (x86 & x64)*

   On a German installation the same two components read:

   | Visual Studio | Component |
   | --- | --- |
   | 2026 (v145) | *C++-MFC für die neuesten Buildtools v145 (x86 und x64)* |
   | 2022 (v143) | *C++-MFC für die neuesten Buildtools v143 (x86 und x64)* |

   One component covers **both** x86 and x64, which is what this project
   needs. Only tick a *with Spectre Mitigations* variant if you build with
   Spectre mitigations on, and only tick a versioned component
   (*C++ v14.xx MFC …*) if you pin that exact toolset.
5. **Modify** to install.

### From the command line

```cmd
"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe" ^
    modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" ^
    --add Microsoft.VisualStudio.Component.VC.ATLMFC ^
    --quiet --norestart
```

Run it from an elevated prompt, and **not** from the installer's own
directory -- it refuses that. `--installPath` must match your installation
and must not end in a backslash; `tools\build.cmd` prints the path it found
as *Path*. The component id `Microsoft.VisualStudio.Component.VC.ATLMFC` is
the same in Visual Studio 2022 and 2026; only its display name changes with
the toolset.

### Letting Visual Studio offer it

The repository carries a [`.vsconfig`](../.vsconfig) listing the three
components this project needs. Visual Studio reads it when the solution is
opened and offers to install whatever is missing, which is the least
error-prone route of the three.

## Checking that it worked

Re-run the build script. It prints what it found before it starts:

```
MSVC toolset  : 14.50.35904
MFC           : installed for x86 x64
```

The toolset version is read from
`VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`, and if that file
is missing or names a toolset that is not on disk, from the newest folder
under `VC\Tools\MSVC`. A `14.5x` version means v145, Visual Studio 2026;
`14.4x` means v143, Visual Studio 2022.

## More than one toolchain, only one with MFC

MFC is per installation and per toolset, so a machine with Visual Studio 2026
for editing and a Build Tools 2022 that carries MFC is a normal setup rather
than a broken one. The same goes for a Build Tools the Visual Studio Installer
never registered -- `vswhere` cannot report those at all.

`tools\build.cmd` handles both: when the newest installation lacks MFC for a
requested platform it looks for the same `mfcs140.lib` in the remaining
installations and builds with the first one that has it for *every*
requested platform, printing which toolchain it settled on:

```
Visual Studio : Build Tools at C:\VS_BuildTools
Path          : C:\VS_BuildTools
MSBuild       : C:\VS_BuildTools\MSBuild\Current\Bin\MSBuild.exe
MSVC toolset  : 14.44.35207
MFC           : installed for x86 x64
```

Probed in that order: every installation `vswhere -all` reports, then the
usual names of an unregistered Build Tools (`%ProgramFiles(x86)%\BuildTools`,
`%ProgramFiles%\BuildTools`, `%SystemDrive%\VS_BuildTools`,
`%SystemDrive%\BuildTools`). Nothing is installed or modified -- the script
only selects a different MSBuild, and only when the default one cannot build
the project.

Two consequences worth knowing:

- The IDE keeps its own, MFC-less installation, so *Build* inside Visual
  Studio still fails with MSB8041. Add the component there if you want that
  to work too; the command line above is the shortcut.
- A pinned toolset has to exist in the toolchain the fallback picked:
  `tools\build.cmd x64 Release v143` asks for v143, which a v145-only
  installation does not have.

## It is installed and the error persists

- **Wrong architecture.** Building `Win32` needs `atlmfc\lib\x86`, not
  `x64`. A versioned MFC component can cover only one of them.
- **Wrong toolset.** MFC is per toolset. With several MSVC versions side by
  side, the one the build picks may not be the one that has MFC. Check what
  `tools\build.cmd` prints as *MSVC toolset*, and pin a toolset explicitly:
  `tools\build.cmd x64 Release v143`.
- **Spectre mitigations.** They need their own MFC component, found under
  `atlmfc\lib\spectre\`.
- **Stale installation.** Untick the component, apply, tick it again, apply.
  That genuinely fixes cases where the files were half removed.

## Build servers

GitHub's `windows-2022` runner image ships MFC for v143, so
`docs/workflows/release.yml` builds without extra steps. On an image or
build agent that does not, add the component before building:

```yaml
- name: Install MFC
  run: >
    "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe"
    modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
    --add Microsoft.VisualStudio.Component.VC.ATLMFC
    --quiet --norestart --nocache
```

There is no way around it: MFC cannot be vendored into the repository, and
without it this project does not build.
