# UI upgrade — FilterKeys Setter 1.10 → 1.11

Reference for the dialog rework: what changed, how to verify it, and how to get
back if something turns out wrong.

## Scope

The application is **C++ / MFC**, not WinForms or WPF, so the upgrade is built
from `WM_CTLCOLOR*` handlers, `SetWindowTheme`, custom draw and GDI — there are
no styles, resource dictionaries, Mica or NuGet packages involved. No new
dependency was added; the only extra import library is `uxtheme.lib`, which
ships with the Windows SDK.

Deliberately **not** done: per-monitor DPI awareness, noted under
*Known trade-offs*.

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
| Help | None | Tooltips on all 28 controls including the three read-outs, flag checkboxes name their `FKF_*` constant |
| Accessible names | Missing — all labels sat at the end of the resource | Each label precedes its control; sliders carry their name in the window text |
| About box | Native light only | Follows the selected theme |
| High contrast | Ignored | Custom palette steps aside automatically |

## Colour palette

| Role | Dark | Light |
|---|---|---|
| Background top | `#2A4A7B` | `COLOR_3DFACE` |
| Background middle | `#1F3A61` | `COLOR_3DFACE` |
| Background bottom | `#14263F` | `COLOR_3DFACE` |
| Surface (edit fields) | `#1D5188` | `COLOR_WINDOW` |
| Accent (slider thumb) | `#5D9CD6` | `#1D5188` |
| Muted accent (slider channel) | `#3778B5` | `#3778B5` |
| Primary text | `#FFFFFF` | `COLOR_WINDOWTEXT` |
| Secondary text | `#E0E0E0` | `COLOR_GRAYTEXT` |
| Disabled text | `#A8BBD6` | `COLOR_GRAYTEXT` |

The background is a **three stop** gradient: `#2A4A7B` at the top, `#1F3A61`
halfway down, `#14263F` at the bottom. `CTheme::PaintBackgroundSlice` walks
its 128 bands in two halves, and `PaintBackgroundSlice` lets a child control
reproduce exactly the slice sitting behind it, so the stops line up across
control boundaries.

Contrast of white text against the three background stops is 8.9:1, 11.4:1 and
15.2:1, and 8.1:1 on the input surface -- all past the 4.5:1 requirement.
Secondary text `#E0E0E0` ranges from 6.7:1 to 11.5:1.

Disabled text had to change with the palette. The old `#8CA4C4` sat on a very
dark background (`#0A2342`, 5.5:1); against the new, lighter top band it would
have dropped to **3.5:1**. `#A8BBD6` restores 4.5:1 at the top of the gradient
and more further down.

`#5D9CD6` and `#3778B5` are used for fills and outlines only, never for text.

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

## Keyboard cues on the custom-drawn buttons

Adding the Alt accelerators broke something the owner-draw commit could not
have anticipated: `DrawText` underlines the character after an `&`
unconditionally, while Windows itself hides those underlines until the user
presses Alt or starts navigating with the keyboard. The eight custom-drawn
push buttons would therefore have been the only controls in the dialog
showing `Appl`<u>`y`</u> underlined from the moment the window opened.

Windows publishes that preference per window as the *UI state*. The painter
now asks for it and suppresses the underline accordingly:

```cpp
UINT uFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
const LRESULT uiState = ::SendMessage(pcd->hdr.hwndFrom, WM_QUERYUISTATE, 0, 0);
if ((uiState & UISF_HIDEACCEL) != 0) {
    uFormat |= DT_HIDEPREFIX;
}
```

No extra message handler is needed: when the state changes the dialog
manager sends `WM_UPDATEUISTATE` down the window tree and the button
invalidates itself, which brings `NM_CUSTOMDRAW` round again with the new
answer. Users who have *Underline access keys* switched on permanently in
*Ease of Access* see the underlines all the time, exactly as elsewhere.

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

## Message map check

