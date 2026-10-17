# Manual test plan

Nothing in this repository has ever been compiled: there is no MSVC here.
Eight static checkers stand in for the compiler, but they cannot press a
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
- [ ] On a clean virtual machine **without** the Visual C++
      Redistributable, the MSI-installed program still starts. If it stops
      with a missing `msvcp140.dll`, the installer's app-local runtime set
      is incomplete -- see *Deployment* in `README.md`, and record which
      DLL was named.
- [ ] The window title is *FilterKeys Setter*, the task bar icon is the
      application icon rather than the generic one.
- [ ] On a machine that has never run this program -- delete
      `HKCU\Software\FilterKeysSetter` first -- the dialog opens in the
      theme Windows is set to: dark under *Settings → Personalisation →
      Colours → Choose your default app mode: Dark*, light under *Light*.
      With no such setting at all (before Windows 10 1809) it opens dark.

## 2. Theme

- [ ] With no stored preference, switching the Windows app mode while
      the dialog is open changes the dialog over with it. After ticking or
      clearing *Dark theme* once, the same switch leaves it alone: the
      explicit choice wins.
- [ ] Clearing *Dark theme* switches immediately, title bar included, to
      the light theme: a white background drifting to pale blue at the
      foot, white fields, navy text, no grey anywhere.
- [ ] The five edit boxes have a flat one pixel blue outline, not the
      grey sunken trench Windows draws by default, and their text does
      not sit hard against that outline. Taking one out of play
      (*Bounce time*, by selecting the first radio button, or *Repeat
      delay* by selecting the second) drops its value to the palette's
      disabled tone rather than to the system grey.
- [ ] The light theme is drawn by the program too, so the same checks
      apply to it: nothing left in the system grey, group box captions
      not struck through, tool tips white with navy text.
- [ ] The choice survives closing and reopening the program.
- [ ] `reg query HKCU\Software\FilterKeysSetter /v Theme` shows `0` after
      clearing it, `1` after ticking it.
- [ ] Turning Windows high contrast on (left `Alt` + left `Shift` +
      `Print screen`) drops **both** brand palettes while the program is
      running and disables the check box: the dialog then looks like the
      high contrast scheme, buttons and check boxes included, because
      every painter stands down. Turning it off restores the stored
      preference. A dialog that stays white or navy under high contrast
      means a painter is still asking `IsDark()` instead of
      `IsBranded()`.

- [ ] In the dark theme **nothing is left white or grey**: the four
      numeric fields, the test field, the five group box frames and their
      captions, the check boxes, the two radio buttons and the band behind
      both sliders are all drawn in the blue palette. A white box, a caption
      with a line through it, or a black or white strip running behind a
      slider means that control stopped receiving its colours -- the
      painters for most of them, `WM_CTLCOLORSTATIC` for the two bands. The
      one exception is the test field's cue banner: comctl32 draws that text
      in the system grey by design, and no message changes it.
- [ ] Hovering a check box lightens its indicator, holding the mouse
      button down darkens it, and the keyboard focus draws a dotted
      rectangle around the caption -- the same three states the push
      buttons show.
- [ ] Clicking *Ignore repeated keystrokes* greys both sliders out: their
      thumbs lose the cyan. Every control inside a group box stays where it
      was, and so does the frame's gap at its caption. A field, a radio
      button or a slider that disappears when a frame is repainted means the
      frame is drawing over its own group.
- [ ] Switching the theme back and forth repeatedly leaves no stripes or
      leftovers behind: every painter redraws the slice of the background
      gradient that sits behind its control.

## 3. Keyboard

- [ ] Holding `Alt` underlines the accelerators.
- [ ] `Alt`+`D` lands in *Repeat delay*, `Alt`+`R` in *Repeat rate*,
      `Alt`+`K` presses *Keyboard*, `Alt`+`P` presses *Apply*.
- [ ] `Tab` walks every control in reading order and never skips a field.
- [ ] The arrow keys move between the two radio buttons, and `Tab` leaves
      the pair rather than cycling inside it.
- [ ] `Esc` closes without applying, `Enter` behaves as *OK*.
- [ ] With Narrator running, typing a new *Repeat rate* makes it read the
      new *(x.x per second)* out, and pressing *Apply* makes it read the
      status line -- in both cases without the focus having moved. Typing
      a digit that does not change the value says nothing.

## 4. Sliders and fields

- [ ] Dragging the *Repeat delay* slider updates the field, and typing in
      the field moves the slider.
- [ ] The same for *Repeat rate*, and the *(x.x per second)* read-out
      follows along.
- [ ] Changing a read-out twice leaves nothing of the value in between:
      `300` after `1` in *Repeat rate* shows `(3.3 per second)` and no digits
      of `(1000.0 per second)`. A label with leftovers grows with every
      change, and the *Flags* read-out does the same over the preset
      buttons.
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
- [ ] In the dark theme the tips are blue with white text, not the system
      pale yellow, and they change over immediately when the *Dark theme*
      check box is toggled. In the light theme they look exactly like
      every other tooltip on the machine, including a custom colour
      scheme if one is set.

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
