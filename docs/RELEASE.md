# Cutting a release

Everything here needs Windows with Visual Studio; nothing in this file can be
done from the static checks alone.

## 1. Check the tree

```cmd
python tools\check-all.py
```

That is the eight checkers and the self test, ending in
`all 9 checks passed`. Anything else is a reason not to cut the release.

`check-resources.py` is the one that matters most here: it compares the
version in `VERSIONINFO`, the about box, the installer `ProductVersion` and
the newest README history entry, and fails when any of the four disagree.
The version lives in these places:

| File | What to change |
| --- | --- |
| `FilterKeysSetter.rc` | `FILEVERSION`, `PRODUCTVERSION`, and the `FileVersion` / `ProductVersion` strings |
| `FilterKeysSetter.rc` | the about box caption `FilterKeysSetter Version 1.11` |
| `FilterKeysSetter.Setup\FilterKeysSetter.Setup.vdproj` | `"ProductVersion" = "8:1.0.11"` |
| `README.md` | a new line at the top of *Version history* |
| `CHANGELOG.md` | a new `## <version> -- <date>` section; its text is what the GitHub release says |

A new `ProductVersion` in the installer also needs a **new `ProductCode`**
GUID, with `UpgradeCode` left alone -- that pair is what turns the next MSI
into an upgrade rather than a second parallel installation.

## 2. Build and collect

```cmd
tools\release.cmd
```

It builds `Release` for `Win32` and `x64`, copies both executables to
`release\FilterKeysSetter-<version>-<platform>.exe`, prints their size and
SHA-256, and picks up `FilterKeysSetter.msi` if the setup project has been
built in Visual Studio (MSBuild alone cannot build a `.vdproj`).

The installer needs the Installer Projects extension. It is not part of the
Visual Studio Installer catalogue -- it comes from the Marketplace as a
`.vsix`, and installing it asks for elevation. With it in place the setup
project builds without opening the IDE:

```cmd
devenv FilterKeysSetter.sln /Build "Release|x64" /Project FilterKeysSetter.Setup /ProjectConfig "Release"
```

Both `/Project` and `/ProjectConfig` are needed: no solution configuration
builds the setup project, so without them devenv skips it and still reports
success. The binary it packages is `..\x64\Release\FilterKeysSetter.exe`, so
that configuration has to be built first.

Default output locations, in case they are needed by hand:

| Platform | Executable |
| --- | --- |
| Win32 | `Release\FilterKeysSetter.exe` |
| x64 | `x64\Release\FilterKeysSetter.exe` |

## 3. Smoke test

The full pass is [TESTING.md](TESTING.md); for a release, run both binaries
once, on a machine other than the one that built them if possible, and
check:

- the dialog opens in the dark theme and the *Dark theme* check box switches
  it, surviving a restart
- `Alt` shows the accelerator underlines, and `Alt`+`K`, `Alt`+`P`, `Alt`+`E`
  reach *Keyboard*, *Apply* and *Current*
- the five *Load settings* buttons fill plausible values, *Apply* changes the
  repeat rate noticeably in the test area, and the status line follows
- high contrast (left `Alt`+left `Shift`+`Print screen`) forces the light
  theme and disables the check box

## 4. Publish

```cmd
tools\release.cmd v1.11
```

With a tag, the script uploads the executables to that release with the
GitHub CLI; drafts work too.

## Without a Windows machine

With `docs/workflows/release.yml` installed under `.github/workflows/`, none
of the build steps below have to happen locally: open the **Actions** tab,
pick **Release**, **Run workflow**, and give it the tag (`v1.11`). The two
executables are built on `windows-2022` and attached to that release, draft
or not. [docs/workflows/README.md](workflows/README.md) explains why those
files cannot be committed by the usual automation.

The local route below stays useful for testing a build before tagging.

The release notes are the `CHANGELOG.md` section for that version:

```cmd
gh release edit v1.11 --notes-file notes.md
```

They should name the version, what
changed, and that the FilterKeys and registry behaviour is unchanged.

The release can also be published first and given its executables afterwards.
Without `gh`, a `POST` to
`https://uploads.github.com/repos/<owner>/<repo>/releases/<id>/assets?name=<file>`
does the same job, with a token from `git credential fill`, and a `PATCH` on
`https://api.github.com/repos/<owner>/<repo>/releases/<id>` replaces the notes.
The 1.11 release, published on 14 October 2026, carries both executables since
17 October 2026.
