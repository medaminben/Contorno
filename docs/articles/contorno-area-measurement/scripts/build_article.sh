#!/usr/bin/env bash
# The article as HTML (MathML) and PDF: pandoc, then Chromium's print to PDF.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
pandoc "$here/contorno-area-measurement.md" --standalone --math-method=mathml \
    --css article.css --include-before-body "$here/../nav.html" --include-before-body "$here/title-logo.html" --resource-path "$here" -o "$here/contorno-area-measurement.html"
browser="$(command -v chromium || command -v chromium-browser || command -v google-chrome)"
"$browser" --headless=new --no-sandbox --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$here/contorno-area-measurement.pdf" "file://$here/contorno-area-measurement.html" 2>/dev/null
echo "wrote $here/contorno-area-measurement.html and .pdf"
