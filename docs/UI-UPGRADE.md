# UI upgrade — FilterKeys Setter 1.10 → 1.11

Reference for the dialog rework: what changed, how to verify it, and how to get
back if something turns out wrong.

## Scope

The application is **C++ / MFC**, not WinForms or WPF, so the upgrade is built
from `WM_CTLCOLOR*` handlers, `SetWindowTheme`, custom draw and GDI — there are
no styles, resource dictionaries, Mica or NuGet packages involved. No new
dependency was added; the only extra import library is `uxtheme.lib`, which
ships with the Windows SDK.

Deliberately **not** done: owner-drawing the push buttons, and per-monitor DPI
awareness. Both are noted under *Known trade-offs*.

## What changed

| Area | Before | After |
|---|---|---|
| Dialog font | `MS Shell Dlg` 8 pt (maps to Tahoma, a Windows XP look) | Segoe UI 9 pt |
| DPI | Not declared — bitmap-stretched and blurry above 100 % scaling | System DPI aware |
| Manifest | Present in `res/`, referenced by nothing, hard-coded `X86` | Merged into the binary; `supportedOS` for Windows 7–11, `longPathAware` |
| Colours | System default | Dark theme from the icon palette, light theme still available |
| Repeat delay / rate | Numeric edit boxes only | Edit boxes plus synchronised sliders with a cyan thumb |
| Flags readout | `(122)` | `Flags: 122 (0x7A)` |
| Status | None | Live line showing what Windows is actually using |
| Help | None | Tooltips on all 25 controls, flag checkboxes name their `FKF_*` constant |
| Accessible names | Missing — all labels sat at the end of the resource | Each label precedes its control; sliders carry their name in the window text |
| About box | Native light only | Follows the selected theme |
| High contrast | Ignored | Custom palette steps aside automatically |

## Colour palette

| Role | Dark | Light |
|---|---|---|
| Background top | `#0A2342` | `COLOR_3DFACE` |
| Background bottom | `#1E5A9E` | `COLOR_3DFACE` |
| Surface (edit fields) | `#123B6E` | `COLOR_WINDOW` |
| Accent (slider thumb) | `#4B9BEE` | `#1E5A9E` |
| Primary text | `#FFFFFF` | `COLOR_WINDOWTEXT` |
| Secondary text | `#E0E0E0` | `COLOR_GRAYTEXT` |

White on `#0A2342` is roughly 15:1, comfortably past the 4.5:1 requirement.
`#4B9BEE` is used for fills only, never for text.

## Files touched

```
Theme.h / Theme.cpp        new — palette, brushes, gradient, title bar, high contrast
FilterKeysSetterDlg.h/.cpp theming, sliders, tooltips, status line, About box
FilterKeysSetter.rc        Segoe UI, new layout (228 × 276 DLU), control order
resource.h                 IDC_DELAY_SLIDER, IDC_REPEAT_SLIDER, IDC_DARKTHEME,
                           IDC_STATUS; IDC_EDIT1 renamed to IDC_TEST_EDIT
FilterKeysSetter.vcxproj   Theme.* added, manifest wired up, DPI awareness,
                           phantom wizard headers removed
res/FilterKeysSetter.manifest  rewritten
framework.h                trimmed to afxwin.h + afxcmn.h
FilterKeysSetter.h/.cpp    wizard placeholders removed, real registry key
FilterKeysSetter.Setup/    version 1.0.11, new ProductCode/PackageCode,
                           RemovePreviousVersions enabled
```

## Behaviour that must not change

Verified untouched by this work:

* `SPI_GETFILTERKEYS` / `SPI_SETFILTERKEYS` calls and their `fWinIni` flags
* `SPIF_UPDATEINIFILE` and `SPIF_SENDCHANGE` driven by the two check boxes
* Reads from `HKEY_CURRENT_USER\Control Panel\Accessibility\Keyboard Response`
  (`DelayBeforeAcceptance`, `AutoRepeatDelay`, `AutoRepeatRate`, `BounceTime`, `Flags`)
* `FKF_*` flag composition in `GetFlagsVal()` / `SetFlags()`
* The Current / Registry / Keyboard / Default / Original presets
* The 20000 ms validation limits

The theme preference is stored in a **separate** key,
`HKEY_CURRENT_USER\Software\FilterKeysSetter\Theme` (`REG_DWORD`, 0 = light,
1 = dark), so it can never collide with the accessibility settings.

## Migration checklist

1. Pull the branch and open `FilterKeysSetter.sln` in Visual Studio 2022.
2. Confirm the *Desktop development with C++* workload includes
   **MFC for latest v143 build tools** — the build fails without it.
