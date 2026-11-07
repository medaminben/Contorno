#!/usr/bin/env python3
"""Procedural, photo-like hide images with a ground-truth mask for each.

Every image is synthesised from noise (no photo pixels are reused), so the
output carries no third-party licence. For each sample N it writes:
    hide_NN.jpg       the "photograph"
    hide_NN_mask.png  white = hide, black = background (holes included)
and one manifest.json with the kind, finish, background and true area in px.

Usage: python3 -I generate_hides.py [count] [seed] [out_dir]
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy import ndimage
from scipy.interpolate import splev, splprep


# ---------------------------------------------------------------- noise

def fractal(rng, h, w, beta=2.0, lo=0.0):
    """1/f^beta noise in [-1, 1]; lo cuts the lowest frequencies (cycles/px)."""
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    amp = f ** (-beta / 2.0)
    amp[0, 0] = 0.0
    if lo:
        amp *= 1.0 - np.exp(-(f / lo) ** 2)
    spec = (rng.standard_normal((h, w // 2 + 1))
            + 1j * rng.standard_normal((h, w // 2 + 1))) * amp
    n = np.fft.irfft2(spec, s=(h, w))
    n -= n.mean()
    return n / (np.abs(n).max() + 1e-9)


def band(rng, h, w, sigma):
    n = ndimage.gaussian_filter(rng.standard_normal((h, w)), sigma)
    return n / (np.abs(n).max() + 1e-9)


def noise1d(rng, n, beta):
    f = np.fft.rfftfreq(n) * n
    f[0] = 1.0
    amp = f ** (-beta / 2.0)
    amp[0] = 0.0
    amp[: 2] = 0.0
    spec = (rng.standard_normal(len(f)) + 1j * rng.standard_normal(len(f))) * amp
    s = np.fft.irfft(spec, n=n)
    return s / (np.abs(s).max() + 1e-9)


# ---------------------------------------------------------------- outline

# Right half of a flayed skin, spine on x = 0, neck at y = -1, tail at y = +1.
HALF = {
    # neck .......................  foreleg (tip = index 5) ..........
    # belly ....................... hind leg (tip = index 12) .........
    # tail
    "cow":   [(0.15, -1.00), (0.26, -0.97), (0.30, -0.82), (0.46, -0.76),
              (0.74, -0.80), (0.92, -0.74), (0.80, -0.60), (0.60, -0.50),
              (0.56, -0.18), (0.58, 0.18), (0.66, 0.38), (0.86, 0.54),
              (0.98, 0.74), (0.86, 0.86), (0.62, 0.80), (0.42, 0.90),
              (0.18, 0.98), (0.07, 1.10)],
    "calf":  [(0.14, -1.00), (0.24, -0.97), (0.27, -0.82), (0.42, -0.76),
              (0.68, -0.82), (0.86, -0.78), (0.74, -0.62), (0.54, -0.52),
              (0.50, -0.18), (0.52, 0.18), (0.60, 0.40), (0.78, 0.58),
              (0.90, 0.80), (0.76, 0.90), (0.54, 0.82), (0.36, 0.92),
              (0.16, 0.98), (0.06, 1.08)],
    "goat":  [(0.12, -1.00), (0.20, -0.98), (0.22, -0.84), (0.36, -0.80),
              (0.66, -0.90), (0.84, -0.92), (0.66, -0.70), (0.48, -0.56),
              (0.44, -0.18), (0.46, 0.20), (0.54, 0.44), (0.72, 0.68),
              (0.84, 0.96), (0.68, 0.98), (0.48, 0.84), (0.32, 0.92),
              (0.14, 0.98), (0.05, 1.06)],
    "sheep": [(0.18, -1.00), (0.28, -0.96), (0.32, -0.84), (0.48, -0.80),
              (0.72, -0.86), (0.86, -0.80), (0.72, -0.64), (0.58, -0.54),
              (0.56, -0.18), (0.58, 0.20), (0.64, 0.42), (0.78, 0.62),
              (0.88, 0.86), (0.74, 0.94), (0.54, 0.86), (0.36, 0.94),
              (0.16, 0.99), (0.06, 1.05)],
}
FORE, HIND = (4, 5, 6), (11, 12, 13)


def outline(rng, kind, side=False):
    """Closed, jagged outline in normalised units (about [-1, 1]^2)."""
    half = np.array(HALF[kind], float)
    jit = 0.025 if kind in ("goat", "sheep") else 0.02
    def legs(p):
        # Each leg gets its own length: skins are flayed and stretched unevenly.
        p = p.copy()
        for idx in (FORE, HIND):
            k = rng.uniform(0.85, 1.25)
            root = p[idx[0] - 1]
            p[list(idx)] = root + (p[list(idx)] - root) * k
        return p
    right = legs(half) + rng.normal(0, jit, half.shape)
    if side:
        # A cow side: one half, cut along the backbone, so one long straight edge.
        spine = np.array([[0.0, 1.04], [0.0, -1.0]])
        spine[:, 0] += rng.normal(0, 0.01, 2)
        pts = np.vstack([right, spine])
        pts[:, 0] -= 0.40
        pts[:, 0] *= 1.25
    else:
        left = legs(half)[::-1] * [-1, 1] + rng.normal(0, jit, half.shape)
        pts = np.vstack([right, left[1:-1]])
    tck, _ = splprep([pts[:, 0], pts[:, 1]], s=0.0002 * len(pts), per=1, k=3)
    n = 2400
    u = np.linspace(0, 1, n, endpoint=False)
    x, y = splev(u, tck)
    dx, dy = splev(u, tck, der=1)
    nl = np.hypot(dx, dy) + 1e-9
    nx, ny = dy / nl, -dx / nl
    rough = {"cow": 0.020, "calf": 0.022, "goat": 0.030, "sheep": 0.026}[kind]
    d = (rough * noise1d(rng, n, 2.2)            # scallops
         + rough * 0.18 * noise1d(rng, n, 1.5))  # small notches
    if side:
        straight = np.abs(x - x.min()) < 0.02  # keep the spine cut clean
        d[straight] *= 0.15
    return np.stack([x + nx * d, y + ny * d], 1)


def rasterise(poly, h, w, cx, cy, scale, angle, ss=3):
    c, s = np.cos(angle), np.sin(angle)
    x = poly[:, 0] * c - poly[:, 1] * s
    y = poly[:, 0] * s + poly[:, 1] * c
    pts = [((cx + xi * scale) * ss, (cy + yi * scale) * ss) for xi, yi in zip(x, y)]
    img = Image.new("L", (w * ss, h * ss), 0)
    ImageDraw.Draw(img).polygon(pts, fill=255)
    return np.asarray(img.resize((w, h), Image.LANCZOS), np.float32) / 255.0


def cut_holes(rng, mask, count):
    """Small tears / bullet-like holes, mostly near the edge, as in real skins."""
    h, w = mask.shape
    inside = ndimage.distance_transform_edt(mask > 0.5)
    yy, xx = np.mgrid[0:h, 0:w]
    for _ in range(count):
        cand = np.argwhere((inside > 25) & (inside < 140))
        if len(cand) == 0:
            break
        py, px = cand[rng.integers(len(cand))]
        rx, ry = rng.uniform(3, 11), rng.uniform(2, 7)
        a = rng.uniform(0, np.pi)
        u = (xx - px) * np.cos(a) + (yy - py) * np.sin(a)
        v = -(xx - px) * np.sin(a) + (yy - py) * np.cos(a)
        r = (u / rx) ** 2 + (v / ry) ** 2
        mask = mask * np.clip(r - 0.6, 0, 1).astype(np.float32) ** 0.5
    return mask


# ---------------------------------------------------------------- surfaces

def shade(height, light=(-0.55, -0.65, 0.52), strength=1.0):
    gy, gx = np.gradient(height)
    nx, ny, nz = -gx * strength, -gy * strength, np.ones_like(height)
    nl = np.sqrt(nx * nx + ny * ny + nz * nz)
    l = np.array(light) / np.linalg.norm(light)
    lam = (nx * l[0] + ny * l[1] + nz * l[2]) / nl
    # Blinn half vector with the camera straight above.
    hv = l + np.array([0, 0, 1.0])
    hv /= np.linalg.norm(hv)
    spec = np.clip((nx * hv[0] + ny * hv[1] + nz * hv[2]) / nl, 0, 1)
    return lam, spec


LEATHER = {
    #            base RGB (0..1)           mottle grain wrinkle gloss
    "brown":     ((0.47, 0.24, 0.15),       0.16, 1.0, 1.0, 0.35),
    "cognac":    ((0.62, 0.34, 0.16),       0.18, 1.0, 0.9, 0.40),
    "dark":      ((0.24, 0.17, 0.13),       0.12, 1.0, 1.1, 0.55),
    "black":     ((0.08, 0.075, 0.07),      0.10, 1.0, 1.0, 0.60),
    "tan":       ((0.74, 0.56, 0.37),       0.14, 0.9, 0.8, 0.25),
    "natural":   ((0.82, 0.66, 0.48),       0.10, 0.8, 0.7, 0.15),
    "burgundy":  ((0.38, 0.09, 0.11),       0.14, 1.0, 1.0, 0.45),
    "olive":     ((0.36, 0.35, 0.20),       0.12, 1.0, 0.9, 0.30),
    "grey":      ((0.50, 0.49, 0.47),       0.10, 0.9, 0.9, 0.30),
    "navy":      ((0.12, 0.15, 0.26),       0.10, 1.0, 1.0, 0.45),
}


def leather_surface(rng, h, w, mask, colour, finish):
    base, mot, grain_k, wrk_k, gloss = LEATHER[colour]
    base = np.array(base)
    if finish in ("nubuck", "suede"):
        grain_k, wrk_k, gloss = grain_k * 0.25, wrk_k * 0.45, 0.0
    # Height in px units, built from explicit scales so nothing looks rocky:
    # broad folds (sigma ~90 px), edge ripples (~30), creases (thin ridges of
    # a ~25 px field), and a faint pore grain (~1 px).
    edge_dist = ndimage.distance_transform_edt(mask > 0.5)
    flank = np.clip(1.0 - edge_dist / (0.22 * min(h, w)), 0.2, 1.0)
    folds = band(rng, h, w, 90.0) * 60.0 + band(rng, h, w, 40.0) * 14.0
    ripple = band(rng, h, w, 28.0) * 16.0 * np.clip(1.0 - edge_dist / 110.0, 0, 1)
    def aniso(sy, sx):
        n = ndimage.gaussian_filter(rng.standard_normal((h, w)), (sy, sx))
        return n / (np.abs(n).max() + 1e-9)
    gate = np.clip(band(rng, h, w, 60.0) * 2.0, 0, 1)  # creases come in patches
    cr1 = (1.0 - np.abs(aniso(10.0, 34.0))) ** 60 * gate
    cr2 = (1.0 - np.abs(aniso(26.0, 8.0))) ** 60 * np.clip(band(rng, h, w, 60.0) * 2.0, 0, 1)
    pores = (1.0 - np.abs(band(rng, h, w, 2.4))) ** 8
    wrinkles = cr1 + 0.7 * cr2 + 0.15 * pores
    grain = band(rng, h, w, 1.0) * 0.35 + band(rng, h, w, 2.2) * 0.5
    height = (folds + ripple - wrinkles * 2.5 * wrk_k * flank
              + grain * grain_k)
    lam, spec = shade(height, strength=1.0)
    mottle = band(rng, h, w, 70.0) * 0.7 + band(rng, h, w, 18.0) * 0.3
    blotch = band(rng, h, w, 6.0)
    col = base[None, None, :] * (1.0 + mot * mottle[..., None]
                                 + 0.05 * blotch[..., None])
    # Wrinkle valleys collect dye: darker.
    col *= (1.0 - 0.18 * wrk_k * np.clip(wrinkles, 0, 1))[..., None]
    if finish == "suede":
        nap = ndimage.gaussian_filter(rng.standard_normal((h, w)), (0.6, 4.0))
        col *= (1.0 + 0.10 * nap / np.abs(nap).max())[..., None]
        col = col * 0.92 + 0.04
    elif finish == "nubuck":
        brush = fractal(rng, h, w, 1.6, lo=0.02)
        col *= (1.0 + 0.06 * brush)[..., None]
    light = 0.62 + 0.45 * lam
    out = col * light[..., None]
    if gloss:
        # Broad satin sheen, not a sharp highlight.
        out += (gloss * 0.22 * spec ** 12)[..., None] * np.array([1.0, 0.97, 0.92])
    # Cut edge: slightly darker rim, sometimes lighter (flesh side showing).
    rim = np.clip(1.0 - edge_dist / 4.0, 0, 1)
    out *= (1.0 - 0.25 * rim)[..., None]
    return out


COWHIDE = {
    # name: (patch colour, ground colour, low-freq share, speckle)
    "black_white":  ((0.05, 0.045, 0.045), (0.90, 0.88, 0.84), 1.0, 0.06),
    "brown_white":  ((0.30, 0.16, 0.08),   (0.90, 0.87, 0.80), 1.0, 0.08),
    "speckled":     ((0.08, 0.06, 0.06),   (0.88, 0.86, 0.82), 0.8, 0.30),
    "tricolour":    ((0.06, 0.05, 0.05),   (0.88, 0.85, 0.79), 1.0, 0.08),
    "red_brindle":  ((0.38, 0.18, 0.09),   (0.58, 0.38, 0.24), 0.9, 0.12),
}


def hair_surface(rng, h, w, mask, pattern):
    dark, light_c, low, speck = COWHIDE[pattern]
    dark, light_c = np.array(dark), np.array(light_c)
    # Hair lies tail-ward: streaky noise, long along y.
    streak = ndimage.gaussian_filter(rng.standard_normal((h, w)), (9.0, 0.9))
    streak /= np.abs(streak).max()
    fine = ndimage.gaussian_filter(rng.standard_normal((h, w)), (3.0, 0.6))
    fine /= np.abs(fine).max()
    field = (low * fractal(rng, h, w, 4.2, lo=0.0015)
             + speck * fractal(rng, h, w, 3.0, lo=0.02)
             + 0.025 * streak + 0.012 * fine)
    thr = rng.uniform(0.0, 0.2)
    patch = 1.0 / (1.0 + np.exp(-(field - thr) * 90.0))
    col = light_c * (1 - patch[..., None]) + dark * patch[..., None]
    if pattern == "tricolour":
        brown = 1.0 / (1.0 + np.exp(-(band(rng, h, w, 45.0) - 0.15 + 0.03 * streak) * 40.0))
        tint = np.array([0.36, 0.20, 0.10])
        k = (brown * patch)[..., None]
        col = col * (1 - k) + tint * k
    # Sheen along the hair; the hair is short, so the sheen is subtle.
    sheen = 1.0 + 0.035 * streak + 0.025 * fine
    edge_dist = ndimage.distance_transform_edt(mask > 0.5)
    height = band(rng, h, w, 90.0) * 60.0 + band(rng, h, w, 40.0) * 12.0
    height += band(rng, h, w, 28.0) * 14.0 * np.clip(1 - edge_dist / 110, 0, 1)
    lam, _ = shade(height, strength=1.0)
    out = col * sheen[..., None] * (0.62 + 0.45 * lam)[..., None]
    # Brushed-up hair at the cut edge: a soft light fringe.
    rim = np.clip(1.0 - edge_dist / 3.0, 0, 1)
    out = out * (1 - 0.2 * rim[..., None]) + 0.1 * rim[..., None]
    return out


# ---------------------------------------------------------------- scenes

def background(rng, h, w, kind):
    yy, xx = np.mgrid[0:h, 0:w] / max(h, w)
    if kind == "studio_white":
        g = 0.95 - 0.05 * ((xx - 0.45) ** 2 + (yy - 0.4) ** 2)
        img = np.stack([g, g * 0.995, g * 0.985], -1)
        img += 0.006 * fractal(rng, h, w, 2.5)[..., None]
    elif kind == "kraft":
        base = np.array([0.80, 0.62, 0.44])
        fib = ndimage.gaussian_filter(rng.standard_normal((h, w)), (0.7, 5.0))
        fib /= np.abs(fib).max()
        img = base * (1 + 0.05 * fractal(rng, h, w, 2.2)[..., None]
                      + 0.04 * fib[..., None])
    elif kind == "concrete":
        base = np.array([0.58, 0.57, 0.55])
        img = base * (1 + 0.08 * fractal(rng, h, w, 1.8)[..., None]
                      + 0.05 * band(rng, h, w, 0.8)[..., None])
        pits = band(rng, h, w, 1.2) > 0.75
        img[pits] *= 0.8
    elif kind == "wood":
        plank = int(rng.integers(140, 230))
        idx = (np.arange(w) // plank)
        tone = rng.uniform(0.9, 1.1, idx.max() + 1)[idx][None, :]
        rings = np.sin((xx * 220 + 9 * fractal(rng, h, w, 2.6)) * 2.0)
        base = np.array([0.66, 0.48, 0.30])
        img = base * (tone[..., None] * (1 + 0.07 * rings[..., None]
                                         + 0.05 * fractal(rng, h, w, 2.0)[..., None]))
        gaps = (np.arange(w) % plank) < 3
        img[:, gaps] *= 0.45
    elif kind == "grey_table":
        g = 0.72 - 0.08 * ((xx - 0.5) ** 2 + (yy - 0.5) ** 2)
        img = np.stack([g, g, g * 1.01], -1)
        img *= 1 + 0.015 * fractal(rng, h, w, 2.0)[..., None]
    elif kind == "green_mat":
        base = np.array([0.16, 0.36, 0.28])
        img = base * (1 + 0.04 * fractal(rng, h, w, 2.2)[..., None])
        step = int(rng.integers(55, 75))
        grid = (np.arange(w) % step < 2)[None, :] | (np.arange(h) % step < 2)[:, None]
        img[grid] = img[grid] * 0.6 + 0.35
    else:
        raise ValueError(kind)
    return np.clip(img, 0, 1)


def camera(rng, img, mask):
    h, w = mask.shape
    # Mild perspective: the phone is never quite square to the table.
    k = rng.uniform(0.0, 0.035)
    src = [(0, 0), (w, 0), (w, h), (0, h)]
    dst = [(rng.uniform(0, k) * w, rng.uniform(0, k) * h),
           (w - rng.uniform(0, k) * w, rng.uniform(0, k) * h),
           (w - rng.uniform(0, k) * w, h - rng.uniform(0, k) * h),
           (rng.uniform(0, k) * w, h - rng.uniform(0, k) * h)]
    coeffs = perspective_coeffs(dst, src)
    pil = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    pil = pil.transform((w, h), Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    m = Image.fromarray((mask * 255).astype(np.uint8))
    m = m.transform((w, h), Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    # The corners revealed by the warp: fill with the nearest pixels.
    img = np.asarray(pil, np.float32) / 255.0
    valid = np.asarray(Image.new("L", (w, h), 255).transform(
        (w, h), Image.PERSPECTIVE, coeffs, Image.NEAREST)) > 0
    if not valid.all():
        idx = ndimage.distance_transform_edt(~valid, return_distances=False,
                                             return_indices=True)
        img = img[idx[0], idx[1]]
    # Lens: slight softness, vignette, white balance drift, sensor noise.
    img = ndimage.gaussian_filter(img, (rng.uniform(0.3, 0.8),) * 2 + (0,))
    yy, xx = np.mgrid[0:h, 0:w]
    r2 = ((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2
    img *= (1 - rng.uniform(0.05, 0.2) * r2 / 2)[..., None]
    img *= np.array([1 + rng.normal(0, 0.02), 1.0, 1 + rng.normal(0, 0.03)])
    img *= rng.uniform(0.93, 1.05)
    lum_noise = rng.normal(0, rng.uniform(0.006, 0.016), (h, w, 1))
    img += lum_noise + rng.normal(0, 0.004, img.shape)
    return np.clip(img, 0, 1), np.asarray(m, np.float32) / 255.0


def perspective_coeffs(dst, src):
    a = []
    for (x, y), (u, v) in zip(dst, src):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
    b = np.array(src, float).reshape(8)
    return np.linalg.solve(np.array(a, float), b).tolist()


# ---------------------------------------------------------------- sample

SPECS = [
    # (animal outline, surface, colour/pattern, finish, side cut)
    ("cow", "hair", "black_white", None, False),
    ("cow", "hair", "speckled", None, False),
    ("calf", "hair", "brown_white", None, False),
    ("cow", "hair", "tricolour", None, False),
    ("calf", "hair", "red_brindle", None, False),
    ("cow", "leather", "brown", "full_grain", True),
    ("cow", "leather", "black", "full_grain", True),
    ("cow", "leather", "cognac", "full_grain", True),
    ("cow", "leather", "tan", "nubuck", True),
    ("calf", "leather", "dark", "full_grain", False),
    ("calf", "leather", "navy", "full_grain", False),
    ("goat", "leather", "brown", "full_grain", False),
    ("goat", "leather", "natural", "nubuck", False),
    ("goat", "leather", "black", "full_grain", False),
    ("goat", "leather", "burgundy", "full_grain", False),
    ("sheep", "leather", "tan", "nubuck", False),
    ("sheep", "leather", "dark", "full_grain", False),
    ("sheep", "leather", "natural", "suede", False),
    ("sheep", "leather", "olive", "suede", False),
    ("sheep", "leather", "grey", "full_grain", False),
]

BACKGROUNDS = ["studio_white", "kraft", "concrete", "wood", "grey_table", "green_mat"]


def make(rng, spec, size):
    animal, surface, colour, finish, side = spec
    w, h = size
    bg_kind = BACKGROUNDS[rng.integers(len(BACKGROUNDS))]
    # Hair-on skins are rarely shot on white; leather often is.
    if surface == "hair" and bg_kind == "studio_white" and rng.random() < 0.6:
        bg_kind = "kraft"
    poly = outline(rng, animal, side)
    fill = rng.uniform(0.80, 0.93)
    ext = poly.max(0) - poly.min(0)
    angle = rng.normal(0, 0.06) + (np.pi / 2 if (w > h and ext[1] > ext[0] * 1.15
                                                   and rng.random() < 0.5) else 0)
    c, s = abs(np.cos(angle)), abs(np.sin(angle))
    ew, eh = ext[0] * c + ext[1] * s, ext[0] * s + ext[1] * c
    scale = fill * min(w / ew, h / eh)
    centre = (poly.max(0) + poly.min(0)) / 2
    cr, sr = np.cos(angle), np.sin(angle)
    off = np.array([centre[0] * cr - centre[1] * sr, centre[0] * sr + centre[1] * cr])
    cx = w / 2 - off[0] * scale + rng.normal(0, 0.015 * w)
    cy = h / 2 - off[1] * scale + rng.normal(0, 0.015 * h)
    mask = rasterise(poly, h, w, cx, cy, scale, angle)
    holes = int(rng.integers(0, 4)) if animal in ("goat", "sheep", "calf") else int(rng.integers(0, 2))
    mask = cut_holes(rng, mask, holes)

    bg = background(rng, h, w, bg_kind)
    if surface == "hair":
        fg = hair_surface(rng, h, w, mask, colour)
    else:
        fg = leather_surface(rng, h, w, mask, colour, finish)
    # Contact shadow: soft, offset away from the light.
    sh = ndimage.gaussian_filter(mask, rng.uniform(5, 12))
    sh = ndimage.shift(sh, (rng.uniform(3, 9), rng.uniform(3, 9)), order=1)
    bg = bg * (1 - rng.uniform(0.25, 0.45) * sh)[..., None]
    img = bg * (1 - mask[..., None]) + np.clip(fg, 0, 1) * mask[..., None]
    img, mask = camera(rng, img, mask)
    info = dict(animal=animal, surface=surface, colour=colour, finish=finish,
                side=side, background=bg_kind, holes=holes,
                area_px=float(mask.sum()), size=[w, h])
    return img, mask, info


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else len(SPECS)
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 2026
    out = Path(sys.argv[3]) if len(sys.argv) > 3 else Path(__file__).parent / "out"
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(seed)
    manifest = {}
    for i in range(count):
        spec = SPECS[i % len(SPECS)]
        size = (1600, 1200) if rng.random() < 0.6 else (1200, 1400)
        img, mask, info = make(rng, spec, size)
        name = f"hide_{i + 1:02d}"
        Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(
            out / f"{name}.jpg", quality=int(rng.integers(84, 93)))
        Image.fromarray(((mask > 0.5) * 255).astype(np.uint8)).save(out / f"{name}_mask.png")
        manifest[name] = info
        print(name, info["animal"], info["surface"], info["colour"],
              info["finish"], info["background"], flush=True)
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
