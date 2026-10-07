#!/usr/bin/env python3
"""Static check for the resource bookkeeping nobody ever looks at.

Two kinds of damage are easy to do by hand and invisible until much later:

  * resource.h drifting out of step -- two symbols sharing a numeric value
    (the dialog manager then wires DDX to the wrong control), an _APS_NEXT_*
    counter that has fallen behind, a symbol used in the .rc but never
    defined, or one defined and never used
  * the version number living in five places and only being bumped in four

Usage:  python tools/check-resources.py [repo root]
Exit code 1 if anything looks wrong; notes alone do not fail the run.
"""

import os
import re
import sys

# Numbers from Windows and MFC that resource.h legitimately does not define.
WELL_KNOWN = {
    "IDOK", "IDCANCEL", "IDABORT", "IDRETRY", "IDIGNORE", "IDYES", "IDNO",
    "IDCLOSE", "IDHELP", "IDC_STATIC", "IDR_MAINFRAME", "IDS_ABOUTBOX",
    "IDM_ABOUTBOX", "ID_APP_ABOUT", "ID_APP_EXIT",
}

# Which _APS_NEXT_* counter guards which prefix.
COUNTERS = {
    "_APS_NEXT_RESOURCE_VALUE": ("IDD_", "IDR_", "IDS_", "IDB_", "IDI_"),
    "_APS_NEXT_CONTROL_VALUE": ("IDC_",),
    "_APS_NEXT_COMMAND_VALUE": ("ID_", "IDM_"),
}


def read(path):
    with open(path, encoding="utf-8-sig", errors="surrogateescape") as handle:
        return handle.read()


def parse_resource_h(text):
    """symbol -> (value, line number), ignoring the APSTUDIO counters."""
    symbols = {}
    counters = {}
    for number, line in enumerate(text.splitlines(), 1):
        match = re.match(r"#define\s+(\w+)\s+(0x[0-9A-Fa-f]+|\d+)", line.strip())
        if not match:
            continue
        name, raw = match.group(1), match.group(2)
        value = int(raw, 16) if raw.lower().startswith("0x") else int(raw)
        if name.startswith("_APS_"):
            counters[name] = value
        else:
            symbols[name] = (value, number)
    return symbols, counters


def prefix_of(name):
    for group in ("IDD_", "IDC_", "IDS_", "IDR_", "IDB_", "IDI_", "IDM_", "ID_"):
        if name.startswith(group):
            return group
    return "?"


def check_symbols(symbols, counters):
    problems, notes = [], []

    # Values only have to be unique inside one family: a dialog and a string
    # may share a number, two controls of the same dialog may not.
    by_family = {}
    for name, (value, line) in symbols.items():
        by_family.setdefault((prefix_of(name), value), []).append((name, line))
    for (group, value), names in sorted(by_family.items()):
        if len(names) > 1:
            problems.append("%s symbols share the value %d: %s"
                            % (group, value,
                               ", ".join("%s (line %d)" % n for n in sorted(names))))

    for counter, groups in COUNTERS.items():
        if counter not in counters:
            notes.append("%s is missing from resource.h" % counter)
            continue
        used = [v for name, (v, _) in symbols.items()
                if any(name.startswith(g) for g in groups)]
        if used and counters[counter] <= max(used):
            problems.append("%s is %d but %d is already taken -- the dialog "
                            "editor would reuse an ID"
                            % (counter, counters[counter], max(used)))
    return problems, notes


def sources(root):
    found = {}
    for folder, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs
                   if d not in (".git", "docs", "tools", "x64", "Debug", "Release")]
        for name in files:
            if name.endswith((".cpp", ".h", ".rc")) and name != "resource.h":
                path = os.path.join(folder, name)
                found[path] = read(path)
    return found


def check_usage(symbols, files):
    problems, notes = [], []
    blob = "\n".join(files.values())
    used = set(re.findall(r"\b(ID[A-Z]*_\w+|IDOK|IDCANCEL)\b", blob))

    for name in sorted(used - set(symbols) - WELL_KNOWN):
        # MFC defines plenty of its own; only flag what looks project-local.
        if name.startswith(("IDC_", "IDD_")):
            problems.append("%s is used but not defined in resource.h" % name)

    for name in sorted(set(symbols) - used - WELL_KNOWN):
        notes.append("%s is defined in resource.h but never used" % name)
    return problems, notes


def check_versions(root):
    problems, notes = [], []
    rc = read(os.path.join(root, "FilterKeysSetter.rc"))

    found = {}
    tuples = re.findall(r"\b(FILEVERSION|PRODUCTVERSION)\s+([\d,\s]+)", rc)
    for key, raw in tuples:
        found[key] = ".".join(part.strip() for part in raw.split(","))
    for key in ("FileVersion", "ProductVersion"):
        match = re.search(r'VALUE\s+"%s",\s*"([\d.]+)"' % key, rc)
        if match:
            found["VALUE " + key] = match.group(1)

    if not found:
        notes.append("no VERSIONINFO found in the .rc")
        return problems, notes
    if len(set(found.values())) > 1:
        # No point comparing the other files against a version that does not
        # agree with itself -- that would just repeat the same fault four times.
        problems.append("VERSIONINFO disagrees with itself: "
                        + ", ".join("%s = %s" % kv for kv in sorted(found.items())))
        return problems, notes

    canonical = sorted(found.values())[0]          # e.g. 1.0.11.0
    short = ".".join(canonical.split(".")[:3])     # e.g. 1.0.11
    marketing = "%s.%s" % (canonical.split(".")[0], canonical.split(".")[2])

    about = re.search(r'"FilterKeysSetter Version ([\d.]+)"', rc)
    if about and about.group(1) != marketing:
        problems.append('the about box says "Version %s" but VERSIONINFO says %s'
                        % (about.group(1), canonical))

    setup = os.path.join(root, "FilterKeysSetter.Setup",
                         "FilterKeysSetter.Setup.vdproj")
    if os.path.exists(setup):
        match = re.search(r'"ProductVersion"\s*=\s*"8:([\d.]+)"', read(setup))
        if match and match.group(1) != short:
            problems.append("the installer builds %s but the binary is %s"
                            % (match.group(1), canonical))

    readme = os.path.join(root, "README.md")
    if os.path.exists(readme):
        match = re.search(r"^\*\s*([\d.]+)\s", read(readme), re.M)
        if match and match.group(1) != marketing:
            problems.append("the newest README version history entry is %s, "
                            "the binary is %s" % (match.group(1), marketing))

    return problems, notes


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    symbols, counters = parse_resource_h(read(os.path.join(root, "resource.h")))

    problems, notes = [], []
    for part in (check_symbols(symbols, counters),
                 check_usage(symbols, sources(root)),
                 check_versions(root)):
        problems += part[0]
        notes += part[1]

    for note in notes:
        print("note: " + note)
    for problem in problems:
        print("  ! " + problem)
    print("%d symbol(s), %d problem(s)" % (len(symbols), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
