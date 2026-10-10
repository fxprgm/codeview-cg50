"""Draw the menu icons (92x64): assets-cg/icon-uns.png and icon-sel.png.

usage: python3 tools/mkicon.py   (needs Pillow and the Impact font of macOS)
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMPACT = '/System/Library/Fonts/Supplemental/Impact.ttf'

# a few lines of "code": (indent, colour, length)
LINES = [(0, (200, 120, 230), 18), (1, (110, 200, 255), 30), (2, (240, 150, 100), 22),
         (1, (200, 120, 230), 12), (0, (120, 210, 120), 26)]


def icon(bg, frame, path):
    im = Image.new('RGB', (92, 64), bg)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 91, 63], outline=frame, width=2)
    d.rectangle([5, 20, 13, 58], fill=tuple(min(255, c + 18) for c in bg))
    for i, (ind, col, n) in enumerate(LINES):
        y = 23 + i * 7
        d.rectangle([8, y + 1, 10, y + 2], fill=(140, 140, 150))  # a line number, as a dot
        d.rectangle([18 + ind * 8, y, 18 + ind * 8 + n * 2, y + 3], fill=col)
    font = ImageFont.truetype(IMPACT, 16)
    w = d.textlength('CodeView', font=font)
    d.text(((92 - w) / 2, 1), 'CodeView', font=font, fill=(240, 240, 245))
    im.save(path)


icon((28, 30, 38), (90, 95, 110), os.path.join(ROOT, 'assets-cg', 'icon-uns.png'))
icon((50, 62, 100), (255, 255, 255), os.path.join(ROOT, 'assets-cg', 'icon-sel.png'))
