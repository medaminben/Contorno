#!/usr/bin/env python3
"""U²-Net against IS-Net on the synthetic sets of Section 6, the networks alone.

    accuracy.py <isnet.pt> <out dir>

Three networks, each through steps 1 to 4 of Section 3.4 (contour.model_region)
with the background check off:
- isnet-torchscript: the article's IS-Net, the TorchScript of model/export_isnet.py;
- isnet-general-use and u2net: rembg's ONNX models, rembg's mask / 255 as the
  probability.
Two sets: the 40 synthetic photos of Section 6.1 (with their shadow) and the
40 photos of the second generator (seed 2026, generated once into <out>/second).

Writes <out>/model_accuracy.csv (model,set,photo,true_px,found_px,error_pct,iou,
offset_px,seconds) and prints a summary per model and set. offset_px is
(found - true) / perimeter; on the second set the perimeter is the length of
the true mask's traced contours. One pixel is taken as 1 mm x 1 mm.
"""
from __future__ import annotations

import csv
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path

import cv2
import numpy as np
from PIL import Image
from rembg import new_session, remove

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "algorithm"))
sys.path.insert(0, str(HERE.parent / "experiment"))
import contour  # noqa: E402
import isnet  # noqa: E402
import measure  # noqa: E402
import synthetic  # noqa: E402

SECOND_COUNT = 40
SECOND_SEED = 2026


def rembg_probability(session):
    def probability(bgr: np.ndarray, _model_path: str, _threshold: float) -> np.ndarray:
        rgb = Image.fromarray(cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB))
        return np.asarray(remove(rgb, session=session, only_mask=True), np.float32) / 255
    return probability


def traced_perimeter(mask: np.ndarray) -> float:
    contours, _ = cv2.findContours((mask > 0).astype(np.uint8), cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    return float(sum(cv2.arcLength(c, True) for c in contours))


def synthetic_set():
    for width, height in measure.FORMATS:
        for look in measure.LOOKS:
            for size in measure.SIZES:
                for seed in measure.SEEDS:
                    s = synthetic.photo(width, height, look, size, seed, True)
                    yield s.name, s.photo, s.truth, s.true_px, s.perimeter_px


def second_set(out: Path):
    folder = out / "second"
    if not (folder / "manifest.json").is_file():
        subprocess.run([sys.executable, "-I", str(HERE.parent / "experiment" / "generate_hides.py"),
                        str(SECOND_COUNT), str(SECOND_SEED), str(folder)], check=True, stdout=subprocess.DEVNULL)
    for i in range(1, SECOND_COUNT + 1):
        name = f"hide_{i:02d}"
        bgr = cv2.imread(str(folder / f"{name}.jpg"), cv2.IMREAD_COLOR)
        truth = np.where(cv2.imread(str(folder / f"{name}_mask.png"), cv2.IMREAD_GRAYSCALE) > 127, 255, 0)
        truth = truth.astype(np.uint8)
        yield name, bgr, truth, np.count_nonzero(truth), traced_perimeter(truth)


def summary(rows: list[dict]) -> None:
    print(f"\n{'model':18} {'set':9} {'n':>3}  {'error % mean ± sd':>19}  {'offset px':>9}  {'median IoU':>10}"
          f"  {'|e|>1%':>6}  {'|e|>5%':>6}  {'median s':>8}")
    for model in dict.fromkeys(r["model"] for r in rows):
        for name in dict.fromkeys(r["set"] for r in rows):
            g = [r for r in rows if r["model"] == model and r["set"] == name]
            e = [r["error_pct"] for r in g]
            print(f"{model:18} {name:9} {len(g):3}  {statistics.mean(e):+8.3f} ± {statistics.stdev(e):7.3f}"
                  f"  {statistics.mean(r['offset_px'] for r in g):+9.3f}"
                  f"  {statistics.median(r['iou'] for r in g):10.4f}"
                  f"  {sum(abs(x) > 1 for x in e):6}  {sum(abs(x) > 5 for x in e):6}"
                  f"  {statistics.median(r['seconds'] for r in g):8.2f}")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    model_file, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    networks = {"isnet-torchscript": isnet.probability}
    for name in ("isnet-general-use", "u2net"):
        networks[name] = rembg_probability(new_session(name))
    warm = synthetic.photo(*measure.FORMATS[0], measure.LOOKS[0], measure.SIZES[0], measure.SEEDS[0], True).photo
    for probability in networks.values():
        probability(warm, model_file, contour.THRESHOLD)  # warm-up

    rows = []
    with open(out / "model_accuracy.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["model", "set", "photo", "true_px", "found_px", "error_pct", "iou", "offset_px", "seconds"])
        for set_name, photos in (("synthetic", synthetic_set()), ("second", second_set(out))):
            for photo, bgr, truth, true_px, perimeter_px in photos:
                for model, probability in networks.items():
                    begin = time.perf_counter()
                    found = contour.model_region(bgr, model_file, measure.MIN_AREA_PX, probability=probability)
                    seconds = time.perf_counter() - begin
                    found_px = np.count_nonzero(found)
                    row = {"model": model, "set": set_name, "photo": photo, "true_px": true_px, "found_px": found_px,
                           "error_pct": 100 * (found_px - true_px) / true_px, "iou": measure.iou(truth, found),
                           "offset_px": (found_px - true_px) / perimeter_px, "seconds": seconds}
                    rows.append(row)
                    w.writerow(row.values())
                    f.flush()
                    print(f"{model:18} {set_name:9} {photo:26} error {row['error_pct']:+8.3f} %"
                          f"  IoU {row['iou']:.4f}  {seconds:.2f} s", flush=True)
    summary(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
