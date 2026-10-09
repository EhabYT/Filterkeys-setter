#!/usr/bin/env python3
"""Self test for the five static checkers in this folder.

Every rule in check-dialog-layout.py, check-message-map.py,
check-resources.py and check-error-handling.py was originally verified the same way: break the source on
purpose, confirm the checker notices, put it back. That verification used to
live in the commit messages only, so a later refactor of a checker could turn
a rule into a no-op without anything failing.

This script replays those mutations. It copies the repository into a scratch
directory, applies one change at a time, runs the matching checker against the
copy and insists that it exits non-zero *and* says the expected thing. The
real working tree is never modified.

Usage:  python tools/selftest.py [-v]
Exit code 1 if any rule failed to fire, or if the unmodified tree is not clean.
"""

import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

RC = "FilterKeysSetter.rc"
DLG = "FilterKeysSetterDlg.cpp"
HDR = "FilterKeysSetterDlg.h"
RES = "resource.h"
VDPROJ = os.path.join("FilterKeysSetter.Setup", "FilterKeysSetter.Setup.vdproj")
VCXPROJ = "FilterKeysSetter.vcxproj"
SLN = "FilterKeysSetter.sln"

LAYOUT = "check-dialog-layout.py"
MAP = "check-message-map.py"
RESOURCES = "check-resources.py"
ERRORS = "check-error-handling.py"
PROJECT = "check-project.py"
DOCS = "check-docs.py"

