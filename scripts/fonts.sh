#!/usr/bin/env bash
# Regenerates the LVGL fonts in src/fonts/ from assets/fonts/. Run it by hand when glyphs or sizes
# change; the output is committed, so normal builds don't need Node.
# Needs Node (npx downloads lv_font_conv on first run) and python3.
#
# The UI fonts carry ASCII (and the CJK the UI itself uses). Anything else a user can type (a display
# name, a bio) comes from a fallback chain, so it renders instead of showing blanks:
#   menlo / hei  ->  font_intl_16 (accents, Greek, Cyrillic, punctuation)  ->  font_cjk_16 (common CJK)
set -euo pipefail
cd "$(dirname "$0")/.."

CONV="npx --yes lv_font_conv@1.5.3"
F=assets/fonts
OUT=src/fonts
ASCII=0x20-0x7E
CJK_UI="集团存在证明名用户"  # every CJK character the UI uses; Hei.ttf has none, so they come from Noto Sans SC
# Latin-1, Latin Extended-A/B, Greek, Cyrillic, general punctuation, currency.
INTL=0xA0-0x24F,0x370-0x3FF,0x400-0x4FF,0x2010-0x205E,0x20A0-0x20BF
CJK_COMMON="$(python3 scripts/cjk_chars.py)"  # ~5,400 characters, ~0.7MB of flash

gen() {  # name size args...
  local name=$1 size=$2; shift 2
  $CONV --bpp 4 --size "$size" --no-compress --format lvgl --lv-include lvgl.h "$@" -o "$OUT/$name.c"
  echo "fonts: $OUT/$name.c"
}

# Points a generated font's fallback at another font (lv_font_conv always writes NULL).
fallback() {  # name fallback_name
  python3 - "$OUT/$1.c" "$2" <<'PY'
import sys
path, fb = sys.argv[1], sys.argv[2]
s = open(path).read()
end = s.index("#endif", s.index("#include")) + len("#endif")  # after the lvgl.h include block
s = s[:end] + "\n\nLV_FONT_DECLARE(%s);" % fb + s[end:]
s = s.replace(".fallback = NULL,", ".fallback = &%s," % fb, 1)
open(path, "w").write(s)
PY
}

mkdir -p "$OUT"
gen font_hei_18   18 --font $F/Hei.ttf -r $ASCII --font $F/NotoSansSC-subset.ttf --symbols "$CJK_UI"  # card header
gen font_menlo_16 16 --font $F/Menlo.ttf -r $ASCII  # display name
gen font_menlo_14 14 --font $F/Menlo.ttf -r $ASCII  # stats
gen font_intl_16  16 --font $F/NotoSansMono-Regular.ttf -r $INTL
gen font_cjk_16   16 --font $F/NotoSansSC-Regular.otf --symbols "$CJK_COMMON"
# Silhouettes (the craft bench, settings, USB power): lightning, zzz, wrench, hammer, beetle,
# from the monochrome Noto Emoji (OFL, NotoEmoji-OFL.txt), its line art filled in to solid shapes.
SHAPES=0x26A1,0x1F4A4,0x1F527,0x1F528,0x1FAB2
gen font_shapes_22 22 --font $F/NotoEmoji.ttf -r $SHAPES
gen font_shapes_40 40 --font $F/NotoEmoji.ttf -r $SHAPES
python3 scripts/fill_shapes.py --bold U+1F4A4 --close U+1F527,U+26A1 $OUT/font_shapes_22.c $OUT/font_shapes_40.c

for f in font_hei_18 font_menlo_16 font_menlo_14; do fallback $f font_intl_16; done
fallback font_intl_16 font_cjk_16
