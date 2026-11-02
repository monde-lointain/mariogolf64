#!/usr/bin/env python3
"""The progress page: a treemap of the binary's code from objdiff's report.json.

Usage:
  python tools/progress_map.py build/<VERSION>/report.json -o site [--history FILE ...] [--commit SHA] [--date ISO]
                               [--repo OWNER/NAME] [--regs FILE]

Writes into the output directory:
  functions.svg  every function as a block whose area is its code size, each library grouped by state (matched,
                 registers only, fuzzy, not written); the page shows it too, with each block named on hover
  progress.svg   the treemap image the README shows: one box per unit, sized by its code bytes and coloured by how
                 much of it matches, under a header with the matched-code percentage
  history.svg    matched code, registers only and fuzzy match over time
  day.svg        matched code and registers only over the last 24 hours, zoomed to their range
  daily.svg      matched code and fuzzy match at the end of each day of the project, with each day's gain
  README.md      the two images, the totals and the table by library
  index.html     the same as one page, with details on hover
  history.json   one point per main build: the --history files (merged by commit) plus this build

tools/progress_publish.py runs this on a local build of main (no builds run on GitHub)
and commits the directory to the `progress` branch (a branch of generated files only; the repository's README shows
its progress.svg). --history is the branch's previous history.json plus tools/progress_seed.json (the builds before
the branch existed). --regs is tools/regsonly.py's JSON: the functions that match except for registers (blue),
counted on top of the matched code.

Units the build does not split yet (dtk's auto_* units) are grouped by the coarse .text layout in REGIONS below.
That layout is a hint: it only picks the box a unit is drawn in. Every size and percentage comes from
the report.
"""
import argparse
import html
import json
import os
import sys
from typing import Dict, List, Optional, Sequence, Tuple

# ---- project settings: edit these for your project --------------------------------------------------------------
TITLE = "Mario Golf (USA)"  # the images' and the page's heading
GAME_ID = "NMFE"  # the build version / game ID shown under the title
BINARY = "mariogolf64.z64"  # the binary the project decompiles
PLATFORM = "Nintendo 64"
PAGE_TITLE = TITLE + " Decomp Progress"  # the browser tab
# ---- end of project settings (REGIONS and GROUP_NAMES below are project settings too) -------------------------

# Coarse .text layout, for auto units only (dtk's auto_* units, which have no progress category yet): one
# (start, end, group id) per address range. Anything outside these goes to "other". Split units take their group
# from their first progress_categories entry in report.json (configure.py), so the ids should match those.
REGIONS = []  # every unit is a source file (tools/progress_report.py)
GROUP_NAMES = {
    "game": "Game code",
    "overlays": "Game overlays",
    "ultralib": "libultra",
    "libnusys": "NuSystem",
    "libnaudio": "n_audio",
    "libmus": "libmus",
    "libkmc": "KMC C library",
    "other": "Other",
}

# Colours (the image has its own dark ground, so it reads the same on a light or dark README).
BG = "#0d1117"
PANEL = "#161b22"
TEXT = "#e6edf3"
MUTED = "#8b949e"
GREEN = (63, 185, 80)
BLUE = (56, 139, 253)
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
    if u["regs"] and u["matched"] + u["regs"] >= u["code"]:
        return BLUE
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


def load_units(report: dict, regs: Optional[dict] = None) -> List[dict]:
    """regs: {report unit name: {function: size}} of the functions that differ in registers only."""
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
        rf = (regs or {}).get(u["name"], {})
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
            "regs": sum(rf.values()),
            "regs_funcs": len(rf),
            "funcs": int(num(m.get("total_functions"))),
            "funcs_matched": int(num(m.get("matched_functions"))),
        })
    return units


def group_totals(units: List[dict]) -> List[dict]:
    groups: Dict[str, dict] = {}
    for u in units:
        g = groups.setdefault(u["group"], {"id": u["group"], "name": GROUP_NAMES.get(u["group"], u["group"]),
                                           "code": 0, "matched": 0, "regs": 0, "fuzzy_bytes": 0.0, "funcs": 0,
                                           "funcs_matched": 0, "units": [], "split": 0})
        g["code"] += u["code"]
        g["matched"] += u["matched"]
        g["regs"] += u["regs"]
        g["fuzzy_bytes"] += u["fuzzy"] * u["code"] / 100.0
        g["funcs"] += u["funcs"]
        g["funcs_matched"] += u["funcs_matched"]
        g["split"] += 0 if u["auto"] else u["code"]
        g["units"].append(u)
    out = sorted(groups.values(), key=lambda g: (-g["code"], g["id"]))
    for g in out:
        g["matched_pct"] = 100.0 * g["matched"] / g["code"]
        g["fuzzy"] = 100.0 * g["fuzzy_bytes"] / g["code"]
        g["regs_pct"] = 100.0 * g["regs"] / g["code"]
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
    regs = "\n%d more match except for registers (%s)" % (u["regs_funcs"], kb(u["regs"])) if u["regs"] else ""
    return "%s\n%s\n%s code, %s matched, %s fuzzy\n%d of %d functions matched%s" % (
        u["name"], where, kb(u["code"]), pct(u["matched_pct"]), pct(u["fuzzy"]), u["funcs_matched"], u["funcs"], regs)


# --- SVG ----------------------------------------------------------------------------------------------------------

