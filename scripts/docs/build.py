#!/usr/bin/env python3
"""Builds the docs website into website/ from README.md, docs/*.md and ROADMAP.md.

The markdown files stay the source (GitHub shows them as they are); this renders them as a site.
Needs: pip install markdown pygments
"""
import html
import json
import posixpath
import re
import shutil
import struct
from pathlib import Path

try:
    import markdown
    from pygments import highlight
    from pygments.formatters import HtmlFormatter
    from pygments.lexers import TextLexer, get_lexer_by_name
    from pygments.util import ClassNotFound
except ImportError:
    raise SystemExit("The docs need: pip install markdown pygments")

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "website"

SITE_NAME = "BeetleBoy32"
TAGLINE = "A pocket device for the remilia.net beetle game."
BRANCH = "master"
HERO_IMAGE = "docs/media/case.webp"
# GitHub's uploaded videos don't play on other sites: the README's video link plays this copy instead.
VIDEO, VIDEO_POSTER = "docs/media/trailer.mp4", "docs/media/trailer.jpg"
SCREEN_RATIO = (7, 6)  # images at the screen's 280x240 ratio get the screen's frame, pixels kept sharp

# Sidebar: (group, [(slug, source file, sidebar label, icon)]). Order here is the reading order.
NAV = [
    ("Get started", [
        ("index", "README.md", "Introduction", "home"),
        ("setup-and-login", "docs/setup-and-login.md", "Setup and login", "key"),
        ("hardware", "docs/hardware.md", "Hardware", "chip"),
    ]),
    ("Develop", [
        ("simulator", "docs/simulator.md", "Simulator", "monitor"),
        ("code", "docs/code.md", "Code", "code"),
    ]),
    ("Project", [
        ("roadmap", "ROADMAP.md", "Roadmap", "map"),
    ]),
]

# Copied into the site as they are: (source, destination in website/).
STATIC = [
    ("scripts/docs/style.css", "assets/style.css"),
    ("scripts/docs/app.js", "assets/app.js"),
    ("assets/fonts/InterVariable.woff2", "assets/fonts/InterVariable.woff2"),
    ("assets/fonts/RemiliaMincho-Regular.otf", "assets/fonts/RemiliaMincho.otf"),
    ("assets/fonts/RemiliaWestern.otf", "assets/fonts/RemiliaWestern.otf"),
    ("assets/icon_beetle.png", "assets/beetle.png"),
]

CODE_STYLE = "github-dark"  # Pygments style for code blocks (dark in both themes)
CODE_LABELS = {"sh": "Terminal", "bash": "Terminal"}

ICONS = {  # 24x24 stroke icons
    "home": '<path d="M3 10.5 12 3l9 7.5"/><path d="M5 9.5V20h5v-6h4v6h5V9.5"/>',
    "key": '<circle cx="8" cy="15" r="4"/><path d="m10.8 12.2 9.2-9.2M17 6l3 3M14.5 8.5l2.5 2.5"/>',
    "chip": '<rect x="6" y="6" width="12" height="12" rx="2"/><rect x="9.5" y="9.5" width="5" height="5"/>'
            '<path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4"/>',
    "monitor": '<rect x="2.5" y="4" width="19" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
    "code": '<path d="m8 7-5 5 5 5M16 7l5 5-5 5M14 4l-4 16"/>',
    "map": '<path d="M9 4 3 6.5v14L9 18l6 2.5 6-2.5v-14L15 6.5z"/><path d="M9 4v14M15 6.5v14"/>',
    "note": '<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',
    "tip": '<path d="M9 18h6M10 21.5h4"/><path d="M12 2.5a6.5 6.5 0 0 0-4 11.6c.6.5 1 1.2 1 2V16h6v-.1c0-.8.4-1.5 1-2A6.5 6.5 0 0 0 12 2.5z"/>',
    "important": '<path d="M4 4h16v12H8l-4 4z"/><path d="M12 7.5v4M12 13.5v.5"/>',
    "warning": '<path d="M12 3 2 20.5h20z"/><path d="M12 10v5M12 17.5v.5"/>',
    "caution": '<path d="M8 2.5h8L21.5 8v8L16 21.5H8L2.5 16V8z"/><path d="M12 7.5v5M12 15.5v.5"/>',
    "arrow-left": '<path d="M19 12H5M11 6l-6 6 6 6"/>',
    "arrow-right": '<path d="M5 12h14M13 6l6 6-6 6"/>',
    "github": '<path d="M9 19c-4.3 1.4-4.3-2.5-6-3m12 5v-3.5c0-1 .1-1.4-.5-2 2.8-.3 5.5-1.4 5.5-6a4.6 4.6 0 0 0-1.3-3.2 '
              '4.2 4.2 0 0 0-.1-3.2s-1.1-.3-3.5 1.3a12.3 12.3 0 0 0-6.2 0C6.5 2.8 5.4 3.1 5.4 3.1a4.2 4.2 0 0 0-.1 3.2A4.6 '
              '4.6 0 0 0 4 9.5c0 4.6 2.7 5.7 5.5 6-.6.6-.6 1.2-.5 2V21"/>',
}


