#!/usr/bin/env python3
"""progress_map: a treemap progress page for a decompilation project, from objdiff's report.json.

Usage:
  python progress_map.py build/GAMEID/report.json -o site [--config progress_map.json] [--history FILE ...]
                         [--commit SHA] [--date ISO] [--repo OWNER/NAME]

Writes into the output directory:
  progress.svg   the treemap image the README shows: one box per unit, sized by its code bytes and coloured by how
                 much of it matches, under a header with the matched-code percentage
  history.svg    matched code and fuzzy match over time
  README.md      the two images, the totals and the table by library
  index.html     the same as one page, with details on hover
  history.json   one point per main build: the --history files (merged by commit) plus this build

Groups: a unit goes in its first objdiff progress category (configure.py's progress_categories); units objdiff
marks auto_generated (not split yet) go by address into the --config "regions", else "other". Group display names
come from the config's "groups".

--config (JSON, every key optional):
  {"title": "My Game (USA)", "subtitle": "GAMEID the binary · code matched byte for byte",
   "page_title": "My Game Decomp Progress", "description": "...",
   "regions": [["0x80006A80", "0x80251648", "game"], ...],
   "groups": {"game": "Game code", "sdk": "SDK", "other": "Libraries not mapped yet"},
   "links": [["status", "blob/main/docs/STATUS.md"]]}
Links are relative to https://github.com/OWNER/NAME (--repo).

GitHub Actions: example-workflow.yml next to this file commits the output to a `progress` branch after every build
of main; the README shows the branch's progress.svg.
"""
import argparse
import html
import json
import os
import sys
from typing import Dict, List, Optional, Sequence, Tuple

# Set from --config (see the docstring); these are the defaults.
TITLE = "Decompilation progress"
SUBTITLE = "code matched byte for byte"
PAGE_TITLE = "Decomp Progress"
DESCRIPTION = "Progress of the decompilation."
# Address ranges for units not split yet (auto units only): (lo, hi, group). Anything outside goes to "other".
REGIONS: List[Tuple[int, int, str]] = []
GROUP_NAMES = {"other": "Libraries not mapped yet"}
LINKS: List[Tuple[str, str]] = []


def load_config(path: str) -> None:
    global TITLE, SUBTITLE, PAGE_TITLE, DESCRIPTION, REGIONS, LINKS
    with open(path, encoding="utf-8") as fh:
        c = json.load(fh)
    TITLE = c.get("title", TITLE)
    SUBTITLE = c.get("subtitle", SUBTITLE)
    PAGE_TITLE = c.get("page_title", TITLE + " Progress")
    DESCRIPTION = c.get("description", DESCRIPTION)
    REGIONS = [(int(str(lo), 0), int(str(hi), 0), g) for lo, hi, g in c.get("regions", [])]
    GROUP_NAMES.update(c.get("groups", {}))
    LINKS = [tuple(x) for x in c.get("links", [])]

# Colours (the image has its own dark ground, so it reads the same on a light or dark README).
BG = "#0d1117"
PANEL = "#161b22"
TEXT = "#e6edf3"
MUTED = "#8b949e"
GREEN = (63, 185, 80)
FUZZY_LO = (58, 65, 75)
FUZZY_HI = (210, 153, 34)
ASM = (39, 45, 53)
FONT = "'Segoe UI', 'Helvetica Neue', Helvetica, Arial, sans-serif"

W = 1200
HEAD = 112
MAP_H = 620
FOOT = 40
PAD = 12


def num(v) -> float:
    try:
        return float(v)
    except (TypeError, ValueError):
        return 0.0


def hexc(c: Tuple[int, int, int]) -> str:
    return "#%02x%02x%02x" % c


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def unit_colour(u: dict) -> Tuple[int, int, int]:
    if u["auto"]:
        return ASM
    if u["matched_pct"] >= 100.0:
        return GREEN
    return lerp(FUZZY_LO, FUZZY_HI, max(0.0, min(1.0, u["fuzzy"] / 100.0)))


def ink(c: Tuple[int, int, int]) -> str:
    lum = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
    return BG if lum > 120 else "#c9d1d9"


# --- report -------------------------------------------------------------------------------------------------------

def code_address(u: dict) -> Optional[int]:
    addrs = [int(s["metadata"]["virtual_address"]) for s in u.get("sections", [])
             if s.get("name") in (".text", ".init") and "virtual_address" in (s.get("metadata") or {})]
    return min(addrs) if addrs else None


