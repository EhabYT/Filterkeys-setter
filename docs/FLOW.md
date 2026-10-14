# How it flows

Three diagrams for the three questions people ask of this repository:
what the program does, who draws what, and how a change gets from an edit
to a release. They are Mermaid, so GitHub renders them in the browser and
they stay diffable as text.

The behaviour shown in the first diagram is the part that was deliberately
**not** changed in 1.11: the `SPI_*` calls, the registry reads and the five
presets work exactly as they did in 1.10.

## 1. What the program does

Everything the dialog knows sits in its member variables. The five *Load
settings* buttons fill them from five different sources, the fields and
sliders edit them, and *Apply* is the only path that leaves the process.

```mermaid
flowchart TD
    start([Program starts]) --> init[OnInitDialog]
    init --> snap["SPI_GETFILTERKEYS once<br/>kept as the Original snapshot"]
    snap --> cur[Load Current]

    subgraph load["Load settings -- five sources, no side effects"]
        cur["Current<br/>SPI_GETFILTERKEYS"]
        reg["Registry<br/>HKCU Control Panel\\Accessibility\\Keyboard Response"]
        kbd["Keyboard<br/>SPI_GETKEYBOARDDELAY + SPI_GETKEYBOARDSPEED"]
        def["Default<br/>flags 122, the Windows defaults"]
        org["Original<br/>the snapshot from startup"]
    end

    cur --> state
    reg --> state
    kbd --> state
    def --> state
    org --> state

    state["Dialog state<br/>mode, wait, delay, repeat, bounce, 7 flag boxes"]
    state <--> edit["Edit boxes and sliders<br/>kept in sync both ways"]
    state --> readout["Read-outs<br/>characters per second, Flags: n (0xnn)"]

    state --> apply{"Apply / OK"}
    apply --> check{"Any value above 20000?"}
    check -- yes --> err["Error box,<br/>focus the offending field,<br/>nothing is written"]
    check -- no --> spi["SPI_SETFILTERKEYS"]

    spi --> ini{"Save to registry?"}
    ini -- ticked --> flag1["SPIF_UPDATEINIFILE"]
    ini -- clear --> flag2["live only"]
    flag1 --> send
    flag2 --> send
    send{"Broadcast change?"}
    send -- ticked --> flag3["SPIF_SENDCHANGE"]
    send -- clear --> flag4["no WM_SETTINGCHANGE"]

    flag3 --> status
    flag4 --> status
    status["Status line re-reads SPI_GETFILTERKEYS<br/>and reports what Windows is actually using"]
```

The status line is the honest one: the fields show what *would* be applied,
the line underneath shows what Windows has. They differ until *Apply* is
pressed, and that is on purpose.

## 2. Who draws what

Three states, not two. `IsDark()` answers which theme is active;
`IsBranded()` answers whether the palette applies at all, and it is false
exactly when Windows high contrast is on.

```mermaid
flowchart TD
    boot([OnInitDialog / theme toggle / WM_SETTINGCHANGE]) --> hc{"CTheme::IsHighContrast()"}

    hc -- yes --> sys["System palette<br/>IsBranded() = false"]
    hc -- no --> pref{"HKCU Software\\FilterKeysSetter\\Theme"}
    pref -- "1 (default)" --> dark["Dark palette<br/>deep end of the spectrum"]
    pref -- 0 --> light["Light palette<br/>luminous end, white canvas"]

    sys --> handback["Every painter returns false<br/>Windows draws everything<br/>edit frames restored"]

    dark --> apply
    light --> apply
    apply["ApplyToTitleBar, ApplyToControl per child, ApplyToToolTip"]

    apply --> route{"Which control?"}
    route -- "push button, group box,<br/>check box, radio, trackbar" --> cd["Stays with the visual style<br/>so NM_CUSTOMDRAW arrives<br/>-> the painters in FilterKeysSetterDlg.cpp"]
    route -- "edit, static" --> ctl["Detached from the visual style<br/>-> WM_CTLCOLOR* decides the colours"]
    route -- "edit, again" --> frame["Client edge removed<br/>-> PaintEditFrames draws the outline"]
    route -- "tool tip" --> tip["Detached<br/>-> TTM_SETTIPBKCOLOR / TTM_SETTIPTEXTCOLOR"]
    route -- "dialog background" --> bg["OnEraseBkgnd<br/>-> three-stop gradient, 128 bands"]
```

The rule behind the split: a control detached from the visual style sends
no `NM_CUSTOMDRAW`, and a control still attached to it ignores the brush
from `WM_CTLCOLOR*`. Each control has to be on exactly one of the two
paths, and `tools/check-message-map.py` fails the build if one ends up on
both or neither. The colours all come from [BRAND.md](BRAND.md).

## 3. From an edit to a release

```mermaid
flowchart LR
    edit([Edit a file]) --> all["tools/check-all.py"]

    subgraph checks["Eight checkers, under a second, no toolchain"]
        c1["check-dialog-layout.py"]
        c2["check-message-map.py"]
        c3["check-resources.py"]
        c4["check-error-handling.py"]
        c5["check-project.py"]
        c6["check-docs.py"]
        c7["check-theme.py"]
        c8["check-images.py"]
    end

    all --> checks
    checks --> self["selftest.py<br/>53 mutations, each has to be caught"]
    self --> verdict{"all 9 checks passed?"}
    verdict -- no --> edit
    verdict -- yes --> build["tools/build.cmd<br/>Win32 and x64, needs MSVC + MFC"]

    build --> manual["docs/TESTING.md<br/>the part no checker can do"]
    manual --> version["Version in six places<br/>see RELEASE.md"]
    version --> rel["tools/release.cmd v1.11<br/>or the Release workflow"]
    rel --> draft["Draft release with both binaries<br/>notes = the CHANGELOG entry"]
```

Two things this diagram is honest about. The build step has never run:
there is no MSVC in the environment these changes were written in, which
is why there are eight checkers in front of it. And the previews under
`img/` come from `tools/render-dialog.py`, not from a running program --
the first real screenshot is still owed.