def icon(name, cls="icon"):
    return (f'<svg class="{cls}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" '
            f'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{ICONS[name]}</svg>')


def repo_url():
    for line in (ROOT / "project.env").read_text().splitlines():
        if line.startswith("REPO_URL="):
            return line.split("=", 1)[1].strip()
    raise SystemExit("project.env has no REPO_URL")


PAGES = [(group, *page) for group, pages in NAV for page in pages]
SLUG_OF = {src: slug for _, slug, src, _, _ in PAGES}
REPO = repo_url()


# ---------- markdown, with the few GitHub extras the docs use ----------

class Stash:
    """Holds pre-rendered HTML while markdown runs, as placeholder paragraphs."""

    def __init__(self):
        self.items = []

    def put(self, fragment):
        self.items.append(fragment)
        return f"\n\nSTASH{len(self.items) - 1}X\n\n"

    def restore(self, text):
        return re.sub(r"<p>STASH(\d+)X</p>", lambda m: self.items[int(m.group(1))], text)


def render_code(lang, code):
    try:
        lexer = get_lexer_by_name(lang) if lang else TextLexer()
    except ClassNotFound:
        lexer = TextLexer()
    body = highlight(code, lexer, HtmlFormatter(nowrap=True))
    label = CODE_LABELS.get(lang, lang)
    return (f'<div class="code"><div class="code-bar"><span>{html.escape(label)}</span>'
            f'<button class="copy" type="button" aria-label="Copy">Copy</button></div>'
            f'<pre><code>{body}</code></pre></div>')


def render_alert(kind, inner, src):
    kind = kind.lower()
    body = to_html(inner, src, top=False)[0]
    return f'<div class="callout callout-{kind}">{icon(kind)}<div class="callout-body">{body}</div></div>'


def render_cards(items, src):
    cards = []
    for title, href, desc in items:
        target = SLUG_OF.get(resolve(href, src))
        ico = next((i for _, s, _, _, i in PAGES if s == target), "note")
        cards.append(f'<a class="card" href="{html.escape(href)}">{icon(ico, "card-icon")}'
                     f'<div class="card-title">{html.escape(title)}</div>'
                     f'<div class="card-desc">{inline(desc[:1].upper() + desc[1:])}</div></a>')
    return f'<div class="cards">{"".join(cards)}</div>'


def render_gallery(table):
    imgs = re.findall(r'<img src="([^"]+)"[^>]*alt="([^"]*)"', table)
    captions = re.findall(r"<td>((?:(?!<img)(?!</td>).)*)</td>", table, re.S)
    figs = "".join(f'<figure class="figure screen"><img src="{src}" alt="{html.escape(alt)}" loading="lazy">'
                   f'<figcaption>{cap.strip()}</figcaption></figure>'
                   for (src, alt), cap in zip(imgs, captions))
    return f'<div class="grid">{figs}</div>'


