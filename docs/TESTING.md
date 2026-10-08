# Manual test plan

Nothing in this repository has ever been compiled: there is no MSVC here.
Six static checkers stand in for the compiler, but they cannot press a
button. This is the list to work through the first time a build runs, and
after that before each release. `docs/RELEASE.md` carries the short version;
this is the long one.

Budget about fifteen minutes.

## What the program touches

Worth knowing before testing, and worth restoring afterwards:

| Where | What |
| --- | --- |
| `SystemParametersInfo(SPI_SETFILTERKEYS)` | the live FilterKeys state: on/off, repeat delay, repeat rate, bounce time |
| `HKCU\Control Panel\Accessibility\Keyboard Response` | the same settings as the system stores them, written when *Save to registry* is ticked |
| `HKCU\Software\FilterKeysSetter`, value `Theme` | the only key this program owns: `1` dark, `0` light |

Before starting, note the current values:

```cmd
reg export "HKCU\Control Panel\Accessibility\Keyboard Response" fk-backup.reg
```

The *Original* button restores whatever was active when the program
started, which covers most accidents within one session. `fk-backup.reg`
covers the rest.

## 1. It starts

- [ ] The executable runs on a machine that has never had Visual Studio on
      it. A missing `mfc140u.dll` or `vcruntime140.dll` means the
      redistributable is needed: the project links MFC dynamically
      (`UseOfMfc` is `Dynamic`), which keeps the executable at roughly
      76 KB but requires the *Microsoft Visual C++ Redistributable* on the
      target machine.
- [ ] The window title is *FilterKeys Setter*, the task bar icon is the
      application icon rather than the generic one.
- [ ] The dialog opens in the **dark** theme on a first run, with a dark
      title bar.

## 2. Theme

- [ ] Clearing *Dark theme* switches to the native light look immediately,
      including the title bar.
- [ ] The choice survives closing and reopening the program.
- [ ] `reg query HKCU\Software\FilterKeysSetter /v Theme` shows `0` after
      clearing it, `1` after ticking it.
- [ ] Turning Windows high contrast on (left `Alt` + left `Shift` +
      `Print screen`) forces the light theme **while the program is
      running** and disables the check box; turning it off restores the
      stored preference.

## 3. Keyboard

- [ ] Holding `Alt` underlines the accelerators.
- [ ] `Alt`+`D` lands in *Repeat delay*, `Alt`+`R` in *Repeat rate*,
      `Alt`+`K` presses *Keyboard*, `Alt`+`P` presses *Apply*.
- [ ] `Tab` walks every control in reading order and never skips a field.
- [ ] The arrow keys move between the two radio buttons, and `Tab` leaves
      the pair rather than cycling inside it.
- [ ] `Esc` closes without applying, `Enter` behaves as *OK*.

## 4. Sliders and fields

- [ ] Dragging the *Repeat delay* slider updates the field, and typing in
      the field moves the slider.
- [ ] The same for *Repeat rate*, and the *(x.x per second)* read-out
      follows along.
- [ ] Typing a value **above** the slider maximum (`5000` into repeat
      delay, whose slider stops at 2000) leaves the field alone and parks
      the slider at its maximum. The field, not the slider, is what gets
      applied.
- [ ] Clearing a field entirely does not crash and does not reset the other
      one.
- [ ] Letters cannot be typed into the numeric fields at all (`ES_NUMBER`).

## 5. Loading and applying

- [ ] Each of the five *Load settings* buttons fills the dialog with
      plausible values: *Current*, *Registry*, *Keyboard*, *Defaults*,
      *Original*.
- [ ] *Apply* with a repeat rate of, say, `500` makes holding a key in the
      test area visibly slow; `10` makes it fast.
- [ ] The status line under the test area reports what Windows is actually
      using, which after *Apply* is what was just applied.
- [ ] The *Flags* read-out shows both the decimal and the hex value and
      changes as the check boxes are ticked.
- [ ] With *Save to registry* ticked, *Apply* updates
      `HKCU\Control Panel\Accessibility\Keyboard Response`; without it, the
      registry is untouched while the live setting still changes.
- [ ] *Original* puts everything back, and the status line agrees.

## 6. Tool tips

- [ ] Hovering any control shows a tip within a second.
- [ ] The three read-outs -- characters per second, the flags value and the
      status line -- have tips as well. They are statics, which only report
      the mouse because they carry `SS_NOTIFY`; if their tips are missing,
      that style was lost.
- [ ] A tip stays on screen long enough to read (15 seconds) and wraps
      rather than running off the screen.

## 7. Display

- [ ] At 150 % scaling the dialog is laid out, not bitmap-stretched: text
      edges stay crisp. The program declares *system* DPI awareness, so
      moving it to a second monitor with a different scale **will** blur
      it until it is restarted. That is a known trade-off, not a new bug.
- [ ] The icon is sharp in the title bar, the task bar, `Alt`+`Tab` and in
      Explorer's large-icon view -- those use four different frames out of
      the ten in the `.ico`.
- [ ] The About box (system menu, *About FilterKeysSetter*) follows the
      current theme and shows **version 1.11**.

## 8. Installer

Only when the MSI was built:

- [ ] Installing over an existing 1.10 **replaces** it: one entry in
      *Installed apps*, not two.
- [ ] The shortcut works and the uninstaller removes the program.
- [ ] After uninstalling, `HKCU\Software\FilterKeysSetter` may remain --
      user preferences are deliberately not removed.

## If something fails

Note which checker should have caught it. A layout fault belongs in
`check-dialog-layout.py`, a dead handler in `check-message-map.py`, a wrong
version in `check-resources.py`, and so on. Each of those has a self test
case in `tools/selftest.py`; add one there as well, so the same fault
cannot come back unnoticed.
