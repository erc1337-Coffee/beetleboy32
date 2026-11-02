#!/usr/bin/env bash
# Renders assets/source/about_plaque.svg as two 180px-wide layers drawn on top of each other:
#   assets/about_plaque.png        the ornate frame, gray here; the app tints it with the theme hue
#   assets/about_plaque_inner.png  the center panel, stays neutral gray
# ImageMagick's SVG renderer ignores gradient fills, so each shape is rendered as a mask and filled
# with a gradient, plus an outline taken from the mask's edge.
set -euo pipefail
cd "$(dirname "$0")/.."
W=180 H=99
T=$(mktemp -d)
python3 -I - assets/source/about_plaque.svg "$T" <<'PY'
import re, sys
src, tmp = sys.argv[1], sys.argv[2]
paths = re.findall(r'<path d="([^"]+)"', open(src).read())
head = '<svg width="255" height="140" viewBox="0 0 255 140" xmlns="http://www.w3.org/2000/svg">'
for name, d in (("outer", paths[0]), ("inner", paths[1])):
    open(f"{tmp}/{name}.svg", "w").write(f'{head}<path d="{d}" fill="white"/></svg>')
PY
layer() {  # mask_svg gradient edge_color out
  magick -background none -density 300 "$1" -resize ${W}x${H}! -alpha extract "$T/m.png"
  magick -size ${W}x${H} "gradient:$2" "$T/m.png" -alpha off -compose CopyOpacity -composite "$T/fill.png"
  magick "$T/m.png" -morphology EdgeIn Diamond:1 "$T/e.png"
  magick -size ${W}x${H} "xc:$3" "$T/e.png" -alpha off -compose CopyOpacity -composite "$T/edge.png"
  magick "$T/fill.png" "$T/edge.png" -compose Over -composite "$4"
}
layer "$T/outer.svg" '#f6f6f6-#b2b2b2' '#8c8c8c' "$T/o.png"
layer "$T/inner.svg" '#efefef-#a4a4a4' '#828282' "$T/i.png"
magick "$T/o.png" PNG32:assets/about_plaque.png
magick "$T/i.png" PNG32:assets/about_plaque_inner.png
rm -rf "$T"
echo "plaque: assets/about_plaque.png + assets/about_plaque_inner.png"
