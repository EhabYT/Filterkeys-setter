#!/usr/bin/env python3
"""Check the documentation against the repository.

The README and the files under docs/ have grown into the main description of
how this project is built, released and checked. Prose rots quietly: a file
is renamed, a heading is reworded, a tool is dropped, and the links keep
looking fine until someone follows one. This checker reads every Markdown
file and verifies:

  * relative links point at a file that exists
  * link fragments (#some-heading) match a heading in the target file
  * HTML <img src="..."> paths exist as well
  * every tools/*.py and tools/*.cmd in the repository is mentioned
    somewhere, so a new tool cannot stay undocumented
  * file paths written in `backticks` exist, when they look like a path
    into this repository rather than a build output or an example
  * the accelerator table in docs/UI-UPGRADE.md lists exactly the letters
    the resource script actually marks with an ampersand
  * every contrast ratio documented as "foreground | background | N:1" is
    what WCAG 2 actually gives for those two colours
  * every colour quoted in the documentation is one the code defines

It deliberately does not touch external URLs: no network here, and a web
page that moved is not this repository's fault.

Run it from the repository root:

    python3 tools/check-docs.py

Exit code 0 when nothing is wrong, 1 otherwise.
"""

import os
import re
import sys

MARKDOWN_LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
HTML_IMAGE = re.compile(r'<img\s+[^>]*src="([^"]+)"')
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*$", re.M)
BACKTICK = re.compile(r"`([^`\n]+)`")

# Paths that appear in the documentation but are produced by a build, come
# from somewhere else entirely, or are deliberately hypothetical.
NOT_IN_REPOSITORY = {
    "Release", "Debug", "x64", "release", "artifacts",
    ".github/workflows", ".github/workflows/build.yml",
    ".github/workflows/release.yml",
    ".github/workflows/cmake-single-platform.yml",
    "CMakeLists.txt",
}

EXTERNAL = ("http://", "https://", "mailto:")


def read(path):
    with open(path, encoding="utf-8-sig", errors="surrogateescape") as handle:
        return handle.read()


def markdown_files(root):
    found = []
    for folder, directories, names in os.walk(root):
        directories[:] = [d for d in directories
                          if d not in (".git", "x64", "Debug", "Release")]
        for name in sorted(names):
            if name.lower().endswith(".md"):
                found.append(os.path.relpath(os.path.join(folder, name), root))
    return sorted(found)


def anchors(text):
    """GitHub's heading anchors: lowercase, punctuation dropped, spaces to -."""
    result = set()
    for _, title in HEADING.findall(text):
        slug = title.strip().lower()
        slug = re.sub(r"[^\w\s-]", "", slug)
        slug = re.sub(r"\s+", "-", slug)
        result.add(slug)
    return result


def check_links(root, path, text, cache):
    problems = []
    folder = os.path.dirname(path)
    targets = [(m.group(1), False) for m in MARKDOWN_LINK.finditer(text)]
    targets += [(m.group(1), True) for m in HTML_IMAGE.finditer(text)]

    for target, is_image in targets:
        if target.startswith(EXTERNAL):
            continue
        if target.startswith("#"):
            fragment, relative = target[1:], path
        elif "#" in target:
            relative, fragment = target.split("#", 1)
        else:
            relative, fragment = target, None

        if relative:
            # Links are relative to the file that holds them, not the root.
            resolved = os.path.normpath(os.path.join(folder, relative))
            full = os.path.join(root, resolved)
            if not os.path.exists(full):
                problems.append("%s links to %s, which does not exist"
                                % (path, target))
                continue
            if os.path.isdir(full):
                continue
        else:
            resolved = path

        if fragment and resolved.lower().endswith(".md"):
            if resolved not in cache:
                cache[resolved] = anchors(read(os.path.join(root, resolved)))
            if fragment.lower() not in cache[resolved]:
                problems.append("%s links to %s, but %s has no such heading"
                                % (path, target, resolved))
    return problems


def check_backticked_paths(root, path, text):
    """A path in backticks that looks like one of ours has to exist."""
    problems = []
    for match in BACKTICK.finditer(text):
        candidate = match.group(1).strip().replace("\\", "/")
        if " " in candidate or candidate in NOT_IN_REPOSITORY:
            continue
        if not re.match(r"^[\w.][\w./-]*$", candidate):
            continue
        # Only paths that name a directory of this repository, so that
        # file names mentioned in passing are not mistaken for links.
        if not candidate.startswith(("tools/", "docs/", "res/",
                                     "FilterKeysSetter.Setup/")):
            continue
        if candidate.rstrip("/") in NOT_IN_REPOSITORY:
            continue
        if not os.path.exists(os.path.join(root, candidate)):
            problems.append("%s mentions `%s`, which does not exist"
                            % (path, candidate))
    return problems


HEX = re.compile(r"#([0-9A-Fa-f]{6})\b")

# Colours that appear in the documentation on purpose without being in the
# current palette: the rejected disabled colour and the background it was
# chosen against.
HISTORICAL_COLOURS = {"8CA4C4", "0A2342"}


def _channel(value):
    value /= 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def luminance(hex_colour):
    red, green, blue = (int(hex_colour[i:i + 2], 16) for i in (0, 2, 4))
    return (0.2126 * _channel(red) + 0.7152 * _channel(green)
            + 0.0722 * _channel(blue))