# (name, checker, file, text to replace, replacement, expected in the output)
CASES = [
    # -- check-dialog-layout.py ------------------------------------------
    ("control pushed off the dialog", LAYOUT, RC,
     '"A&pply",IDC_APPLY,174,256,48,14',
     '"A&pply",IDC_APPLY,174,256,80,14',
     "sticks out of the dialog"),

    ("two controls on top of each other", LAYOUT, RC,
     '"Cancel",IDCANCEL,120,256,48,14',
     '"Cancel",IDCANCEL,70,256,48,14',
     "overlap"),

    ("control pushed out of its group box", LAYOUT, RC,
     'IDC_TEST_EDIT,12,218,204,13',
     'IDC_TEST_EDIT,12,226,204,13',
     "pokes out of its group box"),

    ("caption too long for its button", LAYOUT, RC,
     '"Defau&lt",IDC_SET_DEFAULTS',
     '"Restore the Windows defaults",IDC_SET_DEFAULTS',
     "needs ~"),

    ("the same accelerator used twice", LAYOUT, RC,
     '"Ke&y click"',
     '"&Key click"',
     "accelerator Alt+K is used twice"),

    ("accelerator on a label that focuses the wrong row", LAYOUT, RC,
     '"ms",IDC_STATIC,108,56',
     '"m&s",IDC_STATIC,108,56',
     "its accelerator focuses"),

    ("focusable CONTROL without WS_TABSTOP", LAYOUT, RC,
     'BS_AUTOCHECKBOX | WS_TABSTOP,144,16',
     'BS_AUTOCHECKBOX,144,16',
     "has no WS_TABSTOP"),

    ("radio group without WS_GROUP", LAYOUT, RC,
     'BS_MULTILINE | WS_GROUP | WS_TABSTOP',
     'BS_MULTILINE | WS_TABSTOP',
     "has no WS_GROUP"),

    # -- check-message-map.py --------------------------------------------
    ("handler mapped but not declared", MAP, HDR,
     "afx_msg void OnBnClickedSetDefaults();",
     "",
     "mapped but not declared"),

    ("handler declared but never mapped", MAP, DLG,
     "ON_BN_CLICKED(IDC_SET_DEFAULTS, OnBnClickedSetDefaults)",
     "",
     "declared afx_msg but no message"),

    ("message map entry with an unknown control ID", MAP, DLG,
     "ON_BN_CLICKED(IDC_SET_DEFAULTS,",
     "ON_BN_CLICKED(IDC_SET_DEFALUTS,",
     "unknown control ID"),

    ("tool tip on a static without SS_NOTIFY", MAP, RC,
     'IDC_STATUS,6,240,216,12,SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOTIFY',
     'IDC_STATUS,6,240,216,12,SS_CENTERIMAGE | SS_ENDELLIPSIS',
     "the static has no"),

    # -- check-project.py -------------------------------------------------
    ("project references a file that is gone", PROJECT, VCXPROJ,
     '<ClInclude Include="Theme.h" />',
     '<ClInclude Include="Theme-renamed.h" />',
     "which does not exist"),

    ("source file not in the project", PROJECT, VCXPROJ,
     '<ClCompile Include="Theme.cpp" />',
     "",
     "not in the project"),

    ("a configuration dropped from the project", PROJECT, VCXPROJ,
     '<ProjectConfiguration Include="Release|Win32">',
     '<ProjectConfiguration Include="Release|ARM64">',
     "no Release|Win32 configuration"),

    ("solution no longer maps a platform", PROJECT, SLN,
     ".Release|x86.ActiveCfg = Release|Win32",
     ".Release|x86.ActiveCfg = Release|x64",
     "no solution configuration builds"),

    ("translation unit not starting with pch.h", PROJECT, "Theme.cpp",
     '#include "pch.h"',
     '#include "Theme.h"',
     "needs #include"),

    ("installer downgraded to a parallel install", PROJECT, VDPROJ,
     '"RemovePreviousVersions" = "11:TRUE"',
     '"RemovePreviousVersions" = "11:FALSE"',
     "installs beside the old one"),

    ("installer GUIDs collapsed into one", PROJECT, VDPROJ,
     '"UpgradeCode" = "8:{95D7E837-A095-4D68-9EC3-B4CA7B9C1EEB}"',
     '"UpgradeCode" = "8:{7D28C7CF-03E9-4674-B7C1-4AE6F7B04070}"',
     "could not upgrade"),

    ("installer packaging a path the build never writes", PROJECT, VDPROJ,
     '"SourcePath" = "8:..\\\\x64\\\\Release\\\\FilterKeysSetter.exe"',
     '"SourcePath" = "8:..\\\\bin\\\\FilterKeysSetter.exe"',
     "not an output path of the project"),

    ("changelog left behind at the old version", RESOURCES, "CHANGELOG.md",
     "## 1.11 -- 2026-10-07", "## 1.10 -- 2026-10-07",
     "newest CHANGELOG.md entry"),

    ("release workflow pointed at the solution", PROJECT,
     "docs/workflows/release.yml",
     "msbuild FilterKeysSetter.vcxproj", "msbuild FilterKeysSetter.sln",
     "but the solution only knows"),

    # -- check-docs.py ----------------------------------------------------
    ("documentation link to a file that is gone", DOCS, "README.md",
     "[CHANGELOG.md](CHANGELOG.md)", "[CHANGELOG.md](CHANGES.md)",
     "which does not exist"),

    ("documentation link to a heading that was reworded", DOCS, "README.md",
     "docs/UI-UPGRADE.md#keyboard-accelerators",
     "docs/UI-UPGRADE.md#keyboard-shortcuts",
     "has no such heading"),

    ("documentation naming a tool that is not there", DOCS,
     os.path.join("docs", "UI-UPGRADE.md"),
     "`tools/render-dialog.py`", "`tools/draw-dialog.py`",
     "which does not exist"),

    ("accelerator table out of step with the resource", DOCS,
     os.path.join("docs", "UI-UPGRADE.md"),
     "| T | Dark **t**heme |", "| Z | Dark theme |",
     "documents no accelerator for Alt+T"),

    ("contrast ratio drifting from the colours", DOCS,
     os.path.join("docs", "UI-UPGRADE.md"),
     "| `#FFFFFF` | `#14263F` | 15.2:1 |",
     "| `#FFFFFF` | `#14263F` | 12.2:1 |",
     "it is 15.23:1"),

    ("colour in the documentation that the code does not define", DOCS,
     os.path.join("docs", "UI-UPGRADE.md"),
     "| Surface (edit fields) | `#1D5188` | `COLOR_WINDOW` |",
     "| Surface (edit fields) | `#1D5199` | `COLOR_WINDOW` |",
     "which Theme.cpp does not define"),

    # -- check-error-handling.py -----------------------------------------
    ("SystemParametersInfo result dropped", ERRORS, DLG,
     "\tm_bHaveOriginal = !!SystemParametersInfo(SPI_GETFILTERKEYS,",
     "\tSystemParametersInfo(SPI_GETFILTERKEYS,",
     "the result of SystemParametersInfo is thrown away"),

    ("deliberate (void) marker removed", ERRORS, "Theme.cpp",
     "(void)::RegSetValueEx(",
     "::RegSetValueEx(",
     "the result of RegSetValueEx is thrown away"),

    ("notification handler missing its LRESULT argument", MAP, HDR,
     "afx_msg void OnCustomDrawSlider(NMHDR* pNMHDR, LRESULT* pResult);",
     "afx_msg void OnCustomDrawSlider(NMHDR* pNMHDR);",
     "ON_NOTIFY needs OnCustomDrawSlider declared as"),

    ("handler returning the wrong type", MAP, HDR,
     "afx_msg BOOL OnEraseBkgnd(CDC* pDC);",
     "afx_msg void OnEraseBkgnd(CDC* pDC);",
     "ON_WM_ERASEBKGND needs OnEraseBkgnd declared as"),

    ("wrapper bound to the wrong kind of control", MAP, HDR,
     "CSliderCtrl m_sliderDelay;", "CButton m_sliderDelay;",
     "binds it to a CButton"),

    ("trackbar carrying a button style", LAYOUT, RC,
     'IDC_DELAY_SLIDER,"msctls_trackbar32",TBS_NOTICKS',
     'IDC_DELAY_SLIDER,"msctls_trackbar32",BS_AUTOCHECKBOX',
     "a style of Button"),

    # -- check-resources.py ----------------------------------------------
    ("two symbols sharing a numeric value", RESOURCES, RES,
     "#define IDC_STATUS                      1028",
     "#define IDC_STATUS                      1027",
     "share the value 1027"),

    ("_APS_NEXT_CONTROL_VALUE left behind", RESOURCES, RES,
     "#define _APS_NEXT_CONTROL_VALUE         1029",
     "#define _APS_NEXT_CONTROL_VALUE         1021",
     "is already taken"),

    ("control ID used but not defined", RESOURCES, RES,
     "#define IDC_DARKTHEME                   1027\n",
     "",
     "used but not defined"),

    ("VERSIONINFO disagreeing with itself", RESOURCES, RC,
     'VALUE "FileVersion", "1.0.11.0"',
     'VALUE "FileVersion", "1.0.10.0"',
     "VERSIONINFO disagrees with itself"),

    ("about box left on the old version", RESOURCES, RC,
     "FilterKeysSetter Version 1.11",
     "FilterKeysSetter Version 1.10",
     "the about box says"),

    ("installer version not bumped", RESOURCES, VDPROJ,
     '"ProductVersion" = "8:1.0.11"',
     '"ProductVersion" = "8:1.0.10"',
     "the installer builds"),

    ("icon file missing", RESOURCES, RC,
     'IDR_MAINFRAME           ICON                    "res\\\\FilterKeysSetter.ico"',
     'IDR_MAINFRAME           ICON                    "res\\\\Missing.ico"',
     "not found"),

    ("README history ahead of the binary", RESOURCES, "README.md",
     "* 1.11 ",
     "* 1.12 ",
     "README version history entry"),
]


