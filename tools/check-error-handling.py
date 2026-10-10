#!/usr/bin/env python3
"""Finds Win32 calls whose result is thrown away.

Three bugs in this project had the same shape: a `SystemParametersInfo` read
that nobody checked, followed by code that happily used the buffer the call
had never filled. The buffer is zeroed, and zero is a plausible value for a
timing, so the dialog presented an invention as a system setting.

This checker looks for calls to the functions below that stand alone as a
statement, with nothing done to their result. Ignoring a result is sometimes
right -- failing to store the theme preference is not worth a message box --
so an explicit `(void)` cast marks that decision and silences the warning.

Usage:  python tools/check-error-handling.py [file.cpp ...]
Exit code 1 if anything looks wrong.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

DEFAULT_SOURCES = ["FilterKeysSetterDlg.cpp", "FilterKeysSetter.cpp", "Theme.cpp"]

# Functions that report failure through their return value and whose failure
# leaves the caller with data it must not use.
WATCHED = [
    "SystemParametersInfo", "SystemParametersInfoW", "SystemParametersInfoA",
    "RegOpenKeyEx", "RegOpenKeyExW", "RegOpenKeyExA",
    "RegCreateKeyEx", "RegCreateKeyExW", "RegCreateKeyExA",
    "RegQueryValueEx", "RegQueryValueExW", "RegQueryValueExA",
    "RegSetValueEx", "RegSetValueExW", "RegSetValueExA",
    "RegDeleteValue", "RegDeleteValueW", "RegDeleteValueA",
    # Deliberately not listed: the MFC wrappers with the same names as Win32
    # calls (CWnd::GetClientRect and friends) return void, so a bare call is
    # the only way to write them.
]

CALL = re.compile(r"\b(?:::)?(" + "|".join(WATCHED) + r")\s*\(")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def statement_prefix(text, position):
    """Everything between the previous statement boundary and the call.

    Only ; { } end a statement. Brackets and colons must not, or `if (!::Fn(`
    would look like a bare call -- the scope operator in `::Fn` is part of
    the call itself.
    """
    start = max(text.rfind(ch, 0, position) for ch in ";{}")
    return text[start + 1:position].strip()


def check(path):
    with open(path, encoding="utf-8-sig", errors="surrogateescape") as handle:
        raw = handle.read()
    text = strip_comments(raw)

    problems = []
    for match in CALL.finditer(text):
        prefix = statement_prefix(text, match.start())
        if prefix not in ("", "::"):
            continue        # assigned, negated, compared, returned -- fine
        line = text.count("\n", 0, match.start()) + 1
        problems.append("%s:%d: the result of %s is thrown away; check it or "
                        "write (void) to say the failure is irrelevant"
                        % (os.path.basename(path), line, match.group(1)))
    return problems


def main():
    argv = sys.argv[1:]
    paths = argv or [os.path.join(ROOT, name) for name in DEFAULT_SOURCES]

    problems = []
    for path in paths:
        if os.path.exists(path):
            problems += check(path)

    for problem in problems:
        print("  ! " + problem)
    print("%d file(s), %d problem(s)" % (len(paths), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
