#!/usr/bin/env python3
"""Pre-build check of the solution, the project file and what they reference.

MSBuild reports these mistakes, but only after it has started, often with a
code rather than a sentence (`MSB3030` for a missing file to copy, `C1083`
for a header that moved, `C1010` for a translation unit that forgot the
precompiled header). None of them need a compiler to find.

What is checked:

  * every file the .vcxproj references exists on disk
  * every source and header in the repository is referenced by the project
  * all four Configuration|Platform pairs exist in the project and are
    reachable from a solution configuration -- the solution calls the 32-bit
    platform x86 while the project calls it Win32, so the mapping matters
  * every /p:Platform= passed to the solution in a script or workflow is a
    platform the solution actually defines
  * pch.cpp creates the precompiled header and every other .cpp uses it
  * each .cpp starts by including pch.h, which /Yu requires
  * the files the .rc pulls in -- icon, rc2, manifest -- exist
  * the manifest named in the project settings is the one in the repository
  * the installer: an upgrade rather than a parallel install, distinct GUIDs,
    and a packaged executable path that matches where the project puts it

Usage:  python tools/check-project.py [repo root]
Exit code 1 if anything looks wrong.
"""

import os
import re
import sys

PROJECT = "FilterKeysSetter.vcxproj"
SOLUTION = "FilterKeysSetter.sln"
RC = "FilterKeysSetter.rc"

EXPECTED_PAIRS = {"Debug|Win32", "Debug|x64", "Release|Win32", "Release|x64"}

# Sources that legitimately live outside the project file.
NOT_IN_PROJECT = set()


def read(path):
    with open(path, encoding="utf-8-sig", errors="surrogateescape") as handle:
        return handle.read()


def win_path(root, relative):
    return os.path.join(root, relative.replace("\\", os.sep))


def referenced_files(project):
    """[(item type, path as written)] for everything the project points at."""
    found = []
    for tag in ("ClCompile", "ClInclude", "ResourceCompile", "Image",
                "Manifest", "None", "Midl", "CustomBuild"):
        for match in re.finditer(r'<%s Include="([^"]+)"' % tag, project):
            found.append((tag, match.group(1)))
    return found


def check_files_exist(root, project):
    problems = []
    for tag, relative in referenced_files(project):
        if not os.path.exists(win_path(root, relative)):
            problems.append("%s references %s, which does not exist"
                            % (tag, relative))
    return problems


def check_nothing_orphaned(root, project):
    problems = []
    listed = set(relative.replace("\\", "/").lower()
                 for _, relative in referenced_files(project))
    for name in sorted(os.listdir(root)):
        if not name.endswith((".cpp", ".h")) or name in NOT_IN_PROJECT:
            continue
        if name.lower() not in listed:
            problems.append("%s is in the repository but not in the project"
                            % name)
    return problems


def check_configurations(root, project):
    problems = []
    pairs = set(re.findall(r'<ProjectConfiguration Include="([^"]+)"', project))
    for missing in sorted(EXPECTED_PAIRS - pairs):
        problems.append("the project has no %s configuration" % missing)

    solution = os.path.join(root, SOLUTION)
    if not os.path.exists(solution):
        return problems
    text = read(solution)

    # The solution maps its own names onto the project's: the 32-bit platform
    # is x86 in the solution and Win32 in the project.
    mapped = set(m.strip() for m in
                 re.findall(r"\.ActiveCfg\s*=\s*([^\r\n]+)", text))
    for pair in sorted(EXPECTED_PAIRS - pairs):
        pass
    for pair in sorted(pairs):
        if pair not in mapped:
            problems.append("no solution configuration builds the project's "
                            "%s" % pair)
    return problems


def solution_platforms(text):
    return set(p.strip() for _, p in
               (entry.split("|", 1) for entry in
                re.findall(r"^\s*(\w+\|\w+) = ", text, re.M)))


