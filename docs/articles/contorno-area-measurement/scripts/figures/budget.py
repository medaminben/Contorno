#!/usr/bin/env python3
"""The analytical numbers of the article: §4.3 (thin lens), §4.4 (tilt,
thickness) and §6.5 (the two uncertainty budgets and the calibration target).

    budget.py [<experiment out dir>]

With the experiment's directory, the edge offset and the hides' P/sqrt(A)
are taken from synthetic.csv; without it, the values the article quotes.
The inputs marked ASSUMPTION are typical values, not measurements.
"""
from __future__ import annotations

import csv
import math
import statistics
import sys
from pathlib import Path


def rect(half_width: float) -> float:
    """Standard uncertainty of a rectangular distribution (GUM)."""
    return half_width / math.sqrt(3)


def main() -> int:
    delta_px, shape = 1.23, 4.29  # edge offset at 1600 px, large hides (Table 2), P/sqrt(A) of the synthetic hides
    if len(sys.argv) > 1:
        rows = list(csv.DictReader((Path(sys.argv[1]) / "synthetic.csv").open()))
        large = [float(r["offset_px"]) for r in rows
                 if r["width"] == "1600" and r["size"] == "large" and abs(float(r["error_pct"])) <= 5]
        delta_px = statistics.mean(large)
        shape = statistics.mean(float(r["perimeter_px"]) / math.sqrt(float(r["true_px"])) for r in rows)
    print(f"edge offset delta = {delta_px:.2f} px (1600 px, large hides); hides' P/sqrt(A) = {shape:.2f}")

    print("\n§4.3  pinhole scale against a thin lens focused on the table, Eq. (3)")
    for f, u in ((13.02, 1500), (5.56, 1500), (35.0, 1500)):
        ratio = u / (u - f)
        print(f"  f = {f:5.2f} mm, u = {u} mm: length +{100 * (ratio - 1):.3f} %, area +{100 * (ratio ** 2 - 1):.3f} %"
              f"  (2f/u = {200 * f / u:.3f} %)")

    print("\n§4.4  tilt and thickness (d = 1500 mm)")
    d = 1500.0
    theta = math.radians(2)
    print(f"  tilt 2°, hide centred: -{100 * (1 - math.cos(theta)):.3f} %")
    print(f"  tilt 2°, hide 300 mm off centre: {100 * 2 * 300 * math.tan(theta) / d:.2f} %")
    print(f"  thickness 5 mm: +{100 * ((d / (d - 5)) ** 2 - 1):.3f} %")

    print("\n§6.5  scenario A: nominal values, 35 mm route, no calibration (relative standard uncertainties of the length)")
    a = [("distance ±10 mm (ASSUMPTION)", 100 * rect(10) / d),
         ("focal length, actual vs nominal ±2 % (ASSUMPTION)", rect(2)),
         ("f35 rounding ±2 % (ASSUMPTION)", rect(2))]
    for name, u in a:
        print(f"  {name:50} {u:.3f} %")
    length = math.sqrt(sum(u * u for _, u in a))
    print(f"  combined: length {length:.2f} %, area {2 * length:.2f} % (k = 1), ±{4 * length:.2f} % (k = 2)")

    print("\n§6.5  the calibration target: edge bias e = delta P / A at s = 1.411 mm/px")
    s = 1.411
    delta_mm = delta_px * s
    hide_area = 2.226e6
    hide_bias = delta_mm * shape * math.sqrt(hide_area) / hide_area
    for name, area, perimeter in (("card 500 x 400 mm", 0.2e6, 1800.0), ("board 1000 x 1000 mm", 1e6, 4000.0)):
        target = delta_mm * perimeter / area
        print(f"  {name:22} +{100 * target:.2f} %  -> hides calibrated on it read {100 * (hide_bias - target):+.2f} %")
    print(f"  hide 2.226 m2         +{100 * hide_bias:.2f} %")

    print("\n§6.5  scenario B: calibrated on a 1000 x 1000 mm board (relative standard uncertainties of the area)")
    board = 1e6
    residual = abs(delta_mm * 4000.0 / board - hide_bias)
    b = [("board ±0.5 mm per side (ASSUMPTION)", 100 * math.hypot(rect(0.5) / 1000, rect(0.5) / 1000)),
         ("contour, spread between shapes (Table 2)", 0.06),
         ("edge offset, board vs hide, not corrected", 100 * residual),
         ("thickness board vs hide ±2 mm (ASSUMPTION)", 100 * 2 * rect(2) / d),
         ("tilt 1°, hide 100 mm off centre (ASSUMPTION)", 100 * rect(2 * 100 * math.tan(math.radians(1)) / d))]
    for name, u in b:
        print(f"  {name:44} {u:.3f} %")
    area_u = math.sqrt(sum(u * u for _, u in b))
    print(f"  combined: {area_u:.2f} % (k = 1), ±{2 * area_u:.2f} % (k = 2); lens distortion and repeatability of the photograph not included")
    return 0


if __name__ == "__main__":
    sys.exit(main())