def contrast(first, second):
    """WCAG 2 contrast ratio between two #RRGGBB colours."""
    light, dark = luminance(first), luminance(second)
    if light < dark:
        light, dark = dark, light
    return (light + 0.05) / (dark + 0.05)


def palette_colours(root):
    """Hex colours the code defines, from the RGB() triples in Theme.cpp."""
    path = os.path.join(root, "Theme.cpp")
    if not os.path.exists(path):
        return None
    found = {"FFFFFF", "000000"}
    for match in re.finditer(r"RGB\(\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})"
                             r"\s*,\s*0x([0-9A-Fa-f]{2})\s*\)", read(path)):
        found.add("".join(part.upper() for part in match.groups()))
    return found


def check_colours(root, documents):
    """Quoted colours must exist in Theme.cpp; quoted ratios must be right."""
    problems = []
    palette = palette_colours(root)

    for path, text in documents.items():
        if palette is not None:
            for match in HEX.finditer(text):
                colour = match.group(1).upper()
                if colour not in palette and colour not in HISTORICAL_COLOURS:
                    problems.append("%s mentions the colour #%s, which "
                                    "Theme.cpp does not define"
                                    % (path, colour))

        # Rows of the shape: | `#FFFFFF` | `#1F3A61` | 11.4:1 |
        for row in re.findall(r"(?m)^\|(.+)\|\s*$", text):
            cells = [cell.strip().strip("`") for cell in row.split("|")]
            colours = [c.lstrip("#").upper() for c in cells
                       if HEX.fullmatch(c if c.startswith("#") else "#" + c)]
            claims = [c for c in cells if re.fullmatch(r"\d+(?:\.\d+)?:1", c)]
            if len(colours) != 2 or len(claims) != 1:
                continue
            text_value = claims[0].split(":")[0]
            claimed = float(text_value)
            # Compare at the precision the document chose, so 4.7:1 is a
            # correct way to write 4.6497 and 4.65:1 is as well.
            digits = len(text_value.split(".")[1]) if "." in text_value else 0
            actual = contrast(colours[0], colours[1])
            if round(actual, digits) != claimed:
                problems.append("%s claims #%s on #%s is %s, it is %.2f:1"
                                % (path, colours[0], colours[1],
                                   claims[0], actual))
    return problems


ACCELERATOR_HEADING = "## Keyboard accelerators"


def resource_accelerators(root):
    """Letters marked with & in the dialog captions of the .rc."""
    path = os.path.join(root, "FilterKeysSetter.rc")
    if not os.path.exists(path):
        return None
    text = re.sub(r"//[^\n]*", "", read(path))
    letters = set()
    for dialog in re.finditer(r"\w+\s+DIALOGEX.*?\nBEGIN\n(.*?)\nEND",
                              text, re.S):
        body = re.sub(r",\s*\n\s+", ", ", dialog.group(1))
        for line in body.splitlines():
            caption = re.match(r'\w+\s+"([^"]*)"', line.strip())
            if not caption:
                continue
            marked = re.search(r"&(\w)", caption.group(1).replace("&&", ""))
            if marked:
                letters.add(marked.group(1).upper())
    return letters


def check_accelerator_table(root, documents):
    """The documented accelerators have to be the ones in the resource."""
    problems = []
    letters = resource_accelerators(root)
    if letters is None:
        return problems
    for path, text in documents.items():
        if ACCELERATOR_HEADING not in text:
            continue
        section = text.split(ACCELERATOR_HEADING, 1)[1]
        section = re.split(r"(?m)^## ", section)[0]
        documented = set()
        for row in re.findall(r"(?m)^\|(.+)\|\s*$", section):
            for cell in row.split("|"):
                cell = cell.strip()
                if len(cell) == 1 and cell.isalpha():
                    documented.add(cell.upper())
        if not documented:
            continue
        missing = sorted(letters - documented)
        extra = sorted(documented - letters)
        if missing:
            problems.append("%s documents no accelerator for Alt+%s, which "
                            "the resource script defines"
                            % (path, ", Alt+".join(missing)))
        if extra:
            problems.append("%s documents Alt+%s, which no control in the "
                            "resource script claims"
                            % (path, ", Alt+".join(extra)))
    return problems


def check_tools_documented(root, documents):
    """Every tool has to be named somewhere, or nobody will ever run it."""
    problems = []
    folder = os.path.join(root, "tools")
    if not os.path.isdir(folder):
        return problems
    everything = "\n".join(documents.values())
    for name in sorted(os.listdir(folder)):
        if not name.endswith((".py", ".cmd", ".bat")):
            continue
        if name not in everything:
            problems.append("tools/%s is in the repository but no "
                            "documentation mentions it" % name)
    return problems


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    files = markdown_files(root)
    if not files:
        print("no Markdown files found")
        return 1

    documents = {path: read(os.path.join(root, path)) for path in files}
    problems, cache = [], {}
    for path in files:
        text = documents[path]
        problems += check_links(root, path, text, cache)
        problems += check_backticked_paths(root, path, text)
    problems += check_tools_documented(root, documents)
    problems += check_accelerator_table(root, documents)
    problems += check_colours(root, documents)

    for problem in problems:
        print("  ! %s" % problem)
    print("%d document(s), %d problem(s)" % (len(files), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
