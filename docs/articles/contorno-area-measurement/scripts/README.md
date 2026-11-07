# The article's scripts

Everything in the article that is not prose is made by the Python scripts in
this folder: the model, the contour algorithm, the measurements, the figures,
the tables and the analytical numbers. From the repository's root, two
commands remake it all (about 30 minutes on a 4-core, 8-thread CPU, no GPU):

```sh
S=docs/articles/contorno-area-measurement/scripts
$S/run_experiment.sh      # model, measurements, figures, budget
$S/build_article.sh       # HTML and PDF
```

It needs:
- python3 with venv, and network access on the first run: the packages of
  `requirements.txt` are installed into `build/article/venv`, and the model's
  code and weights are downloaded and checked;
- pandoc and Chromium, for the HTML and PDF.

The photographs are read from `docs/samples/leather` (their sources and
licences: `docs/samples/SOURCES.md`). Everything else goes to
`build/article`, except the figures, which go to `../figures`. The synthetic
photos are drawn from fixed seeds. On them and on the real photos, the method
with IS-Net gives the same numbers on every run, apart from the run times. The
classical contour of Section 3.1 (`classic_px` in `real.csv`) is not as
repeatable: between two runs its pixel count changed on three real photos
(calf_spotted, chekiang_lamb, reindeer; at most 0.2 %). GrabCut starts its
colour models by k-means on OpenCV's shared random number generator, which
`classical.py` does not seed; with `cv2.setRNGSeed(0)` before each call the
counts repeat exactly. The two photos of Table 5 did not change.

## Folders

| Path | What it does |
|---|---|
| `model/export_isnet.py` | Downloads IS-Net's code and general-use weights (pinned, SHA-256 checked) and exports the network to TorchScript (Section 3.3). |
| `algorithm/` | The contour algorithm of Part I. See below. |
| `experiment/synthetic.py` | The synthetic photographs of Section 6.1, with their exact area. |
| `experiment/generate_hides.py` | The second generator of Section 6.3: 40 hide photographs of many kinds, with their true masks (seed 2026). `docs/samples/synthetic` keeps two of them for the tests. |
| `experiment/measure.py` | All measurements of Sections 3.5 and 6. See below. |
| `figures/figures.py` | The plots and crops. It also prints the statistics of Tables 2–5. |
| `figures/fig_background.py` | Fig. 5: the stages of the background check on the spotted calf. |
| `figures/fig_measured.py` | Figs. 1, 4 and 8: the minimal configuration, and two photographs with their contour and a label with the area. |
| `figures/budget.py` | The analytical numbers, printed: the thin-lens bias of Eq. (3), tilt, thickness, both uncertainty budgets of §6.5 and the calibration-target figures. |
| `model_comparison/compare.py` | Table 1 and the panels of Fig. 3: the four candidate models through rembg. See below. |
| `model_comparison/grid.py` | Fig. 3's grid: the four results of `compare.py` on one photo, two by two. |
| `model_comparison/accuracy.py` | U²-Net against IS-Net on the synthetic sets of Section 6, the networks alone. See below. |
| `model_comparison/export_u2net.py` | Downloads U²-Net's code and weights (pinned, SHA-256 checked), exports the network to TorchScript and times one forward against IS-Net's. See below. |
| `run_experiment.sh` | Runs the model export (once), the measurements, then the figure scripts. |
| `build_article.sh` | pandoc (MathML) to HTML, with `../title-logo.html` (the wordmark, `../figures/contorno_wordmark.svg`) above the title, then Chromium to PDF. |

### The algorithm

| File | What it does |
|---|---|
| `isnet.py` | Step 1 of Section 3.4: the network's probability per pixel. |
| `masks.py` | The topological cleaning of step 2, the largest region, the enclosed holes. |
| `contour.py` | Steps 1 to 5 of Section 3.4: `model_region` is the network's region, `background_check` the rule of Section 3.5, `find_hide` both. The area is counted on its mask. |
| `background.py` | The background check of Section 3.5: coarse copy, background model, coarse object, fine edge, and the rule. |
| `classical.py` | The classical contour of Section 3.1: colour statistics, Otsu's threshold, morphology and GrabCut. |
| `outline.py` | Runs the contour on photographs: `outline.py <model.pt> <out dir> <photo>...`. |

### The measurements

`measure.py` writes these files to `build/article`:
- `synthetic.csv` and `synthetic_noshadow.csv`: the 40 synthetic photos with
  their shadow, and the 40 controls without it. Each photo is measured with
  the background check and without it (the `model_*` columns: the network
  alone).
- `threshold.csv`: the threshold sweep of Section 6.2, the network alone.
- `real.csv`: the real photos, the method against the classical contour,
  and the network alone.
- `second.csv`: the second generator's 40 photos against their true masks,
  with the background check and without it.
- the photos (`photos/`, `second/`), the overlays and the masks of the real
  photos.

