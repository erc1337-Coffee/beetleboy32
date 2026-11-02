#!/usr/bin/env bash
# Turns square images into menu tile icons: 44x44, rounded corners baked in (no runtime cost).
#   <source_dir>/pfp-001.png -> <out_dir>/menu_pfp_001.png
# Keep out_dir outside assets/ (everything there gets built in): move the icons you use into
# assets/, then reference them as img_<name> in src/app/menu_config.h.
# Usage: scripts/menu_icons.sh source_dir out_dir
set -euo pipefail
cd "$(dirname "$0")/.."
if [ $# -ne 2 ]; then
  echo "usage: scripts/menu_icons.sh source_dir out_dir" >&2
  exit 1
fi
src=$1
out=$2
size=44 radius=10

mkdir -p "$out"
mask=$(mktemp -t mask).png
magick -size ${size}x${size} xc:none -fill white -draw "roundrectangle 0,0 $((size - 1)),$((size - 1)) $radius,$radius" "$mask"
n=0
for f in "$src"/*.png; do
  name=$(basename "$f" .png | tr 'A-Z-' 'a-z_' | tr -cd 'a-z0-9_')
  magick "$f" -resize ${size}x${size}^ -gravity center -extent ${size}x${size} "$mask" -compose DstIn -composite "PNG32:$out/menu_$name.png"
  n=$((n + 1))
done
rm -f "$mask"
echo "menu icons: $n x $out/menu_*.png"
