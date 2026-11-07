#!/usr/bin/env python3
"""Render the user manual: docs/manual/content/<lang>/*.md -> docs/manual/<lang>/*.html.

Standard library only. Markdown subset:
## / ### headings (optional {#anchor}), paragraphs, - and 1. lists, | tables,
::: note / ::: warning / ::: formula / ::: step blocks, ![caption](shot:name)
for docs/manual/assets/shots/<lang>/<name>.png, **bold**, *italic*,
[text](page#anchor), and `label` for a label of the application, which
--check finds in the language's string table
(applications/Indicator/strings/indicator_<lang>.cpp).

    build_manual.py          write the pages
    build_manual.py --check  report missing pages, broken links, labels the
                             application does not show and pages out of date
"""
from __future__ import annotations

import argparse
import html
import re
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
# the order of the flags in the Set Up dialog
LANGS = ("en", "de", "es", "it", "fr", "nl", "ar")
RTL = {"ar"}
PAGES = (
    "index", "getting-started", "set-up", "measure", "camera", "inspections", "messages", "reference",
)
LANG_NAMES = {"en": "English", "de": "Deutsch", "es": "Español", "it": "Italiano", "fr": "Français",
              "nl": "Nederlands", "ar": "العربية"}
UI = {
    "en": {"contents": "Contents", "prev": "Previous", "next": "Next", "manual": "User manual",
           "language": "Language"},
    "de": {"contents": "Inhalt", "prev": "Zurück", "next": "Weiter", "manual": "Benutzerhandbuch",
           "language": "Sprache"},
    "es": {"contents": "Contenido", "prev": "Anterior", "next": "Siguiente", "manual": "Manual de usuario",
           "language": "Idioma"},
    "it": {"contents": "Indice", "prev": "Precedente", "next": "Successivo", "manual": "Manuale utente",
           "language": "Lingua"},
    "fr": {"contents": "Sommaire", "prev": "Précédent", "next": "Suivant", "manual": "Manuel d'utilisation",
           "language": "Langue"},
    "nl": {"contents": "Inhoud", "prev": "Vorige", "next": "Volgende", "manual": "Handleiding",
           "language": "Taal"},
    "ar": {"contents": "المحتويات", "prev": "السابق", "next": "التالي", "manual": "دليل الاستخدام",
           "language": "اللغة"},
}


# ---------------------------------------------------------------- front matter

def parse_front_matter(text: str) -> tuple[dict[str, str], str]:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return {}, text
    meta: dict[str, str] = {}
    for index, line in enumerate(lines[1:], start=1):
        if line.strip() == "---":
            return meta, "\n".join(lines[index + 1:])
        key, _, value = line.partition(":")
        meta[key.strip()] = value.strip()
    return meta, ""


# ---------------------------------------------------------------- inline

_INLINE = re.compile(
    r"`(?P<ui>[^`]+)`"
    r"|\*\*(?P<b>.+?)\*\*"
    r"|\*(?P<i>[^*]+)\*"
    r"|\[(?P<lt>[^\]]+)\]\((?P<lh>[^)\s]+)\)"
)


def _href(target: str) -> str:
    page, _, anchor = target.partition("#")
    out = f"{page}.html" if page else ""
    return out + (f"#{anchor}" if anchor else "")


def inline(text: str) -> str:
    out: list[str] = []
    pos = 0
    for m in _INLINE.finditer(text):
        out.append(html.escape(text[pos:m.start()], quote=False))
        if m.group("ui") is not None:
            out.append(f'<span class="ui">{html.escape(m.group("ui"), quote=False)}</span>')
        elif m.group("b") is not None:
            out.append(f"<strong>{inline(m.group('b'))}</strong>")
        elif m.group("i") is not None:
            out.append(f"<em>{inline(m.group('i'))}</em>")
        else:
            out.append(f'<a href="{html.escape(_href(m.group("lh")))}">{inline(m.group("lt"))}</a>')
        pos = m.end()
    out.append(html.escape(text[pos:], quote=False))
    return "".join(out)


# ---------------------------------------------------------------- blocks

