"""Fig. 3 of the article: the four results of compare.py on one photo, two by two.

    grid.py <compare.py out dir> <photo stem> <out file>

Each result is halved, so the grid has the size of one result. The run times
are the ones compare.py drew on the results.
"""
import sys
from pathlib import Path
import cv2
import numpy as np

results, stem, out = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
models = ["u2net", "isnet-general-use", "birefnet-general-lite", "birefnet-general"]

tiles = []
for model in models:
    img = cv2.imread(str(results / f"{stem}__{model}.jpg"))
    if img is None:
        sys.exit(f"missing {stem}__{model}.jpg in {results}")
    tiles.append(cv2.resize(img, (img.shape[1] // 2, img.shape[0] // 2), interpolation=cv2.INTER_AREA))
grid = np.vstack([np.hstack(tiles[:2]), np.hstack(tiles[2:])])
cv2.imwrite(str(out), grid, [cv2.IMWRITE_JPEG_QUALITY, 92])
print(f"{out}: {grid.shape[1]} x {grid.shape[0]}")
