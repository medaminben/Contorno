#!/usr/bin/env python3
"""Export U²-Net (u2net.pth, the salient-object weights) to TorchScript, to compare it with IS-Net.

    export_u2net.py --out <model.pt> [--work <download dir>] [--check <photo>] [--against <isnet.pt>]

The network's code and weights are downloaded from pinned URLs and checked by
their SHA-256, each into its own folder under <work>. Contract: input float32
[1,3,320,320] (RGB, x/255, then ImageNet's mean and standard deviation), output
float32 [1,1,320,320] in 0..1 (the fused output d0). --against times one
forward of each model on the CPU at its own input size (median of 5, after a
warm-up).
"""
import argparse
import hashlib
import os
import statistics
import sys
import time
import types
import urllib.request

CODE_URL = ("https://raw.githubusercontent.com/xuebinqin/U-2-Net/"
            "ac7e1c817ecab7c7dff5ce6b1abba61cd213ff29/model/u2net.py")
CODE_SHA = "96dd7a19c7de4f13520ccfc1075ded3350ff4946493be3561b9918a46218f415"
WEIGHTS_URL = ("https://huggingface.co/Carve/u2net-universal/resolve/"
               "10305d785481cf4b2eee1d447c39cd6e5f43d74b/full_weights.pth")
WEIGHTS_SHA = "10025a17f49cd3208afc342b589890e402ee63123d6f2d289a4a0903695cce58"
SIZE = 320
ISNET_SIZE = 1024
MEAN = (0.485, 0.456, 0.406)
STD = (0.229, 0.224, 0.225)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(url, dest, expected):
    if os.path.isfile(dest) and sha256(dest) == expected:
        print(f"using cached {dest}")
        return
    print(f"downloading {url}")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    tmp = dest + ".part"
    with urllib.request.urlopen(url, timeout=60) as r, open(tmp, "wb") as f:
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
    got = sha256(tmp)
    if got != expected:
        os.remove(tmp)
        raise RuntimeError(f"SHA-256 mismatch for {url}: expected {expected}, got {got}")
    os.replace(tmp, dest)


def load_u2net_module(path):
    with open(path, encoding="utf-8") as f:
        src = f.read()
    mod = types.ModuleType("u2net")
    mod.__file__ = path
    exec(compile(src, path, "exec"), mod.__dict__)
    return mod


def preprocess(image_path):
    import numpy as np
    import torch
    from PIL import Image
    img = Image.open(image_path).convert("RGB").resize((SIZE, SIZE), Image.BILINEAR)
    a = (np.asarray(img, dtype=np.float32) / 255.0 - np.float32(MEAN)) / np.float32(STD)
    return torch.from_numpy(a).permute(2, 0, 1).unsqueeze(0).contiguous()


def forward_seconds(model, side):
    import torch
    x = torch.rand(1, 3, side, side)
    with torch.inference_mode():
        model(x)
        times = []
        for _ in range(5):
            begin = time.perf_counter()
            model(x)
            times.append(time.perf_counter() - begin)
    return statistics.median(times)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", required=True)
    ap.add_argument("--work")
    ap.add_argument("--check")
    ap.add_argument("--against")
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    work = os.path.abspath(args.work) if args.work else os.path.dirname(out)
    os.makedirs(os.path.dirname(out), exist_ok=True)

    code = os.path.join(work, "code", "u2net.py")
    weights = os.path.join(work, "weights", "u2net.pth")
    fetch(CODE_URL, code, CODE_SHA)
    fetch(WEIGHTS_URL, weights, WEIGHTS_SHA)

    import torch
    mod = load_u2net_module(code)
    net = mod.U2NET(3, 1)
    net.load_state_dict(torch.load(weights, map_location="cpu", weights_only=True))
    net.eval()

    class Wrapper(torch.nn.Module):
        def __init__(self, net):
            super().__init__()
            self.net = net

        def forward(self, x):
            return self.net(x)[0]

    wrapper = Wrapper(net).eval()
    with torch.no_grad():
        traced = torch.jit.trace(wrapper, torch.rand(1, 3, SIZE, SIZE))
    tmp = out + ".tmp"
    traced.save(tmp)
    os.replace(tmp, out)
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")

    if args.check:
        x = preprocess(args.check)
        with torch.no_grad():
            ref = wrapper(x)
            got = torch.jit.load(out)(x)
        diff = (ref - got).abs().max().item()
        print(f"eager shape {tuple(ref.shape)}, traced shape {tuple(got.shape)}, "
              f"range [{got.min().item():.4f}, {got.max().item():.4f}], max|diff| {diff:.3e}")
        if tuple(got.shape) != (1, 1, SIZE, SIZE) or diff > 1e-4:
            print("check failed", file=sys.stderr)
            return 1

    if args.against:
        u2 = forward_seconds(torch.jit.load(out, map_location="cpu").eval(), SIZE)
        isn = forward_seconds(torch.jit.load(args.against, map_location="cpu").eval(), ISNET_SIZE)
        print(f"one forward on the CPU ({torch.get_num_threads()} threads): U²-Net {SIZE}x{SIZE} {u2:.3f} s, "
              f"IS-Net {ISNET_SIZE}x{ISNET_SIZE} {isn:.3f} s")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # noqa: BLE001
        print(f"export_u2net: {e}", file=sys.stderr)
        sys.exit(1)
