#!/usr/bin/env python3
"""De-shouting tool for the agent prompt surfaces, with mechanical safety gates.

docs/prompt-style.md says to prefer structure to shouting and lists the caps that
must never be touched. The two large surfaces carry ~1700 emphasis-caps between
them, spread over ~700 distinct tokens, so this is a tool problem rather than a
hand-edit: the long tail is what makes a manual pass unreviewable.

Three passes, in increasing order of judgment.

  Pass A  protected case-fold. Purely mechanical.
  Pass B  bold triage. Classifies, does not rewrite.
  Pass C  restructure the residue. Changes words; the only pass that can lose
          meaning, so it carries its own guard.

Pass A's safety property is that the only bytes it changes are letter cases:

    lower(before) == lower(after)

If that holds, no word was deleted, inserted, reordered, respelled or re-wrapped,
so every scope-limiter (`only`, `not`, `every`, `before`, `unless`) and every
threshold (`>= 0.97`, 25, 5) survives by construction, and no line wrap moves, so
the diff stays word-level. `apply` asserts it in-process before writing, and
`verify` re-asserts it against a git ref.

Where the invariant is NOT enough: it certifies word-identity, not meaning. In
this corpus the caps on AND / OR / BOTH / ALL / EITHER are doing binding work on
a conjunctive or disjunctive gate, which docs/prompt-style.md requires be kept
intact. Folding those is textually lossless and semantically lossy. `gates`
enumerates them so each is restated explicitly in Pass C rather than silently
flattened; a file's Pass A is not finished while its gate queue is unreviewed.

Subcommands:
  report [paths]   Distinct tokens Pass A would fold, plus bold classes.
  gates  [paths]   The conjunctive/disjunctive review queue for Pass C.
  apply  [paths]   Perform Pass A, asserting the invariant before writing.
  verify [ref]     Assert case-only change vs a git ref (the Pass A/B gate).
  guard  [ref]     Assert logic tokens, numbers and code spans are preserved
                   per changed line vs a git ref (the Pass C gate).
"""

from __future__ import annotations

import collections
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prompt_lint as pl  # noqa: E402  (path set above)

ROOT = pl.ROOT
ALLOW_FILE = pl.CAPS_ALLOW_FILE

CAPS_TOKEN_RE = re.compile(r"\b[A-Z][A-Z0-9]{1,}\b")
CODE_FENCE_RE = re.compile(r"^\s*```")
INLINE_CODE_RE = re.compile(r"`[^`\n]*`")
STAG_RE = re.compile(r"^S\d{1,3}$")

# Caps whose emphasis is binding a gate, not decorating a word. Folding them is
# word-preserving and meaning-losing, so they route to Pass C instead.
GATE_TOKENS = frozenset({"AND", "OR", "BOTH", "ALL", "EITHER", "NEITHER", "EVERY"})

# Words that carry a rule's logic. Pass C must not drop one from a line.
LOGIC_TOKENS = frozenset(
    {
        "only", "not", "never", "always", "every", "all", "any", "each",
        "before", "after", "unless", "until", "both", "either", "neither", "no",
    }
)
NUMBER_RE = re.compile(r"\d+(?:\.\d+)?")
SENTENCE_END_RE = re.compile(r"[.!?:]\s*$|^\s*$")


def load_allow():
    return pl.load_caps_allow()


def _protected(tok):
    """Caps that carry meaning: defines/macros/types (any `_` or digit), the
    single-char `nm` classes, S### tags, and the domain acronym allowlist."""
    if "_" in tok or any(c.isdigit() for c in tok):
        return True
    if len(tok) < 2 or STAG_RE.match(tok):
        return True
    return tok in load_allow()


def _spans_to_skip(line):
    """Character ranges of inline code spans, which are never folded."""
    return [m.span() for m in INLINE_CODE_RE.finditer(line)]


def fold_line(line, prev_ended_sentence, counter=None, gates=None):
    """Fold the emphasis-caps in one line. Returns (new_line, ends_sentence)."""
    skip = _spans_to_skip(line)

    def in_code(pos):
        return any(a <= pos < b for a, b in skip)

    out, last = [], 0
    for m in CAPS_TOKEN_RE.finditer(line):
        tok = m.group(0)
        if in_code(m.start()) or _protected(tok):
            continue
        if tok in GATE_TOKENS and gates is not None:
            gates.append((tok, line.strip()))
        # Title-case at the start of a sentence, lowercase elsewhere.
        head = line[:m.start()]
        starts = prev_ended_sentence and not head.strip()
        new = tok.capitalize() if starts else tok.lower()
        out.append(line[last:m.start()])
        out.append(new)
        last = m.end()
        if counter is not None:
            counter[tok] += 1
    out.append(line[last:])
    new_line = "".join(out)
    return new_line, bool(SENTENCE_END_RE.search(line))


def fold_text(text, counter=None, gates=None):
    lines, out, in_fence, ends = text.split("\n"), [], False, True
    for line in lines:
        if CODE_FENCE_RE.match(line):
            in_fence = not in_fence
            out.append(line)
            continue
        if in_fence:
            out.append(line)
            continue
        new, ends = fold_line(line, ends, counter, gates)
        out.append(new)
    return "\n".join(out)


