#!/usr/bin/env python3
"""Render a preview of the main dialog straight from FilterKeysSetter.rc.

This is NOT a screenshot. It is a drawing of the resource script: the geometry
of every control comes from the .rc, the colours from the palette in Theme.cpp,
so the preview stays truthful about layout and theme even on a machine that
cannot run the program. Two things it cannot reproduce faithfully:

  * the dialog font (Segoe UI is a Windows font; DejaVu Sans is substituted),
  * native control chrome drawn by the common controls library.

Replace the result with a real screenshot once one is available.

Usage:
    python tools/render-dialog.py                       both themes
    python tools/render-dialog.py --theme dark          one theme
    python tools/render-dialog.py --out docs/img        output directory
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Dialog base units for Segoe UI 9 pt at 96 dpi: 1 horizontal DLU is a quarter
# of the average character width, 1 vertical DLU an eighth of the line height.
DLU_X = 1.75
DLU_Y = 1.875

SCALE = 2        # output pixels per 96 dpi pixel
SS = 2           # extra supersampling, removed again when the image is resized

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

TITLE_H_DLU = 16

# Values shown in the preview. Chosen to look like a plausible session rather
# than to exercise edge cases.
SAMPLE = {
    "IDC_WAIT_EDIT": "0",
    "IDC_DELAY_EDIT": "250",
    "IDC_REPEAT_EDIT": "20",
    "IDC_BOUNCE_EDIT": "0",
    "IDC_TEST_EDIT": "Type here to try the new repeat rate",
    "IDC_CHARS_PER_SEC": "(50.0 per second)",
    "IDC_FLAGVAL": "Flags: 59 (0x3B)",
    "IDC_STATUS": "FilterKeys on: ignore 0 / delay 250 / repeat 20 ms",
}

CHECKED = {"IDC_ON", "IDC_AVAILABLE", "IDC_HOTKEYACTIVE", "IDC_HOTKEYSOUND",
           "IDC_INDICATOR", "IDC_UPDATEINIFILE", "IDC_SENDCHANGE",
           "IDC_DARKTHEME"}

SELECTED_RADIO = "IDC_IGNORE_QUICK"

# Slider thumb position as a fraction of the channel.
THUMB = {"IDC_DELAY_SLIDER": 0.125, "IDC_REPEAT_SLIDER": 0.01}

DARK = {
    "back_top": (0x2A, 0x4A, 0x7B),
    "back_mid": (0x1F, 0x3A, 0x61),
    "back_bottom": (0x14, 0x26, 0x3F),
    "surface": (0x1D, 0x51, 0x88),
    "accent": (0x5D, 0x9C, 0xD6),
    "accent_muted": (0x37, 0x78, 0xB5),
    "text": (0xFF, 0xFF, 0xFF),
    "secondary": (0xE0, 0xE0, 0xE0),
    "disabled": (0xA8, 0xBB, 0xD6),
    "title": (0x14, 0x26, 0x3F),
    "title_text": (0xFF, 0xFF, 0xFF),
    "frame": (0x0D, 0x1A, 0x2C),
}

LIGHT = {
    "back_top": (0xF0, 0xF0, 0xF0),
    "back_mid": (0xF0, 0xF0, 0xF0),
    "back_bottom": (0xF0, 0xF0, 0xF0),
    "surface": (0xFF, 0xFF, 0xFF),
    "accent": (0x1D, 0x51, 0x88),
    "accent_muted": (0x37, 0x78, 0xB5),
    "text": (0x00, 0x00, 0x00),
    "secondary": (0x44, 0x44, 0x44),
    "disabled": (0x6D, 0x6D, 0x6D),
    "title": (0xFF, 0xFF, 0xFF),
    "title_text": (0x00, 0x00, 0x00),
    "frame": (0x9B, 0x9B, 0x9B),
}


def parse_dialog(rc_text, name):
    """Minimal .rc dialog reader: size plus one entry per control."""
    block = rc_text[rc_text.index(name + " DIALOGEX"):]
    head = block[:block.index("BEGIN")]
    body = block[block.index("BEGIN") + 5:block.index("\nEND")]

    size = re.search(r"DIALOGEX\s+\d+,\s*\d+,\s*(\d+),\s*(\d+)", head)
    caption = re.search(r'CAPTION\s+"([^"]*)"', head)

    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r",\s*\n\s+", ", ", body)

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
        text = re.match(r'\w+\s+"([^"]*)"', line)
        klass = re.search(r'"(Button|msctls_trackbar32|Static|Edit)"', line)
        controls.append({
            "kind": line.split()[0],
            "id": ident.group(1) if ident else "IDC_STATIC",
            "rect": (x, y, w, h),
            "text": text.group(1) if text else "",
            "class": klass.group(1) if klass else "",
            "style": line,
        })
    return (int(size.group(1)), int(size.group(2)),
            caption.group(1) if caption else "", controls)


def split_mnemonic(text):
    """'Sa&ve' -> ('Save', 2). Returns -1 when the caption has no & marker."""
    out, idx, i = [], -1, 0
    while i < len(text):
        if text[i] == "&" and i + 1 < len(text):
            if text[i + 1] == "&":
                out.append("&")
                i += 2
                continue
            idx = len(out)
            i += 1
            continue
        out.append(text[i])
        i += 1
    return "".join(out), idx


class Canvas:
    def __init__(self, width_dlu, height_dlu, pal):
        self.k = SCALE * SS
        self.pal = pal
        self.w = int(width_dlu * DLU_X * self.k)
        self.h = int((height_dlu + TITLE_H_DLU) * DLU_Y * self.k)
        self.top = int(TITLE_H_DLU * DLU_Y * self.k)
        self.img = Image.new("RGB", (self.w, self.h), pal["back_top"])
        self.d = ImageDraw.Draw(self.img)
        self.font = ImageFont.truetype(FONT_PATH, int(9 * 96 / 72 * self.k * 0.92))
        self.font_bold = ImageFont.truetype(FONT_BOLD, int(9 * 96 / 72 * self.k * 0.92))

    # -- geometry ---------------------------------------------------------
    def box(self, rect):
        x, y, w, h = rect
        return (x * DLU_X * self.k,
                self.top + y * DLU_Y * self.k,
                (x + w) * DLU_X * self.k,
                self.top + (y + h) * DLU_Y * self.k)

    def px(self, n):
        return n * self.k

    def label(self, xy, text, anchor, colour, font=None):
        """Draws a caption, underlining the character marked with &."""
        font = font or self.font
        clean, idx = split_mnemonic(text)
        x, y = xy
        self.d.text((x, y), clean, font=font, fill=colour, anchor=anchor)
        if idx < 0:
            return
        width = self.d.textlength(clean, font=font)
        if anchor[0] == "r":
            x -= width
        elif anchor[0] == "m":
            x -= width / 2
        before = self.d.textlength(clean[:idx], font=font)
        glyph = self.d.textlength(clean[idx], font=font)
        baseline = y + (font.size * 0.42 if anchor[1] == "m" else font.size * 0.95)
        self.d.line([(x + before, baseline), (x + before + glyph, baseline)],
                    fill=colour, width=max(1, self.k // 3))

    # -- painting ---------------------------------------------------------
    def _gradient(self, t):
        """Three stop gradient, same split as CTheme::PaintBackgroundSlice."""
        stops = (self.pal["back_top"], self.pal["back_mid"], self.pal["back_bottom"])
        if t < 0.5:
            a, b, u = stops[0], stops[1], t * 2
        else:
            a, b, u = stops[1], stops[2], (t - 0.5) * 2
        return tuple(int(a[c] + (b[c] - a[c]) * u) for c in range(3))

    def background(self):
        for i in range(self.top, self.h):
            t = (i - self.top) / max(1, self.h - self.top - 1)
            self.d.line([(0, i), (self.w, i)], fill=self._gradient(t))

    def title_bar(self, caption):
        self.d.rectangle([0, 0, self.w, self.top], fill=self.pal["title"])
        self.d.text((self.px(8), self.top / 2), caption, font=self.font,
                    fill=self.pal["title_text"], anchor="lm")
        for i, glyph in enumerate(("\u2014", "\u2715")):
            cx = self.w - self.px(34) + i * self.px(22)
            self.d.text((cx, self.top / 2), glyph, font=self.font,
                        fill=self.pal["title_text"], anchor="mm")
        self.d.rectangle([0, 0, self.w - 1, self.h - 1], outline=self.pal["frame"],
                         width=max(1, self.k // 2))

    def text(self, rect, s, align="left", colour=None, font=None):
        x0, y0, x1, y1 = self.box(rect)
        colour = colour or self.pal["text"]
        font = font or self.font
        y = (y0 + y1) / 2
        if align == "right":
            self.label((x1, y), s, "rm", colour, font)
        elif align == "center":
            self.label(((x0 + x1) / 2, y), s, "mm", colour, font)
        else:
            self.label((x0, y), s, "lm", colour, font)

    def groupbox(self, rect, caption):
        x0, y0, x1, y1 = self.box(rect)
        # The program draws the frame in the muted accent (see
        # PaintThemedGroupBox); the preview has to use the same colour or it
        # flatters the implementation.
        pen = self.pal["accent_muted"] if self.pal is DARK else self.pal["disabled"]
        self.d.rectangle([x0, y0 + self.px(4), x1, y1], outline=pen,
                         width=max(1, self.k // 2))
        if caption:
            w = self.d.textlength(caption, font=self.font)
            self.d.rectangle([x0 + self.px(6), y0 - self.px(1),
                              x0 + self.px(12) + w, y0 + self.px(9)],
                             fill=self._blend_at(y0))
            self.d.text((x0 + self.px(9), y0 + self.px(4)), caption,
                        font=self.font, fill=self.pal["text"], anchor="lm")

    def _blend_at(self, y):
        t = max(0.0, min(1.0, (y - self.top) / max(1, self.h - self.top - 1)))
        return self._gradient(t)

    def edit(self, rect, value):
        x0, y0, x1, y1 = self.box(rect)
        self.d.rectangle([x0, y0, x1, y1], fill=self.pal["surface"],
                         outline=self.pal["disabled"], width=max(1, self.k // 2))
        self.d.text((x0 + self.px(4), (y0 + y1) / 2), value, font=self.font,
                    fill=self.pal["text"], anchor="lm")

    def button(self, rect, caption, default=False):
        x0, y0, x1, y1 = self.box(rect)
        self.d.rectangle([x0, y0, x1, y1], fill=self.pal["surface"],
                         outline=self.pal["accent"] if default else self.pal["disabled"],
                         width=max(1, self.k // 2) * (2 if default else 1))
        self.label(((x0 + x1) / 2, (y0 + y1) / 2), caption, "mm", self.pal["text"])

    def checkbox(self, rect, caption, checked):
        x0, y0, x1, y1 = self.box(rect)
        side = self.px(11)
        cy = (y0 + y1) / 2
        bx0, by0 = x0, cy - side / 2
        self.d.rectangle([bx0, by0, bx0 + side, by0 + side],
                         fill=self.pal["accent"] if checked else self.pal["surface"],
                         outline=self.pal["accent"] if checked else self.pal["disabled"],
                         width=max(1, self.k // 2))
        if checked:
            self.d.line([(bx0 + side * 0.22, by0 + side * 0.52),
                         (bx0 + side * 0.42, by0 + side * 0.74),
                         (bx0 + side * 0.80, by0 + side * 0.26)],
                        fill=(0xFF, 0xFF, 0xFF) if self.pal is DARK else (0xFF, 0xFF, 0xFF),
                        width=max(2, self.k))
        self.label((bx0 + side + self.px(5), cy), caption, "lm", self.pal["text"])

    def radio(self, rect, caption, selected):
        x0, y0, x1, y1 = self.box(rect)
        side = self.px(11)
        cy = y0 + self.px(7)
        self.d.ellipse([x0, cy - side / 2, x0 + side, cy + side / 2],
                       fill=self.pal["surface"],
                       outline=self.pal["accent"] if selected else self.pal["disabled"],
                       width=max(1, self.k // 2))
        if selected:
            inset = side * 0.3
            self.d.ellipse([x0 + inset, cy - side / 2 + inset,
                            x0 + side - inset, cy + side / 2 - inset],
                           fill=self.pal["accent"])
        self._wrapped(caption, x0 + side + self.px(5), y0 + self.px(2),
                      x1 - (x0 + side + self.px(5)))

    def _wrapped(self, caption, x, y, width):
        clean, idx = split_mnemonic(caption)
        words, line, lines = clean.split(), "", []
        for word in words:
            probe = (line + " " + word).strip()
            if self.d.textlength(probe, font=self.font) <= width:
                line = probe
            else:
                lines.append(line)
                line = word
        lines.append(line)
        consumed = 0
        for i, text in enumerate(lines):
            self.d.text((x, y + i * self.px(9)), text, font=self.font,
                        fill=self.pal["text"], anchor="la")
            # The & may sit on any of the wrapped lines; underline it there.
            if 0 <= idx - consumed < len(text):
                local = idx - consumed
                before = self.d.textlength(text[:local], font=self.font)
                glyph = self.d.textlength(text[local], font=self.font)
                baseline = y + i * self.px(9) + self.font.size * 0.95
                self.d.line([(x + before, baseline), (x + before + glyph, baseline)],
                            fill=self.pal["text"], width=max(1, self.k // 3))
            consumed += len(text) + 1

    def slider(self, rect, position):
        x0, y0, x1, y1 = self.box(rect)
        cy = (y0 + y1) / 2
        self.d.rectangle([x0, cy - self.px(2), x1, cy + self.px(2)],
                         fill=self.pal["surface"], outline=self.pal["accent_muted"],
                         width=max(1, self.k // 2))
        tx = x0 + (x1 - x0 - self.px(8)) * position
        self.d.rectangle([tx, y0 + self.px(1), tx + self.px(8), y1 - self.px(1)],
                         fill=self.pal["accent"], outline=self.pal["text"],
                         width=max(1, self.k // 2))


def render(rc_path, theme, out_path):
    rc_text = open(rc_path, encoding="utf-8-sig").read()
    width, height, caption, controls = parse_dialog(rc_text, "IDD_FILTERKEYSSETTER_DIALOG")

    pal = DARK if theme == "dark" else LIGHT
    c = Canvas(width, height, pal)
    c.background()

    # Group boxes first: in the resource they come last so that they do not
    # steal mouse input, but visually they sit behind everything.
    for ctl in controls:
        if ctl["kind"] == "GROUPBOX":
            c.groupbox(ctl["rect"], ctl["text"])

    for ctl in controls:
        kind, cid, rect, text = ctl["kind"], ctl["id"], ctl["rect"], ctl["text"]
        if kind == "GROUPBOX":
            continue
        if kind == "EDITTEXT":
            c.edit(rect, SAMPLE.get(cid, ""))
        elif kind in ("PUSHBUTTON", "DEFPUSHBUTTON"):
            c.button(rect, text, default=(kind == "DEFPUSHBUTTON"))
        elif kind in ("LTEXT", "RTEXT", "CTEXT"):
            align = {"LTEXT": "left", "RTEXT": "right", "CTEXT": "center"}[kind]
            colour = pal["secondary"] if cid in SAMPLE else pal["text"]
            c.text(rect, SAMPLE.get(cid, text), align=align, colour=colour)
        elif ctl["class"] == "msctls_trackbar32":
            c.slider(rect, THUMB.get(cid, 0.5))
        elif "BS_AUTOCHECKBOX" in ctl["style"]:
            checked = cid in CHECKED
            if cid == "IDC_DARKTHEME":
                checked = theme == "dark"   # the box mirrors the theme shown
            c.checkbox(rect, text, checked)
        elif "BS_AUTORADIOBUTTON" in ctl["style"]:
            c.radio(rect, text, cid == SELECTED_RADIO)

    c.title_bar(caption)

    img = c.img.resize((c.w // SS, c.h // SS), Image.LANCZOS)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    img.save(out_path)
    print("%s  (%d x %d)" % (out_path, img.width, img.height))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rc", default=os.path.join(ROOT, "FilterKeysSetter.rc"))
    ap.add_argument("--theme", choices=("dark", "light", "both"), default="both")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "img"))
    args = ap.parse_args()

    themes = ("dark", "light") if args.theme == "both" else (args.theme,)
    for theme in themes:
        render(args.rc, theme, os.path.join(args.out, "dialog-%s.png" % theme))
    return 0


if __name__ == "__main__":
    sys.exit(main())
