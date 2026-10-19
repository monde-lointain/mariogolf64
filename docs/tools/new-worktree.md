---
type: Tool
title: "new-worktree"
description: "Create a sibling git worktree on a new branch with the gitignored build inputs (baserom, gcc_kmc, .venv, submodules) copied or linked from the main checkout, then build it."
tags: [worktree, setup]
resource: ../../tools/new-worktree
---

```bash
tools/new-worktree <name> [--branch BRANCH] [--base REF] [--no-build]
```

Creates `../<name>` next to the main checkout (always a sibling of the main checkout, even when run from another worktree) on branch `<name>` unless `--branch` says otherwise, based on `HEAD` unless `--base` says otherwise. It refuses an existing target directory or branch.

It then copies `baserom.z64` and `tools/gcc_kmc` from the main checkout (`cp --reflink=auto`, so copies are cheap where the filesystem supports it), symlinks `.venv` to the main checkout's virtualenv, and runs `git submodule update --init`. Git reuses the module clones under the main checkout's `.git/modules`, so this works offline once they exist. A missing input is printed as a warning and the worktree is still created.

Git runs hooks only from the shared hooks directory, so the script does not install any; linked worktrees use the main checkout's hooks.

Last, it runs `./tools/build-and-verify.sh` in the new worktree. That extracts `asm/`, which candidate sources such as `score_functions.py asm/nonmatchings` need, and proves the worktree builds the matching ROM. A failed build exits nonzero and keeps the worktree for inspection. The build is skipped with a warning when an input is missing, and skipped entirely with `--no-build`.

Use this for every [nigel](/tools/task-runner.md#worktree-rule) worktree.
