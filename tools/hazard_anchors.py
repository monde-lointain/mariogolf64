#!/usr/bin/env python3
"""Anchor-stability guard for docs/hazards.md.

The ``##``/``###`` heading text in docs/hazards.md generates GitHub-style
``#anchors`` that the rest of the repo cites as grep-keys: CLAUDE.md's hazard
index, the sprint commands, BACKLOG/RETRO/VELOCITY, ~10 tools/*.py comments, and
the cross-references inside hazards.md itself. A heading edit that changes an
anchor silently orphans every citation of it. This tool makes that anchor set an
enforced invariant so the tooling suite catches such an orphan.

Subcommands:
  manifest   Print the generated anchor for every ##/### heading, one per line.
  cited      Print the distinct cited anchors harvested across the repo.
  classify   Classify each cited anchor against the current manifest as
             exact / prefix-unique / ambiguous / dangling (diagnostic).
  check      Exit non-zero if any cited anchor fails to resolve to exactly one
             heading. This is the invariant the tests/tooling test asserts.

The GitHub slugger: lowercase, drop every character that is not word/space/
hyphen, replace spaces with hyphens, de-duplicate collisions with a -1/-2 suffix.
A citation "resolves" when it equals a generated anchor (exact) or is the leading
slug of exactly one generated anchor (the project's established shorthand form,
e.g. `#file-static` for `file-static-bss-layout-conflict`). "Exactly one" is what
keeps the prefix form sound: a citation that matches zero headings (orphan) or two
(ambiguous) fails.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# tools/hazard_anchors.py -> tools/ -> project root
ROOT = Path(__file__).resolve().parent.parent
HAZARDS = ROOT / "docs" / "hazards.md"

HEADING_RE = re.compile(r"^(#{2,3})\s+(.*?)\s*$")

# External citations: `.../hazards.md#<anchor>` anywhere in the repo.
EXTERNAL_RE = re.compile(r"hazards\.md#([A-Za-z0-9_-]+)")
# Intra-file cross-references: a `#slug` token not glued to a preceding word char.
INTRA_RE = re.compile(r"(?<![A-Za-z0-9_])#([a-z][A-Za-z0-9_-]{2,})")

# `#slug` tokens that are C-preprocessor directives or other non-anchor noise, not
# hazards.md cross-references. Excluded from the cited set so they do not read as
# dangling citations.
NON_ANCHOR_TOKENS = frozenset(
    {
        "define", "ifdef", "ifndef", "include", "endif", "else", "elif",
        "pragma", "undef", "if", "error", "line", "warning", "import", "total",
    }
)

# Files/trees to scan for citations. Prose + tooling + state docs; skips vendored
# trees, build output, asm, and the venv.
CITATION_GLOBS = ("*.md", "*.py", "*.txt", "*.yaml", "*.toml")
# Append-only logs, excluded per docs/prompt-style.md ("their headings are frozen
# anchors, but their historical entry prose is out of scope"). A digest written when
# a shorthand was unique is a historical record, not a live pointer: once a sibling
# section lands, that shorthand reads as ambiguous and the only repair would be
# rewriting history. SPRINT.md is also gitignored, so scanning it makes the result
# depend on whether a sprint happens to be open.
SKIP_FILES = frozenset({"RETRO.md", "BACKLOG.md", "VELOCITY.md", "SPRINT.md"})
SKIP_DIRS = frozenset(
    {".git", "venv", "build", "asm", "nonmatchings", "assets", "expected",
     "__pycache__", ".mypy_cache", ".pytest_cache"}
)
# tools/m2c is a vendored third-party tool; its "template" files are unrelated.
SKIP_PATH_PARTS = frozenset({"m2c"})


def slugify_all(titles):
    """Return the GitHub anchors for an ordered list of heading titles, applying
    the -1/-2 dedupe suffix on collisions in document order."""
    seen = {}
    anchors = []
    for title in titles:
        base = re.sub(r"[^\w\s-]", "", title.lower())
        base = base.replace(" ", "-")
        n = seen.get(base, 0)
        seen[base] = n + 1
        anchors.append(base if n == 0 else f"{base}-{n}")
    return anchors


def headings(md_text):
    """Yield (level, title) for every ##/### heading line."""
    out = []
    for line in md_text.splitlines():
        m = HEADING_RE.match(line)
        if m:
            out.append((len(m.group(1)), m.group(2)))
    return out


