#!/usr/bin/env python3
"""Static sanity check for the dialog layout in FilterKeysSetter.rc.

Catches the mistakes that are easy to make when editing dialog resources by
hand and that otherwise only show up once the program is running:

  * controls sticking out of the dialog
  * controls overlapping each other
  * controls poking out of the group box they visually belong to
  * captions too long for their control at the dialog font
  * the same Alt accelerator claimed by two controls
  * a label accelerator that does not hand the focus to its own input
  * a focusable CONTROL statement without WS_TABSTOP

The text width model is an approximation of Segoe UI, so treat its warnings as
"look at this in the dialog editor", not as hard failures.

Usage:  python tools/check-dialog-layout.py [path/to/FilterKeysSetter.rc]
Exit code 1 if anything looks wrong.
"""

import re
import sys

# Relative advance widths, normalised so an average character is 4 DLU
# (1 horizontal DLU is defined as a quarter of the average character width).
NARROW = set("ijlItf.,:;'\"!|()[]{} /")
WIDE = set("mwMW@%")
CAPS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")

CHECKBOX_GLYPH_DLU = 11   # box plus the gap before the label
BUTTON_PADDING_DLU = 8    # horizontal padding inside a push button
LINE_HEIGHT_DLU = 9       # one line of Segoe UI 9 pt


def text_width(text):
    text = text.replace("&&", "\x00").replace("&", "").replace("\x00", "&")
    total = 0.0
    for ch in text:
        if ch in NARROW:
            total += 1.9
        elif ch in WIDE:
            total += 6.2
        elif ch in CAPS:
            total += 4.6
        else:
            total += 3.8
    return total


def parse_dialog(rc, name):
    block = rc[rc.index(name + " DIALOGEX"):]
    head = block[:block.index("BEGIN")]
    body = block[block.index("BEGIN") + 5:block.index("\nEND")]

    size = re.search(r"DIALOGEX\s+\d+,\s*\d+,\s*(\d+),\s*(\d+)", head)
    width, height = int(size.group(1)), int(size.group(2))

    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r",\s*\n\s+", ", ", body)   # join continuation lines

    controls = []
    for line in body.splitlines():
        line = line.strip()
        if not line:
            continue
        rect = re.findall(r"(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)", line)
        if not rect:
            continue
        x, y, w, h = (int(v) for v in rect[-1])
        ident = re.search(r"\b(ID[CORK]\w*|IDOK|IDCANCEL)\b", line)
        caption = re.match(r"\w+\s+\"([^\"]*)\"", line)
        controls.append({
            "kind": line.split()[0],
            "id": ident.group(1) if ident else "?",
            "rect": (x, y, w, h),
            "text": caption.group(1) if caption else "",
            "line": line,
        })
    return width, height, controls


def mnemonics(controls):
    """control -> accelerator letter, from the & in its caption."""
    found = []
    for ctl in controls:
        text = ctl["text"]
        i = 0
        while i < len(text) - 1:
            if text[i] == "&":
                if text[i + 1] == "&":
                    i += 2
                    continue
                found.append((ctl, text[i + 1].lower()))
                break
            i += 1
    return found


STATIC_KINDS = ("LTEXT", "RTEXT", "CTEXT", "GROUPBOX", "ICON")

# Window classes that can take the focus when they are written as CONTROL.
FOCUSABLE_CLASSES = ("Button", "Edit", "ComboBox", "ListBox",
                     "msctls_trackbar32", "msctls_updown32", "SysListView32",
                     "SysTreeView32", "SysTabControl32")


def is_focusable(ctl):
    """Can the dialog manager put the focus on this control?"""
    if ctl["kind"] in ("EDITTEXT", "PUSHBUTTON", "DEFPUSHBUTTON", "COMBOBOX",
                       "LISTBOX", "SCROLLBAR"):
        return True
    if ctl["kind"] != "CONTROL":
        return False
    return any(('"%s"' % c) in ctl["line"] for c in FOCUSABLE_CLASSES)


