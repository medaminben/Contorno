"""The contour of Part I: Section 3.4, steps 1 to 5, with the background check of Section 3.5.

    find_hide(bgr, model_path) -> uint8 mask, 0 or 255

The area is counted on this mask (Eq. 2). Images are 8-bit BGR (OpenCV's order).
"""
from __future__ import annotations

from typing import Callable

import cv2
import numpy as np

import background
import isnet
from masks import clean_mask, enclosed_holes

THRESHOLD = 0.5        # tau: probability above which a pixel is the object
HOLE_BACKGROUND = 0.2  # a hole is cut out only where the mean probability is under this


def unsure_holes(region: np.ndarray, probability: np.ndarray, hole_background: float) -> np.ndarray:
    """The holes of the region whose mean probability is not under hole_background: the model does not see the table there."""
    n, labels = cv2.connectedComponents((enclosed_holes(region) > 0).astype(np.uint8), connectivity=4)
    if n <= 1:
        return np.zeros(region.shape, np.uint8)
    sums = np.bincount(labels.ravel(), weights=probability.ravel(), minlength=n)
    counts = np.bincount(labels.ravel(), minlength=n)
    unsure = np.where(sums / np.maximum(counts, 1) >= hole_background, 255, 0).astype(np.uint8)
    unsure[0] = 0
    return unsure[labels]


def model_region(bgr: np.ndarray, model_path: str, min_area_px: float = 100, threshold: float = THRESHOLD,
                 hole_background: float = HOLE_BACKGROUND,
                 probability: Callable[[np.ndarray, str, float], np.ndarray] = isnet.probability) -> np.ndarray:
    """Steps 1 to 4 of Section 3.4: the network's region; all zero when nothing of min_area_px pixels is found.

    1. the network's probability over the threshold; 2. topological cleaning;
    3. of the regions of at least min_area_px pixels, the largest; 4. holes
    under min_area_px, and holes where the network is unsure, are filled.
    probability(bgr, model_path, threshold) is the network: IS-Net unless another is given.
    """
    p = probability(bgr, model_path, threshold)
    mask = clean_mask(np.where(p > threshold, 255, 0).astype(np.uint8))
    n, labels, stats, _ = cv2.connectedComponentsWithStats((mask > 0).astype(np.uint8), connectivity=8)
    kept = [i for i in range(1, n) if stats[i, cv2.CC_STAT_AREA] >= min_area_px]
    if not kept:
        return np.zeros(mask.shape, np.uint8)
    best = max(kept, key=lambda i: stats[i, cv2.CC_STAT_AREA])
    region = np.where(labels == best, 255, 0).astype(np.uint8)
    filled = unsure_holes(region, p, hole_background)
    if min_area_px > 0:
        filled |= enclosed_holes(region, min_area_px)
    return clean_mask(region | filled) if filled.any() else region


def background_check(bgr: np.ndarray, region: np.ndarray, min_area_px: float = 100) -> np.ndarray:
    """Step 5: the object found from the background, when the rule of Section 3.5 takes it; else the region."""
    if not region.any():
        return region
    obj = background.background_object(bgr)
    if not background.take_background(region, obj):
        return region
    if min_area_px > 0:
        obj = obj | enclosed_holes(obj, min_area_px)
    return clean_mask(obj)


def find_hide(bgr: np.ndarray, model_path: str, min_area_px: float = 100, threshold: float = THRESHOLD,
              check_background: bool = True) -> np.ndarray:
    """The hide's mask (steps 1 to 5); the area is counted on it by Eq. (2)."""
    region = model_region(bgr, model_path, min_area_px, threshold)
    return background_check(bgr, region, min_area_px) if check_background else region
