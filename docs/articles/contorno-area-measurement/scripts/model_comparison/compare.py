"""Table 1 and Fig. 3 of the article: the four candidate models on sample photos, through rembg.

    compare.py <out dir> <images...>

Writes <photo>__<model>.jpg (outline drawn, run time) and _mask.png. The results of
2026-11-05 are in the article's model_comparison folder (README.md: the run times of Table 1).
"""
import sys, time
from pathlib import Path
import cv2
import numpy as np
from PIL import Image
from rembg import new_session, remove

out = Path(sys.argv[1])
images = [Path(p) for p in sys.argv[2:]]
models = ["u2net", "isnet-general-use", "birefnet-general-lite", "birefnet-general"]

for model in models:
    session = new_session(model)
    for img_path in images:
        img = Image.open(img_path).convert("RGB")
        remove(img, session=session, only_mask=True)  # warm-up
        t = time.perf_counter()
        mask = np.array(remove(img, session=session, only_mask=True))
        dt = time.perf_counter() - t
        binary = (mask > 127).astype(np.uint8) * 255
        contours, _ = cv2.findContours(binary, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
        bgr = cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        thick = max(1, bgr.shape[1] // 400)
        cv2.drawContours(bgr, contours, -1, (0, 255, 0), thick)
        label = f"{model}  {dt:.2f}s"
        cv2.putText(bgr, label, (10, 10 + 20 * thick), cv2.FONT_HERSHEY_SIMPLEX, 0.6 * thick, (0, 0, 255), 2 * thick)
        cv2.imwrite(str(out / f"{img_path.stem}__{model}.jpg"), bgr)
        cv2.imwrite(str(out / f"{img_path.stem}__{model}_mask.png"), binary)
        print(f"{model:24s} {img_path.name:28s} {img.size} {dt:6.2f}s  contours={len(contours)}", flush=True)