def region(addr: Optional[int]) -> str:
    if addr is not None:
        for lo, hi, g in REGIONS:
            if lo <= addr < hi:
                return g
    return "other"


def load_units(report: dict) -> List[dict]:
    units = []
    for u in report.get("units", []):
        m = u.get("measures", {})
        code = int(num(m.get("total_code")))
        if code <= 0:
            continue
        md = u.get("metadata", {})
        auto = bool(md.get("auto_generated"))
        addr = code_address(u)
        name = u["name"][5:] if u["name"].startswith("main/") else u["name"]
        if auto:
            group = region(addr)
            label = "%08X" % addr if addr is not None else name
        else:
            cats = md.get("progress_categories") or []
            group = cats[0] if cats else region(addr)
            label = name.rsplit("/", 1)[-1]
        units.append({
            "name": name,
            "label": label,
            "group": group,
            "auto": auto,
            "addr": addr,
            "source": md.get("source_path", ""),
            "code": code,
            "matched": int(num(m.get("matched_code"))),
            "matched_pct": num(m.get("matched_code_percent")),
            "fuzzy": num(m.get("fuzzy_match_percent")),
            "funcs": int(num(m.get("total_functions"))),
            "funcs_matched": int(num(m.get("matched_functions"))),
        })
    return units


def group_totals(units: List[dict]) -> List[dict]:
    groups: Dict[str, dict] = {}
    for u in units:
        g = groups.setdefault(u["group"], {"id": u["group"], "name": GROUP_NAMES.get(u["group"], u["group"]),
                                           "code": 0, "matched": 0, "fuzzy_bytes": 0.0, "funcs": 0,
                                           "funcs_matched": 0, "units": [], "split": 0})
        g["code"] += u["code"]
        g["matched"] += u["matched"]
        g["fuzzy_bytes"] += u["fuzzy"] * u["code"] / 100.0
        g["funcs"] += u["funcs"]
        g["funcs_matched"] += u["funcs_matched"]
        g["split"] += 0 if u["auto"] else u["code"]
        g["units"].append(u)
    out = sorted(groups.values(), key=lambda g: (-g["code"], g["id"]))
    for g in out:
        g["matched_pct"] = 100.0 * g["matched"] / g["code"]
        g["fuzzy"] = 100.0 * g["fuzzy_bytes"] / g["code"]
        g["units"].sort(key=lambda u: (-u["code"], u["addr"] or 0))
    return out


# --- layout -------------------------------------------------------------------------------------------------------

def _worst(row: List[float], side: float) -> float:
    s = sum(row)
    return max(side * side * max(row) / (s * s), s * s / (side * side * min(row)))


def squarify(sizes: Sequence[float], x: float, y: float, w: float, h: float) -> List[Tuple[float, float, float, float]]:
    """Squarified treemap (Bruls, Huizing, van Wijk 2000). sizes sorted largest first, all > 0; returns one
    (x, y, w, h) per size, in order."""
    total = sum(sizes)
    if total <= 0 or w <= 0 or h <= 0:
        return [(x, y, 0.0, 0.0) for _ in sizes]
    scale = w * h / total
    areas = [s * scale for s in sizes]
    rects = []
    i = 0
    while i < len(areas):
        side = min(w, h)
        row = [areas[i]]
        j = i + 1
        while j < len(areas) and _worst(row + [areas[j]], side) <= _worst(row, side):
            row.append(areas[j])
            j += 1
        s = sum(row)
        if w >= h:  # a column on the left
            cw = s / h
            yy = y
            for a in row:
                rects.append((x, yy, cw, a / cw))
                yy += a / cw
            x += cw
            w -= cw
        else:  # a row along the top
            rh = s / w
            xx = x
            for a in row:
                rects.append((xx, y, a / rh, rh))
                xx += a / rh
            y += rh
            h -= rh
        i = j
    return rects


# --- formatting ---------------------------------------------------------------------------------------------------

def pct(v: float) -> str:
    return "%.2f%%" % v


def kb(n: int) -> str:
    if n >= 1 << 20:
        return "%.2f MB" % (n / float(1 << 20))
    if n >= 1 << 10:
        return "%.1f KB" % (n / 1024.0)
    return "%d B" % n


def text_width(s: str, size: float) -> float:
    return len(s) * size * 0.56


