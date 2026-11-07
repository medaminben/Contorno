#!/usr/bin/env python3
"""The article's figures and summary tables from the output of experiment/measure.py.

    figures.py <experiment out dir> <the article's figures dir>

Reads synthetic.csv, synthetic_noshadow.csv, threshold.csv, real.csv and the overlay
images; writes PNG figures and prints the statistics the article quotes.
"""
from __future__ import annotations

import csv
import math
import statistics
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.image import imread  # noqa: E402

MODEL_SIDE = 1024
GREEN = "#1f9d55"
BLUE = "#2563eb"
GREY = "#6b7280"
RED = "#dc2626"

plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                     "savefig.dpi": 200, "savefig.bbox": "tight"})


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def cell_px(width: int, height: int) -> float:
    """The mean size of one model cell in photo pixels (the photo is resized to 1024 x 1024)."""
    return (width / MODEL_SIDE + height / MODEL_SIDE) / 2


def summary(data: list[dict[str, str]], label: str, prefix: str = "") -> list[tuple]:
    """Table 2 by configuration; prefix "model_": the model alone, without the background check."""
    data = [{**r, "error_pct": r[prefix + "error_pct"], "offset_px": r[prefix + "offset_px"], "iou": r[prefix + "iou"]}
            for r in data]
    out = []
    groups: dict[tuple, list[dict[str, str]]] = {}
    for r in data:
        groups.setdefault((int(r["width"]), r["look"], r["size"]), []).append(r)
    print(f"\n{label}: config, n, error % mean ± sd [min, max], offset px mean, offset in model cells, IoU mean"
          " (gross failures, |error| > 5 %, excluded and listed)")
    for key in sorted(groups):
        g = groups[key]
        good = [r for r in g if abs(float(r["error_pct"])) <= 5]
        if len(good) < 2:
            print(f"  {key}: fewer than two photos within 5 %")
            continue
        bad = [r for r in g if abs(float(r["error_pct"])) > 5]
        e = [float(r["error_pct"]) for r in good]
        o = [float(r["offset_px"]) for r in good]
        iou = [float(r["iou"]) for r in good]
        cells = statistics.mean(o) / cell_px(key[0], key[0] * 3 // 4)
        line = (key, len(g), statistics.mean(e), statistics.stdev(e), min(e), max(e), statistics.mean(o), cells,
                statistics.mean(iou), [(r["seed"], r["error_pct"], r["iou"]) for r in bad])
        out.append(line)
        print(f"  {key[0]} {key[1]:8} {key[2]:5}  n={len(g)}  {line[2]:+.2f} ± {line[3]:.2f} [{line[4]:+.2f}, {line[5]:+.2f}]"
              f"  offset {line[6]:+.2f} px = {cells:.2f} cells  IoU {line[8]:.4f}  failures {line[9]}")
    return out


def fig_errors(shadow: list[dict[str, str]], plain: list[dict[str, str]], out: Path) -> None:
    configs = sorted({(int(r["width"]), r["look"], r["size"]) for r in shadow})
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.8), gridspec_kw={"width_ratios": [3, 2]})
    ax = axes[0]
    for i, c in enumerate(configs):
        for data, dx, colour, name in ((shadow, -0.15, GREEN, "with shadow"), (plain, 0.15, BLUE, "without shadow")):
            ys = [float(r["error_pct"]) for r in data if (int(r["width"]), r["look"], r["size"]) == c
                  and abs(float(r["error_pct"])) <= 5]
            ax.scatter([i + dx] * len(ys), ys, s=16, color=colour, label=name if i == 0 else None, zorder=3)
    ax.axhline(0, color=GREY, lw=0.8)
    ax.set_xticks(range(len(configs)))
    ax.set_xticklabels([f"{w}px\n{look}\n{size}" for w, look, size in configs], fontsize=8)
    ax.set_ylabel("area error (%)")
    ax.set_title("(a) Area error against the true area", loc="left", fontsize=10)
    ax.legend(frameon=False, fontsize=8, loc="upper right")
    ax = axes[1]
    for data, colour, name in ((shadow, GREEN, "with shadow"), (plain, BLUE, "without shadow")):
        for w, marker in ((1600, "o"), (4000, "s")):
            pts = [r for r in data if int(r["width"]) == w and abs(float(r["error_pct"])) <= 5]
            xs = [float(r["offset_px"]) for r in pts]
            ys = [float(r["offset_px"]) / cell_px(w, w * 3 // 4) for r in pts]
            ax.scatter(xs, ys, s=16, color=colour, marker=marker,
                       label=f"{w} px, {name}" if True else None)
    ax.set_xlabel("mean edge offset δ (photo pixels)")
    ax.set_ylabel("δ in model cells")
    ax.set_title("(b) Offset of the outline outward", loc="left", fontsize=10)
    ax.legend(frameon=False, fontsize=7)
    fig.tight_layout()
    fig.savefig(out / "fig_synthetic_errors.png")
    plt.close(fig)


def fig_threshold(sweep: list[dict[str, str]], out: Path) -> None:
    """The outline's offset against the threshold on the model's probability."""
    fig, ax = plt.subplots(figsize=(6, 3.6))
    photos: dict[tuple, list[tuple[float, float]]] = {}
    for r in sweep:
        key = (int(r["width"]), r["look"], r["size"], r["seed"])
        photos.setdefault(key, []).append((float(r["threshold"]), float(r["offset_px"])))
    print("\nthreshold sweep: offset px mean [min, max] per threshold and width (gross failures, |offset| > 10 px, listed)")
    for w in (1600, 4000):
        for t in (0.3, 0.5, 0.7, 0.9):
            o = [float(r["offset_px"]) for r in sweep if int(r["width"]) == w and float(r["threshold"]) == t]
            good = [x for x in o if abs(x) <= 10]
            print(f"  {w} px  t={t}  {statistics.mean(good):+.2f} [{min(good):+.2f}, {max(good):+.2f}]"
                  f"  failures {[x for x in o if abs(x) > 10]}")
    for key, pts in sorted(photos.items()):
        pts.sort()
        good = [(t, o) for t, o in pts if abs(o) <= 10]
        colour = GREEN if key[0] == 1600 else BLUE
        ax.plot([t for t, _ in good], [o for _, o in good], color=colour, alpha=0.6, marker="o", ms=3, lw=1)
        for t, o in pts:
            if abs(o) > 10:
                ax.annotate(f"{key[1]} {key[2]} {key[0]} px: {o:+.0f} px\n(part of the hide lost)", xy=(t, -1.0),
                            xytext=(0.52, -1.25), fontsize=7, color=RED, arrowprops={"arrowstyle": "->", "color": RED})
    ax.plot([], [], color=GREEN, marker="o", ms=3, label="1600 × 1200 px")
    ax.plot([], [], color=BLUE, marker="o", ms=3, label="4000 × 3000 px")
    ax.axhline(0, color=GREY, lw=0.8)
    ax.set_xlabel("threshold on the model's probability")
    ax.set_ylabel("mean edge offset δ (photo pixels)")
    ax.set_ylim(-1.4, 3.9)
    ax.legend(frameon=False, fontsize=8)
    fig.tight_layout()
    fig.savefig(out / "fig_threshold.png")
    plt.close(fig)


def fig_geometry(out: Path) -> None:
    """Pinhole and thin-lens geometry in 3D: the table plane, the lens and the sensor plane, parallel (not to scale)."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection  # noqa: PLC0415

    u, v = 10.0, 2.2   # drawing units: lens to table, lens to sensor
    tw, th = 4.4, 3.3  # half sides of the table area the photo covers
    m = v / u          # magnification: the sensor's half sides are m tw, m th
    fig = plt.figure(figsize=(10, 7))
    ax = fig.add_subplot(projection="3d")
    ax.set_proj_type("persp", focal_length=0.6)

    # (a, b) lie in the planes, z runs along the optical axis; the axis is drawn horizontal, left to right
    def at(a: float, b: float, z: float) -> tuple[float, float, float]:
        return (z, a, b)

    def poly(points: list[tuple[float, float, float]], **style) -> None:
        ax.add_collection3d(Poly3DCollection([[at(*p) for p in points]], **style))

    def line(p: tuple[float, float, float], q: tuple[float, float, float], **style) -> None:
        a, b = at(*p), at(*q)
        ax.plot([a[0], b[0]], [a[1], b[1]], [a[2], b[2]], **style)

    def text(p: tuple[float, float, float], label: str, **style) -> None:
        ax.text(*at(*p), label, **style)

    def plane(z: float, hw: float, hh: float, face: str, edge: str, alpha: float) -> None:
        poly([(-hw, -hh, z), (hw, -hh, z), (hw, hh, z), (-hw, hh, z)], facecolor=face, edgecolor=edge,
             alpha=alpha, lw=1.2)

    t = [2 * math.pi * k / 120 for k in range(121)]
    # the table plane at z = 0, the part of it the photo covers, and a hide on it
    plane(0, tw, th, "#e9d8bd", "#8b5a2b", 0.55)
    poly([(2.4 * math.cos(a) * (1 + 0.12 * math.cos(3 * a) + 0.18 * math.cos(4 * a) ** 8),
           1.7 * math.sin(a) * (1 + 0.1 * math.sin(2 * a) + 0.18 * math.cos(4 * a) ** 8), 0) for a in t],
         facecolor="#5b3a1e", edgecolor="#3b2410", alpha=0.85, lw=1)
    # the lens at z = u, the sensor plane at z = u + v, both parallel to the table
    poly([(0.9 * math.cos(a), 0.9 * math.sin(a), u) for a in t], facecolor="#bfdbfe", edgecolor=BLUE,
         alpha=0.6, lw=1.4)
    plane(u + v, tw * m, th * m, "#374151", "black", 0.85)
    # rays from the corners of the covered area through the lens centre to the sensor's corners (inverted)
    for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        line((sx * tw, sy * th, 0), (-sx * tw * m, -sy * th * m, u + v), color=GREEN, lw=0.9, alpha=0.8)
    # one sensor pixel (enlarged) and its footprint on the table
    pw = 0.09
    px, py = 0.45 * tw * m, 0.3 * th * m
    pixel = [(px, py), (px + pw, py), (px + pw, py + pw), (px, py + pw)]
    poly([(a, b, u + v - 0.01) for a, b in pixel], facecolor=RED, edgecolor=RED)
    poly([(-a / m, -b / m, 0.01) for a, b in pixel], facecolor=RED, edgecolor=RED, alpha=0.9)
    for a, b in pixel:
        line((a, b, u + v), (-a / m, -b / m, 0), color=RED, lw=0.5, alpha=0.6)
    # the optical axis, and the distances along it, under the planes
    line((0, 0, -0.4), (0, 0, u + v + 0.8), color=GREY, lw=0.8, ls="--")
    below = -th - 2.2
    for z0, z1, label in ((0, u, "u   (lens to table)"), (u, u + v, "v")):
        line((0, below, z0), (0, below, z1), color=GREY, lw=1)
        for z in (z0, z1):
            line((0, below - 0.15, z), (0, below + 0.15, z), color=GREY, lw=1)
        text((0, below - 0.5, (z0 + z1) / 2), label, color=GREY, fontsize=11, ha="center", va="top")
    text((0, th + 1.5, 0), "table plane (hide)", color="#8b5a2b", fontsize=11, ha="center")
    text((0, -1.9, u), "lens, focal length f", color=BLUE, fontsize=11, ha="center", va="top")
    text((0, th * m + 1.0, u + v), "sensor, pixel pitch p", fontsize=11, ha="center")
    ax.set_xlim(-0.4, u + v + 0.8)
    ax.set_ylim(-tw, tw)
    ax.set_zlim(-th - 3.0, th + 0.6)
    ax.set_box_aspect((u + v + 1.2, 2 * tw, 2 * th + 3.6))
    ax.view_init(elev=16, azim=-62)
    ax.set_axis_off()
    # the 3D view, its empty margin cut away (the equations are in the text: Eqs. 1 and 3)
    import io  # noqa: PLC0415
    buffer = io.BytesIO()
    fig.savefig(buffer, format="png", dpi=200)
    plt.close(fig)
    buffer.seek(0)
    view = imread(buffer)
    ink = (view[..., :3] < 0.97).any(-1).nonzero()
    view = view[max(0, ink[0].min() - 20):ink[0].max() + 20, max(0, ink[1].min() - 20):ink[1].max() + 20]
    plt.imsave(out / "fig_geometry.png", view)


def fig_budget(out: Path) -> None:
    """Area bias of the pinhole scale p·d/f when the lens is focused at d (d measured to the lens)."""
    fig, ax = plt.subplots(figsize=(6, 3.4))
    distances = [500 + 25 * i for i in range(141)]
    for f, colour in ((4.0, GREY), (13.02, GREEN), (35.0, BLUE)):
        bias = [100 * ((u / (u - f)) ** 2 - 1) for u in distances]
        ax.plot(distances, bias, color=colour, label=f"f = {f:g} mm")
    ax.axvline(1500, color=GREY, lw=0.6, ls=":")
    ax.set_xlabel("distance lens to table u (mm)")
    ax.set_ylabel("area overestimate (%)")
    ax.set_title("Pinhole scale against a thin lens focused on the table", loc="left", fontsize=10)
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(out / "fig_thin_lens_bias.png")
    plt.close(fig)


def worst_failure(data: list[dict[str, str]]) -> dict[str, str] | None:
    """The photo whose area the network alone misses most, when it misses more than 5 %."""
    worst = min(data, key=lambda r: float(r["model_error_pct"]))
    return worst if float(worst["model_error_pct"]) < -5 else None


def second_summary(data: list[dict[str, str]]) -> None:
    """Section 6.3: the second generator's photos, the network alone and with the background check."""
    print(f"\nsecond generator, {len(data)} photos: median |error|, photos over 1 % and over 5 %; then the photos over 5 %")
    for label, col in (("network alone", "model_error_pct"), ("with the check", "error_pct")):
        e = [abs(float(r[col])) for r in data]
        print(f"  {label:15} median {statistics.median(e):.2f} %  over 1 %: {sum(x > 1 for x in e)}"
              f"  over 5 %: {sum(x > 5 for x in e)}")
    for r in data:
        if abs(float(r["model_error_pct"])) > 5 or abs(float(r["error_pct"])) > 5:
            print(f"  {r['photo']} {r['colour']:12} {r['surface']:8} on {r['background']:10}"
                  f" network alone {float(r['model_error_pct']):+7.2f} % (IoU {float(r['model_iou']):.4f}),"
                  f" with the check {float(r['error_pct']):+7.2f} % (IoU {float(r['iou']):.4f}), {r['taken']}")
    taken = [r["photo"] for r in data if r["taken"] == "background"]
    print(f"  the check took the background's object on {len(taken)}: {', '.join(taken)}")


def fig_failure(src: Path, out: Path, row: dict[str, str]) -> None:
    """The figure-ground inversion: (a) the network alone, (b) with the background check."""
    name = f"second_{row['photo']}"
    box = (0.0, 0.0, 1.0, 1.0)
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.4))
    for ax, file, title in ((axes[0], f"{name}_model_overlay.png", "(a) the network alone"),
                            (axes[1], f"{name}_overlay.png", "(b) with the background check")):
        img = imread(src / file)
        h, w = img.shape[:2]
        ax.imshow(img[int(box[1] * h):int(box[3] * h), int(box[0] * w):int(box[2] * w)])
        ax.set_title(title, loc="left", fontsize=10)
        ax.axis("off")
    fig.tight_layout()
    fig.savefig(out / "fig_failure.png", dpi=200)
    plt.close(fig)


