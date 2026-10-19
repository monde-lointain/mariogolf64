---
type: Tool
title: "list-unconsolidated-learnings"
description: "Lists per-function LEARNINGS.md and NEAR_MISS.md notes that have not been merged into docs/reference/kmc-gcc/, and records consolidated notes in a ledger shared by all worktrees."
tags: [learnings, consolidation, nigel, cleanup]
resource: ../../tools/list_unconsolidated_learnings.py
---

```bash
python3 tools/list_unconsolidated_learnings.py            # one repo-relative path per line
python3 tools/list_unconsolidated_learnings.py --json     # JSON array (nigel candidate_source)
python3 tools/list_unconsolidated_learnings.py --mark nonmatchings/<function>/LEARNINGS.md
```

Scans `nonmatchings/<function>/LEARNINGS.md` and `nonmatchings/<function>/NEAR_MISS.md`. Blank or whitespace-only notes are skipped.

# Consolidation ledger

A note is consolidated when its exact content hash is recorded in `$(git rev-parse --git-common-dir)/mg64/consolidated-learnings.txt`, one `<sha256> <path>` line per note. The ledger lives in the common git directory, so every worktree sees it and nigel's reset does not touch it. Editing a note changes its hash, so the note is listed again.

`--mark` is what the [consolidate-learnings](/tools/task-runner.md) task runs after it has merged a note's verified lessons. It refuses paths that are not `nonmatchings/<function>/{LEARNINGS,NEAR_MISS}.md` and missing files.

# Interaction with cleanup

[decomp-env-cleanup](selection-and-status.md) keeps a stale environment that still holds an unconsolidated note, so the lesson is not lost before it reaches `docs/`. `--force` removes such environments anyway.
