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

# The trailing group keeps a contraction whole: without it `DON'T` tokenizes as
# `DON` plus a protected single-char `T`, and the fold emits `don'T`.
CAPS_TOKEN_RE = re.compile(r"\b[A-Z][A-Z0-9]{1,}(?:'[A-Z]+)?\b")
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


def protected_regions(text):
    """Character ranges that must never be folded: fenced blocks and inline code
    spans.

    Computed over the WHOLE document, not per line. These docs wrap at ~100
    columns, so a backticked command routinely straddles a line break -- there are
    134 such spans across the surfaces. A line-scoped regex cannot see them, and
    the first apply proved the cost: it folded the make variable in
    `make nonmatching-func\\n    FUNC=<f>` to `func=<f>`.

    An unmatched backtick swallows the rest of the document, which folds less
    rather than more. That is the safe direction to fail."""
    spans = []
    for m in re.finditer(r"^\s*```.*?^\s*```", text, flags=re.S | re.M):
        spans.append(m.span())

    def fenced(pos):
        return any(a <= pos < b for a, b in spans)

    for m in re.finditer(r"`[^`]*`", text):
        if not fenced(m.start()):
            spans.append(m.span())
    return spans


def fold_text(text, counter=None, gates=None):
    """Pass A over a whole document. Returns the folded text."""
    skip = protected_regions(text)

    def in_code(pos):
        return any(a <= pos < b for a, b in skip)

    # A token starts a sentence when only whitespace or list/emphasis punctuation
    # separates it from the previous sentence terminator or line start.
    out, last = [], 0
    for m in CAPS_TOKEN_RE.finditer(text):
        tok = m.group(0)
        if in_code(m.start()) or _protected(tok):
            continue
        if tok in GATE_TOKENS and gates is not None:
            line_start = text.rfind("\n", 0, m.start()) + 1
            line_end = text.find("\n", m.end())
            gates.append((tok, text[line_start:line_end if line_end > 0 else None].strip()))
        head = text[:m.start()]
        prefix = head[head.rfind("\n") + 1:]
        starts = not prefix.strip(" \t-*>#.|") or bool(
            re.search(r"[.!?:]\s+$", prefix)
        )
        out.append(text[last:m.start()])
        out.append(tok.capitalize() if starts else tok.lower())
        last = m.end()
        if counter is not None:
            counter[tok] += 1
    out.append(text[last:])
    return "".join(out)


# The hazard-entry template labels, with or without trailing punctuation: the doc's
# own header paragraph writes them bare ("**Rule** (the invariant), **Trigger** ...").
TEMPLATE_LABELS = {"Rule", "Trigger", "Procedure", "Provenance", "Caveats",
                   "Sub-cases", "Sub-cases / variants"}


def _classify_one(span, line, start, in_index):
    """Classify one bold span. Shared by the report and the edit so they can never
    disagree about what gets stripped."""
    head = line[:start]
    numbered = bool(re.match(r"^[\s\-*>]*\d+\.\s*$", head))
    at_start = not re.sub(r"^[\s\-*>]*(?:\d+\.)?[\s]*", "", head)
    words = len(span.split())
    bare = span.rstrip(".:")
    if bare in TEMPLATE_LABELS:
        return "keep: template label"
    if in_index and at_start:
        return "keep: index group header"
    if at_start and words <= 14 and (span.rstrip().endswith((".", ":")) or numbered):
        # A lead-in: a sub-case headline, or a bolded step name inside a Procedure.
        # Both are structure. The `:` and numbered-item forms were the bulk of what
        # a first cut dumped into `review`.
        return "keep: lead-in"
    if words > 20 or re.search(r"[.!?]\s+\S", span):
        return "unbold: bolded paragraph"
    if not at_start and words < 6:
        return "unbold: mid-sentence emphasis"
    return "review: other"


