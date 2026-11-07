"""The classical contour of Section 3.1: colour statistics, Otsu's threshold, morphology, GrabCut.

    classical_hide(bgr) -> uint8 mask, 0 or 255 (holes filled), or None when nothing is found

The table's colour is the per-channel median, in CIELAB, of a band along the image
border. Each pixel's colour distance to it is thresholded by Otsu's method, or at
the valley below a second mode under Otsu's threshold (an object of two colours),
never below a minimum contrast. The result is opened and closed, its largest
region refined by GrabCut, and the largest region of that is the hide.
Images are 8-bit BGR (OpenCV's order).
"""
from __future__ import annotations

import cv2
import numpy as np

from masks import enclosed_holes, largest_region

SMOOTHING_PCT = 0.5     # diameter of the morphological kernel, % of the width (at least 3 px)
MIN_CONTRAST = 10.0     # CIELAB distance below which a pixel is never the hide
LIGHTNESS_WEIGHT = 0.5  # lightness counts half as much as chroma
BORDER_BAND_PCT = 2.0   # width of the band giving the table's colour, % of the smaller side
REFINE_PASSES = 5       # GrabCut iterations
MAX_DISTANCE = 128.0    # CIELAB distance that maps to 255
GRABCUT_MAX_SIDE = 1280
MIN_OBJECT_CHROMA = 5.0
ACHROMATIC_FLOOR_SHARE = 0.7


def border_band(h: int, w: int, pct: float) -> np.ndarray:
    b = max(1, int(round(pct / 100 * min(h, w))))
    band = np.zeros((h, w), bool)
    band[:b] = band[-b:] = True
    band[:, :b] = band[:, -b:] = True
    return band


def median_8bit(values: np.ndarray) -> int:
    """The lower median of 8-bit values (0 when there are none)."""
    if values.size == 0:
        return 0
    hist = np.bincount(values.ravel(), minlength=256)
    return int(min(255, np.searchsorted(np.cumsum(hist) * 2, values.size)))


def distance_image(lab: np.ndarray, table: np.ndarray) -> np.ndarray:
    """Weighted CIELAB distance to the table's colour, scaled to 0..255 (8-bit Lab: L is stored as L x 255 / 100)."""
    d = lab.astype(np.float64) - table.astype(np.float64)
    d[..., 0] *= LIGHTNESS_WEIGHT * 100 / 255
    return np.clip(np.sqrt((d * d).sum(-1)) * 255 / MAX_DISTANCE, 0, 255).astype(np.uint8)


def table_threshold(dist: np.ndarray, band: np.ndarray) -> int:
    """Otsu's threshold, or the valley below a second mode under it that does not reach the border (not a shadow)."""
    otsu = int(cv2.threshold(dist, 0, 255, cv2.THRESH_BINARY | cv2.THRESH_OTSU)[0])
    hist = np.bincount(dist.ravel(), minlength=256).astype(np.float64)
    smooth = np.array([hist[max(0, i - 4):min(255, i + 4) + 1].sum() for i in range(256)])
    band_hist = np.bincount(dist[band].ravel(), minlength=256).astype(np.float64)
    band_count = band.sum()

    def is_peak(i: int) -> bool:
        return (i == 0 or smooth[i] >= smooth[i - 1]) and smooth[i] >= smooth[i + 1]

    min_mass = 0.01 * dist.size
    for p in range(otsu):
        if not is_peak(p):
            continue
        valley = p
        for q in range(p + 1, otsu):
            if smooth[q] < smooth[valley]:
                valley = q
            if not is_peak(q) or smooth[valley] >= 0.5 * min(smooth[p], smooth[q]):
                continue
            upper = hist[valley:otsu + 1].sum()
            if hist[:valley].sum() < min_mass or upper < min_mass:
                continue
            if band_hist[valley:otsu + 1].sum() / band_count < 0.5 * upper / dist.size:
                return valley
    return otsu


def mostly_bordering(candidates: np.ndarray, region: np.ndarray) -> np.ndarray:
    """The parts of candidates outside the region whose edge runs mostly along the region (glare, not a shadow)."""
    parts = (candidates > 0) & (region == 0)
    n, labels = cv2.connectedComponents(parts.astype(np.uint8), connectivity=8)
    h, w = parts.shape
    edge = np.zeros(n, np.int64)
    along = np.zeros(n, np.int64)
    outside = ~parts
    seen = np.zeros((4, h, w), np.int32)
    for k, (dr, dc) in enumerate(((-1, 0), (1, 0), (0, -1), (0, 1))):
        shifted = np.zeros((h, w), np.int32)  # the label of the neighbour at (row + dr, col + dc)
        rs = slice(max(0, -dr), h - max(0, dr))
        rd = slice(max(0, dr), h - max(0, -dr))
        cs = slice(max(0, -dc), w - max(0, dc))
        cd = slice(max(0, dc), w - max(0, -dc))
        shifted[rs, cs] = labels[rd, cd]
        # each part counted once per edge pixel: skip a label seen from an earlier direction
        new = (shifted > 0) & outside
        for j in range(k):
            new &= seen[j] != shifted
        seen[k] = np.where(new, shifted, 0)
        np.add.at(edge, shifted[new], 1)
        np.add.at(along, shifted[new & (region > 0)], 1)
    keep = np.where(2 * along > edge, 255, 0).astype(np.uint8)
    keep[0] = 0
    return keep[labels]


