#!/usr/bin/env bash
# This paper as HTML (MathML) and PDF: pandoc, then Chromium's print to PDF.
# The type and the wordmark are those of the measurement article.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
article="$(cd "$here/../contorno-area-measurement" && pwd)"
pandoc "$here/contorno-indicator.md" --standalone --math-method=mathml \
    --css "$article/article.css" --css "$here/paper.css" \
    --include-before-body "$here/title-logo.html" \
    --resource-path "$here:$article" -o "$here/contorno-indicator.html"
# Pandoc writes the stylesheet name as given. Point the HTML at the shared file.
sed -i 's|href="[^"]*article.css"|href="../contorno-area-measurement/article.css"|' \
    "$here/contorno-indicator.html"
sed -i 's|href="[^"]*paper.css"|href="paper.css"|' \
    "$here/contorno-indicator.html"
browser="$(command -v chromium || command -v chromium-browser || command -v google-chrome)"
"$browser" --headless=new --no-sandbox --disable-gpu --no-pdf-header-footer \
    --print-to-pdf="$here/contorno-indicator.pdf" "file://$here/contorno-indicator.html" 2>/dev/null
echo "wrote $here/contorno-indicator.html and .pdf"