def header(o: List[str], m: dict) -> None:
    """The image's header: the title, the matched-code percentage and an overall bar."""
    matched_pct = num(m.get("matched_code_percent"))
    regs = num(m.get("regs_only_percent"))
    fuzzy = num(m.get("fuzzy_match_percent"))
    o.append('<text x="%d" y="50" font-size="32" font-weight="600" fill="%s">%s</text>'
             % (PAD + 12, TEXT, html.escape(TITLE)))
    o.append('<text x="%d" y="78" font-size="15" fill="%s">%s %s · code matched byte for byte</text>'
             % (PAD + 12, MUTED, html.escape(GAME_ID), html.escape(BINARY)))
    # matched (green) / registers only (blue, owner 2026-10-24) / fuzzy (the fuzzy colour of the bar and the
    # treemap), owner 2026-10-23
    o.append('<text x="%d" y="56" font-size="44" font-weight="700" text-anchor="end"><tspan fill="%s">%s</tspan>'
             '<tspan fill="%s" font-weight="400"> / </tspan><tspan fill="%s">%s</tspan>'
             '<tspan fill="%s" font-weight="400"> / </tspan><tspan fill="%s">%s</tspan></text>'
             % (W - PAD - 12, hexc(GREEN), pct(matched_pct), MUTED, hexc(BLUE), pct(regs), MUTED, hexc(FUZZY_HI),
                pct(fuzzy)))
    o.append('<text x="%d" y="80" font-size="14" fill="%s" text-anchor="end">matched / registers only / fuzzy · '
             '%s of %s bytes · '
             '%s of %s functions</text>'
             % (W - PAD - 12, MUTED, format(int(num(m.get("matched_code"))), ","),
                format(int(num(m.get("total_code"))), ","), format(int(num(m.get("matched_functions"))), ","),
                format(int(num(m.get("total_functions"))), ",")))
    # overall bar: fuzzy, matched + registers only over it, matched over that
    bx, by, bw = PAD, 94, W - 2 * PAD
    o.append('<rect x="%d" y="%d" width="%d" height="6" rx="3" fill="%s"/>' % (bx, by, bw, PANEL))
    for v, c in ((fuzzy, FUZZY_HI), (matched_pct + regs if regs else 0, BLUE), (matched_pct, GREEN)):
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
    o.append('<rect x="%d" y="%d" width="14" height="14" rx="2" fill="%s"/>' % (x, ly - 11, hexc(BLUE)))
    o.append('<text x="%d" y="%d" font-size="13" fill="%s">matched except for registers</text>' % (x + 20, ly, MUTED))
    x += 220
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


# --- function map -------------------------------------------------------------------------------------------------

# Every function as a block whose area is its code size (owner 2026-10-29; it was one equal square per function,
# 2026-10-28). Each library is a band of the full width, its height set by its bytes at one scale for the whole image;
# inside it one column per state (matched, registers only, fuzzy 90+ ... 0-9, not written), left to right, each as
# wide as its bytes, its functions squarified largest first.
FMAP_H = 1600  # the height of all the bands together, without their labels
NOT_WRITTEN = (110, 40, 40)
STATE_KEYS = ["m", "r"] + ["f%d" % k for k in range(9, -1, -1)] + ["n"]


