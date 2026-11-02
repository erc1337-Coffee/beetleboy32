// BeetleBoy32 docs: theme toggle, mobile menu, copy buttons, table of contents, search.
(function () {
  const RESULTS_MAX = 12;
  const COPIED_MS = 1500;
  const root = document.documentElement;

  // Theme
  const darkNow = () => root.dataset.theme === "dark" ||
    (!root.dataset.theme && matchMedia("(prefers-color-scheme: dark)").matches);
  document.querySelector(".theme-btn").addEventListener("click", () => {
    const next = darkNow() ? "light" : "dark";
    root.dataset.theme = next;
    try { localStorage.setItem("theme", next); } catch (e) {}
  });

  // Mobile menu
  const body = document.body;
  document.querySelector(".menu-btn").addEventListener("click", () => body.classList.toggle("nav-open"));
  document.querySelector(".scrim").addEventListener("click", () => body.classList.remove("nav-open"));

  // Copy buttons
  document.querySelectorAll(".code .copy").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const code = btn.closest(".code").querySelector("code").innerText;
      try { await navigator.clipboard.writeText(code); } catch (e) { return; }
      btn.textContent = "Copied";
      btn.classList.add("done");
      setTimeout(() => { btn.textContent = "Copy"; btn.classList.remove("done"); }, COPIED_MS);
    });
  });

  // Table of contents: highlight the section being read
  const tocLinks = [...document.querySelectorAll(".toc a")];
  const headings = tocLinks.map((a) => document.getElementById(decodeURIComponent(a.hash.slice(1)))).filter(Boolean);
  const markActive = () => {
    const line = parseInt(getComputedStyle(root).scrollPaddingTop) || 100;
    let current = headings[0];
    for (const h of headings) if (h.getBoundingClientRect().top <= line + 8) current = h;
    if (innerHeight + scrollY >= document.body.scrollHeight - 4) current = headings[headings.length - 1];
    tocLinks.forEach((a) => a.classList.toggle("active", current && a.hash === "#" + current.id));
  };
  if (headings.length) {
    addEventListener("scroll", markActive, { passive: true });
    markActive();
  }

  // Search
  const panel = document.querySelector(".search");
  const input = panel.querySelector("input");
  const list = panel.querySelector(".search-results");
  const index = window.SEARCH_INDEX || [];
  let selected = 0;

  const esc = (s) => s.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  const mark = (s, terms) => {
    let out = esc(s);
    for (const t of terms) out = out.replace(new RegExp("(" + t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ")", "gi"), "<mark>$1</mark>");
    return out;
  };
  const snippet = (text, term) => {
    const at = text.toLowerCase().indexOf(term);
    if (at < 40) return text.slice(0, 160);
    return "..." + text.slice(at - 40, at + 120);
  };

  const run = () => {
    const terms = input.value.toLowerCase().trim().split(/\s+/).filter(Boolean);
    if (!terms.length) { list.innerHTML = ""; return; }
    const hits = [];
    for (const e of index) {
      const title = e.title.toLowerCase(), text = e.text.toLowerCase(), page = e.page.toLowerCase();
      let score = 0;
      for (const t of terms) {
        if (title.includes(t)) score += title.startsWith(t) ? 6 : 4;
        else if (page.includes(t)) score += 2;
        else if (text.includes(t)) score += 1;
        else { score = 0; break; }
      }
      if (score) hits.push({ e, score });
    }
    hits.sort((a, b) => b.score - a.score);
    selected = 0;
    list.innerHTML = hits.length
      ? hits.slice(0, RESULTS_MAX).map(({ e }, i) =>
          `<li${i ? "" : ' class="selected"'}><a href="${e.url}">` +
          `<div class="sr-page">${esc(e.page)}</div><div class="sr-title">${mark(e.title, terms)}</div>` +
          (e.text ? `<div class="sr-text">${mark(snippet(e.text, terms[0]), terms)}</div>` : "") + "</a></li>").join("")
      : '<li class="search-empty">No results</li>';
  };

  const select = (i) => {
    const items = [...list.querySelectorAll("li:not(.search-empty)")];
    if (!items.length) return;
    selected = (i + items.length) % items.length;
    items.forEach((li, j) => li.classList.toggle("selected", j === selected));
    items[selected].scrollIntoView({ block: "nearest" });
  };

  const open = () => { panel.hidden = false; input.focus(); input.select(); run(); };
  const close = () => { panel.hidden = true; };

  document.querySelector(".search-btn").addEventListener("click", open);
  panel.addEventListener("click", (ev) => { if (ev.target === panel) close(); });
  list.addEventListener("click", (ev) => { if (ev.target.closest("a")) close(); });
  input.addEventListener("input", run);
  input.addEventListener("keydown", (ev) => {
    if (ev.key === "ArrowDown") { ev.preventDefault(); select(selected + 1); }
    else if (ev.key === "ArrowUp") { ev.preventDefault(); select(selected - 1); }
    else if (ev.key === "Enter") {
      const a = list.querySelector("li.selected a");
      if (a) { close(); location.href = a.href; }
    }
  });
  addEventListener("keydown", (ev) => {
    if ((ev.metaKey || ev.ctrlKey) && ev.key.toLowerCase() === "k") { ev.preventDefault(); panel.hidden ? open() : close(); }
    else if (ev.key === "/" && panel.hidden && !/input|textarea/i.test(document.activeElement.tagName)) { ev.preventDefault(); open(); }
    else if (ev.key === "Escape") { close(); body.classList.remove("nav-open"); }
  });
})();