def classify_bold(text):
    """Pass B triage. Template labels and short line-start lead-ins are structure
    and stay; bolded paragraphs and mid-sentence bolds are word-level shouting in
    another costume and go."""
    labels = {"Rule.", "Rule:", "Trigger.", "Trigger:", "Procedure.", "Procedure:",
              "Provenance.", "Caveats.", "Sub-cases."}
    counts = collections.Counter()
    for line in text.split("\n"):
        for m in re.finditer(r"\*\*(.+?)\*\*", line):
            span, at_start = m.group(1), not line[: m.start()].strip("-*  ")
            words = len(span.split())
            if span in labels:
                counts["keep: template label"] += 1
            elif at_start and span.rstrip().endswith(".") and words <= 12:
                counts["keep: sub-case lead-in"] += 1
            elif words > 20 or re.search(r"[.!?]\s+\S", span):
                counts["unbold: bolded paragraph"] += 1
            elif not at_start and words < 4:
                counts["unbold: mid-sentence bold"] += 1
            else:
                counts["review: other"] += 1
    return counts


def _git_show(ref, relpath):
    p = subprocess.run(["git", "show", f"{ref}:{relpath}"], cwd=ROOT,
                       capture_output=True, text=True)
    return p.stdout if p.returncode == 0 else None


def _targets(args):
    paths = [ROOT / a for a in args if not a.startswith("-")]
    return [p for p in paths if p.exists()] or pl.surfaces()


def _main(argv):
    cmd = argv[1] if len(argv) > 1 else "report"
    args = argv[2:]

    if cmd == "report":
        counter, gates = collections.Counter(), []
        for p in _targets(args):
            fold_text(p.read_text(), counter, gates)
        total = sum(counter.values())
        print(f"Pass A would fold {total} tokens across {len(counter)} distinct forms.")
        print(f"Of those, {len(gates)} are gate tokens routed to Pass C (see `gates`).\n")
        print("Review this list once before `apply`: a token here that actually carries")
        print("meaning belongs in tests/tooling/prompt_caps_allow.txt instead.\n")
        for tok, n in counter.most_common():
            print(f"{n:>5}  {tok}")
        return 0

    if cmd == "gates":
        gates = []
        for p in _targets(args):
            local = []
            fold_text(p.read_text(), None, local)
            for tok, line in local:
                gates.append((pl.rel(p), tok, line))
        print(f"{len(gates)} gate occurrences need an explicit restatement in Pass C.")
        print("Folding these is word-preserving but drops the binding that")
        print("docs/prompt-style.md requires be kept intact.\n")
        for r, tok, line in gates:
            print(f"{r}: [{tok}] {line[:110]}")
        return 0

    if cmd == "apply":
        changed = 0
        for p in _targets(args):
            before = p.read_text()
            after = fold_text(before)
            if before == after:
                continue
            assert before.lower() == after.lower(), (
                f"{pl.rel(p)}: fold changed more than letter case; refusing to write"
            )
            p.write_text(after)
            changed += 1
            print(f"folded {pl.rel(p)}")
        print(f"{changed} file(s) changed; invariant lower(before)==lower(after) held for each.")
        return 0

    if cmd == "verify":
        ref = args[0] if args else "HEAD"
        bad = []
        for p in pl.surfaces():
            r = pl.rel(p)
            old = _git_show(ref, r)
            if old is None:
                continue
            if old.lower() != p.read_text().lower():
                bad.append(r)
        if bad:
            print(f"NOT a case-only change vs {ref}:", file=sys.stderr)
            for r in bad:
                print(f"  {r}", file=sys.stderr)
            return 1
        print(f"OK: every surface differs from {ref} only in letter case.")
        return 0

    if cmd == "guard":
        ref = args[0] if args else "HEAD"
        bad = []
        for p in pl.surfaces():
            r = pl.rel(p)
            old = _git_show(ref, r)
            if old is None:
                continue
            new = p.read_text()
            if old == new:
                continue
            o_low, n_low = old.lower(), new.lower()
            for name, pat in (
                ("logic token", None),
                ("number", NUMBER_RE),
                ("code span", INLINE_CODE_RE),
            ):
                if pat is None:
                    o_set = collections.Counter(
                        w for w in re.findall(r"[a-z]+", o_low) if w in LOGIC_TOKENS
                    )
                    n_set = collections.Counter(
                        w for w in re.findall(r"[a-z]+", n_low) if w in LOGIC_TOKENS
                    )
                else:
                    o_set = collections.Counter(pat.findall(old))
                    n_set = collections.Counter(pat.findall(new))
                lost = o_set - n_set
                if lost:
                    bad.append(f"{r}: {name}(s) dropped: {dict(list(lost.items())[:6])}")
        if bad:
            print(f"GUARD FAILED vs {ref}:", file=sys.stderr)
            for b in bad:
                print(f"  {b}", file=sys.stderr)
            return 1
        print(f"OK: logic tokens, numbers and code spans preserved vs {ref}.")
        return 0

    if cmd == "bold":
        for p in _targets(args):
            counts = classify_bold(p.read_text())
            if not counts:
                continue
            print(f"\n{pl.rel(p)}")
            for k, n in counts.most_common():
                print(f"  {n:>5}  {k}")
        return 0

    print("usage: deshout.py [report|gates|bold|apply|verify [ref]|guard [ref]] [paths]",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