def check_platform_names(root):
    """A /p:Platform= that the solution does not know fails as MSB4126."""
    problems, notes = [], []
    solution = os.path.join(root, SOLUTION)
    if not os.path.exists(solution):
        return problems, notes
    text = read(solution)
    known = solution_platforms(text)

    for folder, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in (".git", "x64", "Debug", "Release")]
        for name in files:
            # Scripts and workflows only. Prose is excluded on purpose: the
            # README documents the Win32/x86 trap by quoting the command that
            # fails, and that counter-example must not be flagged.
            if not name.endswith((".cmd", ".bat", ".yml", ".yaml")):
                continue
            path = os.path.join(folder, name)
            body = read(path)
            # ${{ matrix.platform }} -> ${{matrix.platform}} so the argument
            # stays one token when the command is scanned.
            body = re.sub(r"\$\{\{\s*([\w.]+)\s*\}\}", r"${{\1}}", body)
            for match in re.finditer(r"msbuild\s+(\S+\.(?:sln|vcxproj))"
                                     r"((?:[^\n]|\n\s+)*)", body, re.I):
                target, tail = match.group(1), match.group(2)
                for platform in re.findall(r"/p:Platform=(\S+)", tail):
                    # A workflow passes the matrix value; resolve the simple
                    # `platform: [Win32, x64]` form rather than skipping it,
                    # because that is exactly where the mismatch hides.
                    if "matrix.platform" in platform:
                        listed = re.search(r"platform:\s*\[([^\]]+)\]", body)
                        if not listed:
                            continue
                        candidates = [v.strip() for v in listed.group(1).split(",")]
                        platform = None
                    elif "$" in platform or "{" in platform:
                        continue      # some other variable
                    else:
                        candidates = [platform]

                    seen_here = set()
                    for candidate in candidates:
                        if candidate in seen_here:
                            continue
                        seen_here.add(candidate)
                        if target.lower().endswith(".sln") and candidate not in known:
                            problems.append(
                                "%s builds %s with /p:Platform=%s, but the "
                                "solution only knows %s"
                                % (os.path.relpath(path, root), target,
                                   candidate, ", ".join(sorted(known))))
                    if target.lower().endswith(".sln") and platform not in known:
                        problems.append("%s builds %s with /p:Platform=%s, "
                                        "but the solution only knows %s"
                                        % (os.path.relpath(path, root), target,
                                           platform, ", ".join(sorted(known))))
    return problems, notes


def check_build_participation(root):
    """A project with ActiveCfg but no Build.0 is never actually built."""
    notes = []
    solution = os.path.join(root, SOLUTION)
    if not os.path.exists(solution):
        return notes
    text = read(solution)
    names = dict((guid, name) for name, guid in
                 re.findall(r'Project\("\{[^}]+\}"\) = "([^"]+)", "[^"]+", '
                            r'"\{([^}]+)\}"', text))
    for guid, name in sorted(names.items(), key=lambda kv: kv[1]):
        active = text.count("{%s}." % guid + "") and ("{%s}" % guid) in text
        builds = re.findall(r"\{%s\}\.[^\r\n]*\.Build\.0" % re.escape(guid), text)
        if active and not builds:
            notes.append("%s is in the solution but no configuration builds "
                         "it" % name)
    return notes


def check_precompiled_header(root, project):
    problems = []
    creators = re.findall(
        r'<ClCompile Include="([^"]+)">(?:(?!</ClCompile>).)*?>Create<',
        project, re.S)
    if sorted(c.lower() for c in creators) != ["pch.cpp"]:
        problems.append("expected pch.cpp alone to create the precompiled "
                        "header, found %s" % (creators or "nothing"))

    for _, relative in referenced_files(project):
        if not relative.lower().endswith(".cpp") or relative.lower() == "pch.cpp":
            continue
        path = win_path(root, relative)
        if not os.path.exists(path):
            continue
        body = read(path)
        first = next((line.strip() for line in body.splitlines()
                      if line.strip() and not line.strip().startswith("//")), "")
        if "pch.h" not in first:
            problems.append('%s starts with "%s"; /Yu needs #include "pch.h" '
                            "first" % (relative, first[:40]))
    return problems


def check_resource_includes(root):
    problems = []
    path = os.path.join(root, RC)
    if not os.path.exists(path):
        return ["%s is missing" % RC]
    text = read(path)

    # Anything the resource script pulls in from the res folder.
    for relative in sorted(set(re.findall(r'"(res\\\\?[\w.\\-]+)"', text))):
        cleaned = relative.replace("\\\\", "\\")
        if not os.path.exists(win_path(root, cleaned)):
            problems.append("%s refers to %s, which does not exist"
                            % (RC, cleaned))
    return problems


def check_manifest(root, project):
    problems = []
    named = re.findall(r"<AdditionalManifestFiles[^>]*>([^<]+)<", project)
    for entry in set(named):
        for piece in entry.split(";"):
            piece = piece.strip()
            if not piece or piece.startswith("%("):
                continue
            if not os.path.exists(win_path(root, piece)):
                problems.append("the manifest %s named in the project is "
                                "missing" % piece)
    if not named:
        problems.append("no AdditionalManifestFiles entry; the DPI and "
                        "common controls manifest would not be embedded")
    return problems