def classify_bold(text, detail=None):
    """Pass B triage. Template labels, index group headers and short line-start
    lead-ins are structure and stay; bolded paragraphs and mid-sentence word
    emphasis are shouting in another costume.

    Precision matters more than recall here, because the output drives an edit.
    A first cut keyed only on punctuated labels put `**Rule**` and the playbook
    index's bolded family headers in the unbold buckets; auto-applying that would
    have flattened the template and the index. Anything not confidently classified
    lands in `review` and is left alone."""
    counts = collections.Counter()
    in_index = False
    for line in text.split("\n"):
        if line.startswith("## "):
            in_index = line.strip() == "## Playbook index"
        # `[^*]` rather than `.` so a span cannot run across an adjacent `**`
        # pair; the greedy form matched the prose BETWEEN two bolds.
        for m in re.finditer(r"\*\*([^*]+)\*\*", line):
            key = _classify_one(m.group(1), line, m.start(), in_index)
            counts[key] += 1
            if detail is not None:
                detail.setdefault(key, []).append(m.group(1))
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
            # Code spans are extracted with the same document-wide pairing the
            # fold uses. A line-scoped regex mis-pairs backticks around a span
            # that wraps, and then reports the GAPS between spans as spans.
            def spans(t):
                return collections.Counter(t[a:b] for a, b in protected_regions(t))

            def logic(t):
                low = t.lower()
                return collections.Counter(
                    w for w in re.findall(r"[a-z]+", low) if w in LOGIC_TOKENS
                )

            for name, extract in (
                ("logic token", logic),
                ("number", lambda t: collections.Counter(NUMBER_RE.findall(t))),
                ("code span", spans),
            ):
                lost = extract(old) - extract(new)
                if lost:
                    bad.append(f"{r}: {name}(s) dropped: {dict(list(lost.items())[:6])}")
        if bad:
            print(f"GUARD FAILED vs {ref}:", file=sys.stderr)
            for b in bad:
                print(f"  {b}", file=sys.stderr)
            return 1
        print(f"OK: logic tokens, numbers and code spans preserved vs {ref}.")
        return 0

    if cmd == "unbold":
        # Strip only the spans the classifier is confident are word emphasis.
        # Anything in `review` is left alone: precision over recall, because the
        # output drives an edit. The invariant is the analogue of Pass A's --
        # with every `**` removed, before and after are identical, so no word was
        # touched and only bold markers changed.
        total = 0
        for p in _targets(args):
            before = p.read_text()
            # Code spans and fences are off-limits, exactly as for the case-fold.
            # `**` inside backticks is content, not markup: hazards.md has `2**4`
            # (exponentiation) and prompt-style.md documents the bold syntax as
            # `**bold lead-in.**`. Stripping either corrupts the text.
            skip = protected_regions(before)

            def in_code(pos):
                return any(a <= pos < b for a, b in skip)

            out, last, in_index = [], 0, False
            for m in re.finditer(r"\*\*([^*\n]+)\*\*", before):
                if in_code(m.start()) or in_code(m.end() - 1):
                    continue
                ls = before.rfind("\n", 0, m.start()) + 1
                le = before.find("\n", m.end())
                line = before[ls: le if le > 0 else None]
                if line.startswith("## "):
                    in_index = line.strip() == "## Playbook index"
                key = _classify_one(m.group(1), line, m.start() - ls, in_index)
                if not key.startswith("unbold:"):
                    continue
                out.append(before[last:m.start()])
                out.append(m.group(1))
                last = m.end()
                total += 1
            out.append(before[last:])
            after = "".join(out)
            if after == before:
                continue
            assert before.replace("**", "") == after.replace("**", ""), (
                f"{pl.rel(p)}: unbold changed more than bold markers; refusing to write"
            )
            p.write_text(after)
            print(f"unbolded {pl.rel(p)}")
        print(f"{total} span(s) unbolded; invariant strip_bold(before)==strip_bold(after) held.")
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
