#!/usr/bin/env python3
"""Fig. 5 of the article: the background check, stage by stage, on the spotted calf.

    fig_background.py <experiment out dir> <the article's figures dir>

The stages (coarse copy, deviation from the background model, background
reached from the border) come from ../algorithm/background.py. The two
outlines are the experiment's (../experiment/measure.py):
real_calf_spotted.jpg_model_mask.png (the network alone) and
real_calf_spotted.jpg_mask.png (with the background check).
Needs opencv-python, numpy and matplotlib.
"""
from __future__ import annotations

import sys
from pathlib import Path

import cv2
import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = Path(__file__).resolve()
REPO = HERE.parents[5]
sys.path.insert(0, str(HERE.parents[1] / "algorithm"))
import background as bg  # noqa: E402

PHOTO = "calf_spotted.jpg"
Z_CUT = bg.MAX_DEVIATION


def outline(rgb: np.ndarray, mask: np.ndarray, colour: tuple[int, int, int], dim: bool) -> np.ndarray:
    out = rgb.copy()
    if dim:
        out[mask == 0] = (0.4 * out[mask == 0]).astype(np.uint8)
    contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    cv2.drawContours(out, contours, -1, colour, max(3, rgb.shape[1] // 250))
    return out


def main() -> int:
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    bgr = cv2.imread(str(REPO / "docs/samples/leather" / PHOTO), cv2.IMREAD_COLOR)
    if bgr is None:
        raise SystemExit(f"cannot read {PHOTO}")
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    model = cv2.imread(str(src / f"real_{PHOTO}_model_mask.png"), cv2.IMREAD_GRAYSCALE)
    final = cv2.imread(str(src / f"real_{PHOTO}_mask.png"), cv2.IMREAD_GRAYSCALE)
    if model is None or final is None:
        raise SystemExit("run experiment/measure.py first: the calf's masks are missing")

    # the coarse stage, as background_object_impl does it
    small, _ = bg.coarse_copy(bgr)
    z = bg.deviation(bg.features(small))
    background = (bg.coarse_object(z) == 0).astype(np.uint8)

    fig, axes = plt.subplots(2, 2, figsize=(9, 8.4))
    share = lambda m: 100 * np.count_nonzero(m) / m.size  # noqa: E731
    panels = [
        (outline(rgb, model, (220, 38, 38), True), f"(a) the model alone: {share(model):.0f} % of the photo"),
        (cv2.cvtColor(small, cv2.COLOR_BGR2RGB), f"(b) coarse copy, {small.shape[1]} × {small.shape[0]} px, median"),
        (None, "(c) deviation from the background"),
        (outline(rgb, final, (31, 157, 85), True), f"(d) with the background check: {share(final):.0f} %"),
    ]
    for ax, (img, title) in zip(axes.flat, panels):
        if img is None:
            shown = ax.imshow(np.minimum(z, 3 * Z_CUT), cmap="magma", interpolation="nearest")
            ax.contour(background, levels=[0.5], colors="#38bdf8", linewidths=1.2)
            bar = fig.colorbar(shown, ax=ax, fraction=0.046, pad=0.02)
            bar.set_label("robust sigmas", fontsize=8)
            bar.ax.axhline(Z_CUT, color="#38bdf8", lw=1.2)
        else:
            ax.imshow(img, interpolation="nearest" if img.shape[1] < 400 else "antialiased")
        ax.set_title(title, fontsize=10, loc="left")
        ax.axis("off")
    fig.tight_layout()
    fig.savefig(out / "fig_background.png", dpi=200, bbox_inches="tight")
    plt.close(fig)
    print(f"model alone {share(model):.1f} %, with the background check {share(final):.1f} % of the photo")
    return 0


if __name__ == "__main__":
    sys.exit(main())