`tools/check-message-map.py` compares the three places an MFC handler has to
appear: the entry in `BEGIN_MESSAGE_MAP`, the `afx_msg` declaration in the
class, and the definition in the `.cpp`. It also checks every control ID used
in a map or in `DDX_*` against `resource.h`, flags duplicate entries, and
reports `afx_msg` members that no map references any more.

```cmd
python tools\check-message-map.py
```

Why a tool for this: the compiler does catch these mistakes, but it reports
them from inside the macro expansion, and the `.cpp` grew eight new
`ON_NOTIFY` entries for the push buttons alone. Verified by injecting three
faults -- a typo in a handler name, a typo in a control ID, and an
`ON_WM_TIMER()` with no handler behind it -- all three were reported.

It also follows the control IDs out of the code and into the resource
script: every ID used by `DDX_*`, by `GetDlgItem` or by the tool tip table
has to belong to a dialog in `FilterKeysSetter.rc`, not merely exist in
`resource.h`. And it knows one MFC trap: a tool tip on a static only ever
appears if that static carries `SS_NOTIFY`, because without it the control
returns `HTTRANSPARENT` and never sees the mouse.

Current state: clean, with one note. `ON_COMMAND(ID_HELP, CWinApp::OnHelp)`
in `FilterKeysSetter.cpp` points at a base class member, which the tool
reports and skips rather than guessing at MFC's own class hierarchy.

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

## Push buttons in the dark theme

The eight push buttons were the last native-grey island in the dark dialog,
and the new, lighter palette made them stand out more than before. They are
now drawn by the dialog, through `NM_CUSTOMDRAW` rather than `BS_OWNERDRAW`:

* no resource change, so the buttons stay ordinary push buttons for the
  dialog manager, for `DDX` and for screen readers,
* a single `CDDS_PREPAINT` branch per button; everything else falls through
  to the default drawing.

| State | Face | Notes |
| --- | --- | --- |
| Normal | `#1D5188` | white text, 8.1:1 |
| Hover | `#3778B5` | white text, 4.7:1 |
| Pressed | `#14263F` | plus the usual one pixel text nudge |
| Disabled | `#1F3A61` | `#A8BBD6` text |

The default button (*OK*) carries a two pixel `#5D9CD6` border, a focused
button the same border plus the system focus rectangle, so keyboard focus
stays visible without relying on colour alone.

This is why `CTheme::ApplyToControl` leaves push buttons attached to the
visual style: an unthemed button falls back to classic drawing and never
sends `NM_CUSTOMDRAW`.

The painter itself is a file-local function, `PaintThemedPushButton`, not a
member of either dialog. Both the main window and the About box call it from
their own two-line `NM_CUSTOMDRAW` handler, so the *OK* button in the About
box cannot drift away from the eight buttons in the main window.

## Installer audit

`FilterKeysSetter.Setup.vdproj` carries a list of "detected dependencies" that
Visual Studio collected years ago. Two of them are worth knowing about.

**The seven `api-ms-win-crt-*.dll` files are now excluded.** They are APISet
forwarders of the Universal CRT, and the installer shipped seven of them
without `ucrtbase.dll`, which they forward to. Microsoft's guidance is
explicit: the UCRT is an operating system component, app-local deployment is
supported but discouraged, and **on Windows 10 and 11 the copy in the system
directory is always used, even when the application ships a newer one**. So
those seven files were dead weight on every supported Windows version, and on
an older one the incomplete set could not have worked either. They are set to
`Exclude = TRUE` rather than deleted, so the entries can be switched back on
in the IDE if someone ever needs a Windows 7 package.

`VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` and `mfc140u.dll` stay in the
package. Those are genuinely redistributable, they make the MSI work without
a separately installed VC++ redistributable, and their names do not change
under the v145 toolset: MSVC 14.50 keeps binary compatibility with everything
back to 2015 and still ships as the **v14** runtime family.

