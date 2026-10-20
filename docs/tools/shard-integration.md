---
type: Tool
title: "Shard integration (sync-shard, integrate-shards, merge drivers)"
description: "Shard worktrees: sync-shard rebases a shard before each candidate, integrate-shards cherry-picks shard commits behind a build gate, merge-decomp-c and merge-splat-yaml resolve src and yaml conflicts."
tags: [nigel, worktree, merge, workflow]
resource: ../../tools/integrate-shards
---

Shard worktrees decompile in parallel and send their commits back to `dev`. `sync-shard` brings a shard up to date, `integrate-shards` cherry-picks shard commits into the checkout behind a build check, and two merge drivers resolve the conflicts parallel matches make in `src/**/*.c` and `mariogolf64.yaml`.

# Merge drivers

`.gitattributes` assigns three merge rules:

| Path | Merge |
|---|---|
| `src/**/*.c` | `decomp-c`, the driver `tools/merge-decomp-c` |
| `mariogolf64.yaml` | `splat-yaml`, the driver `tools/merge-splat-yaml` |
| `docs/log.md` | `union` |

`tools/install-git-hooks.sh` registers both drivers on every run (`merge.<name>.driver "tools/<driver> %O %A %B %L"`). Without a driver, git falls back to a plain text merge for its path.

`merge-decomp-c <base> <ours> <theirs> [<marker-size>]` runs git's three-way merge, then resolves each conflict block whose two sides hold only extern variables, prototypes, typedefs (one line, or a braced block ending `} Name;`), `INCLUDE_ASM` lines and function definitions:

1. The functions must be the same on both sides, in the same order.
2. A definition beats the `INCLUDE_ASM` of the same function. Identical definitions are kept once.
3. Declarations are the union of both sides, minus any already declared identically earlier in the file.
4. A block stays conflicted on a symbol declared differently on the two sides or differently from an earlier declaration in the file, two different definitions of one function, differing `INCLUDE_ASM` lines, or any other line.

`merge-splat-yaml <base> <ours> <theirs> [<marker-size>]` runs git's three-way merge, then resolves each conflict block whose base and two sides hold only `- [0xOFFSET, ...]` subsegment lines at one indent. That is the conflict two shards make when each splits data or rodata out of the same segment. Entries are keyed by offset: a side's change to an offset wins over the unchanged other side, and the result is sorted by offset. An offset both sides changed differently, or any other line, leaves the block conflicted. Two `.rodata` ranges for one C file still merge and then fail the build; their bytes in between have to move into that C file by hand.

`MG64_MERGE_FAVOR=ours|theirs` resolves the remaining blocks of either driver to that side. Neither merge checks that the result matches; `./tools/build-and-verify.sh` does.

| Exit | Meaning |
|---|---|
| 0 | No conflict remains. The result is written to `<ours>`. |
| 1 | Conflict blocks remain with their markers. Resolved blocks stay resolved. |
| 2 | `git merge-file` failed. |

# sync-shard

`tools/sync-shard` runs first in the candidate source of the decompile tasks that call it (see the [task catalog](/tools/task-runner.md)). It does nothing unless `MG64_SYNC_BRANCH` is set and `NIGEL_CANDIDATE` is empty, so it runs when nigel selects a candidate. The judge run that nigel makes after the agent finishes skips it. The script always exits 0 and never fails the candidate source.

1. Record HEAD and rebase onto `$MG64_SYNC_BRANCH`. Commits already integrated into the sync branch drop out. A clean rebase is kept if the build passes; otherwise it is undone and the shard continues unsynced.
2. On conflict, abort and retry with `-X ours` and `MG64_MERGE_FAVOR=ours`, which prefer the sync branch's side. A commit that was integrated with a hand-resolved conflict becomes empty and drops.
3. Keep the retried result only if all of these hold:
   - In `src/` and `include/`, every function the shard decompiled (each `INCLUDE_ASM` line it removed) is still decompiled. Declarations may differ.
   - Elsewhere, every non-blank line the shard added is still present.
   - If HEAD moved, `./tools/build-and-verify.sh` passes.
4. Otherwise reset to the recorded HEAD and print `sync-shard: ... continuing unsynced` on stderr.

# integrate-shards

Run it in the main checkout on `dev`:

```bash
tools/integrate-shards mg64-a mg64-b
```

Usage is `tools/integrate-shards <branch>...`. It exits 2 on a usage error or when tracked files have uncommitted changes.

For each branch it takes the commits that `git cherry HEAD <branch>` lists as missing, oldest first, and prints one line per commit: `<status> <branch> <8-char sha> <subject>`.

| Status | Meaning | Effect |
|---|---|---|
| `skipped` | Already picked here (same author time and subject on this branch), or the commit touches only `src/`, `include/`, `docs/`, `symbol_addrs.txt` and `mariogolf64.yaml`, removes `INCLUDE_ASM` lines, and every function it removes is decompiled here. | Next commit. |
| `picked` | Cherry-picked, and `build-and-verify` passed. | Next commit. |
| `review` | Touches a path outside `src/`, `include/`, `docs/`, `symbol_addrs.txt` and `mariogolf64.yaml` (harness code, tasks, hooks). | Branch stops; exit 1. |
| `conflict` | Cherry-pick failed. It is aborted. | Branch stops; exit 1. |
| `build-failed` | Build check failed. HEAD is reset one commit. | Branch stops; exit 1. |

A `review` commit is left for a person. Once picked by hand, it still shows in `git cherry`, so the same-subject check skips it on later runs.

Exit 1 if any branch stopped, 0 otherwise, so a loop such as `while tools/integrate-shards mg64-a mg64-b mg64-c mg64-d; do sleep 600; done` needs a person only when it ends.

# Related

- [Task runner](/tools/task-runner.md): the decompile tasks that run `sync-shard`.
- [New worktree](/tools/new-worktree.md): creates the shard worktrees.
- [Build and verify](/tools/build-and-verify.md): the build check that sync-shard and integrate-shards run.
- [Git hooks and CI](/harness/git-hook-and-ci.md): hook installation.