VDPROJ = os.path.join("FilterKeysSetter.Setup", "FilterKeysSetter.Setup.vdproj")

GUID = re.compile(r"^\{[0-9A-Fa-f]{8}-(?:[0-9A-Fa-f]{4}-){3}[0-9A-Fa-f]{12}\}$")


def project_output(root, project, platform, configuration):
    """Where MSBuild puts the exe when the project sets no OutDir."""
    folder = configuration if platform == "Win32" else os.path.join(platform,
                                                                    configuration)
    return os.path.join(folder, "FilterKeysSetter.exe")


def check_installer(root, project):
    """The .vdproj is legacy and hand-edited, so its invariants are checked."""
    problems, notes = [], []
    path = os.path.join(root, VDPROJ)
    if not os.path.exists(path):
        return problems, notes
    text = read(path)

    def value(name):
        match = re.search(r'"%s"\s*=\s*"\d+:([^"]*)"' % name, text)
        return match.group(1) if match else None

    def guid_value(name):
        """The prerequisite blocks carry a ProductCode too -- take the GUID."""
        found = re.findall(r'"%s"\s*=\s*"\d+:([^"]*)"' % name, text)
        for candidate in found:
            if GUID.match(candidate):
                return candidate
        return found[0] if found else None

    product, upgrade = guid_value("ProductCode"), guid_value("UpgradeCode")
    for name, guid in (("ProductCode", product), ("UpgradeCode", upgrade)):
        if guid is None:
            problems.append("the installer has no %s" % name)
        elif not GUID.match(guid):
            problems.append("the installer %s %s is not a GUID" % (name, guid))
    if product and upgrade and product == upgrade:
        problems.append("ProductCode and UpgradeCode are the same GUID; the "
                        "next version could not upgrade this one")

    if value("RemovePreviousVersions") != "TRUE":
        problems.append("RemovePreviousVersions is not TRUE, so a new version "
                        "installs beside the old one instead of upgrading it")

    # The packaged executable is a plain file reference, not a project output,
    # so the path has to agree with where the build actually writes.
    for match in re.finditer(r'"SourcePath"\s*=\s*"\d+:([^"]*FilterKeysSetter\.exe)"',
                             text):
        relative = match.group(1).replace("\\\\", "\\")
        cleaned = relative.lstrip(".\\").replace("\\", os.sep)
        expected = set()
        for platform in ("Win32", "x64"):
            for configuration in ("Debug", "Release"):
                expected.add(project_output(root, project, platform,
                                            configuration))
        if cleaned not in expected:
            problems.append("the installer packages %s, which is not an output "
                            "path of the project (%s)"
                            % (relative, ", ".join(sorted(expected))))
        else:
            notes.append("the installer packages %s as a plain file reference, "
                         "so it always ships that configuration" % relative)
    # The runtime set is dependency-scanned by Visual Studio and easy to
    # lose track of, so print it on every run rather than only when it
    # looks wrong. Whether it is complete can only be decided with
    # dumpbin on Windows -- see Deployment in README.md.
    shipped = []
    for match in re.finditer(r'"SourcePath"\s*=\s*"\d+:([^"\\\\]+\.dll)"', text):
        name = match.group(1)
        block = text[match.end():match.end() + 900]
        excluded = re.search(r'"Exclude"\s*=\s*"\d+:(\w+)"', block)
        if excluded and excluded.group(1) == "FALSE":
            shipped.append(name)
    if shipped:
        notes.append("the installer ships these runtime files beside the "
                     "program: %s" % ", ".join(sorted(shipped)))

    return problems, notes


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    path = os.path.join(root, PROJECT)
    if not os.path.exists(path):
        print("  ! %s not found" % PROJECT)
        return 1
    project = read(path)

    problems = []
    problems += check_files_exist(root, project)
    problems += check_nothing_orphaned(root, project)
    problems += check_configurations(root, project)
    problems += check_precompiled_header(root, project)
    problems += check_resource_includes(root)
    problems += check_manifest(root, project)
    platform_problems, _ = check_platform_names(root)
    problems += platform_problems
    notes = check_build_participation(root)
    installer_problems, installer_notes = check_installer(root, project)
    problems += installer_problems
    notes += installer_notes

    for note in notes:
        print("note: " + note)
    for problem in problems:
        print("  ! " + problem)
    print("%d reference(s), %d problem(s)"
          % (len(referenced_files(project)), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