**The packaged executable is hard-coded to `..\x64\Release\FilterKeysSetter.exe`.**
It is a plain file reference, not *Primary output from FilterKeysSetter
(Active)*. Consequences: building the setup in `Debug` or for `Win32` still
packages the x64 release binary, and building it without an x64 release build
present fails. This has not been changed here, because replacing a file
reference with a project output means hand-editing GUID-keyed blocks in a
format no tool validates. In the IDE it is three clicks: remove the file from
*Application Folder*, *Add → Project Output → Primary output*, then re-point
the shortcut at it.

## Keyboard accelerators

Until now not a single control in `IDD_FILTERKEYSSETTER_DIALOG` carried an `&`.
Tab and the arrow keys worked, but nothing could be reached directly, and a
screen-reader user had no way to jump to a field. Every interactive control
now has an Alt accelerator:

| Alt | Control | Alt | Control |
| --- | --- | --- | --- |
| Q | Ignore **q**uick keystrokes (radio) | V | Sa**v**e to registry |
| F | repeated **f**aster than (radio) | B | **B**roadcast change |
| I | **I**gnore under | T | Dark **t**heme |
| D | Repeat **d**elay | E | Curr**e**nt |
| M | Bounce ti**m**e | G | Re**g**istry |
| R | Repeat **r**ate | K | **K**eyboard |
| O | **O**n | L | Defau**l**t |
| A | **A**vailable | N | Origi**n**al |
| U | **U**se shortcut | P | A**p**ply |
| C | **C**onfirm activation | | |
| S | Activation **s**ound | | |
| H | S**h**ow status | | |
| Y | Ke**y** click | | |

Notes on the choices:

- Twenty-two controls compete for twenty-six letters, so a few accelerators
  land mid-word (`Curr&ent`, `Defau&lt`, `Origi&nal`). That is normal in dense
  dialogs; thirteen of them still sit on a word initial.
- Group boxes deliberately get none. Their accelerator would move the focus to
  the next control in z-order, which is a surprise rather than a shortcut.
- OK and Cancel get none either, by convention: Enter and Esc already reach
  them, and `&O`/`&C` would collide with *On* and *Confirm activation*.
- The labels are `RTEXT` statics placed immediately before their edit box in
  z-order, which is exactly what the dialog manager needs — a static
  accelerator hands the focus to the *next* control, so Alt+D lands in the
  delay field.
- `tools/check-dialog-layout.py` now fails when two controls in the same
  dialog claim the same letter, and ignores `&` when it estimates caption
  widths. `tools/render-dialog.py` strips the `&` and underlines the marked
  character, so the preview images match what Windows draws with Alt held.

Three further rules keep the accelerators honest, because all of them depend
on z-order rather than on anything visible in the dialog editor:

| Rule | Why it exists |
| --- | --- |
| A label accelerator must be followed by a focusable control | `RTEXT` cannot take the focus, so the accelerator is handed to the *next* control in z-order. Move a label and the shortcut silently lands somewhere else. |
| …and that control must share the label's row | Catches exactly that case: in a mutation test, an `&` on the `ms` suffix of the delay row was reported as focusing `IDC_DELAY_SLIDER` one row below. |
| A focusable `CONTROL` statement needs an explicit `WS_TABSTOP` | `EDITTEXT`, `PUSHBUTTON` and friends get one implicitly; `CONTROL` does not. A check box written as `CONTROL` without it drops out of the tab chain while still looking perfectly normal. The sole exception is a radio button that follows another one — those are one tab stop, and the first of them is checked for `WS_GROUP` instead. |

All three were verified by mutating the `.rc` and confirming the checker
fails, then restoring it.

## Resource and version check

`tools/check-resources.py` covers the bookkeeping that no compiler complains
about. Run it with `python tools/check-resources.py`; it prints
`33 symbol(s), 0 problem(s)` and exits 0 when the repository is healthy.