def inline(text):
    return markdown.markdown(text).removeprefix("<p>").removesuffix("</p>")


def preprocess(text, src, stash):
    # The README's badge rows are for GitHub: the site has these links already, and loads nothing from
    # other sites.
    text = re.sub(r'<p align="center">(?:(?!</p>).)*img\.shields\.io.*?</p>\n?', "", text, flags=re.S)
    # Fenced code first, so nothing below touches it.
    text = re.sub(r"^```(\w*)\n(.*?)^```\s*$",
                  lambda m: stash.put(render_code(m.group(1), m.group(2))), text, flags=re.S | re.M)

    # GitHub alerts: > [!WARNING] ...
    def alert(m):
        inner = re.sub(r"^> ?", "", m.group(2), flags=re.M)
        return stash.put(render_alert(m.group(1), inner, src))
    text = re.sub(r"^> \[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\n((?:>.*\n?)*)", alert, text, flags=re.M)

    # A list made only of "[Title](page.md): description" lines becomes cards.
    def cards(m):
        items = re.findall(r"^- \[([^\]]+)\]\(([^)]+)\): (.+)$", m.group(0), re.M)
        return stash.put(render_cards(items, src))
    text = re.sub(r"(?:^- \[[^\]]+\]\([^)]+\.md\): .+\n?)+", cards, text, flags=re.M)

    # The README's GIF table becomes a gallery; a bare video link becomes a player.
    text = re.sub(r"<table>.*?</table>", lambda m: stash.put(render_gallery(m.group(0))), text, flags=re.S)
    text = re.sub(r"^https://github\.com/user-attachments/assets/\S+$",
                  lambda m: stash.put(f'<div class="video"><video src="/{VIDEO}" poster="/{VIDEO_POSTER}" controls '
                                      f'playsinline preload="none"></video></div>'), text, flags=re.M)

    # GitHub nests lists with 2 spaces, Python-Markdown wants 4.
    text = re.sub(r"^( +)", lambda m: m.group(1) * 2, text, flags=re.M)
    # GitHub starts a list right after a paragraph line, Python-Markdown wants a blank line first.
    return re.sub(r"^((?!\s*(?:[-*]|\d+\.) ).*\S.*)\n(?=\s*(?:[-*]|\d+\.) )", r"\1\n\n", text, flags=re.M)


def resolve(href, src):
    """A link in src, as a path from the repo root."""
    return posixpath.normpath(posixpath.join(posixpath.dirname(src), href))


def link(href, src):
    if re.match(r"^(https?:|mailto:|#)", href):
        return href
    path, _, frag = href.partition("#")
    target = resolve(path, src)
    if target in SLUG_OF:
        return f"{SLUG_OF[target]}.html" + (f"#{frag}" if frag else "")
    return f"{REPO}/blob/{BRANCH}/{target}" + (f"#{frag}" if frag else "")


