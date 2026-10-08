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

    for problem in problems:
        print("  ! %s" % problem)
    print("%d document(s), %d problem(s)" % (len(files), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
