#!/usr/bin/env python3
"""Draw the palette sheet in docs/img/palette.png straight from Theme.cpp.

Every swatch, every hex value and every contrast figure on the sheet is read
out of the two ThemePalette initialisers in Theme.cpp, so the picture cannot
drift away from the program the way a hand-drawn one would. If a colour
changes in the code, re-run this and the sheet changes with it.

The roles come from the trailing comment on each line, which is also what
keeps the two palettes aligned: they are the same nine roles in the same
order.

Usage:
    python tools/render-palette.py
    python tools/render-palette.py --out docs/img
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Windows first -- Segoe UI is the face the dialog itself asks for -- with the
# DejaVu paths of the Linux box this was written on as the fallback, so the
# sheet can be drawn on either. Neither is a reason to draw it in some other
# face without saying so.
WINDOWS_FONTS = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
DEJAVU = "/usr/share/fonts/truetype/dejavu"
FONTS = {
    "regular": (os.path.join(WINDOWS_FONTS, "segoeui.ttf"),
                os.path.join(DEJAVU, "DejaVuSans.ttf")),
    "bold": (os.path.join(WINDOWS_FONTS, "segoeuib.ttf"),
             os.path.join(DEJAVU, "DejaVuSans-Bold.ttf")),
    "mono": (os.path.join(WINDOWS_FONTS, "consola.ttf"),
             os.path.join(DEJAVU, "DejaVuSansMono.ttf")),
}


def font_path(kind):
    for candidate in FONTS[kind]:
        if os.path.exists(candidate):
            return candidate
    raise SystemExit("no %s font found; looked at %s"
                     % (kind, ", ".join(FONTS[kind])))


FONT_PATH = font_path("regular")
FONT_BOLD = font_path("bold")
FONT_MONO = font_path("mono")

SCALE = 2

# Which role each swatch shows its text in. A swatch that is a background
# shows the text colour of its own palette on it; a swatch that is itself a
# text colour shows the background it is normally read against.
TEXT_OVER = {
    "back top": "text",
    "back middle": "text",
    "back bottom": "text",
    "surface": "text",
    "accent": "text",
    "accent muted": "text",
    "text": "back middle",
    "secondary": "back middle",
    "disabled": "back middle",
}


def read_palettes(path):
    """name -> list of (role, (r, g, b)), in the order Theme.cpp declares."""
    text = open(path, encoding="utf-8-sig").read()
    palettes = {}
    for match in re.finditer(r"const ThemePalette k(\w+)Palette\s*=\s*\{(.*?)\n\t\};",
                             text, flags=re.S):
        entries = []
        for line in match.group(2).splitlines():
            rgb = re.search(r"RGB\(\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})"
                            r"\s*,\s*0x([0-9A-Fa-f]{2})\s*\)", line)
            role = re.search(r"//\s*([a-z ]+?)\s{2,}#", line)
            if rgb and role:
                entries.append((role.group(1).strip(),
                                tuple(int(part, 16) for part in rgb.groups())))
        if entries:
            palettes[match.group(1)] = entries
    return palettes


def _channel(value):
    value /= 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def contrast(first, second):
    def lum(colour):
        red, green, blue = colour
        return (0.2126 * _channel(red) + 0.7152 * _channel(green)
                + 0.0722 * _channel(blue))
    light, dark = lum(first), lum(second)
    if light < dark:
        light, dark = dark, light
    return (light + 0.05) / (dark + 0.05)


def hex_of(colour):
    return "#%02X%02X%02X" % colour


def mix(first, second, position):
    return tuple(round(a + (b - a) * position) for a, b in zip(first, second))


def emit(img, out_path, check=False):
    """Writes the picture, or -- with check -- says whether it is current.

    The generators are deterministic: the same sources produce the same
    bytes. That makes "is the checked-in picture still the picture these
    sources describe?" a byte comparison, which is what --check asks.
    Returns True when the file on disk is what this run would have written.
    """
    import io as _io
    buffer = _io.BytesIO()
    img.save(buffer, format="PNG")
    fresh = buffer.getvalue()

    if check:
        if not os.path.exists(out_path):
            print("%s  MISSING -- run this script without --check" % out_path)
            return False
        if open(out_path, "rb").read() != fresh:
            print("%s  STALE -- the sources have moved on, re-run this script"
                  % out_path)
            return False
        print("%s  current  (%d x %d)" % (out_path, img.width, img.height))
        return True

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as handle:
        handle.write(fresh)
    print("%s  (%d x %d)" % (out_path, img.width, img.height))
    return True

def render(palettes, out_path, check=False):
    px = SCALE
    col_w, row_h, gap = 300 * px, 46 * px, 10 * px
    pad = 24 * px
    head_h = 34 * px
    strip_h = 26 * px

    roles = [role for role, _ in palettes["Dark"]]
    width = pad * 2 + col_w * 2 + gap
    height = (pad * 2 + head_h + strip_h + gap
              + (row_h + gap) * len(roles) + 18 * px)

    img = Image.new("RGB", (width, height), (0xFF, 0xFF, 0xFF))
    draw = ImageDraw.Draw(img)

    font = ImageFont.truetype(FONT_PATH, 11 * px)
    small = ImageFont.truetype(FONT_PATH, 9 * px)
    mono = ImageFont.truetype(FONT_MONO, 10 * px)
    bold = ImageFont.truetype(FONT_BOLD, 13 * px)

    ink = dict(palettes["Light"])["text"]
    quiet = dict(palettes["Light"])["secondary"]

    draw.text((pad, pad - 6 * px), "FilterKeys Setter -- palette, from Theme.cpp",
              font=bold, fill=ink)

    for index, name in enumerate(("Dark", "Light")):
        entries = palettes[name]
        by_role = dict(entries)
        x0 = pad + index * (col_w + gap)
        y = pad + head_h

        draw.text((x0, y - 20 * px), "%s theme" % name, font=font, fill=ink)

        # The background gradient of this theme, as three stops.
        stops = [by_role["back top"], by_role["back middle"], by_role["back bottom"]]
        for step in range(col_w):
            position = step / float(col_w - 1) * 2
            segment = 0 if position < 1 else 1
            colour = mix(stops[segment], stops[segment + 1], position - segment)
            draw.line([(x0 + step, y), (x0 + step, y + strip_h)], fill=colour)
        draw.rectangle([x0, y, x0 + col_w - 1, y + strip_h],
                       outline=by_role["accent muted"])
        y += strip_h + gap

        for role, colour in entries:
            over = by_role[TEXT_OVER[role]]
            if role in ("text", "secondary", "disabled"):
                face, pen = over, colour
            else:
                face, pen = colour, over

            draw.rectangle([x0, y, x0 + col_w - 1, y + row_h - 1], fill=face,
                           outline=by_role["accent muted"])
            draw.text((x0 + 10 * px, y + 8 * px), role, font=font, fill=pen)
            draw.text((x0 + 10 * px, y + 25 * px), hex_of(colour), font=mono,
                      fill=pen)
            # The accents are fills and outlines, never text, so a ratio
            # against them would be a figure nobody should rely on.
            if role in ("accent", "accent muted"):
                note = "fill and outline only"
            else:
                note = "%.2f:1" % contrast(face, pen)
            draw.text((x0 + col_w - 10 * px, y + 25 * px), note, font=small,
                      fill=pen, anchor="ra")
            y += row_h + gap

    draw.text((pad, height - pad - 2 * px),
              "Each row shows the colour against the one it is normally seen "
              "with, and the WCAG 2 ratio between them.",
              font=small, fill=quiet)

    return emit(img, out_path, check)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=os.path.join(ROOT, "docs", "img"))
    parser.add_argument("--check", action="store_true",
                        help="write nothing; fail if the sheet is out of date")
    args = parser.parse_args()

    source = os.path.join(ROOT, "Theme.cpp")
    palettes = read_palettes(source)
    missing = [name for name in ("Dark", "Light") if name not in palettes]
    if missing:
        print("no %s palette found in Theme.cpp" % ", ".join(missing))
        return 1

    if not args.check:
        os.makedirs(args.out, exist_ok=True)
    ok = render(palettes, os.path.join(args.out, "palette.png"), args.check)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