def f1(v: float) -> str:
    return ("%.1f" % v).rstrip("0").rstrip(".")


def unit_tip(u: dict) -> str:
    where = "not split yet (assembly)" if u["auto"] else (u["source"] or "split")
    return "%s\n%s\n%s code, %s matched, %s fuzzy\n%d of %d functions matched" % (
        u["name"], where, kb(u["code"]), pct(u["matched_pct"]), pct(u["fuzzy"]), u["funcs_matched"], u["funcs"])


# --- SVG ----------------------------------------------------------------------------------------------------------

def header(o: List[str], m: dict) -> None:
    """The image's header: the title, the matched-code percentage and an overall bar."""
    matched_pct = num(m.get("matched_code_percent"))
    fuzzy = num(m.get("fuzzy_match_percent"))
    o.append('<text x="%d" y="50" font-size="32" font-weight="600" fill="%s">%s</text>'
             % (PAD + 12, TEXT, html.escape(TITLE)))
    o.append('<text x="%d" y="78" font-size="15" fill="%s">%s</text>'
             % (PAD + 12, MUTED, html.escape(SUBTITLE)))
    o.append('<text x="%d" y="56" font-size="48" font-weight="700" fill="%s" text-anchor="end">%s</text>'
             % (W - PAD - 12, hexc(GREEN), pct(matched_pct)))
    o.append('<text x="%d" y="80" font-size="14" fill="%s" text-anchor="end">%s of %s bytes · %s of %s '
             'functions · %s fuzzy</text>'
             % (W - PAD - 12, MUTED, format(int(num(m.get("matched_code"))), ","),
                format(int(num(m.get("total_code"))), ","), format(int(num(m.get("matched_functions"))), ","),
                format(int(num(m.get("total_functions"))), ","), pct(fuzzy)))
    # overall bar: fuzzy, with matched over it
    bx, by, bw = PAD, 94, W - 2 * PAD
    o.append('<rect x="%d" y="%d" width="%d" height="6" rx="3" fill="%s"/>' % (bx, by, bw, PANEL))
    for v, c in ((fuzzy, FUZZY_HI), (matched_pct, GREEN)):
        if v > 0:
            o.append('<rect x="%d" y="%d" width="%s" height="6" rx="3" fill="%s"/>'
                     % (bx, by, f1(max(bw * min(100.0, v) / 100.0, 2)), hexc(c)))


