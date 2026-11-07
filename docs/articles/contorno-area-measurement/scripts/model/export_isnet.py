#!/usr/bin/env python3
"""Export IS-Net (DIS, general-use weights) to TorchScript: the model of Section 3.3.

    export_isnet.py --out <model.pt> [--work <download dir>] [--check <photo>]

The network's code and weights are downloaded from pinned URLs and checked by
their SHA-256. Contract: input float32 [1,3,1024,1024] (RGB, x/255 - 0.5),
output float32 [1,1,1024,1024] in 0..1.
"""
import argparse
import hashlib
import os
import sys
import types
import urllib.request

CODE_URL = ("https://raw.githubusercontent.com/xuebinqin/DIS/"
            "b6764e20381f6f42a70f83fa3324181529ed1403/IS-Net/models/isnet.py")
CODE_SHA = "83505426387792a3cb9357254a5e21547cd074710d120485960aa8cc9d0b39bb"
WEIGHTS_URL = "https://huggingface.co/bes-dev/DIS/resolve/main/isnet-general-use.pth"
WEIGHTS_SHA = "9e1aafea58f0b55d0c35077e0ceade6ba1ba2bce372fd4f8f77215391f3fac13"
SIZE = 1024


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


def load_isnet_module(path):
    # isnet.py imports torchvision without using it: stub it out.
    stub = types.ModuleType("torchvision")
    stub.models = types.ModuleType("torchvision.models")
    sys.modules.setdefault("torchvision", stub)
    sys.modules.setdefault("torchvision.models", stub.models)
    with open(path, encoding="utf-8") as f:
        src = f.read()
    src = src.replace("nn.BCELoss(size_average=True)", "nn.BCELoss()")
    mod = types.ModuleType("isnet")
    mod.__file__ = path
    exec(compile(src, path, "exec"), mod.__dict__)
    return mod


def preprocess(image_path):
    import numpy as np
    import torch
    from PIL import Image
    img = Image.open(image_path).convert("RGB").resize((SIZE, SIZE), Image.BILINEAR)
    a = np.asarray(img, dtype=np.float32) / 255.0 - 0.5
    return torch.from_numpy(a).permute(2, 0, 1).unsqueeze(0).contiguous()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", required=True)
    ap.add_argument("--work")
    ap.add_argument("--check")
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    work = os.path.abspath(args.work) if args.work else os.path.dirname(out)
    os.makedirs(work, exist_ok=True)
    os.makedirs(os.path.dirname(out), exist_ok=True)

    code = os.path.join(work, "isnet.py")
    weights = os.path.join(work, "isnet-general-use.pth")
    fetch(CODE_URL, code, CODE_SHA)
    fetch(WEIGHTS_URL, weights, WEIGHTS_SHA)

    import torch
    mod = load_isnet_module(code)
    net = mod.ISNetDIS()
    net.load_state_dict(torch.load(weights, map_location="cpu", weights_only=True))
    net.eval()

    class Wrapper(torch.nn.Module):
        def __init__(self, net):
            super().__init__()
            self.net = net

        def forward(self, x):
            return self.net(x)[0][0]

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
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # noqa: BLE001
        print(f"export_isnet: {e}", file=sys.stderr)
        sys.exit(1)