def drop_frames(data, sizes):
    """Rebuilds an .ico without the given frame sizes, for the mutation test."""
    count = struct.unpack("<H", data[4:6])[0]
    entries, payloads = [], []
    for index in range(count):
        entry = bytearray(data[6 + index * 16:22 + index * 16])
        size, length, offset = (entry[0] or 256,
                                struct.unpack("<I", entry[8:12])[0],
                                struct.unpack("<I", entry[12:16])[0])
        if size in sizes:
            continue
        entries.append(entry)
        payloads.append(data[offset:offset + length])

    out = struct.pack("<HHH", 0, 1, len(entries))
    position = 6 + 16 * len(entries)
    for entry, payload in zip(entries, payloads):
        entry[12:16] = struct.pack("<I", position)
        position += len(payload)
        out += bytes(entry)
    return out + b"".join(payloads)


def read(path):
    with open(path, encoding="utf-8-sig", errors="surrogateescape") as handle:
        return handle.read()


def write(path, text):
    with open(path, "w", encoding="utf-8-sig", errors="surrogateescape",
              newline="") as handle:
        handle.write(text)


def run(checker, sandbox):
    """Runs one checker against the scratch copy and returns (code, output)."""
    # Always the copy of the checker inside the sandbox, so that editing a
    # checker is covered by this test as well.
    script = os.path.join(sandbox, "tools", checker)
    if checker in (RESOURCES, PROJECT, DOCS):
        argv = [sys.executable, script, sandbox]
    elif checker == LAYOUT:
        argv = [sys.executable, script, os.path.join(sandbox, RC)]
    else:
        argv = [sys.executable, script]
    done = subprocess.run(argv, cwd=sandbox, capture_output=True, text=True)
    return done.returncode, done.stdout + done.stderr