_HEADING = re.compile(r"^(#{2,3})\s+(.*?)(?:\s+\{#([A-Za-z0-9_-]+)\})?\s*$")
_SHOT = re.compile(r"^!\[(.*)\]\(shot:([A-Za-z0-9_-]+)\)\s*$")
_OL = re.compile(r"^\d+\.\s+(.*)$")


class _Renderer:
    def __init__(self, lang: str):
        self.lang = lang
        self.heading_count = 0

    def render(self, text: str) -> str:
        lines = text.splitlines()
        out: list[str] = []
        i = 0
        while i < len(lines):
            stripped = lines[i].strip()
            if not stripped:
                i += 1
                continue
            if stripped.startswith(":::"):
                kind = stripped[3:].strip()
                j = i + 1
                depth = 1
                while j < len(lines):
                    s = lines[j].strip()
                    if s.startswith(":::") and s[3:].strip():
                        depth += 1
                    elif s == ":::":
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                inner = self.render("\n".join(lines[i + 1:j]))
                if kind == "step":
                    item = f"<li>{inner}</li>"
                    if out and out[-1].startswith('<ol class="steps">'):
                        out[-1] = out[-1][: -len("</ol>")] + item + "</ol>"
                    else:
                        out.append(f'<ol class="steps">{item}</ol>')
                elif kind == "formula":  # one line each, as written
                    rows = "".join(f"<p>{inline(s.strip())}</p>" for s in lines[i + 1:j] if s.strip())
                    out.append(f'<div class="formula">{rows}</div>')
                else:
                    out.append(f'<aside class="{html.escape(kind)}">{inner}</aside>')
                i = j + 1
                continue
            m = _HEADING.match(stripped)
            if m:
                level = len(m.group(1))
                self.heading_count += 1
                anchor = m.group(3) or f"h-{self.heading_count}"
                out.append(f'<h{level} id="{anchor}">{inline(m.group(2))}</h{level}>')
                i += 1
                continue
            m = _SHOT.match(stripped)
            if m:
                caption, shot = m.group(1), m.group(2)
                src = f"../assets/shots/{self.lang}/{shot}.png"
                out.append(
                    f'<figure class="shot"><a href="{src}"><img src="{src}" '
                    f'alt="{html.escape(caption)}" loading="lazy"></a>'
                    f"<figcaption>{inline(caption)}</figcaption></figure>")
                i += 1
                continue
            if stripped.startswith("|"):
                rows = []
                while i < len(lines) and lines[i].strip().startswith("|"):
                    rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                    i += 1
                head, body = rows[0], rows[2:]
                out.append(
                    "<table><thead><tr>" + "".join(f"<th>{inline(c)}</th>" for c in head)
                    + "</tr></thead><tbody>"
                    + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>"
                              for r in body)
                    + "</tbody></table>")
                continue
            if stripped.startswith("- ") or _OL.match(stripped):
                ordered = not stripped.startswith("- ")
                items: list[str] = []
                while i < len(lines):
                    s = lines[i]
                    st = s.strip()
                    if ordered and _OL.match(st):
                        items.append(_OL.match(st).group(1))
                    elif not ordered and st.startswith("- "):
                        items.append(st[2:])
                    elif st and s.startswith("  ") and items:
                        items[-1] += " " + st
                    else:
                        break
                    i += 1
                tag = "ol" if ordered else "ul"
                out.append(f"<{tag}>" + "".join(f"<li>{inline(t)}</li>" for t in items) + f"</{tag}>")
                continue
            para = [stripped]
            i += 1
            while i < len(lines):
                st = lines[i].strip()
                if not st or st.startswith((":::", "#", "|", "- ", "![")) or _OL.match(st):
                    break
                para.append(st)
                i += 1
            out.append(f"<p>{inline(' '.join(para))}</p>")
        return "\n".join(out)