def edge_box() -> tuple[float, float, float, float]:
    """About 50 x 30 px around the right end of the true outline of cowhide_large_1600_1, as fractions."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "experiment"))
    import synthetic  # noqa: PLC0415
    s = synthetic.photo(1600, 1200, "cowhide", "large", 1)
    ys, xs = (s.truth > 0).nonzero()
    row = int(ys.mean())
    x = int(xs[ys == row].max())
    return (x - 27) / 1600, (row - 15) / 1200, (x + 27) / 1600, (row + 15) / 1200


def crop(src: Path, dst: Path, box: tuple[float, float, float, float], width_in: float = 6) -> None:
    """The fractional box (x0, y0, x1, y1) of an image."""
    img = imread(src)
    h, w = img.shape[:2]
    x0, y0, x1, y1 = int(box[0] * w), int(box[1] * h), int(box[2] * w), int(box[3] * h)
    part = img[y0:y1, x0:x1]
    fig = plt.figure(figsize=(width_in, width_in * part.shape[0] / part.shape[1]))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(part, interpolation="nearest")
    ax.axis("off")
    fig.savefig(dst, dpi=200, bbox_inches=None)
    plt.close(fig)


def fig_contour_methods(src: Path, out: Path) -> None:
    """The cowhide's leg tip: the colour-threshold (OpenCV) outline against the IS-Net one, enlarged."""
    img = imread(src / "real_cowhide_stretched.jpg_overlay.png")
    part = img[0:260, 300:700]
    fig = plt.figure(figsize=(6.5, 6.5 * part.shape[0] / part.shape[1]))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(part, interpolation="nearest")
    ax.axis("off")
    fig.savefig(out / "fig_contour_methods.png", dpi=200, bbox_inches=None)
    plt.close(fig)


