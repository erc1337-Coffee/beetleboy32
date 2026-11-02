#!/usr/bin/env bash
# Extracts every STEP-th frame of a GIF into assets/<name>_NN.png, resized to SIZE px.
# The asset build then exposes them as img_<name>_frames[] (see scripts/assets.py).
# Usage: scripts/gif_frames.sh assets/source/loading.gif loading 112 2
set -euo pipefail
cd "$(dirname "$0")/.."
gif=$1 name=$2 size=$3 step=$4
tmp=$(mktemp -d)
magick "$gif" -coalesce -resize "${size}x${size}" "PNG32:$tmp/f_%03d.png"
rm -f assets/"${name}"_[0-9][0-9].png
i=0
for f in $(ls "$tmp"/f_*.png | awk "NR % $step == 1"); do
  mv "$f" "$(printf 'assets/%s_%02d.png' "$name" "$i")"
  i=$((i + 1))
done
rm -rf "$tmp"
echo "frames: $i x assets/${name}_NN.png"
