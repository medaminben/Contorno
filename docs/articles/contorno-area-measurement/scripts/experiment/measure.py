#!/usr/bin/env python3
"""The measurements of the article, Sections 3.5 and 6.

    measure.py <model.pt> <samples/leather> <out dir>

1. The 40 synthetic photos of Section 6.1, with their shadow and without it
   (the control): the contour of Part I with the background check (the
   method), and without it (the network alone, columns model_*).
2. The threshold sweep of Section 6.2: the first two photos of each
   configuration again at four thresholds, the network alone.
3. The real sample photos of Section 6.4: the method's contour against the
   classical contour of Section 3.1, and the network alone.
4. The 40 photos of the second generator (generate_hides.py, seed 2026) of
   Section 6.3: hides of many kinds, colours and supports, with their true
   masks; the method and the network alone.

Writes synthetic.csv, synthetic_noshadow.csv, threshold.csv, real.csv,
second.csv, the photos (photos/, second/), overlays and the real photos'
masks to <out>. One pixel is
taken as 1 mm x 1 mm, so Part II plays no role; areas are in pixels.
"""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import time
from pathlib import Path

import cv2
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "algorithm"))
sys.path.insert(0, str(HERE))
import classical  # noqa: E402
import contour  # noqa: E402
import synthetic  # noqa: E402

MIN_AREA_PX = 100  # 100 mm² at 1 mm/px: regions and holes under it are dropped
FORMATS = ((1600, 1200), (4000, 3000))
LOOKS = ("cowhide", "leather")
SIZES = ("large", "small")
SEEDS = (1, 2, 3, 4, 5)
REAL = ("cowhide_stretched.jpg", "calf_black.jpg", "calf_spotted.jpg", "chekiang_lamb.jpg", "indian_lamb.jpg",
        "reindeer.jpg")


def iou(a: np.ndarray, b: np.ndarray) -> float:
    a, b = a > 0, b > 0
    either = np.count_nonzero(a | b)
    return np.count_nonzero(a & b) / either if either else 1.0