def fig_model_comparison(results: Path, out: Path) -> None:
    """The four candidate models on the cowhide, with their run times (the article's model_comparison folder)."""
    img = imread(results / "grid_cowhide_stretched.jpg")
    fig = plt.figure(figsize=(7, 7 * img.shape[0] / img.shape[1]))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(img)
    ax.axis("off")
    fig.savefig(out / "fig_model_comparison.png", dpi=150, bbox_inches=None)
    plt.close(fig)


def main() -> int:
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    shadow = rows(src / "synthetic.csv")
    plain = rows(src / "synthetic_noshadow.csv")
    summary(shadow, "with shadow")
    summary(plain, "without shadow")
    summary(shadow, "with shadow, the model alone", "model_")
    summary(plain, "without shadow, the model alone", "model_")
    print("\nreal photos (AI outline vs colour-threshold outline; the AI's area without the background check):")
    for r in rows(src / "real.csv"):
        size = int(r["width"]) * int(r["height"])
        print(f"  {r['photo']:22} {r['width']}x{r['height']}  colour - AI {float(r['difference_pct']):+.2f} %"
              f"  IoU {float(r['iou']):.4f}  AI {100 * float(r['ai_px']) / size:.1f} % of the photo,"
              f" model alone {100 * float(r['model_px']) / size:.1f} %")
    fig_errors(shadow, plain, out)
    fig_threshold(rows(src / "threshold.csv"), out)
    fig_geometry(out)
    comparison = Path(__file__).resolve().parents[2] / "model_comparison"
    fig_contour_methods(src, out)
    fig_model_comparison(comparison, out)
    fig_budget(out)
    crop(src / "cowhide_large_1600_1_overlay.png", out / "fig_synthetic_edge.png", edge_box(), 4.5)
    gross = [r for r in shadow + plain if abs(float(r["model_error_pct"])) > 5]
    print(f"\nfirst generator: {len(gross)} gross failures of the network alone")
    second = rows(src / "second.csv")
    second_summary(second)
    failure = worst_failure(second)
    if failure is None:
        print("no gross failure of the network alone in the second set: fig_failure.png not made")
    else:
        print(f"Fig. 12: {failure['photo']}")
        fig_failure(src, out, failure)
    crop(src / "real_cowhide_stretched.jpg_overlay.png", out / "fig_real_cowhide.png", (0.0, 0.0, 1.0, 1.0), 6)
    print(f"\nfigures in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
