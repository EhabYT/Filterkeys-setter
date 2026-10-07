#!/usr/bin/env python3
"""Builds res/FilterKeysSetter.ico from a square master PNG.

Windows picks a different frame for the taskbar, Alt+Tab, the title bar, the
Explorer list views and every DPI scaling step in between, and it rescales
whatever it finds when the exact size is missing. This writes all ten sizes
the shell actually asks for.

Encoding follows what the shell has always been happiest with:

  * frames up to 96 px as uncompressed 32-bit BMP with an AND mask, the form
    every Windows version understands
  * 128 and 256 px PNG-compressed, which is what PNG frames were introduced
    for: as BMPs those two alone would add 170 KB

Pillow's own ICO writer PNG-compresses every frame, which works on Windows
Vista and later but is still unusual for the small sizes, so the container is
assembled here by hand.

Usage:  python tools/make-icon.py [master.png] [output.ico]
"""

import io
import os
import struct
import sys

from PIL import Image

SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
PNG_FROM = 128          # this size and above are stored as PNG


def bmp_frame(image):
    """32-bit bottom-up DIB plus the (unused but mandatory) AND mask."""
    width, height = image.size
    header = struct.pack("<IiiHHIIiiII",
                         40,              # biSize
                         width,
                         height * 2,      # colour data plus mask
                         1,               # biPlanes
                         32,              # biBitCount
                         0,               # BI_RGB
                         0, 0, 0, 0, 0)

    pixels = image.load()
    rows = []
    for y in range(height - 1, -1, -1):   # DIBs are stored bottom-up
        row = bytearray()
        for x in range(width):
            r, g, b, a = pixels[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))

    stride = (width + 31) // 32 * 4       # AND mask rows are DWORD aligned
    mask = bytes(stride * height)         # all zero: alpha does the work
    return header + b"".join(rows) + mask


def png_frame(image):
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def build(master_path, output_path):
    master = Image.open(master_path).convert("RGBA")
    if master.size[0] != master.size[1]:
        raise SystemExit("the master image has to be square, got %dx%d"
                         % master.size)
    if master.size[0] < max(SIZES):
        print("note: master is only %d px, the %d px frame is upscaled"
              % (master.size[0], max(SIZES)))

    frames = []
    for size in SIZES:
        image = (master if master.size[0] == size
                 else master.resize((size, size), Image.LANCZOS))
        data = png_frame(image) if size >= PNG_FROM else bmp_frame(image)
        frames.append((size, data))

    offset = 6 + 16 * len(frames)
    directory = b""
    for size, data in frames:
        directory += struct.pack("<BBBBHHII",
                                 size % 256, size % 256,   # 256 is stored as 0
                                 0, 0, 1, 32, len(data), offset)
        offset += len(data)

    with open(output_path, "wb") as handle:
        handle.write(struct.pack("<HHH", 0, 1, len(frames)))
        handle.write(directory)
        for _, data in frames:
            handle.write(data)

    total = os.path.getsize(output_path)
    print("%s  (%d frames: %s, %.1f KB)"
          % (output_path, len(frames),
             ", ".join(str(s) for s, _ in frames), total / 1024.0))


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    master = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "res", "logo.png")
    output = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "res", "FilterKeysSetter.ico")
    build(master, output)
