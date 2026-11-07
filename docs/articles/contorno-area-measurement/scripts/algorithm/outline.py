#!/usr/bin/env python3
"""Run the contour of Part I on photographs.

    outline.py <model.pt> <out dir> <photo>...

For each photo: the network's region (Section 3.4) and the result of the
background check (Section 3.5). Writes <out>/<photo stem>_outline.png (red:
the network alone; green: the result, outside darkened) and prints each one's
share of the photo.
"""
from __future__ import annotations

import sys
from pathlib import Path

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import contour  # noqa: E402


def draw(mask: np.ndarray, colour: tuple[int, int, int], out: np.ndarray) -> None:
    contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    cv2.drawContours(out, contours, -1, colour, max(2, out.shape[1] // 400))


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__, file=sys.stderr)
        return 2
    model_file, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for name in sys.argv[3:]:
        bgr = cv2.imread(name, cv2.IMREAD_COLOR)
        if bgr is None:
            print(f"{name}: cannot read", file=sys.stderr)
            return 1
        region = contour.model_region(bgr, model_file)
        result = contour.background_check(bgr, region)
        image = bgr.copy()
        image[result == 0] = (0.4 * image[result == 0]).astype(np.uint8)
        draw(region, (38, 38, 220), image)
        draw(result, (85, 157, 31), image)
        cv2.imwrite(str(out / f"{Path(name).stem}_outline.png"), image)
        share = lambda m: 100 * np.count_nonzero(m) / m.size  # noqa: E731
        taken = "background" if not np.array_equal(region, result) else "network"
        print(f"{Path(name).name}: network {share(region):.1f} %, result {share(result):.1f} % ({taken})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
