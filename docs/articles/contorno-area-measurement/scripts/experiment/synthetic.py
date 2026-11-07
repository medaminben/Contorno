"""The synthetic photographs of Section 6.1: hide-like shapes of exactly known area on a textured table.

    photo(width, height, look, size, seed, shadowed) -> Synthetic

Every photo is drawn from its seed, so a run gives the same photos again.
Images are 8-bit BGR (OpenCV's order).
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import cv2
import numpy as np


@dataclass
class Synthetic:
    name: str
    photo: np.ndarray      # 8-bit BGR
    truth: np.ndarray      # uint8 0/255: the hide, its hole cut out
    true_px: float         # shoelace area of the outline minus the hole
    hole_px: float
    perimeter_px: float    # of the outline and the hole


def shoelace(ring: np.ndarray) -> float:
    x, y = ring[:, 0], ring[:, 1]
    return abs(float(np.dot(x, np.roll(y, -1)) - np.dot(np.roll(x, -1), y))) / 2


def perimeter(ring: np.ndarray) -> float:
    return float(np.linalg.norm(ring - np.roll(ring, -1, axis=0), axis=1).sum())


def hide_outline(rng: np.random.Generator, centre: tuple[float, float], radius: float) -> np.ndarray:
    """r(t) = R [1 + sum a_k cos(k t + phi_k) + four legs], stretched 1.2-1.4 times horizontally; 1440 points."""
    amp = {k: 0.10 * rng.uniform() / math.sqrt(k) for k in range(2, 8)}
    phase = {k: 2 * math.pi * rng.uniform() for k in range(2, 8)}
    stretch = 1.2 + 0.2 * rng.uniform()
    t = 2 * np.pi * np.arange(1440) / 1440
    r = np.ones_like(t)
    for k in range(2, 8):
        r += amp[k] * np.cos(k * t + phase[k])
    for leg in (0.25 * np.pi, 0.75 * np.pi, 1.25 * np.pi, 1.75 * np.pi):
        d = np.remainder(t - leg + np.pi, 2 * np.pi) - np.pi
        r += 0.22 * np.exp(-d * d / (2 * 0.08 * 0.08))
    r = np.maximum(r, 0.4)
    return np.stack([centre[0] + stretch * radius * r * np.cos(t), centre[1] + radius * r * np.sin(t)], -1)


def ellipse_ring(centre: tuple[float, float], a: float, b: float) -> np.ndarray:
    t = 2 * np.pi * np.arange(720) / 720
    return np.stack([centre[0] + a * np.cos(t), centre[1] + b * np.sin(t)], -1)


def fill(coverage: np.ndarray, ring: np.ndarray, value: int) -> None:
    """The ring filled, anti-aliased, with 8 bits of sub-pixel precision."""
    fixed = np.round((ring - 0.5) * 256).astype(np.int32)
    cv2.fillPoly(coverage, [fixed], value, cv2.LINE_AA, 8)


def smooth_noise(rng: np.random.Generator, size: tuple[int, int], scale: float) -> np.ndarray:
    """Noise in [-1, 1]: uniform values on a coarse grid, enlarged by bicubic interpolation (size is width, height)."""
    w, h = size
    small = rng.uniform(-1, 1, (max(2, int(h / scale)), max(2, int(w / scale)))).astype(np.float32)
    return cv2.resize(small, (w, h), interpolation=cv2.INTER_CUBIC)


def render(rng: np.random.Generator, coverage: np.ndarray, look: str, shadowed: bool) -> np.ndarray:
    """The photo: textured table, cowhide-like or leather-like hide, soft shadow down-right, sensor noise."""
    h, w = coverage.shape
    s = w / 1600  # features scale with the photo
    grain = smooth_noise(rng, (w, h), 40 * s)
    streaks = cv2.resize(smooth_noise(rng, (w, max(2, h // 30)), 8 * s), (w, h), interpolation=cv2.INTER_LINEAR)
    table_colour = np.array((120, 170, 200) if look == "cowhide" else (150, 140, 128), np.float32)
    table = table_colour * (1 + 0.08 * grain + 0.05 * streaks)[..., None]
    if look == "cowhide":
        blotches = np.zeros((h, w), np.uint8)
        for _ in range(60):
            cx, cy = int(rng.uniform() * w), int(rng.uniform() * h)
            axes = (int((20 + 120 * rng.uniform()) * s), int((15 + 90 * rng.uniform()) * s))
            cv2.ellipse(blotches, (cx, cy), axes, 360 * rng.uniform(), 0, 360, 255, cv2.FILLED, cv2.LINE_AA)
        b = cv2.GaussianBlur(blotches, (0, 0), 3 * s).astype(np.float32)[..., None] / 255
        hide = np.array((225, 228, 230), np.float32) * (1 - b) + np.array((28, 26, 25), np.float32) * b
    else:
        low = smooth_noise(rng, (w, h), 120 * s)
        hide = np.array((45, 75, 125), np.float32) * (1 + 0.18 * low)[..., None]
    shift = np.float32([[1, 0, 12 * s], [0, 1, 14 * s]])
    shadow = cv2.GaussianBlur(cv2.warpAffine(coverage, shift, (w, h)).astype(np.float32) / 255, (0, 0), 10 * s)
    if not shadowed:
        shadow[:] = 0
    a = coverage.astype(np.float32)[..., None] / 255
    photo = table * (1 - 0.3 * shadow)[..., None] * (1 - a) + hide * a
    photo += rng.normal(0, 3, photo.shape).astype(np.float32)
    return np.clip(np.rint(photo), 0, 255).astype(np.uint8)


def photo(width: int, height: int, look: str, size: str, seed: int, shadowed: bool = True) -> Synthetic:
    rng = np.random.default_rng(seed * 7919 + width + (0 if look == "cowhide" else 3) + ord(size[0]))
    radius = (0.27 if size == "large" else 0.11) * height
    centre = (width / 2 + 0.05 * width * (seed % 3 - 1), height / 2 + 0.04 * height * (1 if seed % 2 else -1))
    outline = hide_outline(rng, centre, radius)
    hole = ellipse_ring((centre[0] + 0.2 * radius, centre[1] - 0.15 * radius), 0.09 * radius, 0.06 * radius)
    coverage = np.zeros((height, width), np.uint8)
    fill(coverage, outline, 255)
    fill(coverage, hole, 0)
    hole_px = shoelace(hole)
    name = f"{look}_{size}_{width}_{seed}" + ("" if shadowed else "_noshadow")
    return Synthetic(name, render(rng, coverage, look, shadowed), np.where(coverage >= 128, 255, 0).astype(np.uint8),
                     shoelace(outline) - hole_px, hole_px, perimeter(outline) + perimeter(hole))
