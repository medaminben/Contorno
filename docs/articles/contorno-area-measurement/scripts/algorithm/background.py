"""The background check of Section 3.5: the object as what the background, found from the image border, does not reach.

    background_object(bgr) -> uint8 mask, 0 or 255, or None
    take_background(model_region, obj) -> bool: the rule

Images are 8-bit BGR (OpenCV's order); masks are uint8, 0 or 255.
"""
from __future__ import annotations

import cv2
import numpy as np

COARSE_SIDE_PX = 160      # longest side of the coarse copy
COARSE_MEDIAN_PX = 5      # median aperture on the coarse copy (removes lines of a pixel)
BORDER_BAND_PCT = 4       # width of the band the background is fitted to, % of the smaller side
MAX_DEVIATION = 6.0       # deviation, in robust sigmas, up to which a pixel is background-like
MAX_SHADOW = 1.15         # darker by up to this factor with the background's colour: a shadow
MIN_SIGMA = (0.03, 0.4, 0.4)
FINE_MEDIAN_PX = 9        # median aperture at full size before the edge is placed
FINE_BAND_COARSE_PX = 2.5  # half-width of the band the edge is placed in, in coarse pixels


def features(bgr: np.ndarray) -> np.ndarray:
    """Per pixel: log CIELAB lightness, green and red shares (x 100) of the linear RGB sum."""
    f = bgr.astype(np.float32) / 255.0
    lightness = cv2.cvtColor(f, cv2.COLOR_BGR2Lab)[..., 0]
    linear = np.where(f <= 0.04045, f / 12.92, ((f + 0.055) / 1.055) ** 2.4)
    shares = 100.0 * linear[..., 1:] / (linear.sum(axis=2)[..., None] + 1e-4)
    return np.dstack([np.log(np.maximum(lightness, 0.5)), shares[..., 0], shares[..., 1]])


def border_band(h: int, w: int, pct: float) -> np.ndarray:
    b = max(1, round(pct / 100 * min(h, w)))
    band = np.zeros((h, w), bool)
    band[:b] = band[-b:] = True
    band[:, :b] = band[:, -b:] = True
    return band