def manifest(md_text=None):
    """Ordered list of generated anchors for hazards.md."""
    text = md_text if md_text is not None else HAZARDS.read_text()
    titles = [t for _, t in headings(text)]
    return slugify_all(titles)


def _keep_token(tok, text, end):
    """Drop non-anchor tokens: cpp directives, and the `#exact-100%` count notation
    (a `#token` immediately followed by `%`)."""
    if tok in NON_ANCHOR_TOKENS:
        return False
    if end < len(text) and text[end] == "%":
        return False
    return True


def harvest_cited(root=ROOT):
    """Distinct cited anchors across the repo. Two citation forms are harvested from
    every scanned file: the explicit `hazards.md#anchor` (used in prose, tools, and
    the commands) and the bare `#anchor` (used in CLAUDE.md's hazard index, the
    commands, and hazards.md's own cross-references). Bare tokens that do not resolve
    to a hazards heading are dropped downstream by classify(), so a same-file
    self-link in some other doc never enters the frozen allowlist."""
    cited = set()
    for path in _scan_files(root):
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue
        for m in EXTERNAL_RE.finditer(text):
            if _keep_token(m.group(1), text, m.end()):
                cited.add(m.group(1))
        for line in text.splitlines():
            if HEADING_RE.match(line):  # a heading defines an anchor, not a citation
                continue
            for m in INTRA_RE.finditer(line):
                if _keep_token(m.group(1), line, m.end()):
                    cited.add(m.group(1))
    return cited


def _scan_files(root):
    for glob in CITATION_GLOBS:
        for path in root.rglob(glob):
            parts = set(path.relative_to(root).parts)
            if parts & SKIP_DIRS or parts & SKIP_PATH_PARTS:
                continue
            if path.name in SKIP_FILES:
                continue
            yield path


def resolve(cited, gen):
    """The generated anchors a citation resolves to: exact match, or the leading
    slug of a heading. A citation ending in `-` (a line-wrap split of a longer
    anchor) matches on that boundary directly. Returns the list of matches; a
    sound citation resolves to exactly one."""
    if cited in gen:
        return [cited]
    prefix = cited if cited.endswith("-") else cited + "-"
    return [g for g in gen if g.startswith(prefix)]


def classify(cited, gen):
    """Bucket each cited anchor vs the generated manifest.

    Returns dict: exact / prefix_unique (cited -> anchor) / ambiguous (cited ->
    [anchors]) / dangling (cited)."""
    gen_set = set(gen)
    out = {"exact": set(), "prefix_unique": {}, "ambiguous": {}, "dangling": set()}
    for c in cited:
        if c in gen_set:
            out["exact"].add(c)
            continue
        pref = resolve(c, gen)
        if len(pref) == 1:
            out["prefix_unique"][c] = pref[0]
        elif len(pref) > 1:
            out["ambiguous"][c] = pref
        else:
            out["dangling"].add(c)
    return out


def playbook_index_links(md_text=None):
    """Anchors linked from the `## Playbook index` navigation section, or None if
    that section is absent (it is added during the hazards.md normalization)."""
    text = md_text if md_text is not None else HAZARDS.read_text()
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        m = HEADING_RE.match(line)
        if m and m.group(2).strip().lower() == "playbook index":
            start = i
            break
    if start is None:
        return None
    links = []
    for line in lines[start + 1:]:
        if HEADING_RE.match(line):
            break
        links += re.findall(r"\]\(#([A-Za-z0-9_-]+)\)", line)
    return links