# a number with its unit, a range ("0.1–100 mm"), a product ("15.04 × 11.28 mm")
# or a sensor format ("1/2.3″"), not a digit inside a word ("mm2")
_LTR_RUN = re.compile(r"(?<![\w./])[+−]?\d[\d.]*(?:/\d[\d.]*)?″?(?:\s?[–×x]\s?\d[\d.]*)*"
                      r"(?:\s?(?:mm/px|mm²|cm²|m²|mm|µm|px|%|°|s\b))?")


def isolate_ltr(page: str) -> str:
    """Each number with its unit isolated left to right, in the text between
    tags: in a right-to-left page "1500 mm" would read "mm 1500"."""
    parts = re.split(r"(<[^>]+>)", page)
    return "".join(p if p.startswith("<") else _LTR_RUN.sub(lambda m: f'<bdi dir="ltr">{m.group(0)}</bdi>', p)
                   for p in parts)


def render_markdown(body: str, lang: str) -> str:
    out = _Renderer(lang).render(body)
    return isolate_ltr(out) if lang in RTL else out


# ---------------------------------------------------------------- site

def _pages_for(root: Path, lang: str) -> list[tuple[str, dict[str, str], str]]:
    out = []
    for path in sorted((root / "content" / lang).glob("*.md")):
        meta, body = parse_front_matter(path.read_text(encoding="utf-8"))
        out.append((path.stem, meta, body))
    out.sort(key=lambda p: int(p[1].get("order", "999")))
    return out


def _fill(template: str, values: dict[str, str]) -> str:
    return re.sub(r"\{\{(\w+)\}\}", lambda m: values[m.group(1)], template)


def build(root: Path) -> dict[Path, str]:
    template = (root / "template.html").read_text(encoding="utf-8")
    result: dict[Path, str] = {}
    for lang in LANGS:
        pages = _pages_for(root, lang)
        for index, (name, meta, body) in enumerate(pages):
            nav = "".join(
                f'<li><a href="{n}.html"{current_attr(n, name)}>'
                f"{html.escape(m.get('title', n))}</a></li>" for n, m, _ in pages)
            prev_link = next_link = ""
            if index > 0:
                p = pages[index - 1]
                prev_link = (f'<a class="prev" href="{p[0]}.html"><span>{UI[lang]["prev"]}</span>'
                             f"{html.escape(p[1].get('title', p[0]))}</a>")
            if index + 1 < len(pages):
                n = pages[index + 1]
                next_link = (f'<a class="next" href="{n[0]}.html"><span>{UI[lang]["next"]}</span>'
                             f"{html.escape(n[1].get('title', n[0]))}</a>")
            switcher = "".join(
                f'<a href="../{other}/{name}.html" lang="{other}" title="{LANG_NAMES[other]}"'
                f'{current_true(other, lang)}>'
                f'<img src="../assets/flags/{other}.png" alt="{LANG_NAMES[other]}"></a>'
                for other in LANGS)
            result[root / lang / f"{name}.html"] = _fill(template, {
                "lang": lang,
                "dir": "rtl" if lang in RTL else "ltr",
                "title": html.escape(meta.get("title", name)),
                "summary": html.escape(meta.get("summary", "")),
                "manual": UI[lang]["manual"],
                "contents": UI[lang]["contents"],
                "language": UI[lang]["language"],
                "nav": nav,
                "switcher": switcher,
                "body": render_markdown(body, lang),
                "prev": prev_link,
                "next": next_link,
            })
    result[root / "index.html"] = _chooser()
    return result


def current_attr(page: str, name: str) -> str:
    return ' aria-current="page"' if page == name else ""


def current_true(other: str, lang: str) -> str:
    return ' aria-current="true"' if other == lang else ""


def _chooser() -> str:
    cards = "".join(
        f'<a class="choice" href="{lang}/index.html" lang="{lang}" dir="{"rtl" if lang in RTL else "ltr"}">'
        f'<img src="assets/flags/{lang}.png" alt=""><span>{UI[lang]["manual"]}</span>'
        f"<small>{LANG_NAMES[lang]}</small></a>" for lang in LANGS)
    return (
        '<!doctype html>\n<html lang="en" dir="ltr">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "<title>Contorno Indicator</title>\n"
        '<link rel="icon" href="assets/brand/icon-light.png">\n'
        '<link rel="stylesheet" href="assets/manual.css">\n</head>\n'
        '<body class="chooser">\n<main>\n'
        '<picture class="logo"><source srcset="assets/brand/icon-dark.png" media="(prefers-color-scheme: dark)">'
        '<img src="assets/brand/icon-light.png" alt=""></picture>\n<h1>Contorno Indicator</h1>\n'
        f'<nav class="choices">{cards}</nav>\n</main>\n</body>\n</html>\n')


