#!/usr/bin/env python3
"""Check that the theme stays the single source of colour in the program.

The palettes live in two aggregate initialisers in Theme.cpp and everything
else is supposed to name a role -- clrAccent, clrSurface -- rather than a
colour. Four ways that can quietly stop being true, none of which the
compiler minds:

  * a painter hard-codes an RGB() value, so a colour exists that no palette
    knows about and no theme switch reaches;
  * a system colour is read outside the high contrast fallback, which puts
    a Windows grey back into a branded dialog;
  * a grey stock brush is used as a fallback, same result;
  * a field is added to ThemePalette and one of the two initialisers is not
    extended. Aggregate initialisation fills the missing field with zero,
    so the new colour silently becomes black in that theme.

The last one is the reason this file exists: it produces no warning at any
level, and black is a plausible enough colour that it can survive review.

Usage:  python tools/check-theme.py [repository root]
Exit code 1 if anything is reported.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

THEME_H = "Theme.h"
THEME_CPP = "Theme.cpp"
SOURCES = ["FilterKeysSetterDlg.cpp", "FilterKeysSetter.cpp"]

# GetSysColor belongs to the high contrast fallback and to the tool tip
# colours that go with it. Anywhere else it is a Windows colour entering a
# branded dialog.
SYSCOLOR_FUNCTIONS = ("MakeSystemPalette", "CTheme::ApplyToToolTip")

GREY_BRUSHES = ("GRAY_BRUSH", "DKGRAY_BRUSH", "LTGRAY_BRUSH")


def read(path):
    with open(path, encoding="utf-8-sig") as handle:
        return handle.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def palette_fields(header):
    """The members of struct ThemePalette, in declaration order."""
    block = re.search(r"struct\s+ThemePalette\s*\{(.*?)\n\};", header, flags=re.S)
    if not block:
        return None
    return re.findall(r"COLORREF\s+(\w+)\s*;", block.group(1))


def initialisers(source):
    """name -> (list of role comments, number of RGB() entries, line)."""
    found = {}
    for match in re.finditer(r"const ThemePalette k(\w+)Palette\s*=\s*\{(.*?)\n\t\};",
                             source, flags=re.S):
        roles, count = [], 0
        for line in match.group(2).splitlines():
            if re.search(r"RGB\(", line):
                count += 1
                role = re.search(r"//\s*([a-z ]+?)\s{2,}#", line)
                roles.append(role.group(1).strip() if role else None)
        found[match.group(1)] = (roles, count,
                                 source[:match.start()].count("\n") + 1)
    return found


def check_initialisers(root):
    problems = []
    header = os.path.join(root, THEME_H)
    source = os.path.join(root, THEME_CPP)
    if not (os.path.isfile(header) and os.path.isfile(source)):
        return ["%s or %s is missing" % (THEME_H, THEME_CPP)]

    fields = palette_fields(read(header))
    if fields is None:
        return ["%s: no struct ThemePalette found" % THEME_H]

    found = initialisers(read(source))
    if not found:
        return ["%s: no ThemePalette initialiser found" % THEME_CPP]

    for name in sorted(found):
        roles, count, line = found[name]
        if count != len(fields):
            problems.append("%s:%d: k%sPalette sets %d of the %d fields in "
                            "ThemePalette; the rest are silently zero, which "
                            "is black" % (THEME_CPP, line, name, count,
                                          len(fields)))
        missing = [role for role in roles if role is None]
        if missing:
            problems.append("%s:%d: k%sPalette has %d entr%s without a role "
                            "comment, so tools/render-palette.py cannot label "
                            "them" % (THEME_CPP, line, name, len(missing),
                                      "y" if len(missing) == 1 else "ies"))

    orders = {name: tuple(roles) for name, (roles, _c, _l) in found.items()
              if all(roles)}
    if len(set(orders.values())) > 1:
        problems.append("%s: the palettes list their roles in different "
                        "orders: %s" % (THEME_CPP, "; ".join(
                            "%s = %s" % (name, ", ".join(roles))
                            for name, roles in sorted(orders.items()))))
    return problems


def check_literals(root):
    """RGB() belongs to Theme.cpp; everywhere else a colour has to be a role."""
    problems = []
    for name in SOURCES:
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        text = strip_comments(read(path))
        for match in re.finditer(r"\bRGB\s*\(", text):
            line = text[:match.start()].count("\n") + 1
            problems.append("%s:%d: RGB() outside %s -- a colour here belongs "
                            "to no palette and no theme switch reaches it"
                            % (name, line, THEME_CPP))
    return problems


def check_system_colours(root):
    problems = []
    for name in [THEME_CPP] + SOURCES:
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        text = strip_comments(read(path))

        # Where each function starts, so a hit can be attributed to one.
        bounds = []
        keywords = ("if", "for", "while", "switch", "catch", "else", "do")
        for match in re.finditer(r"\n[\t ]*(?:[\w:&*<>,\t ]+?)\b([\w:]+)\s*\([^;{]*\)\s*(?:const\s*)?\{",
                                 text):
            if match.group(1) in keywords:
                continue
            bounds.append((match.start(), match.group(1)))

        def enclosing(position):
            name_here = None
            for start, function in bounds:
                if start <= position:
                    name_here = function
                else:
                    break
            return name_here

        for match in re.finditer(r"\bGetSysColor\s*\(", text):
            function = enclosing(match.start())
            if function in SYSCOLOR_FUNCTIONS:
                continue
            line = text[:match.start()].count("\n") + 1
            problems.append("%s:%d: GetSysColor in %s; system colours belong "
                            "to the high contrast fallback only"
                            % (name, line, function or "file scope"))

        for brush in GREY_BRUSHES:
            for match in re.finditer(r"\b%s\b" % brush, text):
                line = text[:match.start()].count("\n") + 1
                problems.append("%s:%d: %s -- the palette has no greys, not "
                                "even as a fallback" % (name, line, brush))
    return problems


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else ROOT
    problems = []
    problems += check_initialisers(root)
    problems += check_literals(root)
    problems += check_system_colours(root)

    for problem in problems:
        print("  ! %s" % problem)
    print("%d problem(s)" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