def surface_terms(u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Quadratic surface plus a fourth-power radial term (vignetting); u, v centred on the image."""
    r2 = u * u + v * v
    return np.stack([np.ones_like(u), u, v, u * u, u * v, v * v, r2 * r2], -1)


def fit_background(feat: np.ndarray, band: np.ndarray, passes: int = 5, cut: float = 3.0):
    """Least squares on the band, five times, each time without the pixels over 3 robust sigmas."""
    h, w = feat.shape[:2]
    ys, xs = np.nonzero(band)
    a = surface_terms(xs / w - 0.5, ys / h - 0.5)
    gy, gx = np.mgrid[0:h, 0:w]
    grid = surface_terms(gx / w - 0.5, gy / h - 0.5)
    expected = np.empty_like(feat)
    sigma = np.empty(feat.shape[2])
    for c in range(feat.shape[2]):
        y = feat[ys, xs, c].astype(np.float64)
        keep = np.ones(len(y), bool)
        for _ in range(passes):
            coef, *_ = np.linalg.lstsq(a[keep], y[keep], rcond=None)
            r = y - a @ coef
            s = 1.4826 * np.median(np.abs(r[keep] - np.median(r[keep])))
            keep = np.abs(r) <= cut * max(s, 1e-6)
        expected[..., c] = grid @ coef
        sigma[c] = s
    return expected, sigma


def deviation(feat: np.ndarray) -> np.ndarray:
    """Each pixel's deviation from the background model, in robust sigmas; a light shadow is forgiven."""
    h, w = feat.shape[:2]
    expected, sigma = fit_background(feat, border_band(h, w, BORDER_BAND_PCT))
    sigma = np.maximum(sigma, MIN_SIGMA)
    d = (feat - expected) / sigma
    darker = feat[..., 0] - expected[..., 0]
    shadow = np.log(MAX_SHADOW)
    zl = np.where((darker < 0) & (darker > -shadow), 0.0, d[..., 0])
    zl = np.where(darker <= -shadow, (darker + shadow) / sigma[0], zl)
    return np.sqrt(zl ** 2 + d[..., 1] ** 2 + d[..., 2] ** 2)


def coarse_copy(bgr: np.ndarray) -> tuple[np.ndarray, float]:
    """Shrunk to 160 px on the longest side (area averaging), then a 5 x 5 median: thin lines are gone."""
    scale = COARSE_SIDE_PX / max(bgr.shape[:2])
    small = cv2.resize(bgr, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
    return cv2.medianBlur(small, COARSE_MEDIAN_PX), scale


def coarse_object(z: np.ndarray) -> np.ndarray | None:
    """0/1: the largest region the border-connected background does not reach, its small holes filled."""
    _, lab = cv2.connectedComponents((z <= MAX_DEVIATION).astype(np.uint8), connectivity=4)
    border = np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))
    border = border[border > 0]
    obj = (~np.isin(lab, border)).astype(np.uint8)
    obj = cv2.morphologyEx(obj, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, stats, _ = cv2.connectedComponentsWithStats(obj, connectivity=8)
    if n <= 1:
        return None
    obj = (lab == 1 + int(np.argmax(stats[1:, 4]))).astype(np.uint8)
    # enclosed areas: background-like ones of at least 4 coarse pixels stay holes, the rest is filled
    k, hl, hs, _ = cv2.connectedComponentsWithStats(1 - obj, connectivity=4)
    edge = set(np.unique(np.concatenate([hl[0], hl[-1], hl[:, 0], hl[:, -1]])).tolist())
    for label in range(1, k):
        if label in edge:
            continue
        if hs[label, 4] < 4 or z[hl == label].mean() > MAX_DEVIATION:
            obj[hl == label] = 1
    return obj


def fine_edge(bgr: np.ndarray, obj_small: np.ndarray, scale: float) -> np.ndarray:
    """At full size, a marker watershed in a band around the coarse outline: the sharpest edge."""
    h, w = bgr.shape[:2]
    obj = cv2.resize(obj_small, (w, h), interpolation=cv2.INTER_NEAREST)
    r = max(3, int(round(FINE_BAND_COARSE_PX / scale)))
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * r + 1, 2 * r + 1))
    markers = np.zeros((h, w), np.int32)
    markers[cv2.dilate(obj, kernel) == 0] = 1  # sure background
    markers[cv2.erode(obj, kernel) > 0] = 2    # sure object
    cv2.watershed(cv2.medianBlur(bgr, FINE_MEDIAN_PX), markers)
    m = cv2.morphologyEx((markers == 2).astype(np.uint8), cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
    n, lab, stats, _ = cv2.connectedComponentsWithStats(m, connectivity=8)
    return ((lab == 1 + int(np.argmax(stats[1:, 4]))) * 255).astype(np.uint8) if n > 1 else m * 255


def background_object(bgr: np.ndarray) -> np.ndarray | None:
    """The whole method: coarse copy, background model, coarse object, fine edge. None when nothing is found."""
    small, scale = coarse_copy(bgr)
    obj = coarse_object(deviation(features(small)))
    return None if obj is None else fine_edge(bgr, obj, scale)


BACKGROUND_HOLDS_MODEL = 0.95  # share of the model's region that must lie in the background's object ...
BACKGROUND_MIN_GAIN = 1.10     # ... and how much larger that object must be


def take_background(model: np.ndarray, obj: np.ndarray | None) -> bool:
    """The rule of Section 3.5: the background's object replaces the model's region."""
    if obj is None:
        return False
    m, o = model > 0, obj > 0
    return m.sum() > 0 and o.sum() > BACKGROUND_MIN_GAIN * m.sum() and (m & o).sum() >= BACKGROUND_HOLDS_MODEL * m.sum()
