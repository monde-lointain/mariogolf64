#!/usr/bin/env python3
"""Style + drift guard for the agent-facing prompt surfaces.

docs/prompt-style.md states the house rules for every surface an agent executes:
CLAUDE.md, AGENTS.md, docs/agent-workflow.md, docs/hazards.md, the on-demand
references, the Claude command wrappers, and the Codex skills. Its conformance
checklist has always been a human gate, and the two largest surfaces violate it
roughly 1780 times. This tool mechanizes the checkable half.

Deliberately a sibling of hazard_anchors.py rather than an extension of it: that
tool is narrow, frozen, and owns one allowlist file, and conflating scopes would
make its `freeze` mean two things. It is imported here for the slug/resolve
semantics so both tools agree on what a citation is.

Checks:
  xref        Every `<path>.md ## <Heading>` / doc-scoped `## <Heading>` citation
              resolves to exactly one heading (0 = dangling, >=2 = ambiguous).
  caps        Emphasis ALL-CAPS outside backticks, defines, filenames, acronyms.
  provenance  S### inside a Rule / Trigger / Procedure line.
  budget      Per-file byte budget, plus a per-`##`-section ceiling.
  mirror      The invariant block is byte-identical in CLAUDE.md and AGENTS.md.
  sha1        No hardcoded 40-hex ROM hash (cite tools/verify-rom.sh instead).
  nesting     Bullet depth, as the "nested a sharpening instead of replacing it"
              signal.
  preamble    A surface over PREAMBLE_MIN_LINES opens with a <role> block.
  citesites   Per-anchor citation-site counts (not pass/fail; the move oracle).

`caps`, `provenance` and `nesting` are counted, not zeroed: hazards.md alone
carries ~1420 emphasis-caps and 879 S###, so a hard rule would block every
sprint's retro edits on day one. They ratchet against a frozen baseline instead,
so a counter may fall or hold but never rise. Same UX as hazard_anchors.py:
`freeze` rewrites the baseline as a reviewable diff.

Subcommands:
  check      Exit non-zero on any hard failure. The gate tests/tooling asserts.
  report     Per-file table: bytes vs budget, caps, S###, nesting, delta vs baseline.
  budgets    Budgets and headroom, sorted by headroom.
  citesites  Per-anchor citation-site counts (--json, --diff <file>).
  freeze     Rewrite the baseline from the current tree.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hazard_anchors as h  # noqa: E402  (path set above)

ROOT = Path(__file__).resolve().parent.parent
TESTDATA = ROOT / "tests" / "tooling"
BASELINE_FILE = TESTDATA / "prompt_lint_baseline.json"
BUDGETS_FILE = TESTDATA / "prompt_budgets.txt"
CAPS_ALLOW_FILE = TESTDATA / "prompt_caps_allow.txt"

# The surfaces an agent executes. The append-only logs (BACKLOG/RETRO/VELOCITY)
# and docs/wip/* are out of scope per docs/prompt-style.md: their headings are
# frozen anchors but their historical entry prose is not instruction text.
# SPRINT.md is handled separately by `--sprint` (gitignored, so it must never
# gate CI; a fresh clone has none).
SURFACE_GLOBS = (
    "CLAUDE.md",
    "AGENTS.md",
    "docs/agent-workflow.md",
    "docs/workflow/*.md",
    "docs/hazard-index.md",
    "docs/hazards.md",
    "docs/coding-style.md",
    "docs/prompt-style.md",
    "docs/fanout-prompt.md",
    "docs/levers.md",
    ".claude/commands/*.md",
    ".agents/skills/*/SKILL.md",
)

# SPRINT.md is a prompt surface in practice -- it is read at every resume and its
# `## Method` is pure procedure -- but it is gitignored and regenerated each
# sprint, so only these sections are instruction text, and only under `--sprint`.
SPRINT_FILE = ROOT / "SPRINT.md"
SPRINT_INSTRUCTION_SECTIONS = ("Method", "Definition of Done", "Scope")

# Checks that fail the build. The rest report and ratchet. `section-budget` joins
# once the split satisfies it; `preamble` once the surfaces carry <role>.
HARD_CHECKS = {"xref", "budget", "mirror", "sha1"}

SECTION_BUDGET_BYTES = 24576  # ~6K tokens: one section should fit a single read.
PREAMBLE_MIN_LINES = 100
MAX_BULLET_DEPTH = 3

HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
BULLET_RE = re.compile(r"^(\s*)[-*+]\s+")
SHA1_RE = re.compile(r"\b[0-9a-f]{40}\b")
STAG_RE = re.compile(r"\bS\d{1,3}\b")
FUNC_RE = re.compile(r"\bfunc_[0-9a-fA-F]{8}\b")
ROLE_RE = re.compile(r"^<role>\s*$", re.M)
# Keeps a contraction whole so `DON'T` counts once rather than as `DON` + `T`.
CAPS_TOKEN_RE = re.compile(r"\b[A-Z][A-Z0-9]{1,}(?:'[A-Z]+)?\b")
PROVENANCE_LABEL_RE = re.compile(r"^\s*[-*]?\s*\*\*(Rule|Trigger|Procedure)[.:]\*\*")
# A bold sub-case lead-in that carries its sprint tag inline: `**... (S282).**`.
PROVENANCE_LEADIN_RE = re.compile(r"^\s*[-*]?\s*\*\*[^*]*\(S\d{1,3}[^)]*\)[.:]?\*\*")
MIRROR_BEGIN = "<!-- mg64:invariants:begin -->"
MIRROR_END = "<!-- mg64:invariants:end -->"

# A citation naming its file: `docs/foo.md ## Heading`.
XREF_QUALIFIED_RE = re.compile(r"`([A-Za-z0-9_./-]+\.md)\s+##\s+([^`]+)`")
# A citation relying on a nearby file mention: `## Heading`.
XREF_BARE_RE = re.compile(r"`##\s+([^`]+)`")
MD_PATH_RE = re.compile(r"`([A-Za-z0-9_./-]+\.md)`")


def _strip_code(text):
    """Remove fenced blocks and inline code spans. Those carry literal tokens
    (defines, filenames, make variables, register names) that are never emphasis.

    The inline pattern deliberately allows a newline: these docs wrap at ~100
    columns, so a backticked command often straddles a line break, and a
    `[^`\\n]*` pattern would leave its contents to be counted as shouting."""
    text = re.sub(r"```.*?```", "", text, flags=re.S)
    text = re.sub(r"`[^`]*`", "", text)
    return text


def surfaces():
    """Every in-scope surface that exists, in a stable order."""
    out = []
    for pat in SURFACE_GLOBS:
        if any(c in pat for c in "*?["):
            out.extend(sorted(ROOT.glob(pat)))
        else:
            p = ROOT / pat
            if p.exists():
                out.append(p)
    return out


def rel(path):
    return str(Path(path).resolve().relative_to(ROOT))


def load_caps_allow():
    """One token per line; `#` starts a comment, whole-line or trailing."""
    if not CAPS_ALLOW_FILE.exists():
        return set()
    out = set()
    for line in CAPS_ALLOW_FILE.read_text().splitlines():
        tok = line.split("#", 1)[0].strip()
        if tok:
            out.add(tok)
    return out


def load_budgets():
    out = {}
    if not BUDGETS_FILE.exists():
        return out
    for line in BUDGETS_FILE.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        path, _, cap = line.rpartition(" ")
        out[path.strip()] = int(cap)
    return out


def load_baseline():
    if not BASELINE_FILE.exists():
        return {}
    return json.loads(BASELINE_FILE.read_text())


# --- individual checks -------------------------------------------------------


def caps_hits(text, allow):
    """Emphasis ALL-CAPS tokens. Skips code, and anything that carries meaning in
    caps: a token with `_` or a digit (define/macro/type/opt-level), an S### tag,
    and the domain acronym allowlist."""
    hits = []
    for m in CAPS_TOKEN_RE.finditer(_strip_code(text)):
        tok = m.group(0)
        if "_" in tok or any(c.isdigit() for c in tok):
            continue
        if STAG_RE.fullmatch(tok) or tok in allow:
            continue
        hits.append(tok)
    return hits


def provenance_hits(text):
    """S### in a line that states a rule rather than its provenance.

    Two shapes. The labelled `**Rule:**` / `**Trigger:**` / `**Procedure:**` lines
    are the letter of docs/prompt-style.md, and they are already clean across the
    tree (0 of 153 in hazards.md carry a tag). The violations live in the other
    shape: a bold sub-case lead-in that ends `... (S###).**`, which fuses the rule
    and its provenance into one line. That is the bulk of the real work, and it is
    repaired by stripping the trailing tag into the section's Provenance slot.

    A tag attached to a concrete worked example naming a function is the documented
    exception -- deleting it would lose the pointer to the evidence."""
    hits = []
    for line in text.splitlines():
        labelled = PROVENANCE_LABEL_RE.match(line)
        leadin = PROVENANCE_LEADIN_RE.match(line)
        if not (labelled or leadin):
            continue
        if FUNC_RE.search(line):
            continue
        stripped = re.sub(r"`[^`\n]*`", "", line)
        hits.extend(STAG_RE.findall(stripped))
    return hits


def max_nesting(text):
    """Deepest bullet level, 1-based. A 4th level is the mechanical tell that a
    sprint nested a sharpening under the rule it sharpens instead of replacing it."""
    depth = 0
    for line in _strip_code(text).splitlines():
        m = BULLET_RE.match(line)
        if m:
            depth = max(depth, len(m.group(1)) // 2 + 1)
    return depth


def sections(text):
    """(title, body_bytes) for each `##` section."""
    out, title, buf = [], None, []
    for line in text.splitlines(keepends=True):
        m = HEADING_RE.match(line.rstrip("\n"))
        if m and len(m.group(1)) == 2:
            if title is not None:
                out.append((title, sum(len(b) for b in buf)))
            title, buf = m.group(2), [line]
        else:
            buf.append(line)
    if title is not None:
        out.append((title, sum(len(b) for b in buf)))
    return out


def headings_of(path):
    try:
        return [t for _, t in h.headings(path.read_text())]
    except OSError:
        return None


def xref_failures():
    """Citations that do not resolve to exactly one heading.

    Two forms. A qualified `docs/foo.md ## Heading` names its target. A bare
    `## Heading` is resolved against the single .md path mentioned earlier in the
    same paragraph; zero or several such paths make the citation unscoped, which
    is a failure whose fix is always to backtick the path in that paragraph."""
    bad = []
    for path in surfaces():
        text = path.read_text()
        for m in XREF_QUALIFIED_RE.finditer(text):
            target, heading = m.group(1), m.group(2).strip()
            bad.extend(_resolve_xref(path, target, heading))
        for para in re.split(r"\n\s*\n", text):
            bare = list(XREF_BARE_RE.finditer(para))
            if not bare:
                continue
            paths = [p for p in MD_PATH_RE.findall(para)]
            qualified = {
                m.group(2).strip() for m in XREF_QUALIFIED_RE.finditer(para)
            }
            for m in bare:
                heading = m.group(1).strip()
                if heading in qualified:
                    continue
                uniq = sorted(set(paths))
                if len(uniq) != 1:
                    bad.append(
                        f"{rel(path)}: `## {heading}` is unscoped "
                        f"({len(uniq)} .md paths in its paragraph); backtick the target path"
                    )
                    continue
                bad.extend(_resolve_xref(path, uniq[0], heading))
    return bad


def _resolve_xref(src, target, heading):
    tpath = (ROOT / target).resolve()
    if not tpath.exists():
        return [f"{rel(src)}: `{target} ## {heading}` -> no such file"]
    titles = headings_of(tpath)
    if titles is None:
        return [f"{rel(src)}: `{target}` is unreadable"]
    exact = [t for t in titles if t == heading]
    if len(exact) == 1:
        return []
    pref = [t for t in titles if t.startswith(heading)]
    if len(pref) == 1:
        return []
    if not pref:
        return [f"{rel(src)}: `{target} ## {heading}` -> dangling (no such heading)"]
    return [
        f"{rel(src)}: `{target} ## {heading}` -> ambiguous, matches {len(pref)}: "
        + "; ".join(pref[:3])
    ]


def sha1_failures():
    allow = {"mariogolf64.yaml"}
    bad = []
    for path in surfaces():
        for m in SHA1_RE.finditer(path.read_text()):
            if rel(path) in allow:
                continue
            bad.append(
                f"{rel(path)}: hardcoded ROM hash {m.group(0)[:12]}...; "
                "cite tools/verify-rom.sh (it derives the hash from mariogolf64.yaml "
                "and asserts the OK line first)"
            )
    return bad


def mirror_failures():
    """The invariant block must be byte-identical in CLAUDE.md and AGENTS.md.
    Inert until the markers exist, so it can land before the Phase 8 edit."""

    def block(p):
        if not p.exists():
            return None
        t = p.read_text()
        i, j = t.find(MIRROR_BEGIN), t.find(MIRROR_END)
        return t[i + len(MIRROR_BEGIN) : j] if 0 <= i < j else None

    a, b = block(ROOT / "CLAUDE.md"), block(ROOT / "AGENTS.md")
    if a is None or b is None:
        return []
    if a != b:
        return ["CLAUDE.md and AGENTS.md invariant blocks differ; they must be byte-identical"]
    return []


def budget_failures():
    """Per-file byte budget. Hard: every surface is inside its cap today."""
    budgets = load_budgets()
    bad = []
    for path in surfaces():
        r = rel(path)
        cap = budgets.get(r)
        size = len(path.read_text().encode())
        if cap is not None and size > cap:
            bad.append(f"{r}: {size} B exceeds budget {cap} B (retire before you add)")
    return bad


def section_budget_failures():
    """Per-`##`-section ceiling: one section should fit a single read.

    Advisory until the restructure satisfies it. Two sections are over today and
    both are the reason the rework exists -- `## Conventions (every-sprint)` is
    oversized because the 139-row hazard index is nested inside it, so every
    conventions read drags the whole lookup table along. Promote to a hard check
    once the split lands."""
    bad = []
    for path in surfaces():
        r = rel(path)
        for title, nbytes in sections(path.read_text()):
            if nbytes > SECTION_BUDGET_BYTES:
                bad.append(
                    f"{r}: section '## {title[:50]}' is {nbytes} B, over the "
                    f"{SECTION_BUDGET_BYTES} B per-section ceiling; split it or lift a sub-part out"
                )
    return bad


def preamble_failures():
    bad = []
    for path in surfaces():
        text = path.read_text()
        if len(text.splitlines()) <= PREAMBLE_MIN_LINES:
            continue
        if not ROLE_RE.search(text):
            bad.append(
                f"{rel(path)}: over {PREAMBLE_MIN_LINES} lines and has no <role> block "
                "naming its audience and the phase that loads it"
            )
    return bad


def citesites():
    """Per-anchor citation-site counts across the harvested surfaces.

    hazard_anchors' `cited` emits the distinct anchor SET, so a deletion that
    drops the last citation of an anchor still cited elsewhere leaves the set --
    and its fingerprint -- unchanged. This counts sites, which is what makes a
    move or a deletion auditable."""
    counts = {}
    for path in h._scan_files(ROOT):
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue
        for m in h.EXTERNAL_RE.finditer(text):
            counts[m.group(1)] = counts.get(m.group(1), 0) + 1
        for line in text.splitlines():
            if h.HEADING_RE.match(line):
                continue
            for m in h.INTRA_RE.finditer(line):
                if h._keep_token(m.group(1), line, m.end()):
                    counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    return counts


# --- aggregation -------------------------------------------------------------


def measure():
    allow = load_caps_allow()
    out = {}
    for path in surfaces():
        text = path.read_text()
        out[rel(path)] = {
            "bytes": len(text.encode()),
            "lines": len(text.splitlines()),
            "caps": len(caps_hits(text, allow)),
            "provenance": len(provenance_hits(text)),
            "max_nesting": max_nesting(text),
        }
    return out


RATCHETED = ("caps", "provenance", "max_nesting")


def baseline_failures(now, base):
    bad = []
    for path, cur in sorted(now.items()):
        prev = base.get(path)
        if prev is None:
            continue
        for key in RATCHETED:
            if cur[key] > prev.get(key, cur[key]):
                bad.append(
                    f"{path}: {key} rose {prev[key]} -> {cur[key]}; "
                    "de-shout or retire elsewhere in this file, or re-freeze deliberately"
                )
    return bad


def run_checks():
    """Returns (hard_failures, advisories) as {check: [messages]}."""
    results = {
        "xref": xref_failures(),
        "budget": budget_failures(),
        "mirror": mirror_failures(),
        "section-budget": section_budget_failures(),
        "sha1": sha1_failures(),
        "preamble": preamble_failures(),
        "baseline": baseline_failures(measure(), load_baseline()),
    }
    hard = {k: v for k, v in results.items() if v and k in HARD_CHECKS}
    soft = {k: v for k, v in results.items() if v and k not in HARD_CHECKS}
    return hard, soft


def sprint_report():
    """Advisory pass over SPRINT.md's instruction sections. Never gates: the file
    is gitignored, so CI has none."""
    if not SPRINT_FILE.exists():
        return ["SPRINT.md absent (no sprint open)"]
    allow = load_caps_allow()
    text = SPRINT_FILE.read_text()
    out = []
    for title, _ in sections(text):
        if not any(title.startswith(s) for s in SPRINT_INSTRUCTION_SECTIONS):
            continue
        body = _section_body(text, title)
        n_caps, n_prov = len(caps_hits(body, allow)), len(provenance_hits(body))
        if n_caps or n_prov:
            out.append(f"SPRINT.md '## {title}': {n_caps} emphasis-caps, {n_prov} S### in rule lines")
    return out or ["SPRINT.md instruction sections are clean"]


def _section_body(text, want):
    keep, buf = False, []
    for line in text.splitlines(keepends=True):
        m = HEADING_RE.match(line.rstrip("\n"))
        if m and len(m.group(1)) == 2:
            keep = m.group(2) == want
            continue
        if keep:
            buf.append(line)
    return "".join(buf)


# --- CLI ---------------------------------------------------------------------


def _main(argv):
    cmd = argv[1] if len(argv) > 1 else "check"
    args = argv[2:]

    if cmd == "check":
        if "--sprint" in args:
            for line in sprint_report():
                print(line)
            return 0
        hard, soft = run_checks()
        for name, msgs in sorted(soft.items()):
            for m in msgs:
                print(f"warn [{name}] {m}")
        if not hard:
            n = len(surfaces())
            print(f"OK: {n} prompt surfaces pass the hard checks "
                  f"({', '.join(sorted(HARD_CHECKS))}).")
            return 0
        print("PROMPT LINT FAILED:", file=sys.stderr)
        for name, msgs in sorted(hard.items()):
            for m in msgs:
                print(f"  [{name}] {m}", file=sys.stderr)
        return 1

    if cmd == "report":
        now, base, budgets = measure(), load_baseline(), load_budgets()
        if "--json" in args:
            print(json.dumps(now, indent=2, sort_keys=True))
            return 0
        print(f"{'surface':<42}{'bytes':>9}{'budget':>9}{'caps':>7}{'S###':>7}{'nest':>6}")
        for path, cur in sorted(now.items()):
            cap = budgets.get(path)
            prev = base.get(path, {})

            def d(key):
                if key not in prev:
                    return ""
                delta = cur[key] - prev[key]
                return "" if delta == 0 else f"{delta:+d}"

            print(
                f"{path:<42}{cur['bytes']:>9}{(cap if cap else '-'):>9}"
                f"{str(cur['caps']) + d('caps'):>7}"
                f"{str(cur['provenance']) + d('provenance'):>7}"
                f"{str(cur['max_nesting']) + d('max_nesting'):>6}"
            )
        return 0

    if cmd == "budgets":
        now, budgets = measure(), load_budgets()
        rows = []
        for path, cur in now.items():
            cap = budgets.get(path)
            if cap:
                rows.append((cap - cur["bytes"], path, cur["bytes"], cap))
        for head, path, size, cap in sorted(rows):
            pct = 100.0 * size / cap
            print(f"{path:<42}{size:>9}/{cap:<9} {pct:5.1f}%  headroom {head:>8} B")
        return 0

    if cmd == "citesites":
        counts = citesites()
        if "--diff" in args:
            prev = json.loads(Path(args[args.index("--diff") + 1]).read_text())
            drops = {k: (v, counts.get(k, 0)) for k, v in prev.items() if counts.get(k, 0) < v}
            gone = [k for k, (_, now_n) in drops.items() if now_n == 0]
            for k, (was, now_n) in sorted(drops.items()):
                mark = "GONE" if now_n == 0 else "drop"
                print(f"{mark} {k}: {was} -> {now_n}")
            if gone:
                print(f"\n{len(gone)} anchor(s) lost their last citation", file=sys.stderr)
                return 1
            if not drops:
                print("no citation-site drops")
            return 0
        if "--json" in args:
            print(json.dumps(counts, indent=2, sort_keys=True))
            return 0
        for k, v in sorted(counts.items()):
            print(f"{v:>4}  {k}")
        return 0

    if cmd == "freeze":
        now = measure()
        BASELINE_FILE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE_FILE.write_text(json.dumps(now, indent=2, sort_keys=True) + "\n")
        print(f"wrote {rel(BASELINE_FILE)} ({len(now)} surfaces)")
        return 0

    print(
        "usage: prompt_lint.py [check [--sprint]|report [--json]|budgets|"
        "citesites [--json|--diff FILE]|freeze]",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