def treemap_svg(report: dict, groups: List[dict], commit: str, date: str, for_html: bool) -> str:
    m = report.get("measures", {})
    matched_pct = num(m.get("matched_code_percent"))
    head = 8 if for_html else HEAD  # the page has its own header
    H = head + MAP_H + FOOT
    o = []
    o.append('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
             'font-family="%s" role="img" aria-label="%s: %s of the code matched">'
             % (W, H, W, H, html.escape(FONT), html.escape(TITLE), pct(matched_pct)))
    o.append('<rect width="%d" height="%d" fill="%s"/>' % (W, H, BG))
    # header (the image only)
    if not for_html:
        header(o, m)
    # map
    gx0, gy0, gw0, gh0 = PAD, head, W - 2 * PAD, MAP_H
    grects = squarify([g["code"] for g in groups], gx0, gy0, gw0, gh0)
    for g, (x, y, w, h) in zip(groups, grects):
        x, y, w, h = x + 2, y + 2, w - 4, h - 4
        if w <= 1 or h <= 1:
            continue
        o.append('<g><rect x="%s" y="%s" width="%s" height="%s" rx="3" fill="%s"/>' % (f1(x), f1(y), f1(w), f1(h), PANEL))
        band = 20 if (h > 48 and w > 120) else 0
        if band:
            lab = "%s · %s · %s matched" % (g["name"], kb(g["code"]), pct(g["matched_pct"]))
            if text_width(lab, 12) > w - 12:
                lab = g["name"]
            if text_width(lab, 12) <= w - 12:
                o.append('<text x="%s" y="%s" font-size="12" font-weight="600" fill="%s">%s</text>'
                         % (f1(x + 6), f1(y + 14), TEXT, html.escape(lab)))
        ix, iy, iw, ih = x + 1, y + band + 1, w - 2, h - band - 2
        urects = squarify([u["code"] for u in g["units"]], ix, iy, iw, ih)
        for u, (ux, uy, uw, uh) in zip(g["units"], urects):
            if uw <= 0 or uh <= 0:
                continue
            c = unit_colour(u)
            stroke = ' stroke="%s" stroke-width="1"' % PANEL if min(uw, uh) >= 3 else ""
            tip = unit_tip(u)
            if for_html:
                attrs = ' data-t="%s"' % html.escape(tip.replace("\n", "|"))
                o.append('<rect x="%s" y="%s" width="%s" height="%s" fill="%s"%s%s/>'
                         % (f1(ux), f1(uy), f1(uw), f1(uh), hexc(c), stroke, attrs))
            else:
                o.append('<rect x="%s" y="%s" width="%s" height="%s" fill="%s"%s><title>%s</title></rect>'
                         % (f1(ux), f1(uy), f1(uw), f1(uh), hexc(c), stroke, html.escape(tip)))
            for size in (() if u["auto"] else (13, 11, 10)):  # not-split boxes are labelled in the tooltip only
                if uh >= size + 5 and text_width(u["label"], size) <= uw - 6:
                    o.append('<text x="%s" y="%s" font-size="%d" fill="%s" pointer-events="none">%s</text>'
                             % (f1(ux + 3), f1(uy + size + 1), size, ink(c), html.escape(u["label"])))
                    break
        o.append("</g>")
    # legend
    ly = head + MAP_H + 24
    x = PAD + 4
    o.append('<rect x="%d" y="%d" width="14" height="14" rx="2" fill="%s"/>' % (x, ly - 11, hexc(GREEN)))
    o.append('<text x="%d" y="%d" font-size="13" fill="%s">matched</text>' % (x + 20, ly, MUTED))
    x += 96
    o.append('<defs><linearGradient id="fz"><stop offset="0" stop-color="%s"/><stop offset="1" stop-color="%s"/>'
             '</linearGradient></defs>' % (hexc(FUZZY_LO), hexc(FUZZY_HI)))
    o.append('<rect x="%d" y="%d" width="64" height="14" rx="2" fill="url(#fz)"/>' % (x, ly - 11))
    o.append('<text x="%d" y="%d" font-size="13" fill="%s">source, fuzzy match 0–100%%</text>' % (x + 70, ly, MUTED))
    x += 270
    o.append('<rect x="%d" y="%d" width="14" height="14" rx="2" fill="%s" stroke="%s"/>' % (x, ly - 11, hexc(ASM), PANEL))
    o.append('<text x="%d" y="%d" font-size="13" fill="%s">not split yet (assembly)</text>' % (x + 20, ly, MUTED))
    foot = "box area = code size"
    if date:
        foot += " · " + date[:10]
    if commit:
        foot += " · " + commit[:7]
    o.append('<text x="%d" y="%d" font-size="13" fill="%s" text-anchor="end">%s</text>'
             % (W - PAD - 4, ly, MUTED, html.escape(foot)))
    o.append("</svg>")
    return "\n".join(o)


# --- history ------------------------------------------------------------------------------------------------------

def history_point(report: dict, commit: str, date: str) -> dict:
    m = report.get("measures", {})
    return {
        "date": date,
        "commit": commit,
        "total_code": int(num(m.get("total_code"))),
        "matched_code": int(num(m.get("matched_code"))),
        "matched_code_percent": round(num(m.get("matched_code_percent")), 4),
        "fuzzy_match_percent": round(num(m.get("fuzzy_match_percent")), 4),
        "total_functions": int(num(m.get("total_functions"))),
        "matched_functions": int(num(m.get("matched_functions"))),
        "total_data": int(num(m.get("total_data"))),
        "matched_data": int(num(m.get("matched_data"))),
    }


def merge_history(files: Sequence[str], point: Optional[dict]) -> List[dict]:
    by_commit: Dict[str, dict] = {}
    for f in files:
        try:
            with open(f, encoding="utf-8") as fh:
                data = json.load(fh)
        except (OSError, ValueError):
            continue
        if not isinstance(data, list):
            continue
        for p in data:
            if isinstance(p, dict) and p.get("commit") and p.get("date"):
                by_commit[p["commit"]] = p
    if point and point.get("commit"):
        by_commit[point["commit"]] = point
    elif point:
        by_commit["(local)"] = point
    return sorted(by_commit.values(), key=lambda p: (p["date"], p["commit"]))