def allowlist(root=ROOT):
    """The frozen set of cited anchors that resolve at baseline (exact matches plus
    unique-prefix shorthands). A pre-existing dangling citation is excluded: the
    guard protects working citations, not pre-existing breaks."""
    gen = manifest()
    b = classify(harvest_cited(root), gen)
    return sorted(b["exact"] | set(b["prefix_unique"]))


def check_allowlist(frozen, root=ROOT):
    """Return (ok, orphaned). Every frozen allowlist anchor must still resolve to
    exactly one heading in the current manifest."""
    gen = manifest()
    orphaned = [a for a in frozen if len(resolve(a, gen)) != 1]
    return (not orphaned), orphaned


ALLOWLIST_FILE = ROOT / "tests" / "tooling" / "hazard_cited_anchors.txt"


def load_frozen_allowlist():
    """The frozen allowlist from tests/tooling/hazard_cited_anchors.txt, or None if
    it has not been generated yet."""
    if not ALLOWLIST_FILE.exists():
        return None
    return [
        line.strip()
        for line in ALLOWLIST_FILE.read_text().splitlines()
        if line.strip() and not line.startswith("#")
    ]


def check(root=ROOT):
    """Return (ok, orphaned). Every anchor in the frozen allowlist must still
    resolve to exactly one heading. This is the enforced invariant; a pre-existing
    dangling citation (never in the allowlist) is out of scope. Falls back to the
    full cited set only when no allowlist has been frozen yet."""
    frozen = load_frozen_allowlist()
    if frozen is None:
        gen = manifest()
        buckets = classify(harvest_cited(root), gen)
        return not buckets["ambiguous"], sorted(buckets["dangling"])
    return check_allowlist(frozen, root)


def _main(argv):
    cmd = argv[1] if len(argv) > 1 else "check"
    if cmd == "manifest":
        print("\n".join(manifest()))
        return 0
    if cmd == "cited":
        print("\n".join(sorted(harvest_cited())))
        return 0
    if cmd == "allowlist":
        print("\n".join(allowlist()))
        return 0
    if cmd == "freeze":
        dest = ROOT / "tests" / "tooling" / "hazard_cited_anchors.txt"
        lines = allowlist()
        header = (
            "# Frozen allowlist: cited hazards.md anchors that resolve at baseline.\n"
            "# Regenerate with `tools/hazard_anchors.py freeze` after an INTENDED\n"
            "# anchor change (add/rename a section + update its citations in lockstep).\n"
        )
        dest.write_text(header + "\n".join(lines) + "\n")
        print(f"wrote {dest} ({len(lines)} anchors)")
        return 0
    if cmd == "classify":
        gen = manifest()
        b = classify(harvest_cited(), gen)
        print(f"generated anchors : {len(gen)}")
        print(f"cited (distinct)  : {sum(len(v) for v in b.values())}")
        print(f"  exact           : {len(b['exact'])}")
        print(f"  prefix-unique   : {len(b['prefix_unique'])}")
        for c, g in sorted(b["prefix_unique"].items()):
            print(f"      {c}  ->  {g}")
        print(f"  ambiguous       : {len(b['ambiguous'])}")
        for c, gs in sorted(b["ambiguous"].items()):
            print(f"      {c}  ->  {gs}")
        print(f"  dangling        : {len(b['dangling'])}")
        for c in sorted(b["dangling"]):
            print(f"      {c}")
        return 0
    if cmd == "check":
        ok, orphaned = check()
        if ok:
            frozen = load_frozen_allowlist()
            n = len(frozen) if frozen is not None else "all"
            print(f"OK: {n} frozen cited anchors resolve to exactly one heading.")
            return 0
        print("ANCHOR CHECK FAILED: these citations no longer resolve:", file=sys.stderr)
        for c in orphaned:
            print(f"  {c}", file=sys.stderr)
        return 1
    print(f"usage: hazard_anchors.py [manifest|cited|classify|check]", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
