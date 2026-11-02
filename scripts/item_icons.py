# Downloads every item icon a remilia.net API response mentions into assets/item_icons/, so the
# firmware ships them (scripts/assets.py embeds that folder) and hunts/claims don't wait on them.
#
#   python3 scripts/item_icons.py request.curl
#
# request.curl: the /api/beetle/cards request copied from the browser's devtools ("Copy as cURL"). Its cookie
# is only sent to remilia.net and never printed. Icons already in the folder aren't fetched again;
# run it again after new items show up. Icons are shrunk to ICON_SIZE (macOS sips) so they take
# less flash; the device fits them to the same size anyway.

import json
import os
import shlex
import struct
import subprocess
import sys
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "item_icons")
SITE = "https://www.remilia.net"
ICON_PREFIX = "/api/beetle/images/icons/"
ICON_SIZE = 80  # App::ITEM_ICON_SIZE
UA = ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/130.0.0.0 Safari/537.36")


def read_curl(path):
    """URL and the headers worth replaying (cookie, authorization) from a 'Copy as cURL' file."""
    args = shlex.split(open(path).read().replace("\\\n", " "))
    url, headers = None, {}
    i = 1
    while i < len(args):
        a = args[i]
        if a in ("-H", "--header") and i + 1 < len(args):
            name, _, value = args[i + 1].partition(":")
            if name.strip().lower() in ("cookie", "authorization"):
                headers[name.strip().lower()] = value.strip()
            i += 2
        elif a in ("-b", "--cookie") and i + 1 < len(args):
            headers["cookie"] = args[i + 1].lstrip("$")  # bash's $'...' quoting, as Chrome copies it
            i += 2
        elif a == "--url" and i + 1 < len(args):
            url = args[i + 1]
            i += 2
        elif a.startswith("http"):
            url = a
            i += 1
        else:
            i += 1
    if not url or not url.startswith(SITE):
        sys.exit("%s: no %s URL in it" % (path, SITE))
    return url, headers


def get(url, headers):
    req = urllib.request.Request(url, headers={**headers, "User-Agent": UA, "Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def shrink(path):
    """Fits the PNG in ICON_SIZE x ICON_SIZE, if it's bigger."""
    w, h = struct.unpack(">II", open(path, "rb").read()[16:24])
    if max(w, h) > ICON_SIZE:
        subprocess.run(["sips", "-Z", str(ICON_SIZE), path], check=True, capture_output=True)


def icon_paths(node, found):
    """Every PNG icon path anywhere in the response, as the device sees it (BeetleItem::icon)."""
    if isinstance(node, dict):
        for v in node.values():
            icon_paths(v, found)
    elif isinstance(node, list):
        for v in node:
            icon_paths(v, found)
    elif isinstance(node, str) and node.lower().endswith(".png") and "/images/icons/" in node:
        path = node.split("/images/icons/", 1)[1]
        if not path.startswith("shelves/"):  # profile decorations, not game items
            found.add(ICON_PREFIX + path)
    return found


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: python3 scripts/item_icons.py request.curl")
    url, headers = read_curl(sys.argv[1])
    try:
        data = json.loads(get(url, headers))
    except urllib.error.HTTPError as e:
        sys.exit("%s: HTTP %d (copy a fresh request: the cookie expires)" % (url.split("?")[0], e.code))
    paths = sorted(icon_paths(data, set()))
    print("%d icons mentioned" % len(paths))

    new = failed = 0
    for path in paths:
        dest = os.path.join(OUT_DIR, *path[len(ICON_PREFIX):].split("/"))
        if os.path.exists(dest):
            continue
        try:
            png = get(SITE + path, headers)
        except urllib.error.HTTPError as e:
            print("  %s: HTTP %d" % (path, e.code))
            failed += 1
            continue
        if not png.startswith(b"\x89PNG"):
            print("  %s: not a PNG" % path)
            failed += 1
            continue
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(png)
        shrink(dest)
        new += 1
    total = sum(len(f) for _, _, f in os.walk(OUT_DIR)) if os.path.isdir(OUT_DIR) else 0
    print("%d new, %d failed, %d in assets/item_icons/" % (new, failed, total))


if __name__ == "__main__":
    main()