HIST_STYLE = (".grid { stroke: #30363d; stroke-width: 1; } .tick { fill: #8b949e; font-size: 12px; }"
              " .mc { fill: none; stroke: #3fb950; stroke-width: 2.5; }"
              " .fz { fill: none; stroke: #d29922; stroke-width: 1.5; stroke-dasharray: 5 4; }")


def history_svg(hist: List[dict], standalone: bool = False) -> str:
    """Matched code and fuzzy match over time, as step lines. standalone: an image file with its own ground,
    styles and key (the page styles the inline one)."""
    pts = [p for p in hist if p.get("date")]
    if len(pts) < 2:
        return ""
    from datetime import datetime

    def t(p):
        return datetime.fromisoformat(p["date"].replace("Z", "+00:00")).timestamp()
    w, h, l, r, top, bot = 1000, 260, 52, 28, (40 if standalone else 14), 30  # the image's key sits above the plot
    t0, t1 = t(pts[0]), t(pts[-1])
    if t1 <= t0:
        t1 = t0 + 1
    ymax = max(max(p["matched_code_percent"], p["fuzzy_match_percent"]) for p in pts)
    steps = [0.5, 1, 2, 5, 10, 20, 25, 50, 100]
    step = next((s for s in steps if ymax / s <= 5), 100)
    ytop = max(step, step * (int(ymax / step) + 1))

    def X(tt):
        return l + (w - l - r) * (tt - t0) / (t1 - t0)

    def Y(v):
        return h - bot - (h - top - bot) * v / ytop
    if standalone:
        o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="%s" '
             'role="img" aria-label="Matched code over time">' % (w, h, w, h, html.escape(FONT)),
             '<style>%s</style>' % HIST_STYLE, '<rect width="%d" height="%d" fill="%s"/>' % (w, h, BG)]
    else:
        o = ['<svg viewBox="0 0 %d %d" role="img" aria-label="Matched code over time" class="hist">' % (w, h)]
    v = 0.0
    while v <= ytop + 1e-9:
        o.append('<line x1="%d" x2="%d" y1="%s" y2="%s" class="grid"/>' % (l, w - r, f1(Y(v)), f1(Y(v))))
        o.append('<text x="%d" y="%s" text-anchor="end" class="tick">%s%%</text>' % (l - 8, f1(Y(v) + 4), f1(v)))
        v += step
    # time ticks: the smallest interval giving at most 8, in the latest build's time zone; a day's first tick shows
    # the date, the others the hour
    tz = datetime.fromisoformat(pts[-1]["date"].replace("Z", "+00:00")).tzinfo
    iv = next((s for s in (3600, 3 * 3600, 6 * 3600, 12 * 3600, 86400, 2 * 86400, 7 * 86400, 14 * 86400, 28 * 86400)
               if (t1 - t0) / s <= 8), 91 * 86400)
    day0 = datetime.fromtimestamp(t0, tz).replace(hour=0, minute=0, second=0, microsecond=0)
    tick = day0.timestamp()
    while tick <= t1:
        if tick >= t0:
            dt = datetime.fromtimestamp(tick, tz)
            lab = dt.strftime("%b %d") if (dt.hour == 0 and dt.minute == 0) else dt.strftime("%H:%M")
            o.append('<line x1="%s" x2="%s" y1="%d" y2="%d" class="grid"/>' % (f1(X(tick)), f1(X(tick)), h - bot, h - bot + 4))
            o.append('<text x="%s" y="%d" text-anchor="middle" class="tick">%s</text>' % (f1(X(tick)), h - 8, lab))
        tick += iv

    def step_path(key):
        s = []
        prev = None
        for p in pts:
            x, y = X(t(p)), Y(p[key])
            if prev is None:
                s.append("M%s %s" % (f1(x), f1(y)))
            else:
                s.append("H%s V%s" % (f1(x), f1(y)))
            prev = y
        s.append("H%s" % f1(X(t1)))
        return " ".join(s)
    o.append('<path d="%s" class="fz"/>' % step_path("fuzzy_match_percent"))
    o.append('<path d="%s" class="mc"/>' % step_path("matched_code_percent"))
    if standalone:
        kx, ky = l, 20
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="mc"/>' % (kx, kx + 22, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">matched code</text>' % (kx + 28, ky))
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="fz"/>' % (kx + 130, kx + 152, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">fuzzy match</text>' % (kx + 158, ky))
    o.append("</svg>")
    return "\n".join(o)


# --- page ---------------------------------------------------------------------------------------------------------