def save_overlay(file: Path, photo: np.ndarray, truth: np.ndarray, found: np.ndarray) -> None:
    """Blue: the reference outline; green: the outline found; red tint: missed; magenta tint: taken in addition."""
    out = photo.copy()
    t, f = truth > 0, found > 0
    out[t & ~f] = (40, 40, 230)
    out[f & ~t] = (200, 40, 200)
    line = max(1, photo.shape[1] // 1000)
    for mask, colour in ((truth, (255, 80, 0)), (found, (40, 220, 40))):
        contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
        cv2.drawContours(out, contours, -1, colour, line, cv2.LINE_8)
    cv2.imwrite(str(file), out)


def filled(mask: np.ndarray) -> np.ndarray:
    contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    out = np.zeros(mask.shape, np.uint8)
    cv2.drawContours(out, contours, -1, 255, cv2.FILLED)
    return out


def synthetic_run(model_file: str, out: Path, shadowed: bool) -> list[synthetic.Synthetic]:
    name = "synthetic.csv" if shadowed else "synthetic_noshadow.csv"
    swept = []
    with open(out / name, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["width", "height", "look", "size", "seed", "true_px", "hole_px", "perimeter_px", "found_px",
                    "error_pct", "iou", "offset_px", "seconds", "model_px", "model_error_pct", "model_iou",
                    "model_offset_px"])
        for width, height in FORMATS:
            for look in LOOKS:
                for size in SIZES:
                    for seed in SEEDS:
                        s = synthetic.photo(width, height, look, size, seed, shadowed)
                        cv2.imwrite(str(out / "photos" / f"{s.name}.png"), s.photo)
                        begin = time.perf_counter()
                        model = contour.model_region(s.photo, model_file, MIN_AREA_PX)
                        found = contour.background_check(s.photo, model, MIN_AREA_PX)
                        seconds = time.perf_counter() - begin
                        found_px, model_px = np.count_nonzero(found), np.count_nonzero(model)
                        error = 100 * (found_px - s.true_px) / s.true_px
                        model_error = 100 * (model_px - s.true_px) / s.true_px
                        w.writerow([width, height, look, size, seed, s.true_px, s.hole_px, s.perimeter_px, found_px,
                                    error, iou(s.truth, found), (found_px - s.true_px) / s.perimeter_px, seconds,
                                    model_px, model_error, iou(s.truth, model),
                                    (model_px - s.true_px) / s.perimeter_px])
                        f.flush()
                        print(f"{s.name:26} error {error:+7.3f} %  model alone {model_error:+7.3f} %  {seconds:.1f} s",
                              flush=True)
                        if seed == 1 or abs(error) > 5 or abs(model_error) > 5:
                            save_overlay(out / f"{s.name}_overlay.png", s.photo, s.truth, found)
                            save_overlay(out / f"{s.name}_model_overlay.png", s.photo, s.truth, model)
                        if shadowed and seed in (1, 2):
                            swept.append((width, look, size, seed))
    return swept


def threshold_run(model_file: str, out: Path, swept: list) -> None:
    with open(out / "threshold.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["width", "look", "size", "seed", "threshold", "true_px", "perimeter_px", "found_px", "error_pct",
                    "offset_px"])
        for width, look, size, seed in swept:
            s = synthetic.photo(width, width * 3 // 4, look, size, seed, True)
            for threshold in (0.3, 0.5, 0.7, 0.9):
                found_px = np.count_nonzero(contour.model_region(s.photo, model_file, MIN_AREA_PX, threshold))
                error = 100 * (found_px - s.true_px) / s.true_px
                w.writerow([width, look, size, seed, threshold, s.true_px, s.perimeter_px, found_px, error,
                            (found_px - s.true_px) / s.perimeter_px])
                print(f"{s.name:26} threshold {threshold:.1f}  error {error:+7.3f} %", flush=True)


def real_run(model_file: str, samples: Path, out: Path) -> None:
    with open(out / "real.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["photo", "width", "height", "ai_px", "classic_px", "difference_pct", "iou", "model_px"])
        for name in REAL:
            bgr = cv2.imread(str(samples / name), cv2.IMREAD_COLOR)
            if bgr is None:
                raise SystemExit(f"cannot read {samples / name}")
            region = contour.model_region(bgr, model_file, MIN_AREA_PX)
            found = contour.background_check(bgr, region, MIN_AREA_PX)
            classic = classical.classical_hide(bgr)
            if classic is None:
                raise SystemExit(f"{name}: the classical contour found nothing")
            ai = filled(found)  # the classical mask has its holes filled: compare with the method's filled too
            a, c = np.count_nonzero(ai), np.count_nonzero(classic)
            w.writerow([name, bgr.shape[1], bgr.shape[0], a, c, 100 * (c - a) / a, iou(ai, classic),
                        np.count_nonzero(region)])
            f.flush()
            print(f"{name:22} method {100 * np.count_nonzero(found) / found.size:5.1f} % of the photo,"
                  f" network alone {100 * np.count_nonzero(region) / region.size:5.1f} %,"
                  f" classical - method {100 * (c - a) / a:+6.2f} %, IoU {iou(ai, classic):.4f}", flush=True)
            save_overlay(out / f"real_{name}_overlay.png", bgr, classic, ai)
            cv2.imwrite(str(out / f"real_{name}_mask.png"), found)
            cv2.imwrite(str(out / f"real_{name}_model_mask.png"), region)


def second_run(model_file: str, out: Path, count: int = 40, seed: int = 2026) -> None:
    folder = out / "second"
    subprocess.run([sys.executable, "-I", str(HERE / "generate_hides.py"), str(count), str(seed), str(folder)],
                   check=True, stdout=subprocess.DEVNULL)
    kinds = json.loads((folder / "manifest.json").read_text())
    with open(out / "second.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["photo", "colour", "surface", "background", "width", "height", "true_px", "found_px",
                    "error_pct", "iou", "model_px", "model_error_pct", "model_iou", "taken"])
        for i in range(1, count + 1):
            name = f"hide_{i:02d}"
            bgr = cv2.imread(str(folder / f"{name}.jpg"), cv2.IMREAD_COLOR)
            truth = np.where(cv2.imread(str(folder / f"{name}_mask.png"), cv2.IMREAD_GRAYSCALE) > 127, 255, 0)
            truth = truth.astype(np.uint8)
            true_px = np.count_nonzero(truth)
            region = contour.model_region(bgr, model_file, MIN_AREA_PX)
            found = contour.background_check(bgr, region, MIN_AREA_PX)
            error = 100 * (np.count_nonzero(found) - true_px) / true_px
            model_error = 100 * (np.count_nonzero(region) - true_px) / true_px
            taken = "background" if not np.array_equal(region, found) else "network"
            k = kinds[name]
            w.writerow([name, k["colour"], k["surface"], k["background"], bgr.shape[1], bgr.shape[0], true_px,
                        np.count_nonzero(found), error, iou(truth, found), np.count_nonzero(region), model_error,
                        iou(truth, region), taken])
            f.flush()
            print(f"second {name} {k['colour']:12} on {k['background']:10} error {error:+7.2f} %"
                  f"  network alone {model_error:+7.2f} %  ({taken})", flush=True)
            if abs(error) > 5 or abs(model_error) > 5:
                save_overlay(out / f"second_{name}_overlay.png", bgr, truth, found)
                save_overlay(out / f"second_{name}_model_overlay.png", bgr, truth, region)


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    model_file, samples, out = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
    (out / "photos").mkdir(parents=True, exist_ok=True)
    swept = synthetic_run(model_file, out, shadowed=True)
    synthetic_run(model_file, out, shadowed=False)
    threshold_run(model_file, out, swept)
    real_run(model_file, samples, out)
    second_run(model_file, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
