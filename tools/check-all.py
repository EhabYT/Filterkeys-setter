#!/usr/bin/env python3
"""Run every checker in this folder and summarise the result.

Six checkers plus their self test is a lot to remember and easy to run
incompletely. This runs all of them in a fixed order -- cheapest and most
specific first -- prints each one's own output underneath its name, and
ends with a single verdict.

    python3 tools/check-all.py          the six checkers and the self test
    python3 tools/check-all.py --quick  skip the self test

The self test is last because it is the slow one: it copies the tree and
runs the checkers once per mutation. Everything else finishes in
milliseconds.

Exit code 0 when every checker passed, 1 otherwise.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

CHECKERS = [
    ("check-dialog-layout.py", "dialog geometry, captions, accelerators"),
    ("check-message-map.py", "message maps, DDX and tool tip wiring"),
    ("check-resources.py", "resource IDs, the icon container, versions"),
    ("check-error-handling.py", "Win32 results that are thrown away"),
    ("check-project.py", "project files, build scripts, installer"),
    ("check-docs.py", "links, headings and file names in the docs"),
]

SELFTEST = ("selftest.py", "the checkers themselves, by breaking the tree")


def run(script):
    done = subprocess.run([sys.executable, os.path.join(HERE, script)],
                          cwd=ROOT, capture_output=True, text=True)
    return done.returncode, (done.stdout + done.stderr).rstrip()


def main():
    scripts = list(CHECKERS)
    if "--quick" not in sys.argv:
        scripts.append(SELFTEST)

    failed = []
    for script, description in scripts:
        code, output = run(script)
        status = "ok  " if code == 0 else "FAIL"
        print("%s  %-26s %s" % (status, script, description))
        for line in output.splitlines():
            print("      %s" % line)
        if code != 0:
            failed.append(script)

    print()
    if failed:
        print("%d of %d failed: %s"
              % (len(failed), len(scripts), ", ".join(failed)))
        return 1
    print("all %d checks passed" % len(scripts))
    return 0


if __name__ == "__main__":
    sys.exit(main())