PAGE_CSS = """
:root { --bg: #0d1117; --panel: #161b22; --line: #30363d; --fg: #e6edf3; --muted: #8b949e; --green: #3fb950;
  --amber: #d29922; color-scheme: dark; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--fg);
  font: 15px/1.5 'IBM Plex Sans', 'Segoe UI', Helvetica, Arial, sans-serif; }
main { max-width: 1240px; margin: 0 auto; padding: 28px 16px 48px; display: grid; gap: 28px; }
main > *, header > * { min-width: 0; }
header { display: flex; flex-wrap: wrap; align-items: flex-end; justify-content: space-between; gap: 8px 24px; }
h1 { margin: 0; font-size: clamp(26px, 4vw, 36px); font-weight: 600; text-wrap: balance; }
h2 { margin: 0 0 10px; font-size: 13px; font-weight: 600; letter-spacing: .08em; text-transform: uppercase;
  color: var(--muted); }
.sub { color: var(--muted); margin: 2px 0 0; }
.big { font: 700 clamp(40px, 7vw, 60px)/1 'IBM Plex Mono', Consolas, monospace; color: var(--green); }
.num { font-family: 'IBM Plex Mono', Consolas, monospace; font-variant-numeric: tabular-nums; }
.stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 12px; }
.stat { border-top: 2px solid var(--line); padding-top: 8px; }
.stat b { display: block; font: 600 22px 'IBM Plex Mono', Consolas, monospace; font-variant-numeric: tabular-nums; }
.stat span { color: var(--muted); font-size: 13px; }
.map svg { width: 100%; max-width: 100%; height: auto; display: block; border-radius: 6px; }
.map rect[data-t]:hover { stroke: var(--fg); stroke-width: 2; }
.scroll { overflow-x: auto; }
table { width: 100%; border-collapse: collapse; min-width: 640px; }
th, td { text-align: right; padding: 7px 10px; border-bottom: 1px solid var(--line); white-space: nowrap; }
th { color: var(--muted); font-weight: 500; font-size: 13px; }
th:first-child, td:first-child { text-align: left; }
td.bar { width: 28%; }
.track { position: relative; height: 8px; background: var(--panel); border-radius: 4px; overflow: hidden; }
.track i { position: absolute; left: 0; top: 0; bottom: 0; border-radius: 4px; }
.track i.f { background: var(--amber); } .track i.m { background: var(--green); }
.hist { width: 100%; height: auto; display: block; }
.hist .grid { stroke: var(--line); stroke-width: 1; }
.hist .tick { fill: var(--muted); font-size: 12px; font-family: 'IBM Plex Mono', Consolas, monospace; }
.hist .mc { fill: none; stroke: var(--green); stroke-width: 2.5; }
.hist .fz { fill: none; stroke: var(--amber); stroke-width: 1.5; stroke-dasharray: 5 4; }
.key { display: flex; flex-wrap: wrap; gap: 6px 18px; color: var(--muted); font-size: 13px; margin-top: 8px; }
.key i { display: inline-block; width: 18px; height: 3px; vertical-align: middle; margin-right: 6px; }
#tip { position: fixed; pointer-events: none; z-index: 2; max-width: min(420px, 90vw); padding: 8px 10px;
  background: #1c2128; border: 1px solid var(--line); border-radius: 6px; font-size: 13px; line-height: 1.45;
  box-shadow: 0 8px 24px rgba(0,0,0,.4); white-space: pre-line; overflow-wrap: anywhere; }
#tip b { font-family: 'IBM Plex Mono', Consolas, monospace; font-weight: 600; }
footer { color: var(--muted); font-size: 13px; }
a { color: #58a6ff; }
"""

PAGE_JS = """
(function () {
  var tip = document.getElementById('tip');
  function show(e) {
    var r = e.target.closest ? e.target.closest('rect[data-t]') : null;
    if (!r) { tip.hidden = true; return; }
    var parts = r.getAttribute('data-t').split('|');
    tip.textContent = '';
    var b = document.createElement('b'); b.textContent = parts[0]; tip.appendChild(b);
    tip.appendChild(document.createTextNode('\\n' + parts.slice(1).join('\\n')));
    tip.hidden = false;
    var x = e.clientX + 14, y = e.clientY + 14, w = tip.offsetWidth, h = tip.offsetHeight;
    if (x + w > window.innerWidth - 8) x = Math.max(8, e.clientX - w - 14);
    if (y + h > window.innerHeight - 8) y = Math.max(8, e.clientY - h - 14);
    tip.style.left = x + 'px'; tip.style.top = y + 'px';
  }
  var map = document.querySelector('.map');
  map.addEventListener('mousemove', show);
  map.addEventListener('click', show);
  map.addEventListener('mouseleave', function () { tip.hidden = true; });
})();
"""