def load_functions(report: dict, regs: Optional[dict], groups: List[dict]) -> List[Tuple[dict, List[dict]]]:
    """Every function of the report, by library (the treemap's order), grouped by state (owner 2026-10-28: grouped,
    not in place): matched, registers only, then by fuzzy match high to low, not written last; address order
    within a state. state: m(atched), r(egisters only), n(ot written: no function of that name in our object) or ""
    (the fuzzy match decides)."""
    group_of = {u["name"]: g["id"] for g in groups for u in g["units"]}
    funcs: Dict[str, List[dict]] = {}
    for u in report.get("units", []):
        name = u["name"][5:] if u["name"].startswith("main/") else u["name"]
        rf = (regs or {}).get(u["name"], {})
        for f in u.get("functions", []):
            fz = f.get("fuzzy_match_percent")
            state = "m" if fz is not None and fz >= 100.0 else "r" if f["name"] in rf else "n" if fz is None else ""
            funcs.setdefault(group_of.get(name, "other"), []).append({
                "name": f["name"], "unit": name, "size": int(num(f.get("size"))), "fuzzy": num(fz), "state": state,
                "addr": int(num((f.get("metadata") or {}).get("virtual_address")))})
    rank = lambda f: ({"m": 0, "r": 1, "n": 3}.get(f["state"], 2), -min(9, int(f["fuzzy"] // 10)), f["addr"])
    return [(g, sorted(funcs.get(g["id"], []), key=rank)) for g in groups if funcs.get(g["id"])]


def state_key(f: dict) -> str:
    """The function's colour class: m, r, n, or f0-f9 (ten fuzzy steps)."""
    return f["state"] or "f%d" % min(9, int(f["fuzzy"] // 10))


def key_colour(k: str) -> Tuple[int, int, int]:
    if k == "m":
        return GREEN
    if k == "r":
        return BLUE
    if k == "n":
        return NOT_WRITTEN
    return lerp(FUZZY_LO, FUZZY_HI, int(k[1:]) / 9.0)


def function_blocks(fs: List[dict], i0: int, x: float, y: float, w: float, h: float) -> List[Tuple[int, str, tuple]]:
    """(index into the page's data, colour class, (x, y, w, h)) per function of one library band."""
    cols: Dict[str, List[int]] = {}
    for i, f in enumerate(fs):
        if f["size"] > 0:
            cols.setdefault(state_key(f), []).append(i)
    total = float(sum(f["size"] for f in fs if f["size"] > 0))
    out = []
    for k in STATE_KEYS:
        idx = cols.get(k)
        if not idx:
            continue
        cw = w * sum(fs[i]["size"] for i in idx) / total
        idx.sort(key=lambda i: (-fs[i]["size"], fs[i]["addr"]))
        for i, r in zip(idx, squarify([fs[i]["size"] for i in idx], x, y, cw, h)):
            out.append((i0 + i, k, r))
        x += cw
    return out


def functions_svg(sections: List[Tuple[dict, List[dict]]], commit: str, date: str, for_html: bool) -> str:
    """One block per function, its area its code size (function_blocks). The colours are classes, which keeps the
    file small; blocks of 2 px and more get a thin outline in the ground colour so neighbours stay apart."""
    head = 8 if for_html else 56
    band = 24
    gw = W - 2 * PAD
    total = sum(f["size"] for _, fs in sections for f in fs if f["size"] > 0)
    heights = [max(2.0, FMAP_H * sum(f["size"] for f in fs if f["size"] > 0) / total) for _, fs in sections]
    H = int(head + sum(band + h + 10 for h in heights) + FOOT + 0.999)
    css = "".join(".fb .%s{fill:%s}" % (k, hexc(key_colour(k))) for k in STATE_KEYS)
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="%s" '
         'role="img" aria-label="Every function of %s as a block sized by its code"%s>'
         % (W, H, W, H, html.escape(FONT), html.escape(BINARY), ' class="fgrid"' if for_html else ""),
         '<style>%s.fb .o{stroke:%s;stroke-width:.5}</style>' % (css, BG),
         '<rect width="%d" height="%d" fill="%s"/>' % (W, H, BG)]
    if not for_html:
        nf = sum(len(fs) for _, fs in sections)
        done = sum(1 for _, fs in sections for f in fs if f["state"] == "m")
        o.append('<text x="%d" y="36" font-size="22" font-weight="600" fill="%s">%s: every function</text>'
                 % (PAD + 4, TEXT, html.escape(TITLE)))
        o.append('<text x="%d" y="36" font-size="15" fill="%s" text-anchor="end">%s of %s functions matched</text>'
                 % (W - PAD - 4, MUTED, format(done, ","), format(nf, ",")))
    y, index = head, 0
    for (g, fs), bh in zip(sections, heights):
        done = sum(1 for f in fs if f["state"] == "m")
        o.append('<text x="%d" y="%d" font-size="12" font-weight="600" fill="%s">%s · %s · %s of %s functions matched'
                 '</text>' % (PAD + 2, y + 16, TEXT, html.escape(g["name"]), kb(sum(f["size"] for f in fs)),
                              format(done, ","), format(len(fs), ",")))
        gy = y + band
        o.append('<g class="fb">')
        for i, k, (bx, by, bw, bhh) in function_blocks(fs, index, PAD, gy, gw, bh):
            cls = k + " o" if min(bw, bhh) >= 2 else k
            o.append('<rect x="%s" y="%s" width="%s" height="%s" class="%s"%s/>'
                     % (f1(bx), f1(by), f1(bw), f1(bhh), cls, ' data-i="%d"' % i if for_html else ""))
        o.append('</g>')
        y = gy + bh + 10
        index += len(fs)
    ly = y + 24
    x = PAD + 4
    for col, lab, step in ((GREEN, "matched", 96), (BLUE, "matched except for registers", 220),
                           (NOT_WRITTEN, "not written yet", 140)):
        o.append('<rect x="%d" y="%d" width="14" height="14" rx="2" fill="%s"/>' % (x, ly - 11, hexc(col)))
        o.append('<text x="%d" y="%d" font-size="13" fill="%s">%s</text>' % (x + 20, ly, MUTED, lab))
        x += step
    o.append('<defs><linearGradient id="fzf"><stop offset="0" stop-color="%s"/><stop offset="1" stop-color="%s"/>'
             '</linearGradient></defs>' % (hexc(FUZZY_LO), hexc(FUZZY_HI)))
    o.append('<rect x="%d" y="%d" width="64" height="14" rx="2" fill="url(#fzf)"/>' % (x, ly - 11))
    o.append('<text x="%d" y="%d" font-size="13" fill="%s">written, fuzzy match 0–100%%</text>' % (x + 70, ly, MUTED))
    foot = "block area = function size"
    if date:
        foot += " · " + date[:10]
    if commit:
        foot += " · " + commit[:7]
    o.append('<text x="%d" y="%d" font-size="13" fill="%s" text-anchor="end">%s</text>'
             % (W - PAD - 4, ly, MUTED, html.escape(foot)))
    o.append("</svg>")
    return "\n".join(o)


def functions_data(sections: List[Tuple[dict, List[dict]]]) -> str:
    """The page's hover data: [name, unit, size, state or fuzzy %] per function; each block's data-i is its index."""
    return json.dumps([[f["name"], f["unit"], f["size"], f["state"] or round(f["fuzzy"], 1)]
                       for _, fs in sections for f in fs], separators=(",", ":"))


# --- history ------------------------------------------------------------------------------------------------------

def utc(date: str) -> str:
    """An ISO 8601 date in UTC, so that the dates sort as text and the charts ignore daylight saving time."""
    from datetime import datetime, timezone
    return datetime.fromisoformat(date.replace("Z", "+00:00")).astimezone(timezone.utc).isoformat()


def history_point(report: dict, commit: str, date: str) -> dict:
    m = report.get("measures", {})
    return {
        "date": date,
        "commit": commit,
        "total_code": int(num(m.get("total_code"))),
        "matched_code": int(num(m.get("matched_code"))),
        "matched_code_percent": round(num(m.get("matched_code_percent")), 4),
        "fuzzy_match_percent": round(num(m.get("fuzzy_match_percent")), 4),
        **({"regs_only_code": int(m["regs_only_code"]), "regs_only_percent": round(num(m["regs_only_percent"]), 4)}
           if "regs_only_code" in m else {}),
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
                by_commit[p["commit"]] = {**p, "date": utc(p["date"])}
    if point and point.get("commit"):
        by_commit[point["commit"]] = point
    elif point:
        by_commit["(local)"] = point
    return sorted(by_commit.values(), key=lambda p: (p["date"], p["commit"]))


HIST_STYLE = (".grid { stroke: #30363d; stroke-width: 1; } .tick { fill: #8b949e; font-size: 12px; }"
              " .mc { fill: none; stroke: #3fb950; stroke-width: 2.5; }"
              " .rg { fill: none; stroke: #388bfd; stroke-width: 2; }"
              " .fz { fill: none; stroke: #d29922; stroke-width: 1.5; stroke-dasharray: 5 4; }")


def history_svg(hist: List[dict], standalone: bool = False) -> str:
    """Matched code, matched + registers only (from the first build that has it) and fuzzy match over time, as step
    lines. standalone: an image file with its own ground,
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
    ymax = max(max(p["matched_code_percent"] + p.get("regs_only_percent", 0.0), p.get("fuzzy_match_percent", 0.0))
               for p in pts)
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

    def step_path(key, add=None):
        s = []
        prev = None
        for p in pts:
            if key not in p:
                continue
            x, y = X(t(p)), Y(p[key] + (p[add] if add else 0.0))
            if prev is None:
                s.append("M%s %s" % (f1(x), f1(y)))
            else:
                s.append("H%s V%s" % (f1(x), f1(y)))
            prev = y
        s.append("H%s" % f1(X(t1)))
        return " ".join(s)
    o.append('<path d="%s" class="fz"/>' % step_path("fuzzy_match_percent"))
    if any("regs_only_percent" in p for p in pts):
        o.append('<path d="%s" class="rg"/>' % step_path("regs_only_percent", "matched_code_percent"))
    o.append('<path d="%s" class="mc"/>' % step_path("matched_code_percent"))
    if standalone:
        kx, ky = l, 20
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="mc"/>' % (kx, kx + 22, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">matched code</text>' % (kx + 28, ky))
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="fz"/>' % (kx + 130, kx + 152, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">fuzzy match</text>' % (kx + 158, ky))
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="rg"/>' % (kx + 260, kx + 282, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">matched + registers only</text>' % (kx + 288, ky))
    o.append("</svg>")
    return "\n".join(o)


DAY = 86400


def day_window(hist: List[dict]) -> List[dict]:
    """The points of the last 24 hours before the latest build, led by the value at the window's start (the last
    point before it, moved to the start)."""
    from datetime import datetime, timezone
    pts = [p for p in hist if p.get("date")]
    if not pts:
        return []

    def t(p):
        return datetime.fromisoformat(p["date"].replace("Z", "+00:00")).timestamp()
    t1 = t(pts[-1])
    t0 = t1 - DAY
    win = [p for p in pts if t(p) >= t0]
    before = [p for p in pts if t(p) < t0]
    if before:
        tz = datetime.fromisoformat(pts[-1]["date"].replace("Z", "+00:00")).tzinfo or timezone.utc
        win.insert(0, dict(before[-1], date=datetime.fromtimestamp(t0, tz).isoformat(), carried=True))
    return win


def day_svg(hist: List[dict], standalone: bool = False) -> str:
    """Matched code and matched + registers only over the 24 hours before the latest build, as step lines on an axis
    zoomed to their range (a day moves them by tenths of a point), with the day's gain in the corner."""
    pts = day_window(hist)
    if len(pts) < 2:
        return ""
    from datetime import datetime

    def t(p):
        return datetime.fromisoformat(p["date"].replace("Z", "+00:00")).timestamp()
    w, h, l, r, top, bot = 1000, 220, 60, 28, (40 if standalone else 30), 30
    t1 = t(pts[-1])
    t0 = t1 - DAY
    has_rg = any("regs_only_percent" in p for p in pts)  # from the first build that has it
    vals = [p["matched_code_percent"] for p in pts]
    vals += [p["matched_code_percent"] + p["regs_only_percent"] for p in pts if "regs_only_percent" in p]
    lo, hi = min(vals), max(vals)
    span = max(hi - lo, 0.05)
    step = next((s for s in (0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20) if span / s <= 5), 25)
    ybot = step * int(lo / step)
    ytop = step * (int(hi / step) + 1)

    def X(tt):
        return l + (w - l - r) * (tt - t0) / (t1 - t0)

    def Y(v):
        return h - bot - (h - top - bot) * (v - ybot) / (ytop - ybot)
    label = "Matched code, last 24 hours"
    if standalone:
        o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="%s" '
             'role="img" aria-label="%s">' % (w, h, w, h, html.escape(FONT), label),
             '<style>%s</style>' % HIST_STYLE, '<rect width="%d" height="%d" fill="%s"/>' % (w, h, BG)]
    else:
        o = ['<svg viewBox="0 0 %d %d" role="img" aria-label="%s" class="hist">' % (w, h, label)]
    nd = 2 if step < 0.1 else 1
    v = ybot
    while v <= ytop + 1e-9:
        o.append('<line x1="%d" x2="%d" y1="%s" y2="%s" class="grid"/>' % (l, w - r, f1(Y(v)), f1(Y(v))))
        o.append('<text x="%d" y="%s" text-anchor="end" class="tick">%.*f%%</text>' % (l - 8, f1(Y(v) + 4), nd, v))
        v += step
    # hour ticks every 3 hours, in the latest build's time zone
    tz = datetime.fromisoformat(pts[-1]["date"].replace("Z", "+00:00")).tzinfo
    tick = datetime.fromtimestamp(t0, tz).replace(minute=0, second=0, microsecond=0).timestamp()
    while tick <= t1:
        dt = datetime.fromtimestamp(tick, tz)
        if tick >= t0 and dt.hour % 3 == 0:
            lab = dt.strftime("%b %d") if dt.hour == 0 else dt.strftime("%H:%M")
            o.append('<line x1="%s" x2="%s" y1="%d" y2="%d" class="grid"/>' % (f1(X(tick)), f1(X(tick)), h - bot, h - bot + 4))
            o.append('<text x="%s" y="%d" text-anchor="middle" class="tick">%s</text>' % (f1(X(tick)), h - 8, lab))
        tick += 3600

    def step_path(key, add=None):
        s = []
        for p in pts:
            if key not in p:
                continue
            x, y = X(t(p)), Y(p[key] + (p[add] if add else 0.0))
            s.append("M%s %s" % (f1(x), f1(y)) if not s else "H%s V%s" % (f1(x), f1(y)))
        s.append("H%s" % f1(X(t1)))
        return " ".join(s)
    if has_rg:
        o.append('<path d="%s" class="rg"/>' % step_path("regs_only_percent", "matched_code_percent"))
    o.append('<path d="%s" class="mc"/>' % step_path("matched_code_percent"))
    for p in pts:
        if not p.get("carried"):
            o.append('<circle cx="%s" cy="%s" r="3" fill="#3fb950"><title>%s %s: %.2f%%</title></circle>'
                     % (f1(X(t(p))), f1(Y(p["matched_code_percent"])), html.escape(p["date"][:16].replace("T", " ")),
                        html.escape((p.get("commit") or "")[:7]), p["matched_code_percent"]))
    a, b = pts[0], pts[-1]
    gain = "%+.2f pp matched · %+d functions · %s bytes · %d landings" % (
        b["matched_code_percent"] - a["matched_code_percent"], b["matched_functions"] - a["matched_functions"],
        format(b["matched_code"] - a["matched_code"], "+,"), sum(1 for p in pts if not p.get("carried")))
    o.append('<text x="%d" y="%d" text-anchor="end" class="tick">%s</text>' % (w - r, top - 12, html.escape(gain)))
    if standalone:
        kx, ky = l, 20
        o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="mc"/>' % (kx, kx + 22, ky - 4, ky - 4))
        o.append('<text x="%d" y="%d" class="tick">matched code</text>' % (kx + 28, ky))
        if has_rg:
            o.append('<line x1="%d" x2="%d" y1="%d" y2="%d" class="rg"/>' % (kx + 130, kx + 152, ky - 4, ky - 4))
            o.append('<text x="%d" y="%d" class="tick">matched + registers only</text>' % (kx + 158, ky))
    o.append("</svg>")
    return "\n".join(o)


def daily_points(hist: List[dict]) -> List[dict]:
    """One row per calendar day (the latest build's time zone): the day's last build, its gain over the day before
    and the number of builds that day."""
    from datetime import datetime
    pts = [p for p in hist if p.get("date")]
    if not pts:
        return []
    tz = datetime.fromisoformat(pts[-1]["date"].replace("Z", "+00:00")).tzinfo
    days: Dict[str, dict] = {}
    for p in pts:
        d = datetime.fromisoformat(p["date"].replace("Z", "+00:00")).astimezone(tz).strftime("%Y-%m-%d")
        row = days.setdefault(d, {"day": d, "builds": 0})
        row["builds"] += 1
        row["last"] = p
    from datetime import date, timedelta
    rows, d, end = [], date.fromisoformat(min(days)), date.fromisoformat(max(days))
    while d <= end:  # every calendar day; a day without builds keeps the day before's value
        k = d.isoformat()
        rows.append(days.get(k) or {"day": k, "builds": 0, "last": rows[-1]["last"]})
        d += timedelta(days=1)
    prev = None
    for row in rows:
        p = row["last"]
        row["pct"] = p["matched_code_percent"]
        row["fuzzy"] = p.get("fuzzy_match_percent", 0.0)
        row["fz_gain"] = row["fuzzy"] - (prev.get("fuzzy_match_percent", 0.0) if prev else 0.0)
        row["gain"] = p["matched_code_percent"] - (prev["matched_code_percent"] if prev else 0.0)
        row["fn_gain"] = p["matched_functions"] - (prev["matched_functions"] if prev else 0)
        prev = p
    return rows


def daily_svg(hist: List[dict], standalone: bool = False) -> str:
    """Matched code at the end of each day of the project (bars, labelled with the percentage), the day's gain
    (the label above each bar) and fuzzy match (an amber bar behind, labelled at its top)."""
    rows = daily_points(hist)
    if len(rows) < 2:
        return ""
    from datetime import date as _date
    w, h, l, r, top, bot = 1000, 280, 52, 16, (44 if standalone else 26), 34
    ymax = max(max(row["pct"], row["fuzzy"]) for row in rows)
    steps = [1, 2, 5, 10, 20, 25, 50, 100]
    step = next((s for s in steps if ymax / s <= 5), 100)
    ytop = max(step, step * (int(ymax / step) + 1))

    def Y(v):
        return h - bot - (h - top - bot) * v / ytop
    label = "Matched code and fuzzy match at the end of each day"
    if standalone:
        o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" font-family="%s" '
             'role="img" aria-label="%s">' % (w, h, w, h, html.escape(FONT), label),
             '<style>%s .bar { fill: #3fb950; } .fzbar { fill: #d29922; fill-opacity: .3; } .val { fill: #e6edf3; '
             'font-size: 11px; } .up { fill: #3fb950; font-size: 11px; } .fzv { fill: #d29922; font-size: 11px; }'
             '</style>' % HIST_STYLE, '<rect width="%d" height="%d" fill="%s"/>' % (w, h, BG)]
    else:
        o = ['<svg viewBox="0 0 %d %d" role="img" aria-label="%s" class="hist daily">' % (w, h, label)]
    v = 0.0
    while v <= ytop + 1e-9:
        o.append('<line x1="%d" x2="%d" y1="%s" y2="%s" class="grid"/>' % (l, w - r, f1(Y(v)), f1(Y(v))))
        o.append('<text x="%d" y="%s" text-anchor="end" class="tick">%s%%</text>' % (l - 8, f1(Y(v) + 4), f1(v)))
        v += step
    slot = (w - l - r) / len(rows)
    bw = max(2.0, slot * 0.7)
    every = max(1, int(56 / slot) + 1)  # date labels at most one per 56 px
    for i, row in enumerate(rows):
        x = l + slot * i + (slot - bw) / 2
        y = Y(row["pct"])
        d = _date.fromisoformat(row["day"])
        tip = "%s: %.2f%% matched (%+.2f pp, %+d functions, %d builds), %.2f%% fuzzy (%+.2f pp)" % (
            d.strftime("%b %d"), row["pct"], row["gain"], row["fn_gain"], row["builds"], row["fuzzy"], row["fz_gain"])
        fy = Y(row["fuzzy"])
        if row["fuzzy"] > row["pct"]:  # fuzzy counts every matched byte, so it is never below the green bar
            o.append('<rect x="%s" y="%s" width="%s" height="%s" rx="2" class="fzbar"><title>%s</title></rect>'
                     % (f1(x), f1(fy), f1(bw), f1(h - bot - fy), html.escape(tip)))
        o.append('<rect x="%s" y="%s" width="%s" height="%s" rx="2" class="bar"><title>%s</title></rect>'
                 % (f1(x), f1(y), f1(bw), f1(h - bot - y), html.escape(tip)))
        cx = x + bw / 2
        if bw >= 30:
            o.append('<text x="%s" y="%s" text-anchor="middle" class="val">%.1f%%</text>' % (f1(cx), f1(y - 5), row["pct"]))
            o.append('<text x="%s" y="%s" text-anchor="middle" class="up">%+.2f</text>' % (f1(cx), f1(y - 18), row["gain"]))
            if row["fuzzy"] > row["pct"]:  # above the two matched labels when the bars' tops are close
                o.append('<text x="%s" y="%s" text-anchor="middle" class="fzv">%.1f%%</text>'
                         % (f1(cx), f1(min(fy - 5, y - 31)), row["fuzzy"]))
        if i % every == 0 or i == len(rows) - 1:
            o.append('<text x="%s" y="%d" text-anchor="middle" class="tick">%s</text>' % (f1(cx), h - 12, d.strftime("%b %d")))
    if standalone:
        o.append('<text x="%d" y="22" class="tick">matched code at the end of each day · above each bar: the day\'s '
                 'gain in points · amber: fuzzy match</text>' % l)
    o.append("</svg>")
    return "\n".join(o)


# --- page ---------------------------------------------------------------------------------------------------------

PAGE_CSS = """
:root { --bg: #0d1117; --panel: #161b22; --line: #30363d; --fg: #e6edf3; --muted: #8b949e; --green: #3fb950;
  --amber: #d29922; --blue: #388bfd; color-scheme: dark; }
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
.big { font: 700 clamp(32px, 6vw, 60px)/1 'IBM Plex Mono', Consolas, monospace; color: var(--green); }
.big .fz { color: var(--amber); } .big .rg { color: var(--blue); } .big .sep { color: var(--muted); font-weight: 400; }
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
.track i.f { background: var(--amber); } .track i.r { background: var(--blue); }
.track i.m { background: var(--green); }
.hist { width: 100%; height: auto; display: block; }
.hist .grid { stroke: var(--line); stroke-width: 1; }
.hist .tick { fill: var(--muted); font-size: 12px; font-family: 'IBM Plex Mono', Consolas, monospace; }
.hist .mc { fill: none; stroke: var(--green); stroke-width: 2.5; }
.hist .fz { fill: none; stroke: var(--amber); stroke-width: 1.5; stroke-dasharray: 5 4; }
.hist .rg { fill: none; stroke: var(--blue); stroke-width: 2; }
.daily .bar { fill: var(--green); } .daily .bar:hover { fill: #56d364; }
.daily .val { fill: var(--fg); font-size: 11px; font-family: 'IBM Plex Mono', Consolas, monospace; }
.daily .up { fill: var(--green); font-size: 11px; font-family: 'IBM Plex Mono', Consolas, monospace; }
.daily .fzbar { fill: var(--amber); fill-opacity: .3; }
.daily .fzv { fill: var(--amber); font-size: 11px; font-family: 'IBM Plex Mono', Consolas, monospace; }
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
  // the function map: each block names its function by data-i (an index into the page's data)
  var grid = document.querySelector('svg.fgrid'), data = document.getElementById('fdata');
  if (!grid || !data) return;
  var fn = JSON.parse(data.textContent);
  var states = { m: 'matched', r: 'matched except for registers', n: 'not written yet' };
  function at(e) {
    var i = e.target.getAttribute ? e.target.getAttribute('data-i') : null;
    return i === null ? null : fn[+i];
  }
  function gshow(e) {
    var f = at(e);
    if (!f) { tip.hidden = true; return; }
    tip.textContent = '';
    var b = document.createElement('b'); b.textContent = f[0]; tip.appendChild(b);
    var st = states[f[3]] || (f[3] + '% fuzzy match');
    tip.appendChild(document.createTextNode('\\n' + f[1] + '\\n' + f[2] + ' bytes · ' + st));
    tip.hidden = false;
    var x = e.clientX + 14, y = e.clientY + 14, w = tip.offsetWidth, h = tip.offsetHeight;
    if (x + w > window.innerWidth - 8) x = Math.max(8, e.clientX - w - 14);
    if (y + h > window.innerHeight - 8) y = Math.max(8, e.clientY - h - 14);
    tip.style.left = x + 'px'; tip.style.top = y + 'px';
  }
  grid.addEventListener('mousemove', gshow);
  grid.addEventListener('click', gshow);
  grid.addEventListener('mouseleave', function () { tip.hidden = true; });
})();
"""


def page_html(report: dict, groups: List[dict], hist: List[dict], commit: str, date: str, repo: str,
              sections: Optional[List[Tuple[dict, List[dict]]]] = None) -> str:
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
         '<title>%s</title>' % html.escape(PAGE_TITLE),
         '<meta name="description" content="Progress of the %s (%s, %s) %s decompilation.">'
         % tuple(html.escape(v) for v in (TITLE, PLATFORM, GAME_ID, BINARY)),
         '<link rel="preconnect" href="https://fonts.googleapis.com">',
         '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600;700'
         '&family=IBM+Plex+Sans:wght@400;500;600&display=swap">',
         '<style>%s</style>' % PAGE_CSS, '</head>', '<body>', '<main>']
    o.append('<header><div><h1>%s</h1><p class="sub">Decompilation of <span class="num">%s</span> %s: '
             'code matched byte for byte</p></div><div class="big" title="matched / registers only / fuzzy">%s'
             '<span class="sep"> / </span><span class="rg">%s</span><span class="sep"> / </span>'
             '<span class="fz">%s</span></div></header>'
             % (e(TITLE), e(GAME_ID), e(BINARY), pct(num(m.get("matched_code_percent"))), pct(num(m.get("regs_only_percent"))),
                pct(num(m.get("fuzzy_match_percent")))))
    stats = [
        ("%s / %s" % (format(matched_code, ","), format(total_code, ",")), "code bytes matched"),
        ("%s (%s)" % (pct(num(m.get("regs_only_percent"))), format(int(num(m.get("regs_only_code"))), ",")),
         "code bytes that match except for registers"),
        (pct(num(m.get("fuzzy_match_percent"))), "fuzzy match (source that compiles to nearly the same code)"),
        ("%s / %s" % (format(funcs_matched, ","), format(funcs, ",")), "functions matched"),
        ("%s / %s" % (format(matched_data, ","), format(total_data, ",")), "data bytes matched"),
    ]
    o.append('<section class="stats">' + "".join('<div class="stat"><b>%s</b><span>%s</span></div>' % (e(a), e(b))
                                                 for a, b in stats) + '</section>')
    o.append('<section class="map" aria-label="Treemap of the code">%s</section>'
             % treemap_svg(report, groups, commit, date, for_html=True))
    if sections:
        o.append('<section class="map"><h2>Every function</h2>%s<script type="application/json" id="fdata">%s'
                 '</script></section>' % (functions_svg(sections, commit, date, for_html=True),
                                         functions_data(sections).replace("</", "<\\/")))
    rows = []
    for g in groups:
        rows.append('<tr><td>%s</td><td class="num">%s</td><td class="num">%s</td><td class="num">%s</td>'
                    '<td class="num">%s</td><td class="num">%s / %s</td><td class="num">%s</td><td class="bar">'
                    '<div class="track"><i class="f" style="width:%s%%"></i><i class="r" style="width:%s%%"></i>'
                    '<i class="m" style="width:%s%%"></i></div></td></tr>'
                    % (e(g["name"]), kb(g["code"]), pct(g["matched_pct"]), pct(g["regs_pct"]), pct(g["fuzzy"]),
                       format(g["funcs_matched"], ","), format(g["funcs"], ","),
                       pct(100.0 * g["split"] / g["code"]), f1(min(100.0, g["fuzzy"])),
                       f1(min(100.0, g["matched_pct"] + g["regs_pct"])), f1(min(100.0, g["matched_pct"]))))
    o.append('<section><h2>By library</h2><div class="scroll"><table><thead><tr><th>Library</th><th>Code</th>'
             '<th>Matched</th><th>Registers only</th><th>Fuzzy</th><th>Functions matched</th><th>Split into source units</th>'
             '<th></th></tr></thead><tbody>%s</tbody></table></div></section>' % "".join(rows))
    dly = daily_svg(hist)
    if dly:
        o.append('<section><h2>Progress by day</h2>%s<div class="key"><span><i style="background:var(--green)"></i>'
                 'matched code at the end of each day; above each bar: the day\'s gain in points</span><span>'
                 '<i style="background:var(--amber)"></i>fuzzy match</span><span>hover a bar for functions and builds'
                 '</span></div></section>' % dly)
    ds = day_svg(hist)
    if ds:
        o.append('<section><h2>Last 24 hours</h2>%s<div class="key"><span><i style="background:var(--green)">'
                 '</i>matched code</span><span><i style="background:var(--blue)"></i>matched + registers only</span>'
                 '<span>dots: landings on main (hover for the commit)</span></div></section>' % ds)
    hs = history_svg(hist)
    if hs:
        o.append('<section><h2>Matched code over time</h2>%s<div class="key"><span><i style="background:var(--green)">'
                 '</i>matched code</span><span><i style="background:var(--blue)"></i>matched + registers only</span>'
                 '<span><i style="background:var(--amber)"></i>fuzzy match</span>'
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
        links = ' · <a href="%s">repository</a>' % repo_url
    o.append('<footer>%s. Groups for code not split yet follow a coarse address layout; sizes and '
             'percentages come from objdiff\'s report.%s</footer>' % (" ".join(foot) or "Local build", links))
    js = PAGE_JS
    o.append('</main><div id="tip" hidden></div><script>%s</script></body></html>' % js)
    return "\n".join(o)


def branch_readme(report: dict, groups: List[dict], hist: List[dict], commit: str, date: str, repo: str) -> str:
    """README.md of the progress branch: what GitHub shows when the branch is opened."""
    m = report.get("measures", {})
    o = ["# %s: decompilation progress" % TITLE, "", "![Treemap of %s's code](progress.svg)" % BINARY, "",
         "![Every function as a block sized by its code, grouped by state](functions.svg)", ""]
    o.append("**%s** of the code matched byte for byte (%s of %s bytes) · %s more matches except for registers · "
             "%s of %s functions · %s fuzzy match · data %s of %s bytes"
             % (pct(num(m.get("matched_code_percent"))), format(int(num(m.get("matched_code"))), ","),
                format(int(num(m.get("total_code"))), ","), pct(num(m.get("regs_only_percent"))),
                format(int(num(m.get("matched_functions"))), ","),
                format(int(num(m.get("total_functions"))), ","), pct(num(m.get("fuzzy_match_percent"))),
                format(int(num(m.get("matched_data"))), ","), format(int(num(m.get("total_data"))), ",")))
    o += ["", "| Library | Code | Matched | Registers only | Fuzzy | Functions matched | Split into source units |",
          "|---|--:|--:|--:|--:|--:|--:|"]
    for g in groups:
        o.append("| %s | %s | %s | %s | %s | %s / %s | %s |"
                 % (g["name"], kb(g["code"]), pct(g["matched_pct"]), pct(g["regs_pct"]), pct(g["fuzzy"]),
                    format(g["funcs_matched"], ","),
                    format(g["funcs"], ","), pct(100.0 * g["split"] / g["code"])))
    if len(daily_points(hist)) >= 2:
        o += ["", "## Progress by day", "", "![Matched code and fuzzy match at the end of each day](daily.svg)"]
    if len(day_window(hist)) >= 2:
        o += ["", "## Last 24 hours", "", "![Matched code over the last 24 hours](day.svg)"]
    if len(hist) >= 2:
        o += ["", "## Matched code over time", "", "![Matched code over time](history.svg)", "",
              "%d builds of main." % len(hist)]
    src = commit[:7]
    if commit and repo:
        src = "[%s](https://github.com/%s/commit/%s)" % (commit[:7], repo, commit)
    o += ["", "---", "",
          "Generated by `tools/progress_map.py` from the build of %s%s. tools/progress_publish.py adds one commit to "
          "this branch per landing on main, from a local build; nothing else is kept here. Opened on its own ([progress.svg](progress.svg)), the map "
          "names each box on hover; `index.html` is the same as a page (download it and open it in a browser). Code "
          "not split yet is grouped by a coarse address layout; sizes and percentages come from "
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
    p.add_argument("--regs", help="tools/regsonly.py's JSON: functions that match except for registers")
    a = p.parse_args(argv)
    if a.date:
        a.date = utc(a.date)
    with open(a.report, encoding="utf-8") as fh:
        report = json.load(fh)
    regs = None
    if a.regs:
        with open(a.regs, encoding="utf-8") as fh:
            regs = json.load(fh)
    units = load_units(report, regs)
    m = report.setdefault("measures", {})
    if regs is not None:
        m["regs_only_code"] = sum(u["regs"] for u in units)
        m["regs_only_percent"] = 100.0 * m["regs_only_code"] / max(1.0, num(m.get("total_code")))
    groups = group_totals(units)
    if not groups:
        print("progress_map: no units with code in %s" % a.report, file=sys.stderr)
        return 1
    point = history_point(report, a.commit, a.date) if a.date else None
    hist = merge_history(a.history, point)
    sections = load_functions(report, regs, groups)
    os.makedirs(a.out, exist_ok=True)
    files = {
        "progress.svg": treemap_svg(report, groups, a.commit, a.date, for_html=False) + "\n",
        "functions.svg": functions_svg(sections, a.commit, a.date, for_html=False) + "\n",
        "index.html": page_html(report, groups, hist, a.commit, a.date, a.repo, sections) + "\n",
        "history.svg": history_svg(hist, standalone=True) + "\n",
        "day.svg": day_svg(hist, standalone=True) + "\n",
        "daily.svg": daily_svg(hist, standalone=True) + "\n",
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
