---
type: Tool
title: "list-unconsolidated-learnings"
description: "Lists per-function LEARNINGS.md and NEAR_MISS.md notes not yet merged into docs/reference/kmc-gcc/; --mark records a note in a ledger shared by all worktrees."
tags: [learnings, consolidation, nigel, cleanup]
resource: ../../tools/list_unconsolidated_learnings.py
---

```bash
python3 tools/list_unconsolidated_learnings.py                                 # one repo-relative path per line
python3 tools/list_unconsolidated_learnings.py --json                          # JSON array
python3 tools/list_unconsolidated_learnings.py --mark nonmatchings/<function>/LEARNINGS.md
```

# Listing

- Scans `nonmatchings/<function>/LEARNINGS.md` and `nonmatchings/<function>/NEAR_MISS.md`, one directory level down.
- Skips notes that are empty or whitespace-only.
- Prints paths sorted, relative to the repo root, with `/` separators. `--json` prints `[]` when nothing is pending.

# Consolidation ledger

- Location: `<git-common-dir>/mg64/consolidated-learnings.txt`, where the common git directory comes from `git rev-parse --git-common-dir`. Every worktree shares it, and nigel's reset does not touch it.
- Format: one `<sha256> <repo-relative path>` line per note. The digest is the lowercase hex SHA-256 of the note's raw bytes.
- Matching uses the digest alone. Editing a note changes its digest, so the note is listed again.

# Marking

`--mark PATH` records one note as consolidated and prints `consolidated <path>`.

- A relative PATH resolves against the repo root, not the working directory.
- PATH must be `nonmatchings/<function>/LEARNINGS.md` or `nonmatchings/<function>/NEAR_MISS.md`, after symlinks are resolved.
- A digest already in the ledger is not appended again; the command still succeeds.
- Blank notes are not rejected.
- Errors exit `2`: `error: not a nonmatchings note (LEARNINGS.md or NEAR_MISS.md): <path>` or `error: no such note: <path>`.

# Callers

- The [consolidate-learnings](/tools/task-runner.md#task-catalog) task uses `--json` as its candidate source, and its prompt runs `--mark` on the note after merging the lessons into `docs/reference/kmc-gcc/`.
- [decomp-env-cleanup](/tools/selection-and-status.md#decomp-env-cleanup) keeps a stale directory whose `nonmatchings/<dir>/` prefix appears in the output, unless `--force` is given.
