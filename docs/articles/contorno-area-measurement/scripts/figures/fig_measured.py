#!/usr/bin/env python3
"""Figs. 1, 4 and 8 of the article: the minimal configuration, and two photographs measured.

    fig_measured.py <experiment out dir> <the article's figures dir>

Fig. 1 (fig_configuration.png): what the three configuration values of
Section 1.2 give for a photograph, by Eq. (1). Figs. 4 and 8
(fig_calf_black.png, fig_cowhide_stretched.png): the photograph with the
contour of Part I and a label with the area, Eq. (2), counted on the mask
that experiment/measure.py writes (real_<photo>_mask.png).

Neither photograph's distance to the table was measured, so both areas
illustrate the method; they are not measurements of these skins.
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
from matplotlib.patches import FancyBboxPatch  # noqa: E402

HERE = Path(__file__).resolve()
REPO = HERE.parents[5]
sys.path.insert(0, str(HERE.parents[1] / "algorithm"))
import camera as cam  # noqa: E402

GREEN = "#1f9d55"
GREY = "#6b7280"
FT2 = 0.09290304  # m² per ft², exactly

# The configuration of Fig. 1: sensor by its size, distance, focal length (Section 1.2).
SENSOR_MM = (15.04, 11.28)
DISTANCE_MM = 1500.0
FOCAL_MM = 13.02
CONFIGURATION = cam.Configuration(DISTANCE_MM, FOCAL_MM, sensor_mm=SENSOR_MM)


def scale_from_sensor(long_side_px: int) -> float:
    """Eq. (1) with the pitch from the sensor's long side over the photo's long side (Section 4.2), mm/px."""
    return DISTANCE_MM * (SENSOR_MM[0] / long_side_px) / FOCAL_MM


def fig_configuration(out: Path) -> None:
    width, height = 1600, 1200
    pitch_um = 1000 * SENSOR_MM[0] / width
    s = scale_from_sensor(width)
    fig, ax = plt.subplots(figsize=(9.5, 3.0))
    ax.axis("off")
    ax.set_xlim(0, 11.4)
    ax.set_ylim(0, 3.4)

    def box(x: float, title: str, lines: list[str], colour: str) -> None:
        ax.add_patch(FancyBboxPatch((x, 0.25), 4.5, 2.9, boxstyle="round,pad=0.05", fc="#f3f5f3", ec=colour, lw=1.4))
        ax.text(x + 0.2, 2.75, title, fontsize=10, weight="bold", color=colour, va="center")
        for i, line in enumerate(lines):
            ax.text(x + 0.2, 2.2 - 0.48 * i, line, fontsize=9, va="center")

    box(0.1, "Configuration", [f"sensor  {SENSOR_MM[0]:.2f} × {SENSOR_MM[1]:.2f} mm",
                               f"distance to the table  d = {DISTANCE_MM:.0f} mm",
                               f"focal length  f = {FOCAL_MM:.2f} mm",
                               f"photograph  {width} × {height} px"], GREY)
    ax.annotate("", xy=(6.75, 1.7), xytext=(4.75, 1.7), arrowprops={"arrowstyle": "-|>", "color": GREY, "lw": 1.4})
    ax.text(5.75, 1.95, "Eq. (1)", ha="center", fontsize=9, color=GREY)
    box(6.8, "What it gives", [f"pixel pitch  p = {pitch_um:.2f} µm",
                               f"scale  s = d p / f = {s:.3f} mm/px",
                               f"table covered  {s * width:.0f} × {s * height:.0f} mm",
                               f"one pixel  {s * s:.2f} mm²"], GREEN)
    fig.savefig(out / "fig_configuration.png", dpi=200, bbox_inches="tight")
    plt.close(fig)


def fig_photo(src: Path, out: Path, photo: str, credit: str) -> tuple[float, cam.Camera]:
    path = REPO / "docs/samples/leather" / photo
    bgr = cv2.imread(str(path), cv2.IMREAD_COLOR)
    mask = cv2.imread(str(src / f"real_{photo}_mask.png"), cv2.IMREAD_GRAYSCALE)
    if bgr is None or mask is None:
        raise SystemExit(f"{photo}: the photo or its mask (run experiment/measure.py first) is missing")
    camera = cam.resolve(CONFIGURATION, str(path))
    scale = camera.scale_mm_per_px[0]
    source = ("f and p from the photo" if camera.pixel_source.startswith("photo") and camera.focal_source == "photo"
              else "configuration of Fig. 1")
    area_m2 = cam.area_mm2(mask, camera) / 1e6  # Eq. (2)
    h, w = mask.shape
    width_in, strip_in = 6.0, 0.62
    photo_in = width_in * h / w
    fig = plt.figure(figsize=(width_in, photo_in + strip_in))
    ax = fig.add_axes([0, strip_in / (photo_in + strip_in), 1, photo_in / (photo_in + strip_in)])
    ax.imshow(cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB))
    contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    for c in contours:
        ring = np.vstack([c[:, 0, :], c[:1, 0, :]])
        ax.plot(ring[:, 0], ring[:, 1], color="#39ff7a", lw=1.6)
    ax.axis("off")
    strip = fig.add_axes([0, 0, 1, strip_in / (photo_in + strip_in)])
    strip.set_facecolor("#111511")
    strip.set_xticks([])
    strip.set_yticks([])
    for side in strip.spines.values():
        side.set_visible(False)
    strip.text(0.02, 0.5, f"{area_m2:.3f} m²  ({area_m2 / FT2:.1f} ft²)", fontsize=13, weight="bold",
               color="#39ff7a", va="center", ha="left", transform=strip.transAxes)
    strip.text(0.98, 0.62, f"{scale:.3f} mm/px, {source}", fontsize=8.5, color="white", va="center", ha="right",
               transform=strip.transAxes)
    strip.text(0.98, 0.28, credit, fontsize=6.5, color="#9ca3af", va="center", ha="right", transform=strip.transAxes)
    fig.savefig(out / f"fig_{Path(photo).stem}.png", dpi=200)
    plt.close(fig)
    return area_m2, camera


def main() -> int:
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    fig_configuration(out)
    print(f"scale of Fig. 1: {scale_from_sensor(1600):.4f} mm/px at 1600 px")
    for photo, credit in (("calf_black.jpg", "photo: kuerschner, Wikimedia Commons, public domain"),
                          ("cowhide_stretched.jpg", "photo: Enda012, Wikimedia Commons, CC BY 4.0, resized")):
        area, camera = fig_photo(src, out, photo, credit)
        print(f"{photo}: f = {camera.focal_length_mm:.2f} mm ({camera.focal_source}),"
              f" p = {camera.pixel_um[0]:.3f} µm ({camera.pixel_source}),"
              f" s = {camera.scale_mm_per_px[0]:.4f} mm/px, area {area:.3f} m²"
              f" (illustrative, d = {DISTANCE_MM:.0f} mm assumed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