def page_html(report: dict, groups: List[dict], hist: List[dict], commit: str, date: str, repo: str) -> str:
    m = report.get("measures", {})
    total_code = int(num(m.get("total_code")))
    matched_code = int(num(m.get("matched_code")))
    funcs = int(num(m.get("total_functions")))
    funcs_matched = int(num(m.get("matched_functions")))
    total_data = int(num(m.get("total_data")))
    matched_data = int(num(m.get("matched_data")))
    e = html.escape
    repo_url = "https://github.com/%s" % repo if repo else ""
    o = ['<!doctype html>', '<html lang="en">', '<head>', '<meta charset="utf-8">',
         '<meta name="viewport" content="width=device-width, initial-scale=1">',
         '<title>%s</title>' % e(PAGE_TITLE),
         '<meta name="description" content="%s">' % e(DESCRIPTION),
         '<link rel="preconnect" href="https://fonts.googleapis.com">',
         '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600;700'
         '&family=IBM+Plex+Sans:wght@400;500;600&display=swap">',
         '<style>%s</style>' % PAGE_CSS, '</head>', '<body>', '<main>']
    o.append('<header><div><h1>%s</h1><p class="sub">%s</p></div><div class="big">%s</div></header>'
             % (e(TITLE), e(SUBTITLE), pct(num(m.get("matched_code_percent")))))
    stats = [
        ("%s / %s" % (format(matched_code, ","), format(total_code, ",")), "code bytes matched"),
        (pct(num(m.get("fuzzy_match_percent"))), "fuzzy match (source that compiles to nearly the same code)"),
        ("%s / %s" % (format(funcs_matched, ","), format(funcs, ",")), "functions matched"),
        ("%s / %s" % (format(matched_data, ","), format(total_data, ",")), "data bytes matched"),
    ]
    o.append('<section class="stats">' + "".join('<div class="stat"><b>%s</b><span>%s</span></div>' % (e(a), e(b))
                                                 for a, b in stats) + '</section>')
    o.append('<section class="map" aria-label="Treemap of the code">%s</section>'
             % treemap_svg(report, groups, commit, date, for_html=True))
    rows = []
    for g in groups:
        rows.append('<tr><td>%s</td><td class="num">%s</td><td class="num">%s</td><td class="num">%s</td>'
                    '<td class="num">%s / %s</td><td class="num">%s</td><td class="bar"><div class="track">'
                    '<i class="f" style="width:%s%%"></i><i class="m" style="width:%s%%"></i></div></td></tr>'
                    % (e(g["name"]), kb(g["code"]), pct(g["matched_pct"]), pct(g["fuzzy"]),
                       format(g["funcs_matched"], ","), format(g["funcs"], ","),
                       pct(100.0 * g["split"] / g["code"]), f1(min(100.0, g["fuzzy"])),
                       f1(min(100.0, g["matched_pct"]))))
    o.append('<section><h2>By library</h2><div class="scroll"><table><thead><tr><th>Library</th><th>Code</th>'
             '<th>Matched</th><th>Fuzzy</th><th>Functions matched</th><th>Split into source units</th>'
             '<th></th></tr></thead><tbody>%s</tbody></table></div></section>' % "".join(rows))
    hs = history_svg(hist)
    if hs:
        o.append('<section><h2>Matched code over time</h2>%s<div class="key"><span><i style="background:var(--green)">'
                 '</i>matched code</span><span><i style="background:var(--amber)"></i>fuzzy match</span>'
                 '<span>%d builds of main</span></div></section>' % (hs, len(hist)))
    foot = []
    if commit:
        c = e(commit[:7])
        foot.append('Built from <a href="%s/commit/%s">%s</a>' % (repo_url, e(commit), c) if repo_url
                    else "Built from %s" % c)
    if date:
        foot.append("on " + e(date[:10]))
    links = ""
    if repo_url:
        links = ' · <a href="%s">repository</a>' % repo_url + "".join(
            ' · <a href="%s/%s">%s</a>' % (repo_url, e(path), e(name)) for name, path in LINKS)
    o.append('<footer>%s. Groups for code not split yet follow the configured address ranges; sizes and '
             'percentages come from objdiff\'s report.%s</footer>' % (" ".join(foot) or "Local build", links))
    o.append('</main><div id="tip" hidden></div><script>%s</script></body></html>' % PAGE_JS)
    return "\n".join(o)


