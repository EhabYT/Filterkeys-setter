# Workflows

These two files belong in `.github/workflows/`. They are kept here as well
because the automation that maintains this repository pushes with a token
that has no `workflow` scope: any commit touching `.github/workflows/` is
rejected with

```
refusing to allow an OAuth App to create or update workflow
`.github/workflows/build.yml` without `workflow` scope
```

Keeping the copies under `docs/` means the files are still reviewed, diffed
and version controlled rather than living only in someone's clipboard.

## Installing them

In the browser: **Add file -> Create new file**, name it
`.github/workflows/build.yml`, paste the contents of
[build.yml](build.yml), commit. Repeat for
[release.yml](release.yml). From a clone with normal credentials:

```cmd
mkdir .github\workflows
copy docs\workflows\*.yml .github\workflows\
git add .github\workflows
git commit -m "Add the build and release workflows"
git push
```

There is also an inherited `.github/workflows/cmake-single-platform.yml`
from the GitHub starter templates. It runs CMake against a project that has
no `CMakeLists.txt`, so it has never done anything but fail; delete it.

## What they do

| File | Trigger | What happens |
| --- | --- | --- |
| `build.yml` | push, pull request, manual | Static checks on Ubuntu, then `Release` and `Debug` builds of `Win32` and `x64` on both the Visual Studio 2022 and the Visual Studio 2026 runner image. Proves the project still builds with either toolset. |
| `release.yml` | manual with a tag, or a pushed `v*` tag | Static checks, then `Release` builds of `Win32` and `x64` on `windows-2022`, named `FilterKeysSetter-<version>-<platform>.exe` and attached to the release with `gh release upload --clobber`. |

`release.yml` is the one that removes the need for a Windows machine: with
it installed, the existing `v1.11` draft can be filled in from the Actions
tab. Pick **Release**, **Run workflow**, enter `v1.11`, and the two
executables appear on the draft a few minutes later. Then press **Publish
release**.

Two details worth knowing:

- Both workflows build `FilterKeysSetter.vcxproj`, never the solution. The
  solution calls the 32-bit platform `x86` while the project calls it
  `Win32`, so `msbuild FilterKeysSetter.sln /p:Platform=Win32` fails with
  `MSB4126`.
- The version in the asset names is read out of `FilterKeysSetter.rc`, not
  taken from the tag, so a mistyped tag cannot mislabel a binary. The
  checkers have already confirmed that the resource script, the about box,
  the installer, the README and the changelog agree on that number.

`tools\release.cmd` does the same job locally and stays useful for testing a
build before tagging anything; see [../RELEASE.md](../RELEASE.md).
