"""Guards the docs/hazards.md anchor "API" against silent drift.

Heading text in hazards.md generates GitHub #anchors that the rest of the repo
cites as grep-keys (CLAUDE.md's hazard index, the sprint commands, the tools, and
hazards.md's own cross-references). A heading edit that changes an anchor orphans
every citation of it. These tests make the anchor set an enforced invariant so the
prompt-doc rework (and any future heading edit) cannot break a working citation.

The frozen allowlist lives in hazard_cited_anchors.txt. Regenerate it only after an
INTENDED anchor change, with `tools/hazard_anchors.py freeze`.
"""

from __future__ import annotations

from pathlib import Path

from conftest import load_tool

h = load_tool("hazard_anchors")

ALLOWLIST_FILE = Path(__file__).resolve().parent / "hazard_cited_anchors.txt"


def _frozen_allowlist():
    return [
        line.strip()
        for line in ALLOWLIST_FILE.read_text().splitlines()
        if line.strip() and not line.startswith("#")
    ]


def test_frozen_allowlist_resolves():
    """Every cited anchor frozen at baseline still resolves to exactly one heading.
    A heading edit that orphans a working citation fails here."""
    frozen = _frozen_allowlist()
    assert frozen, "allowlist file is empty"
    ok, orphaned = h.check_allowlist(frozen)
    assert ok, f"cited anchors no longer resolve to exactly one heading: {orphaned}"


def test_no_ambiguous_citations():
    """No cited anchor resolves to two or more headings (an ambiguous grep-key)."""
    gen = h.manifest()
    buckets = h.classify(h.harvest_cited(), gen)
    assert not buckets["ambiguous"], f"ambiguous citations: {buckets['ambiguous']}"


def test_no_duplicate_anchor_slugs():
    """No two headings slug to the same base anchor. A collision makes GitHub append
    a silent -1/-2 suffix, breaking the un-suffixed citation."""
    titles = [t for _, t in h.headings(h.HAZARDS.read_text())]
    bases = [t for t in (h.slugify_all([x])[0] for x in titles)]
    dupes = sorted({b for b in bases if bases.count(b) > 1})
    assert not dupes, f"heading slugs collide (silent -1/-2 suffix): {dupes}"


def test_playbook_index_covers_all_sections():
    """Once the `## Playbook index` navigation TOC exists, it must link every other
    section (so the TOC cannot silently drift from the section set) and every link
    must resolve. Skipped until the TOC is added during normalization."""
    links = h.playbook_index_links()
    if links is None:
        import pytest

        pytest.skip("## Playbook index not present yet")
    gen = h.manifest()
    # Every link resolves to exactly one heading.
    bad = [a for a in links if len(h.resolve(a, gen)) != 1]
    assert not bad, f"Playbook index links do not resolve: {bad}"
    # Every section (except the index itself) is listed.
    linked = set(links)
    missing = [
        a
        for a in gen
        if a != "playbook-index" and not any(a == x or a.startswith(x) for x in linked)
    ]
    assert not missing, f"sections missing from Playbook index: {missing}"