def image_size(path):
    """(width, height) of a PNG or GIF, else None."""
    head = path.read_bytes()[:32] if path.suffix in (".png", ".gif") else b""
    if head[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", head[16:24])
    if head[:3] == b"GIF":
        return struct.unpack("<HH", head[6:10])
    return None


def figure(img, src):
    """A markdown image as a figure: its title becomes the caption."""
    attrs = dict(re.findall(r'(\w+)="([^"]*)"', img))
    path = ROOT / resolve(attrs["src"], src)
    size = image_size(path) if path.exists() else None
    screen = size and size[0] * SCREEN_RATIO[1] == size[1] * SCREEN_RATIO[0]
    cls = "figure screen" if screen else "figure"
    dims = f' width="{size[0]}" height="{size[1]}"' if size else ""
    caption = f"<figcaption>{attrs['title']}</figcaption>" if attrs.get("title") else ""
    return (f'<figure class="{cls}"><img src="{attrs["src"]}" alt="{attrs.get("alt", "")}"{dims} loading="lazy">'
            f'{caption}</figure>')


def figures(body, src):
    """Paragraphs made only of images: one becomes a figure, several a grid."""
    def para(m):
        imgs = re.findall(r"<img [^>]*?/?>", m.group(1))
        if not imgs or re.sub(r"<img [^>]*?/?>|\s", "", m.group(1)):
            return m.group(0)
        figs = "".join(figure(i, src) for i in imgs)
        return figs if len(imgs) == 1 else f'<div class="grid">{figs}</div>'
    return re.sub(r"<p>(.*?)</p>", para, body, flags=re.S)


def media(body, src, copies):
    """Local images and videos: served at their path from the repo root, and copied there."""
    def one(m):
        attr, value = m.group(1), m.group(2)
        if re.match(r"^(https?:|data:)", value):
            return m.group(0)
        path = value[1:] if value.startswith("/") else resolve(value, src)
        copies.add(path)
        return f'{attr}="{path}"'
    return re.sub(r'\b(src|poster)="([^"]+)"', one, body)


def postprocess(body, src, copies):
    body = figures(body, src)
    body = media(body, src, copies)
    body = re.sub(r'href="([^"]+)"', lambda m: f'href="{link(html.unescape(m.group(1)), src)}"', body)
    body = re.sub(r'href="(https?://[^"]+)"', r'href="\1" target="_blank" rel="noopener"', body)
    body = re.sub(r"<li>\s*(<p>)?\[( |x)\] ",
                  lambda m: f'<li class="task{" done" if m.group(2) == "x" else ""}">{m.group(1) or ""}'
                            f'<span class="check" aria-hidden="true"></span>', body)
    return re.sub(r"<table>(.*?)</table>", r'<div class="table"><table>\1</table></div>', body, flags=re.S)


def to_html(text, src, top=True, copies=None):
    stash = Stash()
    md = markdown.Markdown(extensions=["tables", "sane_lists", "toc"],
                           extension_configs={"toc": {"permalink": "#", "permalink_class": "anchor"}})
    body = stash.restore(md.convert(preprocess(text, src, stash)))
    return (postprocess(body, src, copies if copies is not None else set()) if top else body), md.toc_tokens


# ---------- pages ----------

def split_title(text):
    m = re.match(r"\s*# (.+)\n", text)
    return (m.group(1).strip(), text[m.end():]) if m else ("", text)


def descriptions():
    """Page subtitles, from the README's Docs list."""
    text = (ROOT / "README.md").read_text()
    return {SLUG_OF.get(resolve(href, "README.md")): desc
            for href, desc in re.findall(r"^- \[[^\]]+\]\(([^)]+)\): (.+)$", text, re.M)}


def sidebar(current):
    out = []
    active = ' class="active" aria-current="page"'
    for group, pages in NAV:
        items = "".join(
            f'<li><a href="{slug}.html"{active if slug == current else ""}>'
            f'{icon(ico)}<span>{html.escape(label)}</span></a></li>'
            for slug, _, label, ico in pages)
        out.append(f'<div class="nav-group"><div class="nav-title">{html.escape(group)}</div><ul>{items}</ul></div>')
    return "".join(out)


def toc(tokens):
    def items(tokens):
        return "".join(f'<li><a href="#{t["id"]}" data-level="{t["level"]}">{t["name"]}</a></li>'
                       + items(t["children"]) for t in tokens)
    return f'<div class="toc-title">On this page</div><ul>{items(tokens)}</ul>' if tokens else ""


def pager(i):
    def card(j, dir_):
        group, slug, _, label, _ = PAGES[j]
        arrow = icon("arrow-left" if dir_ == "prev" else "arrow-right")
        return (f'<a class="pager-{dir_}" href="{slug}.html"><span class="pager-hint">'
                f'{"Previous" if dir_ == "prev" else "Next"}</span><span class="pager-label">'
                f'{arrow if dir_ == "prev" else ""}{html.escape(label)}{arrow if dir_ == "next" else ""}</span></a>')
    prev = card(i - 1, "prev") if i > 0 else "<span></span>"
    nxt = card(i + 1, "next") if i < len(PAGES) - 1 else "<span></span>"
    return f'<nav class="pager">{prev}{nxt}</nav>'


def logo_text():
    return f'BeetleBoy<span class="num">32</span>'


def hero():
    first = PAGES[1]
    return (f'<header class="hero"><div class="hero-text">'
            f'<h1 class="hero-title">{logo_text()}</h1><p class="lead">{html.escape(TAGLINE)}</p>'
            f'<div class="hero-actions"><a class="btn btn-primary" href="{first[1]}.html">Get started{icon("arrow-right")}</a>'
            f'<a class="btn" href="{REPO}" target="_blank" rel="noopener">{icon("github")}GitHub</a></div></div>'
            f'<img class="hero-image" src="{HERO_IMAGE}" alt="BeetleBoy32 in its case"></header>')


def search_entries(slug, label, body):
    """One entry per section: the page intro, then each h2/h3."""
    entries = []
    parts = re.split(r'(<h[23] id="[^"]+">.*?</h[23]>)', body, flags=re.S)
    heading, anchor = label, ""
    for part in parts:
        m = re.match(r'<h[23] id="([^"]+)">(.*?)</h[23]>', part, re.S)
        if m:
            anchor = m.group(1)
            heading = html.unescape(re.sub(r"<[^>]+>|#$", "", re.sub(r'<a class="anchor".*?</a>', "", m.group(2))))
            continue
        text = html.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", part))).strip()
        if text or anchor:
            entries.append({"page": label, "title": heading.strip(), "url": f"{slug}.html" + (f"#{anchor}" if anchor else ""),
                            "text": text[:600]})
    return entries


def build():
    template = (HERE / "page.html").read_text()
    subtitles = descriptions()
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "assets" / "fonts").mkdir(parents=True)

    search = []
    copies = {HERO_IMAGE}
    for i, (group, slug, src, label, _) in enumerate(PAGES):
        title, text = split_title((ROOT / src).read_text())
        body, tokens = to_html(text, src, copies=copies)
        desc = subtitles.get(slug, "")
        desc = desc[:1].upper() + desc[1:]
        if slug == "index":
            head = hero()
            page_title = f"{SITE_NAME}: {TAGLINE.rstrip('.')}"
            meta_desc = TAGLINE
        else:
            head = (f'<header class="page-head"><div class="eyebrow">{html.escape(group)}</div>'
                    f'<h1>{html.escape(title)}</h1>' + (f'<p class="lead">{inline(desc)}</p>' if desc else "")
                    + '</header>')
            page_title = f"{title} · {SITE_NAME}"
            meta_desc = desc or TAGLINE
        search += search_entries(slug, label, body)
        page = (template
                .replace("{{title}}", html.escape(page_title))
                .replace("{{description}}", html.escape(re.sub(r"<[^>]+>", "", meta_desc)))
                .replace("{{logo}}", logo_text())
                .replace("{{repo}}", REPO)
                .replace("{{sidebar}}", sidebar(slug))
                .replace("{{head}}", head)
                .replace("{{body}}", body)
                .replace("{{pager}}", pager(i))
                .replace("{{toc}}", toc(tokens))
                .replace("{{page_class}}", "page-home" if slug == "index" else ""))
        (OUT / f"{slug}.html").write_text(page)

    for path in sorted(copies):
        (OUT / path).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(ROOT / path, OUT / path)
    for src, dest in STATIC:
        shutil.copy(ROOT / src, OUT / dest)
    code_css = HtmlFormatter(style=CODE_STYLE).get_style_defs(".code pre")
    (OUT / "assets" / "code.css").write_text(code_css)
    (OUT / "assets" / "search.js").write_text(f"window.SEARCH_INDEX = {json.dumps(search)};\n")
    (OUT / ".nojekyll").write_text("")
    print(f"Built {len(PAGES)} pages into {OUT.relative_to(ROOT)}/")


if __name__ == "__main__":
    build()