def grabcut_refine(bgr, lab, table, dist, floor, otsu, region, kernel) -> np.ndarray:
    smooth = cv2.blur(dist, kernel.shape[::-1])
    h, w = dist.shape
    band = border_band(h, w, BORDER_BAND_PCT) & (region == 0) & (smooth <= 0.5 * floor)
    strong = np.where((region > 0) & (dist > otsu), 255, 0).astype(np.uint8)
    sure = cv2.erode(strong, kernel, iterations=3)
    d = lab[..., 1:].astype(np.float64) - table[..., 1:].astype(np.float64)
    chroma = np.clip(np.sqrt((d * d).sum(-1)), 0, 255).astype(np.uint8)
    if median_8bit(chroma[sure > 0]) > 2 * MIN_OBJECT_CHROMA:
        probable = (dist > floor) & (chroma > MIN_OBJECT_CHROMA)
    else:
        probable = mostly_bordering(np.where(smooth > ACHROMATIC_FLOOR_SHARE * floor, 255, 0).astype(np.uint8),
                                    region) > 0
    seeds = np.full((h, w), cv2.GC_PR_BGD, np.uint8)
    seeds[probable | (strong > 0)] = cv2.GC_PR_FGD
    seeds[sure > 0] = cv2.GC_FGD
    seeds[band] = cv2.GC_BGD
    scale = min(1.0, GRABCUT_MAX_SIDE / max(w, h))
    image, mask = bgr, seeds
    if scale < 1.0:
        small = (max(1, int(round(w * scale))), max(1, int(round(h * scale))))
        image = cv2.resize(bgr, small, interpolation=cv2.INTER_AREA)
        mask = cv2.resize(seeds, small, interpolation=cv2.INTER_NEAREST)
    bg_model = np.zeros((1, 65), np.float64)
    fg_model = np.zeros((1, 65), np.float64)
    cv2.grabCut(image, mask, None, bg_model, fg_model, REFINE_PASSES, cv2.GC_INIT_WITH_MASK)
    obj = np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 255, 0).astype(np.uint8)
    if scale < 1.0:
        soft = cv2.resize(obj.astype(np.float32) / 255, (w, h), interpolation=cv2.INTER_LINEAR)
        obj = np.where(soft > 0.5, 255, 0).astype(np.uint8)
    obj = cv2.morphologyEx(obj, cv2.MORPH_CLOSE, kernel)

    def wide_parts(change: np.ndarray) -> np.ndarray:
        core = cv2.dilate(cv2.morphologyEx(change, cv2.MORPH_OPEN, kernel), kernel)
        return change & core

    added = wide_parts(obj & ~region)
    removed = wide_parts(region & ~obj)
    return (region & ~removed) | added


def classical_hide(bgr: np.ndarray) -> np.ndarray | None:
    lab = cv2.cvtColor(bgr, cv2.COLOR_BGR2Lab)
    h, w = lab.shape[:2]
    band = border_band(h, w, BORDER_BAND_PCT)
    table = np.empty_like(lab)
    table[:] = [median_8bit(lab[..., c][band]) for c in range(3)]
    dist = distance_image(lab, table)
    floor = MIN_CONTRAST * 255 / MAX_DISTANCE
    otsu = max(floor, cv2.threshold(dist, 0, 255, cv2.THRESH_BINARY | cv2.THRESH_OTSU)[0])
    binary = np.where(dist > max(floor, table_threshold(dist, band)), 255, 0).astype(np.uint8)
    k = max(3, 2 * int(round(SMOOTHING_PCT / 200 * w)) + 1)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
    binary = cv2.morphologyEx(cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel), cv2.MORPH_CLOSE, kernel)
    min_area = 0.01 * h * w
    seed, area = largest_region(binary)
    if area < min_area:
        return None
    region, area = largest_region(grabcut_refine(bgr, lab, table, dist, floor, otsu, seed, kernel))
    if area < min_area:
        return None
    return region | enclosed_holes(region)
