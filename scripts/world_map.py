# Renders assets/world_map.png (the time screen's map) from Natural Earth's 1:110m land polygons
# (public domain, assets/source/ne_110m_land.geojson, from github.com/nvkelso/natural-earth-vector).
# Equirectangular: x = longitude, y = latitude, the whole world. Needs ImageMagick (`magick`).
# Usage: python3 scripts/world_map.py

import json
import os
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "source", "ne_110m_land.geojson")
OUT = os.path.join(ROOT, "assets", "world_map.png")
W, H = 200, 100
LAND = "#adb2bd"  # soft grey; the sea is left transparent (the screen shades it at night)
SCALE = 10  # SVG units per pixel: coordinates keep their precision, magick scales down


def point(lon, lat):
    return "%.1f,%.1f" % ((lon + 180) / 360 * W * SCALE, (90 - lat) / 180 * H * SCALE)


def main():
    shapes = json.load(open(SRC))["features"]
    paths = []
    for f in shapes:
        g = f["geometry"]
        polygons = [g["coordinates"]] if g["type"] == "Polygon" else g["coordinates"]
        for polygon in polygons:
            d = " ".join("M" + " L".join(point(lon, lat) for lon, lat in ring) + " Z" for ring in polygon)
            paths.append('<path d="%s"/>' % d)
    svg = (
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
        % (W * SCALE, H * SCALE, W * SCALE, H * SCALE)
        + '<g fill="%s" fill-rule="evenodd">%s</g></svg>' % (LAND, "".join(paths)))
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as tmp:
        tmp.write(svg)
    try:
        subprocess.run(["magick", "-background", "none", tmp.name, "-resize", "%dx%d!" % (W, H),
                        "-depth", "8", "PNG32:" + OUT], check=True)
    finally:
        os.remove(tmp.name)
    print("world_map: %s (%dx%d)" % (OUT, W, H))


if __name__ == "__main__":
    main()
