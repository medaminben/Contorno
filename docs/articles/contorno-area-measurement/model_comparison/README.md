# Foreground cut-out models: comparison

Class-agnostic foreground segmentation models, compared to replace the
colour-threshold + GrabCut outline of Vision with an AI mask run through
LibTorch. The scripts are with the article they serve, in
`docs/articles/contorno-area-measurement/scripts/model_comparison`; how to
install and run them is in `../scripts/README.md`.

## Run times (Table 1)

`compare.py`, 2026-11-05: the four models through `rembg` 2.0.86 (ONNX,
onnxruntime 1.31.1) on the CPU (Intel Core i7-10510U, 4 cores, 8 threads,
no GPU), one call after a warm-up.
The mask is thresholded at 128 and traced with `cv::findContours` (green).

| Model | calf_black.jpg (1334×1602) | cowhide_stretched.jpg (1600×1200) |
|---|---|---|
| u2net | 0.53 s | 0.79 s |
| isnet-general-use | 1.48 s | 1.64 s |
| birefnet-general-lite | 11.25 s | 13.74 s |
| birefnet-general | 22.71 s | 30.74 s |

The four masks agree closely. Their pairwise IoU is 0.9958 to 0.9990 on the
calf and 0.9944 to 0.9981 on the cowhide; U²-Net's mask is in the three
lowest pairs on both photos. Traced contours: U²-Net 1 on each photo,
isnet-general-use 2 (calf) and 4 (cowhide), birefnet-general-lite 2 (calf)
and 1 (cowhide), birefnet-general 5 (calf) and 1 (cowhide). On
the cowhide the masks are byte-identical to those of the first run
(2026-10-28); only the run times printed on the images changed.

## Accuracy: U²-Net against IS-Net

`accuracy.py`, 2026-11-05, results in `model_accuracy.csv`. Three networks,
each through steps 1 to 4 of Section 3.4 (`contour.model_region`, minimum
area 100 px) with the background check off, so the network alone is measured:

- `isnet-torchscript`: the article's IS-Net, the TorchScript of
  `model/export_isnet.py`, input 1024 × 1024;
- `isnet-general-use`: the same network as rembg's ONNX model, input 1024 × 1024;
- `u2net`: rembg's ONNX model, input 320 × 320.

The sets are the 40 synthetic photos of Section 6.1 (with their shadow) and
the 40 photos of the second generator (seed 2026). Error is
(found − true) / true; offset is (found − true) / perimeter, where the
second set's perimeter is the length of the true mask's traced contours.
Run times are for the whole of `model_region` on the CPU.

| Model | Set | Error % (mean ± sd) | Offset px (mean) | Median IoU | \|e\| > 1 % | \|e\| > 5 % | Median s |
|---|---|---|---|---|---|---|---|
| isnet-torchscript | synthetic | +1.026 ± 0.421 | +1.601 | 0.9924 | 15 | 0 | 2.34 |
| isnet-general-use | synthetic | +0.963 ± 0.847 | +1.270 | 0.9925 | 18 | 0 | 2.19 |
| u2net | synthetic | +1.445 ± 0.683 | +2.390 | 0.9883 | 29 | 0 | 1.10 |
| isnet-torchscript | second | −3.743 ± 12.707 | −5.515 | 0.9974 | 4 | 4 | 2.16 |
| isnet-general-use | second | −2.911 ± 11.043 | −4.242 | 0.9976 | 3 | 3 | 1.70 |
| u2net | second | +0.168 ± 0.626 | +0.225 | 0.9954 | 3 | 0 | 0.67 |

Findings:

- On the synthetic set, IS-Net is closer: its outline lies 1.6 px outside the
  true one on average, U²-Net's 2.4 px, and U²-Net is over 1 % on 29 of the
  40 photos against 15. No network is off by more than 5 % there (worst:
  U²-Net +3.29 %, rembg's IS-Net −3.71 %).
- On the second set, IS-Net alone fails grossly on four photos, the
  figure–ground inversions of Section 6.3: hide_03 −46.2 %, hide_22
  −60.6 %, hide_23 −21.9 %, hide_24 −23.9 % (the same values as the
  article's run). U²-Net has no failure there; its worst photo is hide_21 at
  −2.72 %. IS-Net's sd and mean on this set are driven by those four
  photos. Without them, IS-Net's mean error is +0.08 %; U²-Net's is +0.17 %
  on all 40.
- The pipeline around the network matters less than the network, except on failures:
  per photo, the TorchScript IS-Net and rembg's IS-Net differ by a median of
  0.02 percentage points on both sets, against 0.35 (synthetic) and 0.18
  (second) between IS-Net and U²-Net. On
  hide_23, rembg's IS-Net stays within 5 % where the TorchScript one is off
  by −21.9 % (rembg's −0.71 %), on hide_22 they differ by 10 points, and on
  cowhide_large_4000_5 rembg's IS-Net is −3.71 % against +0.63 % for the
  TorchScript one. The two pipelines differ around the same network: the
  article resizes with OpenCV's bilinear filter, divides by 255, min-max
  normalises the output only when its maximum reaches the threshold
  (`isnet.py`), and resizes the float output back bilinearly; rembg resizes
  with Lanczos, divides by the photo's maximum, always min-max normalises
  the output, and resizes its 8-bit mask back with Lanczos. Which of these
  makes the difference was not tested.
- The background check of Section 3.5, which repairs IS-Net's four failures
  in the article, was not run with U²-Net. Neither set has real photos with
  a true area, so nothing here says which network is closer on real hides.

## Export of U²-Net to TorchScript

`export_u2net.py` exports U²-Net (u2net.pth, the salient-object weights) with
`torch.jit.trace` at 320 × 320. Run on 2026-11-05 with torch 2.12.0+cpu:

- the traced model and the eager one give the same output on calf_black.jpg
  (max |diff| 0, limit 1e-4); the file is 177 075 402 bytes;
- one forward on the CPU (torch, 4 threads, median of 5 after a warm-up,
  `--against`): U²-Net at 320 × 320 0.643 s, the article's IS-Net at
  1024 × 1024 1.822 s;
- against rembg's `u2net.onnx` (MD5 60024c5c889badc19c04ad937298a77b, the
  checksum rembg 2.0.86 pins), the traced model differs by at most 3.5e-5 on
  calf_black.jpg: the ONNX model has the same weights. This was measured
  once by hand on 2026-11-05; no committed script reproduces it.

Provenance:

- Code: `model/u2net.py` of github.com/xuebinqin/U-2-Net at commit
  ac7e1c817ecab7c7dff5ce6b1abba61cd213ff29, SHA-256
  96dd7a19c7de4f13520ccfc1075ded3350ff4946493be3561b9918a46218f415. The
  repository's licence is Apache-2.0 (its LICENSE file).
- Weights: the repository's README links u2net.pth (176.3 MB) only on Google
  Drive (file id 1ao1ovG1Qtx4b7EoskHXmi2E9rp5CHLcZ) and Baidu Pan, and states
  no licence for them; a Drive link is not pinned to a content. The script
  fetches the Hugging Face mirror Carve/u2net-universal at revision
  10305d785481cf4b2eee1d447c39cd6e5f43d74b (`full_weights.pth`, 176 290 937
  bytes, SHA-256
  10025a17f49cd3208afc342b589890e402ee63123d6f2d289a4a0903695cce58),
  whose model card declares Apache-2.0. The file downloaded by hand from the
  author's Google Drive link on 2026-11-05 has the same SHA-256; no committed
  script reproduces that download. Other Hugging
  Face mirrors of the same file declare MIT (niraj128/u2net, leonelhs/u2net)
  or nothing. This is the same standing as IS-Net's weights: a mirror
  declares a licence, the original repository states none. The training data
  (DUTS-TR) has its own terms; they were not checked.

Not done: U²-Net was not run through the article's own pipeline in
TorchScript (`accuracy.py` uses rembg's ONNX for it), not at an input size
other than 320 × 320, and not in the C++ application.

## Files

- `<photo>__<model>.jpg` / `_mask.png`: each result of `compare.py` and its binary mask
- `model_accuracy.csv`: every photo of `accuracy.py`
- `grid_cowhide_stretched.jpg`: the four models side by side on the cowhide
  (Fig. 3), assembled by `grid.py` from the outputs and run times of 2026-11-05