| It fails when | Why that matters |
| --- | --- |
| Two symbols of the same family share a value | `IDC_DARKTHEME` and `IDC_STATUS` both on 1027 means `DDX` writes into the wrong control and `GetDlgItem` returns the wrong window. Nothing warns you. |
| An `_APS_NEXT_*` counter has fallen behind | The dialog editor hands out the next ID from that counter, so the next control added in the IDE silently duplicates an existing one. |
| An `IDC_`/`IDD_` symbol is used but not defined | The `.rc` still compiles if some other header happens to define it. |
| `FILEVERSION`, `PRODUCTVERSION` and the two `VALUE` strings disagree | Windows shows one number in the file properties and another in the installer. |
| The about box, the installer `ProductVersion` or the newest README entry do not match `VERSIONINFO` | The version lives in five places; bumping four of them is the normal outcome. |

Symbols that are defined but never used are reported as a note, not a
failure. Every rule was verified by mutating a throw-away copy of the tree
and confirming the exit code turns to 1.

## The icon container

`res/FilterKeysSetter.ico` is built by `tools/make-icon.py` from the 256 px
master in `res/logo.png`:

```
pip install Pillow
python tools\make-icon.py
```

It writes ten frames -- 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 px.
The unusual ones earn their place: 20 and 40 px are what the shell asks for
at 125 % and 150 % DPI, and without them Windows rescales the 16 and 32 px
frames into something visibly soft. Everything up to 96 px is stored as an
uncompressed 32-bit BMP with an AND mask, the form every Windows version
understands; 128 and 256 px are PNG-compressed, which is what PNG frames in
icons were introduced for -- as BMPs those two would add about 170 KB.
Pillow's own ICO writer PNG-compresses every frame, so the container is
assembled by hand in that script.

`tools/check-resources.py` reads the container back without Pillow (an .ico
directory is a six-byte header plus sixteen bytes per frame) and fails when
the file named in the `.rc` is missing, when it does not parse, or when the
16, 32, 48 or 256 px frame is absent. Frames below 32 bpp and an
uncompressed 256 px frame are reported as notes.

## Self test for the checkers

Three checkers now gate this repository, and each of their rules was verified
once, by hand, by breaking the source on purpose. That verification lived in
the commit messages, which means a later refactor could quietly turn a rule
into a no-op: the checkers would still print `ok` on a healthy tree, and
nobody would notice that they had stopped checking.

`tools/selftest.py` replays those mutations:

```
python tools\selftest.py -v
```

It copies the tree into a scratch directory, confirms all three checkers are
clean on the untouched copy, then applies **22 mutations** one at a time --
a button pushed off the dialog, a control moved out of its group box, a
duplicate accelerator, a label accelerator pointing at the wrong row, a check
box without `WS_TABSTOP`, a handler that is mapped but not declared, a tool
tip on a static without `SS_NOTIFY`, two resource symbols sharing a number,
an installer version left behind, a truncated icon file, and so on. Each one has to make the right
checker exit non-zero *and* print the expected sentence; afterwards the file
is restored and the checkers must be clean again. The working tree is never
touched.

The test was itself verified by neutering the duplicate-accelerator rule in
`check-dialog-layout.py`: `22 case(s), 1 failure(s)` --
`the same accelerator used twice: check-dialog-layout.py did not notice`.

## Known trade-offs

**Check boxes, radio buttons and group boxes look flat in the dark theme.**
They have to be detached from the visual style with
`SetWindowTheme(hwnd, L"", L"")`, because the theme engine paints its own text
in black and ignores `WM_CTLCOLORSTATIC` entirely. Classic rendering is the
price of readable labels.

**System DPI, not per-monitor.** Per-monitor v2 requires handling
`WM_DPICHANGED` and rebuilding fonts and layout at runtime, which this dialog
does not do. Declaring it without that work looks worse than system awareness:
the window would be re-laid out at the wrong scale when moved between monitors.