# ---------------------------------------------------------------- checks

_ENTRY = re.compile(r'\{"([^"]+)",\s*"((?:[^"\\]|\\.)*)"\}')


def load_ui_labels(strings_dir: Path) -> dict[str, set[str]]:
    """Every text of the application's string tables, per language."""
    labels: dict[str, set[str]] = {}
    for lang in LANGS:
        text = (strings_dir / f"indicator_{lang}.cpp").read_text(encoding="utf-8")
        labels[lang] = {m.group(2).replace('\\"', '"').replace("\\\\", "\\") for m in _ENTRY.finditer(text)}
    return labels


def _norm(label: str) -> str:
    return label.strip().rstrip(":…").strip()


def check_site(root: Path, pages: dict[Path, str]) -> list[str]:
    problems: list[str] = []
    labels = load_ui_labels(REPO / "applications/Indicator/strings")
    # a text with values ("Table covered: {value}") also counts by its words before the first value
    norm_labels = {lang: {_norm(v) for v in vals} | {_norm(v.split("{")[0]) for v in vals if "{" in v}
                   for lang, vals in labels.items()}
    for lang in LANGS:
        seen_orders: dict[str, str] = {}
        for name in PAGES:
            src = root / "content" / lang / f"{name}.md"
            if not src.exists():
                problems.append(f"missing page: {lang}/{name}")
                continue
            meta, _ = parse_front_matter(src.read_text(encoding="utf-8"))
            for key in ("title", "order", "summary"):
                if not meta.get(key):
                    problems.append(f"{lang}/{name}: front matter lacks {key}")
            order = meta.get("order", "")
            if order in seen_orders:
                problems.append(f"{lang}/{name}: order {order} also used by {seen_orders[order]}")
            seen_orders[order] = name
        extra = {p.stem for p in (root / "content" / lang).glob("*.md")} - set(PAGES)
        problems.extend(f"{lang}/{name}: not in PAGES" for name in sorted(extra))
    for path, text in pages.items():
        rel = path.relative_to(root)
        if "http://" in text or "https://" in text:
            problems.append(f"{rel}: external URL")
        for m in re.finditer(r'(?:src|srcset|href)="([^"]+)"', text):
            target = m.group(1)
            file_part, _, anchor = target.partition("#")
            dest = (path.parent / file_part).resolve() if file_part else path
            if dest not in pages and not dest.exists():
                kind = "missing image" if target.endswith(".png") else "broken link"
                problems.append(f"{rel}: {kind} {target}")
                continue
            if anchor:
                dest_text = pages.get(dest) or dest.read_text(encoding="utf-8")
                if f'id="{anchor}"' not in dest_text:
                    problems.append(f"{rel}: missing anchor {target}")
        lang = rel.parts[0] if len(rel.parts) > 1 else None
        if lang in LANGS:
            for m in re.finditer(r'<span class="ui">([^<]+)</span>', text):
                label = html.unescape(m.group(1))
                if _norm(label) not in norm_labels[lang]:
                    problems.append(f"{rel}: not a {lang} label of the application: {label}")
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            problems.append(f"{rel}: out of date (run docs/manual/build_manual.py)")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=REPO / "docs" / "manual")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    pages = build(args.root)
    if args.check:
        problems = check_site(args.root, pages)
        for p in problems:
            print(p)
        return 1 if problems else 0
    for lang in LANGS:  # pages removed from the sources go from the site too
        shutil.rmtree(args.root / lang, ignore_errors=True)
    for path, text in pages.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    print(f"wrote {len(pages)} pages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