def check_focus_targets(controls):
    """A static's accelerator moves the focus to the *next* control in z-order.

    That is only useful when the next control really is the input the label
    describes, so the pairing is checked here rather than trusted.
    """
    problems = []
    for index, ctl in enumerate(controls):
        if ctl["kind"] not in ("LTEXT", "RTEXT", "CTEXT"):
            continue
        letters = dict((c["id"], l) for c, l in mnemonics([ctl]))
        if not letters:
            continue
        following = controls[index + 1:]
        target = next((c for c in following if is_focusable(c)), None)
        if target is None:
            problems.append('%s: caption "%s" has an accelerator but no '
                            'focusable control follows it'
                            % (ctl["id"], ctl["text"]))
            continue
        if following[0] is not target:
            problems.append('%s: accelerator jumps past %s to %s'
                            % (ctl["id"], following[0]["id"], target["id"]))
        lx, ly, lw, lh = ctl["rect"]
        tx, ty, tw, th = target["rect"]
        if min(ly + lh, ty + th) - max(ly, ty) <= 0:
            problems.append('%s sits on row %d but its accelerator focuses %s '
                            'on row %d' % (ctl["id"], ly, target["id"], ty))
    return problems


def check_tabstops(controls):
    """CONTROL statements get no implicit WS_TABSTOP, unlike EDITTEXT & co."""
    problems = []
    previous_radio = False
    for ctl in controls:
        radio = "BS_AUTORADIOBUTTON" in ctl["line"] or "BS_RADIOBUTTON" in ctl["line"]
        if ctl["kind"] == "CONTROL" and is_focusable(ctl):
            if "WS_TABSTOP" not in ctl["line"] and not (radio and previous_radio):
                problems.append("%s is focusable but has no WS_TABSTOP"
                                % ctl["id"])
            if radio and not previous_radio and "WS_GROUP" not in ctl["line"]:
                problems.append("%s starts a radio group but has no WS_GROUP"
                                % ctl["id"])
        previous_radio = radio
    return problems


def overlap(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return (min(ax + aw, bx + bw) - max(ax, bx),
            min(ay + ah, by + bh) - max(ay, by))


def contains(outer, inner):
    ox, oy, ow, oh = outer
    ix, iy, iw, ih = inner
    return ox <= ix and oy <= iy and ix + iw <= ox + ow and iy + ih <= oy + oh


def check(rc, name):
    width, height, controls = parse_dialog(rc, name)
    problems = []

    groups = [c for c in controls if c["kind"] == "GROUPBOX"]
    others = [c for c in controls if c["kind"] != "GROUPBOX"]

    for c in controls:
        x, y, w, h = c["rect"]
        if x < 0 or y < 0 or x + w > width or y + h > height:
            problems.append("%s sticks out of the dialog at %d,%d %dx%d"
                            % (c["id"], x, y, w, h))

    for i in range(len(others)):
        for j in range(i + 1, len(others)):
            ox, oy = overlap(others[i]["rect"], others[j]["rect"])
            if ox > 0 and oy > 0:
                problems.append("%s and %s overlap by %dx%d DLU"
                                % (others[i]["id"], others[j]["id"], ox, oy))

    for c in others:
        touching = [g for g in groups
                    if all(v > 0 for v in overlap(g["rect"], c["rect"]))]
        if touching and not any(contains(g["rect"], c["rect"]) for g in touching):
            problems.append("%s pokes out of its group box" % c["id"])

    for c in others:
        if not c["text"]:
            continue
        x, y, w, h = c["rect"]
        avail = w
        if "BS_AUTOCHECKBOX" in c["line"] or "BS_AUTORADIOBUTTON" in c["line"]:
            avail -= CHECKBOX_GLYPH_DLU
        if c["kind"] in ("PUSHBUTTON", "DEFPUSHBUTTON"):
            avail -= BUTTON_PADDING_DLU
        rows = max(1, h // LINE_HEIGHT_DLU) if "BS_MULTILINE" in c["line"] else 1

        capacity = avail * rows
        needed = text_width(c["text"])
        if capacity <= 0 or needed > capacity:
            problems.append('%s: caption "%s" needs ~%d DLU but has %d'
                            % (c["id"], c["text"], needed, capacity))

    seen = {}
    for ctl, letter in mnemonics(controls):
        if letter in seen:
            problems.append('accelerator Alt+%s is used twice: %s and %s'
                            % (letter.upper(), seen[letter], ctl["id"]))
        else:
            seen[letter] = ctl["id"]

    problems += check_focus_targets(controls)
    problems += check_tabstops(controls)

    return width, height, len(controls), problems


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "FilterKeysSetter.rc"
    with open(path, encoding="utf-8-sig") as handle:
        rc = handle.read()

    failed = False
    for name in ("IDD_FILTERKEYSSETTER_DIALOG", "IDD_ABOUTBOX"):
        width, height, count, problems = check(rc, name)
        print("%s  (%d x %d DLU, %d controls)" % (name, width, height, count))
        if problems:
            failed = True
            for p in problems:
                print("  ! " + p)
        else:
            print("  ok")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
