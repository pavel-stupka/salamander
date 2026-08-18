#!/usr/bin/env python3
"""Generate src/res/syncpend.ico - the cloud "sync pending" overlay badge.

The badge is original artwork drawn in the Windows overlay style (feature 059):
a full-frame transparent icon whose glyph - two blue circular arrows - sits in
the lower-left quadrant, matching how shell overlay icons position their
glyphs (measured against the OneDrive overlay set: 32 px frame -> ~16 px
glyph, 16 px frame -> ~10 px glyph). It is deliberately NOT a copy of any
Microsoft icon resource.

Usage:  python tools/brand/gen_overlay_syncpend.py
Output: src/res/syncpend.ico (frames 16/32/48, 32-bit with alpha)
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw

# Windows accent blue, matching the system sync-badge hue.
BLUE = (0, 120, 215, 255)
SS = 8  # supersampling factor

# frame size -> glyph box size (lower-left corner), measured from the shell's
# own overlay proportions
FRAMES = {16: 10, 32: 16, 48: 24}


def _arrowhead(draw, cx, cy, r, theta_deg, size):
    """Filled triangular arrowhead at circle angle theta (deg, y-down,
    0 = +x, clockwise positive), pointing in the direction of travel."""
    th = math.radians(theta_deg)
    px, py = cx + r * math.cos(th), cy + r * math.sin(th)
    # direction of travel for clockwise motion (angles increasing)
    tx, ty = -math.sin(th), math.cos(th)
    # radial unit vector (outward)
    rx, ry = math.cos(th), math.sin(th)
    tip = (px + tx * size, py + ty * size)
    base1 = (px - tx * size * 0.2 + rx * size * 0.85, py - ty * size * 0.2 + ry * size * 0.85)
    base2 = (px - tx * size * 0.2 - rx * size * 0.85, py - ty * size * 0.2 - ry * size * 0.85)
    draw.polygon([tip, base1, base2], fill=BLUE)


def render_frame(frame: int, glyph: int) -> Image.Image:
    g = glyph * SS
    img = Image.new("RGBA", (g, g), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    stroke = max(2 * SS, round(g * 0.14))
    margin = round(g * 0.04) + stroke // 2
    bbox = [margin, margin, g - 1 - margin, g - 1 - margin]
    cx = (bbox[0] + bbox[2]) / 2.0
    cy = (bbox[1] + bbox[3]) / 2.0
    r = (bbox[2] - bbox[0]) / 2.0

    # two opposing arcs; arrowheads continue each arc's clockwise travel
    draw.arc(bbox, start=190, end=340, fill=BLUE, width=stroke)
    draw.arc(bbox, start=10, end=160, fill=BLUE, width=stroke)
    ah = stroke * 1.35
    _arrowhead(draw, cx, cy, r, 340, ah)
    _arrowhead(draw, cx, cy, r, 160, ah)

    glyph_img = img.resize((glyph, glyph), Image.LANCZOS)

    out = Image.new("RGBA", (frame, frame), (0, 0, 0, 0))
    out.paste(glyph_img, (0, frame - glyph))  # lower-left quadrant
    return out


def main():
    root = Path(__file__).resolve().parents[2]
    target = root / "src" / "res" / "syncpend.ico"
    # base frame must be the largest one; Pillow derives the directory from it
    frames = [render_frame(f, g) for f, g in sorted(FRAMES.items(), reverse=True)]
    frames[0].save(
        target,
        format="ICO",
        append_images=frames[1:],
        sizes=[(f, f) for f in sorted(FRAMES, reverse=True)],
        bitmap_format="bmp",
    )
    print(f"written: {target}")


if __name__ == "__main__":
    main()
