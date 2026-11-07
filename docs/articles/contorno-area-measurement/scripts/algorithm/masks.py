"""Operations on binary masks (uint8, 0 or 255) shared by the contour pipelines."""
from __future__ import annotations

import cv2
import numpy as np


def clean_mask(mask: np.ndarray) -> np.ndarray:
    """Step 2 of Section 3.4: every contour becomes a simple closed curve.

    Repeats until nothing changes: pixels in no 2 x 2 block of set pixels are
    cleared (lines and spurs one pixel wide), and of each 2 x 2 block where
    only the two diagonal pixels are set, the upper one is cleared (two
    regions touching at a corner).
    """
    m = mask > 0
    if m.shape[0] < 2 or m.shape[1] < 2:
        return np.zeros_like(mask)
    while True:
        block = m[:-1, :-1] & m[:-1, 1:] & m[1:, :-1] & m[1:, 1:]
        keep = np.zeros_like(m)
        keep[:-1, :-1] |= block
        keep[:-1, 1:] |= block
        keep[1:, :-1] |= block
        keep[1:, 1:] |= block
        changed = bool((m & ~keep).any())
        m &= keep
        a, b, d, e = m[:-1, :-1], m[:-1, 1:], m[1:, :-1], m[1:, 1:]
        falling = a & e & ~b & ~d  # top-left and bottom-right only
        rising = b & d & ~a & ~e   # top-right and bottom-left only
        if falling.any() or rising.any():
            changed = True
            m[:-1, :-1] &= ~falling
            m[:-1, 1:] &= ~rising
        if not changed:
            return np.where(m, 255, 0).astype(np.uint8)


def largest_region(mask: np.ndarray) -> tuple[np.ndarray, int]:
    """The largest 8-connected region (all zero when none) and its area in pixels."""
    n, labels, stats, _ = cv2.connectedComponentsWithStats((mask > 0).astype(np.uint8), connectivity=8)
    if n <= 1:
        return np.zeros(mask.shape, np.uint8), 0
    best = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    return np.where(labels == best, 255, 0).astype(np.uint8), int(stats[best, cv2.CC_STAT_AREA])


def enclosed_holes(region: np.ndarray, max_area_px: float = np.inf) -> np.ndarray:
    """The 4-connected parts of the complement that do not reach the border and have fewer than max_area_px pixels."""
    h, w = region.shape
    n, labels, stats, _ = cv2.connectedComponentsWithStats((region == 0).astype(np.uint8), connectivity=4)
    holes = np.zeros(n, np.uint8)
    for i in range(1, n):
        x, y, bw, bh, area = stats[i]
        touches = x == 0 or y == 0 or x + bw == w or y + bh == h
        holes[i] = 255 if area < max_area_px and not touches else 0
    return holes[labels]
