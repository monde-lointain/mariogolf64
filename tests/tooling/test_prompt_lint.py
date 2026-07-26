"""Guards the agent prompt surfaces against style and structure drift.

docs/prompt-style.md's conformance checklist was a human gate, and the surfaces it
governs drifted ~1700 emphasis-caps past it. tools/prompt_lint.py mechanizes the
checkable half and tools/deshout.py carries the repair, with the invariant that
makes the repair auditable.

Two deliberate design choices here, both learned from the tooling debt this sprint
paid off:

1. The counted checks (caps, S###, nesting) ratchet against a frozen baseline
   rather than asserting zero. hazards.md alone carries ~1430 caps, so a hard rule
   would block every sprint's retro edits from day one; a ratchet ships now and
   converges.
2. The deshout property tests run on synthetic text, not on the live corpus. The
   two tests this sprint had to repair both failed because they pinned live ranker
   state that legitimately moved -- one hardcoded a function that got mined out,
   the other assumed a backlog size. A property test cannot rot that way.
"""

from __future__ import annotations

from conftest import load_tool

pl = load_tool("prompt_lint")
ds = load_tool("deshout")


# --- the gate ---------------------------------------------------------------


def test_hard_checks_pass():
    """The gate the later rework phases lean on. Advisories are allowed through;
    a hard failure is a dead cross-reference, an over-budget file, or a drifted
    invariant block."""
    hard, _soft = pl.run_checks()
    assert not hard, "\n".join(
        f"[{k}] {m}" for k, msgs in sorted(hard.items()) for m in msgs
    )


def test_no_dead_xrefs():
    """Every `<path>.md ## <Heading>` citation resolves to exactly one heading.

    This is the check that would have caught the two dead references found while
    planning this rework: a wrapper citing a `sprint-plan procedure` section that
    never existed, and hazards.md pointing at a hazard index that had moved."""
    assert not pl.xref_failures()


def test_within_per_file_budget():
    assert not pl.budget_failures()


def test_counters_not_above_baseline():
    """caps / S### / nesting may fall or hold, never rise. Re-freeze deliberately
    with `prompt_lint.py freeze` when a rise is intended."""
    assert not pl.baseline_failures(pl.measure(), pl.load_baseline())


def test_invariant_block_mirrors():
    """CLAUDE.md and AGENTS.md must carry byte-identical invariants. Inert until
    the markers exist, so it can land before the edit that adds them."""
    assert not pl.mirror_failures()


def test_baseline_covers_every_surface():
    """A surface missing from the baseline ratchets against nothing."""
    missing = sorted(set(pl.measure()) - set(pl.load_baseline()))
    assert not missing, f"re-run `prompt_lint.py freeze`; unbaselined: {missing}"


# --- deshout properties (synthetic input) -----------------------------------


def test_fold_changes_only_letter_case():
    """Pass A's whole safety argument. If this holds, no word was deleted,
    inserted, reordered, respelled or re-wrapped."""
    src = (
        "This is NOT a fresh pack, and the residual is BOTH a coloring wall AND\n"
        "a scheduling coin. Keep `base.c` at its BEST form. See VERSION_J and O2.\n"
        "- **Rule.** The build must reach EXACT instruction count first (S272).\n"
    )
    out = ds.fold_text(src)
    assert out.lower() == src.lower()
    assert out != src


def test_fold_preserves_thresholds_and_scope_limiters():
    src = "Run the permuter ONLY at `percent >= 0.97`, and NEVER before 25 iterations.\n"
    out = ds.fold_text(src)
    assert ">= 0.97" in out and "25" in out
    assert "only" in out and "never" in out


def test_fold_protects_meaningful_caps():
    """Backticked tokens, defines/macros (any `_` or digit), single-char nm
    classes, S### tags and the domain acronym allowlist all survive."""
    src = "The `ROM SHA-1` gate uses VERSION_J, F3DEX_GBI_2, O2, nm T/D/b, and S282.\n"
    out = ds.fold_text(src)
    for tok in ("VERSION_J", "F3DEX_GBI_2", "O2", "S282", "`ROM SHA-1`"):
        assert tok in out, tok


def test_fold_skips_code_fences():
    src = "Shout NOT here.\n\n```\nkeep THIS EXACTLY as written\n```\n"
    out = ds.fold_text(src)
    assert "keep THIS EXACTLY as written" in out
    assert "not here" in out


def test_gate_tokens_are_routed_to_review():
    """AND / OR / BOTH bind a conjunctive or disjunctive gate, so folding them is
    word-preserving and meaning-losing. They must surface in the review queue."""
    gates = []
    ds.fold_text("Hold integration until every subagent AND its permuter reported.\n",
                 None, gates)
    assert [t for t, _ in gates] == ["AND"]


def test_allowlist_excludes_shouted_english():
    """The allowlist must not quietly absorb the gate family: allowlisting AND
    would make the review queue silently empty."""
    allow = pl.load_caps_allow()
    assert not (allow & ds.GATE_TOKENS), (
        f"gate tokens must not be allowlisted: {sorted(allow & ds.GATE_TOKENS)}"
    )


def test_bold_triage_keeps_template_labels():
    counts = ds.classify_bold("**Rule.** The invariant.\n**Trigger:** a flag.\n")
    assert counts["keep: template label"] == 2