3. Build `Release|x64` and `Release|Win32`.
4. If `TBS_NOTICKS` fails to resolve, check that `#include <commctrl.h>` is
   still present near the top of `FilterKeysSetter.rc`; the Visual Studio
   resource editor drops it if it rewrites the file.
5. Open the dialog editor once and eyeball `IDD_FILTERKEYSSETTER_DIALOG`. The
   tightest control is the multi-line radio button at 108 × 24 units.
6. Delete any stale `Debug/`, `Release/`, `x64/` output; the manifest is now
   embedded and old binaries will not pick it up.

## Layout check

`tools/check-dialog-layout.py` parses `FilterKeysSetter.rc` and reports controls
that leave the dialog, overlap each other, poke out of their group box, or carry
a caption too long for their width at Segoe UI 9 pt. Run it after any change to
the resource:

```cmd
python tools\check-dialog-layout.py
```

It found three undersized check boxes and the *Keyboard* preset button after the
font change; all four have been widened.

## Toolchain: Visual Studio 2022 and 2026

Visual Studio 2026 (18.x) ships the **v145** platform toolset and no longer
carries v143. A project with a hard-coded `<PlatformToolset>v143</PlatformToolset>`
fails there before a single file is compiled:

```
error MSB8020: The build tools for v143 (Platform Toolset = 'v143')
cannot be found.
```

Rather than move the project to v145 and lock out VS 2022, the toolset is now
resolved from the Visual Studio that is running the build:

```xml
<PlatformToolset Condition="'$(PlatformToolset)' == ''">$(DefaultPlatformToolset)</PlatformToolset>
<PlatformToolset Condition="'$(PlatformToolset)' == ''">v143</PlatformToolset>
```

`$(DefaultPlatformToolset)` is set by `Microsoft.Cpp.Default.props`, which is
imported above the configuration property groups, so it is already available:
v143 under VS 2022, v145 under VS 2026. The second line is a fallback for
exotic environments where the property is empty. Both stay out of the way of
`/p:PlatformToolset=...` on the command line.

Two things to know when building with VS 2026:

* **Install the MFC component for v145** (*C++ MFC for latest v145 build tools*).
  It is not part of the *Desktop development with C++* workload; without it the
  build stops at `afxwin.h`.
* **Do not accept *Retarget solution*.** It writes a fixed `v145` into the
  `.vcxproj` and undoes the conditional above. Choose *Install missing platform
  toolset* instead, or simply dismiss the dialog — the project builds as is.

`ConformanceMode` is now pinned to `false` in a global `ItemDefinitionGroup`.
It was already off by omission, but the v145 toolset is a good deal stricter
and a future template default flipping to `/permissive-` would bury real
diagnostics under conformance errors in this 2013-era MFC code. The decision is
recorded in the project file instead of depending on a default.

The setup project needs *Microsoft Visual Studio Installer Projects* **3.0.0 or
newer** under VS 2026; older builds of the extension crash on it.

`tools/build.cmd` wraps all of this: it resolves the Visual Studio installation
with `vswhere` (so it works regardless of which version is present), reports
whether the MFC component is installed, and builds `FilterKeysSetter.vcxproj`
rather than the solution -- the `.vdproj` and its extension have no bearing on
whether the application compiles. Errors and warnings from every platform are
collected into one list at the end.

## Compacting the dialog

After the font change the dialog had grown to 228 × 322 DLU (399 × 604 px at
100 %), with more air than content in places. It is now **228 × 276 DLU**
(399 × 518 px), roughly 15 % shorter, without dropping a single control:

| Change | DLU saved |
| --- | --- |
| Row pitch in the *Settings* group 18 → 16, first row moved up | 14 |
| Slider height 14 → 12, gap to the edit above tightened | 6 |
| *Load settings* and *Test area* group boxes 30 → 28 high | 4 |
| *Appearance* group box dropped; *Dark theme* moved beside *OK* | 22 |

The *Appearance* frame carried a single check box, and its caption said
nothing that *Dark theme* did not already say. Next to the *OK* button the
check box is just as easy to find, and the right-hand column now ends level
with the left one.

Nothing moved in z-order, so the tab order and `DDX_Radio` are untouched; only
coordinates changed. Verified with `tools/check-dialog-layout.py` (clean) and
`tools/render-dialog.py`.

## Dialog preview

`tools/render-dialog.py` draws the main dialog from the resource script and the
theme palette:

```cmd
python tools\render-dialog.py            :: both themes into docs\img
python tools\render-dialog.py --theme dark
```

It shares its parser with the layout checker, so the geometry in
`docs/img/dialog-dark.png` and `docs/img/dialog-light.png` is the geometry in
`FilterKeysSetter.rc`: if a control is moved, the picture moves with it on the
next run. What it cannot show is the dialog font (Segoe UI is not available
outside Windows) and the native chrome of the common controls. It is a layout
preview and a stand-in for the README, not a substitute for a screenshot.