def copy_tree(destination):
    # The whole tree apart from the repository metadata and build output:
    # the workflow files under docs/workflows are checked like any other
    # build script, and check-docs.py follows links into docs/img.
    shutil.copytree(ROOT, destination,
                    ignore=shutil.ignore_patterns(".git", "x64",
                                                  "Debug", "Release", "*.log"))


def main():
    verbose = "-v" in sys.argv
    failures = []
    ran = 0

    with tempfile.TemporaryDirectory() as tmp:
        sandbox = os.path.join(tmp, "tree")
        copy_tree(sandbox)

        # A mutation test only means something if the baseline is clean.
        for checker in (LAYOUT, MAP, RESOURCES, ERRORS, PROJECT, DOCS):
            code, output = run(checker, sandbox)
            if code != 0:
                failures.append("baseline: %s already fails:\n%s"
                                % (checker, output))
            elif verbose:
                print("baseline %-24s clean" % checker)

        for name, checker, target, old, new, expected in CASES:
            ran += 1
            path = os.path.join(sandbox, target)
            original = read(path)
            if original.count(old) < 1:
                failures.append('%s: the text to mutate is no longer in %s'
                                % (name, target))
                continue
            write(path, original.replace(old, new, 1))
            code, output = run(checker, sandbox)
            write(path, original)

            if code == 0:
                failures.append("%s: %s did not notice" % (name, checker))
            elif expected not in output:
                failures.append('%s: %s complained, but not about "%s":\n%s'
                                % (name, checker, expected, output.strip()))
            elif verbose:
                print("ok  %-48s %s" % (name, checker))

        # The icon is binary, so it gets its own pair of cases rather than a
        # text replacement.
        icon = os.path.join(sandbox, "res", "FilterKeysSetter.ico")
        original = open(icon, "rb").read()
        for name, broken, expected in (
                ("truncated icon file", original[:400], "damaged"),
                ("icon without a 256 px frame", drop_frames(original, (256,)),
                 "no 256 px frame")):
            ran += 1
            with open(icon, "wb") as handle:
                handle.write(broken)
            code, output = run(RESOURCES, sandbox)
            with open(icon, "wb") as handle:
                handle.write(original)
            if code == 0:
                failures.append("%s: %s did not notice" % (name, RESOURCES))
            elif expected not in output:
                failures.append('%s: complained, but not about "%s":\n%s'
                                % (name, expected, output.strip()))
            elif verbose:
                print("ok  %-48s %s" % (name, RESOURCES))

        # Nothing may be left behind in the scratch copy.
        for checker in (LAYOUT, MAP, RESOURCES, ERRORS, PROJECT, DOCS):
            code, output = run(checker, sandbox)
            if code != 0:
                failures.append("after restoring, %s fails:\n%s"
                                % (checker, output))

    for failure in failures:
        print("  ! " + failure)
    print("%d case(s), %d failure(s)" % (ran, len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
