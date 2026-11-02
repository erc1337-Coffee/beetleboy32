# Prints the CJK characters the fallback font carries: the common ones, not all ~21,000 (flash).
#  - GB2312 level 1: the 3,755 most used Chinese characters
#  - JIS X 0208 level 1: the 2,965 most used Japanese kanji (mostly overlapping)
#  - kana, CJK punctuation, fullwidth forms
# Used by scripts/fonts.sh.

chars = set()
for hi in range(0xB0, 0xD8):  # GB2312 level 1 rows
    for lo in range(0xA1, 0xFF):
        try:
            chars.add(bytes([hi, lo]).decode("gb2312"))
        except UnicodeDecodeError:
            pass
for code in range(0x889F, 0x9873):  # Shift_JIS range of JIS level 1 kanji
    try:
        c = code.to_bytes(2, "big").decode("shift_jis")
        if "一" <= c <= "鿿":
            chars.add(c)
    except UnicodeDecodeError:
        pass
for start, end in [(0x3000, 0x303F), (0x3040, 0x309F), (0x30A0, 0x30FF), (0xFF01, 0xFF5E)]:
    chars.update(chr(c) for c in range(start, end + 1))
print("".join(sorted(chars)), end="")