One pixel is taken as 1 mm × 1 mm, so Part II plays no role and the areas
are in pixels.

### The model comparison

The three scripts need `requirements.txt` and `model_comparison/requirements.txt`,
installed into their own venv (the runs of 2026-11-05 used `build/review/venv`):

```sh
S=docs/articles/contorno-area-measurement/scripts
python3 -m venv build/review/venv
build/review/venv/bin/pip install -r $S/requirements.txt -r $S/model_comparison/requirements.txt
build/review/venv/bin/python -I $S/model_comparison/compare.py build/review/compare \
    docs/samples/leather/cowhide_stretched.jpg docs/samples/leather/calf_black.jpg
build/review/venv/bin/python -I $S/model_comparison/accuracy.py build/article/model/isnet-general-use.pt build/review/accuracy
build/review/venv/bin/python -I $S/model_comparison/export_u2net.py --out build/review/u2net.pt \
    --work build/review/u2net --check docs/samples/leather/calf_black.jpg --against build/article/model/isnet-general-use.pt
```

- `compare.py <out dir> <photos...>`: the four models through rembg; Table 1's
  run times. rembg keeps its ONNX models in `~/.rembg`.
- `accuracy.py <isnet.pt> <out dir>`: IS-Net (the article's TorchScript, and
  rembg's ONNX) and U²-Net (rembg's ONNX) through steps 1 to 4 of Section 3.4,
  without the background check, on the 40 synthetic photos of Section 6.1 and
  the second generator's 40. Writes `model_accuracy.csv` and prints a summary
  per model and set.
- `export_u2net.py`: U²-Net to TorchScript, at its input size of 320 × 320.

Their results are in `../model_comparison`: the images, `model_accuracy.csv`,
and in `README.md` the run times of Table 1, the accuracy summary and the
export. The four-panel image of Fig. 3, `grid_cowhide_stretched.jpg`, is
made from the outputs of 2026-11-05:

```sh
build/review/venv/bin/python -I $S/model_comparison/grid.py docs/articles/contorno-area-measurement/model_comparison \
    cowhide_stretched docs/articles/contorno-area-measurement/model_comparison/grid_cowhide_stretched.jpg
```

## Where each figure and table comes from

The figure files are in `../figures`; the experiment's files are in `build/article`.

| In the article | File | Made by |
|---|---|---|
| Fig. 1 the minimal configuration and what it gives | `fig_configuration.png` | `fig_measured.py` (`fig_configuration`) |
| Fig. 2 classical vs IS-Net at a cowhide leg | `fig_contour_methods.png` | `figures.py` (`fig_contour_methods`) from `real_cowhide_stretched.jpg_overlay.png` |
| Fig. 3 the four candidate models | `fig_model_comparison.png` | `figures.py` (`fig_model_comparison`) from `../model_comparison/grid_cowhide_stretched.jpg` |
| Fig. 4 black calf skin, contour and area | `fig_calf_black.png` | `fig_measured.py` (`fig_photo`) from `real_calf_black.jpg_mask.png` |
| Fig. 5 background check on the spotted calf | `fig_background.png` | `fig_background.py` from `real_calf_spotted.jpg_mask.png`, `real_calf_spotted.jpg_model_mask.png` and `algorithm/background.py` |
| Fig. 6 geometry | `fig_geometry.png` | `figures.py` (`fig_geometry`) |
| Fig. 7 thin-lens bias | `fig_thin_lens_bias.png` | `figures.py` (`fig_budget`) |
| Fig. 8 cowhide, contour and area | `fig_cowhide_stretched.png` | `fig_measured.py` (`fig_photo`) from `real_cowhide_stretched.jpg_mask.png` |
| Fig. 9 area errors and offset | `fig_synthetic_errors.png` | `figures.py` (`fig_errors`) from `synthetic*.csv` |
| Fig. 10 edge detail | `fig_synthetic_edge.png` | `figures.py` (`crop`) from `cowhide_large_1600_1_overlay.png` |
| Fig. 11 threshold sweep | `fig_threshold.png` | `figures.py` (`fig_threshold`) from `threshold.csv` |
| Fig. 12 figure–ground inversion, the network alone and with the background check | `fig_failure.png` | `figures.py` (`fig_failure`) from the overlays of the second set's worst failure of the network alone |
| Fig. 13 real cowhide, classical vs IS-Net | `fig_real_cowhide.png` | `figures.py` (`crop`) from `real_cowhide_stretched.jpg_overlay.png` |
| Table 1 model run times | – | `../model_comparison/README.md` (`model_comparison/compare.py`) |
| Tables 2, 3, 4, 5 | – | printed by `figures.py` (Table 4 from `second.csv`) |
| §3.5 numbers (shares, run times) | – | `real.csv`, `synthetic.csv` (`seconds`) |
| §4.3, §4.4, §6.5 numbers | – | printed by `budget.py` |
