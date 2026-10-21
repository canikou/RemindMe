# SPDX-License-Identifier: MIT
"""Draw RemindMe's app icon: an amber clock face on a navy split-flap plate.

The plate is two-tone like a real flap (lighter upper half) and split by a
see-through seam that cuts the clock too, so the split-flap read survives
from 256px down to the 16px tray icon. Small sizes thin the hands and drop
the centre dot so the hands don't clot.

Writes resources/icons/app.png (256px, with provenance) and
resources/icons/app.ico (16-256px).

usage: python scripts/make-app-icon.py   (needs Pillow)
"""
from pathlib import Path

from PIL import Image, ImageDraw
from PIL.PngImagePlugin import PngInfo

UPPER = (0x22, 0x30, 0x5A, 255)
LOWER = (0x16, 0x21, 0x3A, 255)
AMBER = (0xF5, 0xC8, 0x4B, 255)
SUPERSAMPLE = 8
PROVENANCE = (
    "Origin: drawn in code by scripts/make-app-icon.py (Pillow), not by an image model. "
    "Two-tone navy split-flap plate (#22305A upper, #16213A lower) split by a transparent seam, "
    "with an amber (#F5C84B) clock face, hands at 12 and 10."
)


def draw(size: int) -> Image.Image:
    s = size * SUPERSAMPLE
    small = size <= 32

    plate = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(plate)
    d.rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.22), fill=LOWER)
    upper = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(upper).rounded_rectangle((0, 0, s - 1, s - 1), radius=int(s * 0.22), fill=UPPER)
    plate.paste(upper.crop((0, 0, s, s // 2)), (0, 0))

    c = s / 2
    r = s * (0.31 if small else 0.29)
    ring = s * (0.10 if small else 0.075)
    d = ImageDraw.Draw(plate)
    d.ellipse((c - r, c - r, c + r, c + r), outline=AMBER, width=int(ring))
    hand = s * (0.085 if size <= 16 else 0.09 if small else 0.065)
    d.line((c, c, c, c - r * 0.60), fill=AMBER, width=int(hand))
    # Hour hand at 10 o'clock keeps both hands above the seam, which runs through the pivot.
    d.line((c, c, c - r * 0.46 * 0.866, c - r * 0.46 * 0.5), fill=AMBER, width=int(hand))
    if not small:
        dot = hand * 0.8
        d.ellipse((c - dot, c - dot, c + dot, c + dot), fill=AMBER)

    # The seam: a see-through gap across the whole plate, at least one real pixel.
    gap = max(SUPERSAMPLE, int(s * (0.06 if small else 0.03)))
    d.rectangle((0, s // 2 - gap // 2, s, s // 2 + gap - gap // 2), fill=(0, 0, 0, 0))

    return plate.resize((size, size), Image.LANCZOS)


def main() -> None:
    root = Path(__file__).resolve().parent.parent / "resources" / "icons"
    sizes = [256, 128, 64, 48, 32, 24, 16]
    frames = {size: draw(size) for size in sizes}
    info = PngInfo()
    info.add_text("impeccable:prompt", PROVENANCE)
    frames[256].save(root / "app.png", pnginfo=info)
    frames[256].save(root / "app.ico", sizes=[(n, n) for n in sizes],
                     append_images=[frames[n] for n in sizes[1:]])
    print("wrote", root / "app.png", "and", root / "app.ico")


if __name__ == "__main__":
    main()
