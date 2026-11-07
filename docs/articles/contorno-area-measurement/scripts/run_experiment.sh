#!/usr/bin/env bash
# The article's measurements, figures and analytical numbers, from its Python scripts:
# exports the IS-Net model (once), runs the experiment on synthetic photos and on
# docs/samples/leather, then draws the figures.
#
#   run_experiment.sh [work dir]        (default: <repo>/build/article)
#
# Needs python3 with venv and network access on the first run; the packages of
# requirements.txt are installed into <work dir>/venv.
set -euo pipefail
scripts="$(cd "$(dirname "$0")" && pwd)"
article="$(cd "$scripts/.." && pwd)"
repo="$(cd "$article/../../.." && pwd)"
work="${1:-$repo/build/article}"
mkdir -p "$work"
venv="$work/venv"
if ! "$venv/bin/python" -c "import cv2, matplotlib, numpy, PIL, scipy, torch" 2>/dev/null; then
    python3 -m venv "$venv"
    "$venv/bin/python" -m pip install -q -r "$scripts/requirements.txt"
fi
py="$venv/bin/python"
model="$work/model/isnet-general-use.pt"
[ -f "$model" ] || "$py" "$scripts/model/export_isnet.py" --out "$model"
"$py" "$scripts/experiment/measure.py" "$model" "$repo/docs/samples/leather" "$work"
"$py" "$scripts/figures/figures.py" "$work" "$article/figures"
"$py" "$scripts/figures/fig_background.py" "$work" "$article/figures"
"$py" "$scripts/figures/fig_measured.py" "$work" "$article/figures"
"$py" "$scripts/figures/budget.py" "$work"