## Testing checklist

**Functional**

- [ ] Each of the five presets loads plausible values
- [ ] Switching the two radio buttons enables/disables the right fields *and* sliders
- [ ] Apply writes the settings; the status line updates to match
- [ ] OK applies and closes; Cancel discards
- [ ] A value above 20000 is still rejected with the existing message box
- [ ] *Save to registry* survives a sign-out; without it the change is session-only
- [ ] `Control Panel\Accessibility\Keyboard Response` holds the expected values afterwards

**Sliders**

- [ ] Dragging the delay slider updates the edit box and vice versa
- [ ] Dragging the rate slider also updates the characters-per-second readout
- [ ] Typing a value beyond the slider range clamps the thumb but keeps the typed value
- [ ] No flicker or infinite update loop between slider and edit box

**Visual**

- [ ] Gradient is smooth, no banding, no seam behind the sliders
- [ ] Slider thumb is cyan when enabled, grey when disabled
- [ ] No black-on-dark or white-on-white text anywhere
- [ ] Title bar is dark in the dark theme, light in the light theme
- [ ] About box matches the main window
- [ ] Theme switch takes effect immediately and survives a restart

**DPI and themes**

- [ ] 100 %, 125 %, 150 %, 200 % scaling — text crisp, nothing clipped
- [ ] Light theme looks like a native Windows dialog
- [ ] Turning Windows high contrast on while running drops the custom palette
- [ ] The *Dark theme* check box is disabled while high contrast is active

**Accessibility**

- [ ] Tab reaches every control; focus is always visible
- [ ] Narrator announces a meaningful name for each edit box and both sliders
- [ ] Sliders respond to arrow keys, Page Up/Down, Home/End
- [ ] Tooltips appear on hover and stay long enough to read
- [ ] Accesskeys and Esc/Enter still behave

## Known trade-offs

**Check boxes, radio buttons and group boxes look flat in the dark theme.**
They have to be detached from the visual style with
`SetWindowTheme(hwnd, L"", L"")`, because the theme engine paints its own text
in black and ignores `WM_CTLCOLORSTATIC` entirely. Classic rendering is the
price of readable labels. Push buttons keep their native look on purpose.

**System DPI, not per-monitor.** Per-monitor v2 requires handling
`WM_DPICHANGED` and rebuilding fonts and layout at runtime, which this dialog
does not do. Declaring it without that work looks worse than system awareness:
the window would be re-laid out at the wrong scale when moved between monitors.

**The dialog is about 15 % larger.** Dialog units are derived from the font
metrics, so Segoe UI 9 pt scales the whole layout. This is expected, and the
reason the geometry was left proportional instead of being hand-tuned.

## Latent bugs fixed along the way

Not part of the UI work, but found while reading the surrounding code:

* The value members (`m_nWait`, `m_nDelay`, ...) were never initialised, so the
  first `UpdateData(FALSE)` ran on indeterminate memory.
* `GetStringRegKey()` relied on `RegQueryValueEx` null terminating its result,
  which it does not promise. `_wtoi()` could then read past the buffer. The
  helper now reserves room for a terminator, writes one, and verifies the value
  type instead of reinterpreting whatever bytes are stored.
* `GetDWORDRegKey()` and `GetBoolRegKey()` were dead code; both are gone.
* A narrow string literal was assigned to a `CStringW`.
* All inputs lacked an accessible name because every label sat at the end of the
  resource file.

## Rollback

Each step is a separate commit, so a single change can be reverted on its own:

| Commit | Change |
|---|---|
| `8cf69e8` | Segoe UI font, DPI awareness, manifest |
| `bd0907d` | Dark theme, sliders, status line |
| `3f61620` | About box theming, tooltips, flags readout |

Full rollback of the UI work:

```cmd
git revert --no-commit 3f61620 bd0907d 8cf69e8
git commit -m "Revert the 1.11 UI upgrade"
```

To keep the code but disable the dark theme for everyone, make
`CTheme::LoadPreference()` return `ThemeMode::Light`; nothing else depends on
the mode. Users can also clear the preference themselves by deleting
`HKEY_CURRENT_USER\Software\FilterKeysSetter`.

## Still open

`.github/workflows/build.yml` replaces the unusable CMake-on-Ubuntu starter
workflow with a dialog-layout check on Ubuntu plus MSBuild on both
`windows-2022` (VS 2022, v143) and `windows-2025` (VS 2026, v145), for `Win32`
and `x64`. It could not be pushed: GitHub rejects workflow files from an app
without the `workflows` permission. Until it lands, none of the above has been
compiled in CI.
