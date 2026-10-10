#!/usr/bin/env python3
"""Static check of the MFC message maps and dialog wiring in this project.

A message map is three things that have to agree: the entry in
BEGIN_MESSAGE_MAP, the `afx_msg` declaration in the class, and the definition
in the .cpp. When they disagree the compiler and linker do say so -- but the
errors are famously indirect ("term does not evaluate to a function taking 2
arguments", unresolved externals pointing at a macro), and on a machine
without Visual Studio they are not available at all.

What is checked:

  * every handler named in a message map is declared in its class,
  * every handler named in a message map is defined in the .cpp,
  * every control ID used in a map or in DDX exists in resource.h,
  * `afx_msg` members that no map ever references (dead handlers),
  * duplicate entries for the same notification and ID,
  * every control ID used by `DDX_*`, the tool tip table or `GetDlgItem`
    exists in a dialog in the .rc, not merely in resource.h,
  * tool tips attached to a static carry SS_NOTIFY, without which the static
    never sees the mouse and the tip never appears,
  * each DDX_Control member has a type that matches the control's window
    class -- a CButton bound to a trackbar compiles and then misbehaves,
  * the declared signature of each handler matches what its macro expands
    to -- the wrong parameter list is what produces MFC's famously opaque
    "term does not evaluate to a function taking 2 arguments".

Usage:  python tools/check-message-map.py [file.cpp ...]
Exit code 1 if anything looks wrong.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

DEFAULT_SOURCES = ["FilterKeysSetterDlg.cpp", "FilterKeysSetter.cpp"]

# ON_WM_* macros expand to a fixed handler name. Only the ones this project
# uses need to be listed; anything else is reported as "not checked" instead
# of being treated as an error.
WM_HANDLERS = {
    "ON_WM_SYSCOMMAND": "OnSysCommand",
    "ON_WM_PAINT": "OnPaint",
    "ON_WM_QUERYDRAGICON": "OnQueryDragIcon",
    "ON_WM_ERASEBKGND": "OnEraseBkgnd",
    "ON_WM_CTLCOLOR": "OnCtlColor",
    "ON_WM_HSCROLL": "OnHScroll",
    "ON_WM_VSCROLL": "OnVScroll",
    "ON_WM_SETTINGCHANGE": "OnSettingChange",
    "ON_WM_DESTROY": "OnDestroy",
    "ON_WM_TIMER": "OnTimer",
    "ON_WM_SIZE": "OnSize",
    "ON_WM_CLOSE": "OnClose",
}

# What each macro expects the handler to look like: return type and
# parameter types, names stripped. MFC casts the handler to a fixed
# signature inside the map, so a mismatch is a compile error at the
# END_MESSAGE_MAP line, pointing nowhere near the handler itself.
SIGNATURES = {
    "ON_WM_SYSCOMMAND": ("void", ["UINT", "LPARAM"]),
    "ON_WM_PAINT": ("void", []),
    "ON_WM_QUERYDRAGICON": ("HCURSOR", []),
    "ON_WM_ERASEBKGND": ("BOOL", ["CDC*"]),
    "ON_WM_CTLCOLOR": ("HBRUSH", ["CDC*", "CWnd*", "UINT"]),
    "ON_WM_HSCROLL": ("void", ["UINT", "UINT", "CScrollBar*"]),
    "ON_WM_VSCROLL": ("void", ["UINT", "UINT", "CScrollBar*"]),
    "ON_WM_SETTINGCHANGE": ("void", ["UINT", "LPCTSTR"]),
    "ON_WM_DESTROY": ("void", []),
    "ON_WM_TIMER": ("void", ["UINT_PTR"]),
    "ON_WM_SIZE": ("void", ["UINT", "int", "int"]),
    "ON_WM_CLOSE": ("void", []),
    "ON_COMMAND": ("void", []),
    "ON_BN_CLICKED": ("void", []),
    "ON_EN_CHANGE": ("void", []),
    "ON_EN_KILLFOCUS": ("void", []),
    "ON_EN_SETFOCUS": ("void", []),
    "ON_STN_CLICKED": ("void", []),
    "ON_CBN_SELCHANGE": ("void", []),
    "ON_NOTIFY": ("void", ["NMHDR*", "LRESULT*"]),
    "ON_NOTIFY_EX": ("BOOL", ["UINT", "NMHDR*", "LRESULT*"]),
}

# Spellings that mean the same thing to the compiler.
TYPE_ALIASES = {
    "LPNMHDR": "NMHDR*",
    "NMHDR *": "NMHDR*",
    "LPCWSTR": "LPCTSTR",
    "LPCSTR": "LPCTSTR",
    "const wchar_t*": "LPCTSTR",
    "UINT_PTR": "UINT_PTR",
    "unsigned int": "UINT",
    "WPARAM": "WPARAM",
}


def normalise_type(text):
    """'CScrollBar *pScrollBar' -> 'CScrollBar*', 'const CString& s' -> 'const CString&'."""
    text = re.sub(r"\s+", " ", text).strip()
    # Drop the parameter name: the last identifier, unless the whole thing
    # is just a type.
    text = re.sub(r"\b(\w+)\s*$", lambda m: "" if not _is_type_word(m.group(1)) else m.group(1),
                  text).strip()
    text = text.replace(" *", "*").replace("* ", "*")
    text = text.replace(" &", "&").replace("& ", "&")
    return TYPE_ALIASES.get(text, text)


_TYPE_WORDS = {"void", "int", "UINT", "BOOL", "LPARAM", "WPARAM", "HCURSOR",
               "HBRUSH", "LRESULT", "UINT_PTR", "LPCTSTR", "LPCWSTR",
               "DWORD", "WORD", "char", "wchar_t", "bool", "long", "short",
               "unsigned", "size_t"}


def _is_type_word(word):
    return word in _TYPE_WORDS


def parse_signature(declaration):
    """'afx_msg void OnHScroll(UINT a, UINT b, CScrollBar* c)' -> ('void', [...])."""
    match = re.match(r"\s*(?:afx_msg\s+)?(.+?)\b(\w+)\s*\((.*)\)\s*$",
                     declaration.strip(), re.S)
    if match is None:
        return None
    ret = re.sub(r"\s+", " ", match.group(1)).strip()
    ret = ret.replace(" *", "*").replace("* ", "*")
    inner = match.group(3).strip()
    if inner in ("", "void"):
        return ret, []
    params, depth, current = [], 0, ""
    for ch in inner:
        if ch == "," and depth == 0:
            params.append(current)
            current = ""
            continue
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        current += ch
    params.append(current)
    return ret, [normalise_type(p) for p in params]


# Macros that name their handler explicitly, with the argument index of the
# handler and of the control ID (None when the macro carries no ID).
CONTROL_MACROS = {
    "ON_COMMAND": (1, 0),
    "ON_BN_CLICKED": (1, 0),
    "ON_EN_CHANGE": (1, 0),
    "ON_EN_KILLFOCUS": (1, 0),
    "ON_EN_SETFOCUS": (1, 0),
    "ON_CBN_SELCHANGE": (1, 0),
    "ON_NOTIFY": (2, 1),
    "ON_STN_CLICKED": (1, 0),
}


def read(path):
    with open(path, encoding="utf-8-sig") as handle:
        return handle.read()


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def resource_ids(path):
    ids = set()
    for line in read(path).splitlines():
        match = re.match(r"\s*#define\s+(\w+)\s+", line)
        if match:
            ids.add(match.group(1))
    # Defined by MFC (afxres.h) and the Windows headers rather than by
    # resource.h.
    ids.update({"IDOK", "IDCANCEL", "IDC_STATIC", "IDABORT", "IDRETRY",
                "IDIGNORE", "IDYES", "IDNO", "IDHELP",
                "ID_HELP", "ID_CONTEXT_HELP", "ID_DEFAULT_HELP",
                "ID_APP_ABOUT", "ID_APP_EXIT", "ID_FILE_NEW", "ID_FILE_OPEN",
                "ID_FILE_SAVE", "ID_FILE_SAVE_AS", "ID_EDIT_COPY",
                "ID_EDIT_CUT", "ID_EDIT_PASTE", "ID_EDIT_UNDO"})
    return ids


def declarations(sources):
    """class name -> set of afx_msg member names, from headers and sources."""
    found = {}
    for path in sources:
        text = strip_comments(read(path))
        for match in re.finditer(r"class\s+(\w+)\s*:\s*public\s+\w+\s*\{", text):
            name = match.group(1)
            body, depth, i = [], 0, match.end() - 1
            while i < len(text):
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                body.append(text[i])
                i += 1
            joined = "".join(body)
            members = set(re.findall(r"afx_msg[^;]*?\b(\w+)\s*\(", joined))
            found.setdefault(name, set()).update(members)
            for line in re.findall(r"afx_msg[^;]+;", joined):
                parsed = parse_signature(line.rstrip(";").replace("afx_msg", "", 1))
                if parsed is not None:
                    member = re.search(r"\b(\w+)\s*\(", line)
                    if member:
                        SIGNATURES_SEEN.setdefault(name, {})[member.group(1)] = parsed
    return found


# class -> member -> (return type, [parameter types]), filled by declarations()
SIGNATURES_SEEN = {}


def definitions(text):
    """class name -> set of member functions defined in this translation unit."""
    found = {}
    for match in re.finditer(r"\b(\w+)::(\w+)\s*\(", text):
        found.setdefault(match.group(1), set()).add(match.group(2))
    return found


def message_maps(text):
    """class name -> list of (macro, args, line number)."""
    maps = {}
    for match in re.finditer(r"BEGIN_MESSAGE_MAP\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)(.*?)END_MESSAGE_MAP",
                             text, flags=re.S):
        name = match.group(1)
        line0 = text[:match.start()].count("\n") + 1
        entries = []
        for offset, line in enumerate(match.group(3).splitlines()):
            entry = re.match(r"\s*(ON_\w+)\s*\(([^)]*)\)", line)
            if entry:
                args = [a.strip() for a in entry.group(2).split(",") if a.strip()]
                entries.append((entry.group(1), args, line0 + offset))
        maps.setdefault(name, []).extend(entries)
    return maps


def ddx_ids(text):
    return set(re.findall(r"DDX_\w+\s*\(\s*pDX\s*,\s*(\w+)", text))


# MFC control wrapper -> the resource statements and window classes it can
# legitimately be attached to. DDX_Control takes a CWnd&, so the compiler
# accepts any pairing; what goes wrong goes wrong at run time.
WRAPPER_TARGETS = {
    "CEdit": ("EDITTEXT", "Edit"),
    "CButton": ("PUSHBUTTON", "DEFPUSHBUTTON", "GROUPBOX", "Button"),
    "CStatic": ("LTEXT", "RTEXT", "CTEXT", "ICON", "Static"),
    "CSliderCtrl": ("msctls_trackbar32",),
    "CSpinButtonCtrl": ("msctls_updown32",),
    "CProgressCtrl": ("msctls_progress32",),
    "CComboBox": ("COMBOBOX", "ComboBox"),
    "CListBox": ("LISTBOX", "ListBox"),
    "CListCtrl": ("SysListView32",),
    "CTreeCtrl": ("SysTreeView32",),
    "CTabCtrl": ("SysTabControl32",),
}


def member_types(paths):
    """member name -> declared type, for the control wrappers we know."""
    found = {}
    for path in paths:
        text = strip_comments(read(path))
        for match in re.finditer(r"\b(C[A-Za-z]+)\s+(m_\w+)\s*;", text):
            if match.group(1) in WRAPPER_TARGETS:
                found[match.group(2)] = match.group(1)
    return found


def check_ddx_types(text, controls, types, rel):
    """DDX_Control(pDX, IDC_X, m_y) with a wrapper the control cannot be."""
    problems = []
    for match in re.finditer(r"DDX_Control\s*\(\s*\w+\s*,\s*(\w+)\s*,\s*(\w+)\s*\)",
                             text):
        control, member = match.group(1), match.group(2)
        wrapper = types.get(member)
        if wrapper is None or control not in controls:
            continue
        kind, line = controls[control]
        if kind == "CONTROL":
            cls = re.search(r'"[^"]*"\s*,\s*[\w()+ -]+,\s*"([^"]+)"', line)
            actual = cls.group(1) if cls else kind
        else:
            actual = kind
        allowed = WRAPPER_TARGETS[wrapper]
        if actual not in allowed:
            problems.append("%s: %s is a %s, but %s binds it to a %s"
                            % (rel, control, actual, member, wrapper))
    return problems


# ApplyToControl() in Theme.cpp detaches whole window classes from the visual
# style. A detached control sends no NM_CUSTOMDRAW, so a control that is both
# detached and custom drawn is simply never painted by its handler.
THEME_SOURCE = "Theme.cpp"

# Resource statement -> (window class, button type) as the dialog manager
# creates it.
STATEMENT_KINDS = {
    "PUSHBUTTON": ("Button", "BS_PUSHBUTTON"),
    "DEFPUSHBUTTON": ("Button", "BS_DEFPUSHBUTTON"),
    "GROUPBOX": ("Button", "BS_GROUPBOX"),
    "EDITTEXT": ("Edit", None),
    "LTEXT": ("Static", None),
    "RTEXT": ("Static", None),
    "CTEXT": ("Static", None),
    "ICON": ("Static", None),
}


def apply_to_control(theme_path):
    """(classes ApplyToControl detaches, button types it leaves alone)."""
    text = strip_comments(read(theme_path))
    body = re.search(r"void\s+CTheme::ApplyToControl\s*\([^)]*\)[^{]*\{(.*?)\n\}",
                     text, flags=re.S)
    if not body:
        return None, None
    code = body.group(1)
    classes = set(re.findall(r'_tcsicmp\s*\(\s*szClass\s*,\s*_T\("(\w+)"\)', code))

    # The button types that survive are the ones named in the condition that
    # returns early, not every BS_ constant mentioned in the function: a flag
    # computed and then dropped from that condition exempts nothing.
    exempt = set()
    for guard in re.finditer(r"if\s*\(([^)]*(?:\([^)]*\)[^)]*)*)\)\s*\{\s*return\s*;", code):
        for flag in set(re.findall(r"\b(\w+)\b", guard.group(1))):
            if flag.startswith("BS_"):
                exempt.add(flag)
                continue
            decl = re.search(r"\b%s\s*=\s*([^;]*);" % re.escape(flag), code)
            if decl:
                exempt |= set(re.findall(r"\bBS_[A-Z0-9]+\b", decl.group(1)))
    exempt.discard("BS_TYPEMASK")
    return classes, exempt


def control_kind(kind, line):
    """(window class, set of button type styles) for one resource line."""
    if kind in STATEMENT_KINDS:
        cls, style = STATEMENT_KINDS[kind]
        return cls, set([style]) if style else set()
    if kind == "CONTROL":
        cls = re.search(r'"[^"]*"\s*,\s*[\w()+ -]+,\s*"([^"]+)"', line)
        return (cls.group(1) if cls else None), set(re.findall(r"\bBS_[A-Z0-9]+\b", line))
    return None, set()


def check_customdraw(text, controls, rel):
    """NM_CUSTOMDRAW on a control that ApplyToControl detaches from the style."""
    theme_path = os.path.join(ROOT, THEME_SOURCE)
    if not os.path.isfile(theme_path):
        return []
    detached, exempt = apply_to_control(theme_path)
    if detached is None:
        return ["%s: no CTheme::ApplyToControl found, cannot check custom draw"
                % THEME_SOURCE]

    problems = []
    for match in re.finditer(r"ON_NOTIFY\s*\(\s*NM_CUSTOMDRAW\s*,\s*(\w+)", text):
        control = match.group(1)
        if control not in controls:
            continue
        kind, line = controls[control]
        cls, styles = control_kind(kind, line)
        if cls is None or not any(c.lower() == cls.lower() for c in detached):
            continue
        if cls.lower() == "button" and styles & exempt:
            continue
        problems.append("%s: %s is custom drawn, but %s detaches %s controls "
                        "from the visual style, which stops NM_CUSTOMDRAW"
                        % (rel, control, THEME_SOURCE, cls))
    return problems


def dialog_controls(rc_path):
    """control ID -> (kind, full resource line), across every dialog."""
    text = strip_comments(read(rc_path))
    controls = {}
    for dialog in re.finditer(r"(\w+)\s+DIALOGEX.*?\nBEGIN\n(.*?)\nEND", text, flags=re.S):
        body = re.sub(r",\s*\n\s+", ", ", dialog.group(2))
        for line in body.splitlines():
            line = line.strip()
            if not line:
                continue
            ident = re.search(r"\b(ID[CORKM]\w*|IDOK|IDCANCEL)\b", line)
            if not ident or ident.group(1) == "IDC_STATIC":
                continue
            controls[ident.group(1)] = (line.split()[0], line)
    return controls


def tooltip_ids(text):
    block = re.search(r"kToolTips\[\]\s*=\s*\{(.*?)\n\t\};", text, flags=re.S)
    if not block:
        return set()
    return set(re.findall(r"\{\s*(ID\w+)", block.group(1)))


def check_wiring(sources, rc_path):
    """Control IDs referenced from code must exist in the .rc dialogs."""
    problems, notes = [], []
    controls = dialog_controls(rc_path)
    if not controls:
        return ["%s: no dialog controls found" % os.path.relpath(rc_path, ROOT)], []

    static_kinds = ("LTEXT", "RTEXT", "CTEXT")
    headers = [os.path.join(ROOT, h) for h in
               ("FilterKeysSetterDlg.h", "FilterKeysSetter.h")]
    types = member_types(sources + [h for h in headers if os.path.isfile(h)])

    for path in sources:
        text = strip_comments(read(path))
        rel = os.path.relpath(path, ROOT)

        problems += check_ddx_types(text, controls, types, rel)
        problems += check_customdraw(text, controls, rel)

        referenced = set()
        referenced |= ddx_ids(text)
        referenced |= set(re.findall(r"(?:Get|Set)DlgItem\w*\s*\(\s*(ID[CORKM]\w*)", text))

        tips = tooltip_ids(text)
        referenced |= tips

        for control in sorted(referenced):
            if control in ("IDC_STATIC", "IDOK", "IDCANCEL"):
                continue
            if control not in controls:
                problems.append("%s: %s is used in code but is in no dialog in %s"
                                % (rel, control, os.path.relpath(rc_path, ROOT)))

        for control in sorted(tips):
            kind, line = controls.get(control, (None, ""))
            if kind in static_kinds and "SS_NOTIFY" not in line:
                problems.append("%s: tool tip on %s, but the static has no "
                                "SS_NOTIFY and will never see the mouse" % (rel, control))

        if tips:
            # Buttons that need no explanation, and the icon in the About
            # box, are not gaps.
            uncovered = sorted(c for c, (kind, _line) in controls.items()
                               if c not in tips
                               and c not in ("IDOK", "IDCANCEL")
                               and kind not in ("ICON", "GROUPBOX"))
            for control in uncovered:
                notes.append("%s: %s has no tool tip" % (rel, control))

    return problems, notes


def check(sources, header_paths, resource_path):
    problems = []
    notes = []

    ids = resource_ids(resource_path)
    decls = declarations(sources + header_paths)

    for path in sources:
        raw = read(path)
        text = strip_comments(raw)
        defs = definitions(text)
        maps = message_maps(text)
        rel = os.path.relpath(path, ROOT)

        used = {}
        for cls, entries in maps.items():
            seen = set()
            for macro, args, line in entries:
                handler = None
                control = None

                if macro in WM_HANDLERS:
                    handler = WM_HANDLERS[macro]
                elif macro in CONTROL_MACROS:
                    hidx, cidx = CONTROL_MACROS[macro]
                    if len(args) > hidx:
                        handler = args[hidx]
                    if cidx is not None and len(args) > cidx:
                        control = args[cidx]
                elif macro.startswith("ON_WM_"):
                    notes.append("%s:%d: %s not in the table, entry not checked"
                                 % (rel, line, macro))
                    continue
                else:
                    notes.append("%s:%d: %s not in the table, entry not checked"
                                 % (rel, line, macro))
                    continue

                key = (macro, tuple(args[:2]))
                if key in seen:
                    problems.append("%s:%d: duplicate entry %s(%s)"
                                    % (rel, line, macro, ", ".join(args)))
                seen.add(key)

                if control and control not in ids:
                    problems.append("%s:%d: %s uses unknown control ID %s"
                                    % (rel, line, macro, control))

                if handler and "::" in handler:
                    # A handler inherited from the base class, such as
                    # ON_COMMAND(ID_HELP, CWinApp::OnHelp). Nothing to check
                    # in this class.
                    notes.append("%s:%d: %s handled by the base class, not checked"
                                 % (rel, line, handler))
                    continue

                if handler:
                    handler = handler.lstrip("&")
                    used.setdefault(cls, set()).add(handler)
                    if handler not in decls.get(cls, set()):
                        problems.append("%s:%d: %s is mapped but not declared "
                                        "as afx_msg in %s" % (rel, line, handler, cls))
                    if handler not in defs.get(cls, set()):
                        problems.append("%s:%d: %s is mapped but not defined in %s"
                                        % (rel, line, handler, cls))

                    expected = SIGNATURES.get(macro)
                    actual = SIGNATURES_SEEN.get(cls, {}).get(handler)
                    if expected and actual:
                        want = "%s(%s)" % (expected[0], ", ".join(expected[1]))
                        have = "%s(%s)" % (actual[0], ", ".join(actual[1]))
                        if want != have:
                            problems.append("%s:%d: %s needs %s declared as "
                                            "%s, but it is %s"
                                            % (rel, line, macro, handler,
                                               want, have))

        for cls, members in decls.items():
            if cls not in maps:
                continue
            for member in sorted(members - used.get(cls, set())):
                problems.append("%s: %s::%s is declared afx_msg but no message "
                                "map entry refers to it" % (rel, cls, member))

        for control in sorted(ddx_ids(text)):
            if control not in ids:
                problems.append("%s: DDX uses unknown control ID %s" % (rel, control))

    return problems, notes


def main():
    argv = sys.argv[1:]
    sources = [os.path.join(ROOT, p) for p in (argv or DEFAULT_SOURCES)]
    headers = [os.path.join(ROOT, p) for p in ("FilterKeysSetterDlg.h", "FilterKeysSetter.h")]
    resource = os.path.join(ROOT, "resource.h")

    missing = [p for p in sources + headers + [resource] if not os.path.isfile(p)]
    if missing:
        for path in missing:
            print("missing: %s" % path)
        return 1

    problems, notes = check(sources, headers, resource)

    rc_path = os.path.join(ROOT, "FilterKeysSetter.rc")
    if os.path.isfile(rc_path):
        more_problems, more_notes = check_wiring(sources, rc_path)
        problems += more_problems
        notes += more_notes

    for note in notes:
        print("note: %s" % note)
    for problem in problems:
        print("PROBLEM: %s" % problem)

    print("%d problem(s)" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
