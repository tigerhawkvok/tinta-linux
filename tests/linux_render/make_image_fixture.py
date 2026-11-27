#!/usr/bin/env python3
"""Build tests/fixtures/image-cache-large.md with its generated images.

The assets match tests/validate_large_images.py (issue #220): flat and
noisy JPEGs up to 6000x4000, a translucent PNG, a 300 DPI PNG, a
12000x12 panorama, a broken JPEG and a BMP whose header claims
20000x20000. Needs Pillow and NumPy.

  make_image_fixture.py OUT_DIR   -> OUT_DIR/image-cache-large.md + assets
"""
import shutil
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[2]


def main():
    out = Path(sys.argv[1]).resolve()
    assets = out / "image-cache-assets"
    assets.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", (256, 128), (45, 130, 190)).save(assets / "small.png")
    pixels = np.random.default_rng(220).integers(0, 256, (4000, 6000, 3), dtype=np.uint8)
    Image.fromarray(pixels).save(assets / "large.jpg", quality=60)
    del pixels
    shutil.copy2(assets / "large.jpg", assets / "large-second.jpg")
    Image.new("RGBA", (128, 64), (20, 50, 100, 128)).save(assets / "alpha.png")
    Image.new("RGB", (300, 150), (190, 130, 45)).save(assets / "dpi.png", dpi=(300, 300))
    Image.new("RGB", (12000, 12), (45, 130, 190)).save(assets / "wide.png")
    (assets / "broken.jpg").write_bytes(b"not an image")
    (assets / "oversized.bmp").write_bytes(
        struct.pack("<2sIHHI", b"BM", 54, 0, 0, 54) +
        struct.pack("<IiiHHIIiiII", 40, 20000, 20000, 1, 24, 0, 0, 0, 0, 0, 0))
    shutil.copy2(REPO / "tests/fixtures/image-cache-large.md", out / "image-cache-large.md")
    print(out / "image-cache-large.md")


if __name__ == "__main__":
    main()
