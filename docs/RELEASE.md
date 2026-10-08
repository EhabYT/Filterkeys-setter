# Cutting a release

Everything here needs Windows with Visual Studio; nothing in this file can be
done from the static checks alone.

## 1. Check the tree

```cmd
python tools\check-dialog-layout.py
python tools\check-message-map.py
python tools\check-resources.py
python tools\check-error-handling.py
python tools\check-project.py
python tools\selftest.py
```

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

Default output locations, in case they are needed by hand:

| Platform | Executable |
| --- | --- |
| Win32 | `Release\FilterKeysSetter.exe` |
| x64 | `x64\Release\FilterKeysSetter.exe` |

## 3. Smoke test

Run both binaries once, on a machine other than the one that built them if
possible, and check:

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
GitHub CLI; drafts work too. The release notes are the `CHANGELOG.md` section for that version:

```cmd
gh release edit v1.11 --notes-file notes.md
```

They should name the version, what
changed, and that the FilterKeys and registry behaviour is unchanged.

A draft release for 1.11 already exists in the repository, with notes, and is
waiting for the two executables.
