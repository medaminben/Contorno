"""Step 1 of Section 3.4: IS-Net's probability that a pixel belongs to the object.

The model is the TorchScript file that model/export_isnet.py writes. Images are
8-bit BGR (OpenCV's order).
"""
from __future__ import annotations

import cv2
import numpy as np
import torch

MODEL_SIDE = 1024
_models: dict[str, torch.jit.ScriptModule] = {}


def probability(bgr: np.ndarray, model_path: str, threshold: float = 0.5) -> np.ndarray:
    """float32 in [0, 1] at the photo's size: RGB, 1024 x 1024, x / 255 - 0.5, then the sigmoid output.

    The output is min-max normalised unless nothing reaches the threshold, so
    that an empty table stays empty.
    """
    if model_path not in _models:
        _models[model_path] = torch.jit.load(model_path, map_location="cpu").eval()
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    x = cv2.resize(rgb, (MODEL_SIDE, MODEL_SIDE), interpolation=cv2.INTER_LINEAR).astype(np.float32) / 255 - 0.5
    with torch.inference_mode():
        y = _models[model_path](torch.from_numpy(x).permute(2, 0, 1)[None].contiguous())[0, 0].float().numpy()
    lo, hi = float(y.min()), float(y.max())
    if hi >= threshold and hi > lo:
        y = (y - lo) / (hi - lo)
    return cv2.resize(y, (bgr.shape[1], bgr.shape[0]), interpolation=cv2.INTER_LINEAR)
