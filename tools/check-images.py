#!/usr/bin/env python3
"""Check that the generated pictures still match the sources they came from.

Five of the pictures in docs/img are not drawings. Four are rendered from
FilterKeysSetter.rc and the palette in Theme.cpp by render-dialog.py, and
the colour sheet is read straight out of Theme.cpp by render-palette.py.
That is a pleasant arrangement right up to the moment someone moves a
control, changes a colour and does not re-run the generators. The
documentation then shows a dialog that no longer exists, and nothing
complains -- a stale picture compiles, links and spell-checks perfectly.

So this asks the generators the only question that matters: render
everything again, in memory, and compare the bytes with the files that are
checked in. They are deterministic, so identical sources give identical
bytes; a difference means the pictures are behind the code.

Both generators need Pillow. Where it is missing -- a bare CI runner, a
machine that only wants to build the program -- this check reports that it
was skipped and exits 0 rather than failing for a missing drawing library.

    python3 tools/check-images.py [root]

Exit code 0 when every picture is current or the check was skipped, 1 when
a picture is out of date or a generator failed.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

GENERATORS = [
    ("render-dialog.py", "the two dialogs in both themes"),
    ("render-palette.py", "the colour sheet"),
]


def have_pillow():
    probe = subprocess.run([sys.executable, "-c", "import PIL"],
                           capture_output=True, text=True)
    return probe.returncode == 0


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(HERE)
    tools = os.path.join(root, "tools")

    if not have_pillow():
        print("Pillow is not installed, skipping (pip install pillow)")
        return 0

    problems = 0
    pictures = 0
    for script, description in GENERATORS:
        path = os.path.join(tools, script)
        if not os.path.exists(path):
            print("%s is missing" % script)
            problems += 1
            continue

        done = subprocess.run([sys.executable, path, "--check"],
                              cwd=root, capture_output=True, text=True)
        output = (done.stdout + done.stderr).rstrip()
        for line in output.splitlines():
            if " current " in line:
                pictures += 1
            else:
                print("  %s" % line.strip())
        if done.returncode != 0:
            problems += 1

    print("%d picture(s), %d generator(s) out of date" % (pictures, problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
