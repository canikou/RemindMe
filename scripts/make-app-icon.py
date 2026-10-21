# SPDX-License-Identifier: MIT
"""Draw RemindMe's app icon: an amber clock face on a navy split-flap tile.

Writes resources/icons/app.png (256px) and resources/icons/app.ico (16-256px).
Small sizes use a simplified drawing (no tile, thicker strokes) so they stay legible.

usage: python scripts/make-app-icon.py   (needs Pillow)
"""
from pathlib import Path

from PIL import Image, ImageDraw

BOARD = (0x16, 0x21, 0x3A, 255)
RAISED = (0x22, 0x30, 0x5A, 255)
AMBER = (0xF5, 0xC8, 0x4B, 255)
SUPERSAMPLE = 8


def draw(size: int) -> Image.Image:
    s = size * SUPERSAMPLE
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    small = size <= 32

    d.rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.22), fill=BOARD)

    if not small:
        # The split-flap tile, with its seam across the middle.
        inset = int(s * 0.13)
        d.rounded_rectangle((inset, inset, s - inset, s - inset), radius=int(s * 0.10), fill=RAISED)
        seam = max(SUPERSAMPLE, int(s * 0.022))
        d.rectangle((inset, s // 2 - seam // 2, s - inset, s // 2 + seam // 2), fill=BOARD)

    c = s / 2
    r = s * (0.33 if small else 0.27)
    ring = s * (0.11 if small else 0.065)
    d.ellipse((c - r, c - r, c + r, c + r), outline=AMBER, width=int(ring))

    hand = s * (0.10 if small else 0.06)
    # Minute hand to 12, hour hand to 3: an L that reads at 16px.
    d.line((c, c, c, c - r * 0.62), fill=AMBER, width=int(hand))
    d.line((c, c, c + r * 0.48, c), fill=AMBER, width=int(hand))
    dot = hand * 0.75
    d.ellipse((c - dot, c - dot, c + dot, c + dot), fill=AMBER)

    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    root = Path(__file__).resolve().parent.parent / "resources" / "icons"
    sizes = [256, 128, 64, 48, 32, 24, 16]
    frames = {size: draw(size) for size in sizes}
    frames[256].save(root / "app.png")
    frames[256].save(root / "app.ico", sizes=[(n, n) for n in sizes],
                     append_images=[frames[n] for n in sizes[1:]])
    print("wrote", root / "app.png", "and", root / "app.ico")


if __name__ == "__main__":
    main()