**The dialog is about 15 % larger.** Dialog units are derived from the font
metrics, so Segoe UI 9 pt scales the whole layout. This is expected, and the
reason the geometry was left proportional instead of being hand-tuned.

## Latent bugs fixed along the way

**Two failure paths in the FilterKeys reads did the wrong thing.** Neither
is reachable on a healthy desktop -- `SPI_GETFILTERKEYS` fails under a
restricted desktop or a user session that is going away -- but both would
have destroyed settings rather than reporting a problem:

- *Current* showed `Failed to fetch current settings` and then **loaded the
  struct anyway**. At that point it contains nothing but its own `cbSize`,
  so the error box was followed by every field in the dialog quietly
  becoming zero. It now returns after the message.
- The start-up read was not checked at all, so after a failure *Original*
  offered to "restore" FilterKeys off with all four timings at 0 -- the one
  button whose whole purpose is to put things back. The result is now
  remembered in `m_bHaveOriginal`; if the read failed the button is disabled
  in `OnInitDialog` and the handler refuses a stray click as well.

**A third read had the same shape.** *Keyboard* calls `SPI_GETKEYBOARDSPEED`
and `SPI_GETKEYBOARDDELAY` and checked neither. On failure both variables
stay 0, and 0 is not an obviously wrong value here -- it comes out as a
500 ms repeat and a 250 ms delay, a plausible pair that the dialog would
then have presented as "the standard Windows keyboard settings". It now
reports the failure and changes nothing.

While in there, the two flag constants stopped being magic numbers:

| Was | Is | Meaning |
| --- | --- | --- |
| `dwFlags = 122` | `kDefaultFlags` | `FKF_AVAILABLE \| FKF_CONFIRMHOTKEY \| FKF_HOTKEYSOUND \| FKF_INDICATOR \| FKF_CLICKON` |
| `dwFlags = 59` | `kKeyboardFlags` | `FKF_FILTERKEYSON \| FKF_AVAILABLE \| FKF_CONFIRMHOTKEY \| FKF_HOTKEYSOUND \| FKF_INDICATOR` |

Both sums were checked against the `FKF_*` values before the swap, and
`kDefaultFlags` is now also the fallback for a missing `Flags` value in the
registry -- which is what the literal 122 there had always meant.

The success path is byte for byte what it was: same calls, same arguments,
same order.

**`CTheme` owned two GDI brushes with the default copy constructor still in
place.** Nothing copies a `CTheme` today -- there is one in each dialog --
but a copy would have handed the same two handles to a second destructor,
and a double `DeleteObject` is the kind of fault that shows up as a random
painting failure somewhere else entirely. The copy constructor and the
assignment operator are now `= delete`.

Two smaller ones in the same file: if `CreateSolidBrush` ever fails, the
theme used to hand `NULL` back to `WM_CTLCOLOR*`, which is not a legal
answer -- the control then paints with whatever brush happens to be
selected. It now falls back to stock brushes and remembers not to delete
them. And the gradient divided by `steps - 1`, which is only safe because
`bands` happens to be 128; the divisor is now guarded so that changing that
constant cannot divide by zero.

## More latent bugs fixed along the way

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
workflow with the four static checks on Ubuntu --
`check-dialog-layout.py`, `check-message-map.py`, `check-resources.py` and
`selftest.py` -- plus MSBuild on both `windows-2022` (VS 2022, v143) and
`windows-2025` (VS 2026, v145), for `Win32` and `x64`. It could not be
pushed: GitHub rejects workflow files from an app without the `workflows`
permission. Until it lands, none of the above has been compiled in CI, and
the checks only run when someone runs them by hand:

```
python tools\check-dialog-layout.py
python tools\check-message-map.py
python tools\check-resources.py
python tools\selftest.py
```

The expected output is four clean runs: two `ok` lines, `0 problem(s)` with
a single note about `CWinApp::OnHelp`, `33 symbol(s), 0 problem(s)` and
`22 case(s), 0 failure(s)`. Anything else is a regression.