def branch_readme(report: dict, groups: List[dict], hist: List[dict], commit: str, date: str, repo: str) -> str:
    """README.md of the progress branch: what GitHub shows when the branch is opened."""
    m = report.get("measures", {})
    o = ["# %s: decompilation progress" % TITLE, "", "![Treemap of the code](progress.svg)", ""]
    o.append("**%s** of the code matched byte for byte (%s of %s bytes) · %s of %s functions · %s fuzzy "
             "match · data %s of %s bytes"
             % (pct(num(m.get("matched_code_percent"))), format(int(num(m.get("matched_code"))), ","),
                format(int(num(m.get("total_code"))), ","), format(int(num(m.get("matched_functions"))), ","),
                format(int(num(m.get("total_functions"))), ","), pct(num(m.get("fuzzy_match_percent"))),
                format(int(num(m.get("matched_data"))), ","), format(int(num(m.get("total_data"))), ",")))
    o += ["", "| Library | Code | Matched | Fuzzy | Functions matched | Split into source units |",
          "|---|--:|--:|--:|--:|--:|"]
    for g in groups:
        o.append("| %s | %s | %s | %s | %s / %s | %s |"
                 % (g["name"], kb(g["code"]), pct(g["matched_pct"]), pct(g["fuzzy"]), format(g["funcs_matched"], ","),
                    format(g["funcs"], ","), pct(100.0 * g["split"] / g["code"])))
    if len(hist) >= 2:
        o += ["", "## Matched code over time", "", "![Matched code over time](history.svg)", "",
              "%d builds of main." % len(hist)]
    src = commit[:7]
    if commit and repo:
        src = "[%s](https://github.com/%s/commit/%s)" % (commit[:7], repo, commit)
    o += ["", "---", "",
          "Generated by `progress_map.py` from the build of %s%s. CI adds one commit to this branch after "
          "every build of main; nothing else is kept here. Opened on its own ([progress.svg](progress.svg)), the map "
          "names each box on hover; `index.html` is the same as a page (download it and open it in a browser). Code "
          "not split yet is grouped by the configured address ranges; sizes and percentages come from "
          "objdiff's report." % (src or "a local tree", (" on " + date[:10]) if date else "")]
    return "\n".join(o) + "\n"


def main(argv: Optional[Sequence[str]] = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("report", help="objdiff report.json")
    p.add_argument("-o", "--out", default="site", help="output directory (default: site)")
    p.add_argument("--history", action="append", default=[], help="earlier history.json (any number; missing is fine)")
    p.add_argument("--commit", default="", help="the commit this report was built from")
    p.add_argument("--date", default="", help="its commit date (ISO 8601)")
    p.add_argument("--repo", default="", help="OWNER/NAME for links")
    p.add_argument("--config", default="", help="JSON with title, subtitle, regions, groups, links (see the docstring)")
    a = p.parse_args(argv)
    if a.config:
        load_config(a.config)
    with open(a.report, encoding="utf-8") as fh:
        report = json.load(fh)
    groups = group_totals(load_units(report))
    if not groups:
        print("progress_map: no units with code in %s" % a.report, file=sys.stderr)
        return 1
    point = history_point(report, a.commit, a.date) if a.date else None
    hist = merge_history(a.history, point)
    os.makedirs(a.out, exist_ok=True)
    files = {
        "progress.svg": treemap_svg(report, groups, a.commit, a.date, for_html=False) + "\n",
        "index.html": page_html(report, groups, hist, a.commit, a.date, a.repo) + "\n",
        "history.svg": history_svg(hist, standalone=True) + "\n",
        "history.json": json.dumps(hist, indent=1) + "\n",
        "README.md": branch_readme(report, groups, hist, a.commit, a.date, a.repo),
    }
    for name, text in files.items():
        with open(os.path.join(a.out, name), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
    print("progress_map: %s matched, %d groups, %d history points -> %s"
          % (pct(num(report["measures"].get("matched_code_percent"))), len(groups), len(hist), a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
