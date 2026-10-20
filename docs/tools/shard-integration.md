---
type: Tool
title: "Shard integration (sync-shard, integrate-shards, merge drivers)"
description: "Shard worktree integration: sync-shard rebases a shard onto the sync branch, integrate-shards cherry-picks shard commits behind a build check, two merge drivers resolve src and yaml conflicts."
tags: [nigel, worktree, merge, workflow]
---

Shard worktrees decompile in parallel, each on its own branch. The integrator's checkout takes their commits with `integrate-shards`, and each shard catches up with the sync branch through `sync-shard` before it selects a candidate. Parallel matches conflict in `src/**/*.c` and `mariogolf64.yaml`; the merge drivers resolve most of those conflicts.

# Merge drivers

`.gitattributes` assigns three merge rules:

| Path | Merge |
|---|---|
| `src/**/*.c` | `decomp-c`, the driver `tools/merge-decomp-c` |
| `mariogolf64.yaml` | `splat-yaml`, the driver `tools/merge-splat-yaml` |
| `docs/log.md` | `union` (built into git) |

Git runs each driver as `<driver> %O %A %B %L`: base, ours, theirs and the conflict marker size. A driver runs git's three-way merge, resolves the conflict blocks it recognises, and leaves the rest marked. Neither driver checks that the result builds; `./tools/build-and-verify.sh` does.

| Exit | Meaning |
|---|---|
| 0 | No conflict block remains. The result is written to `<ours>`. |
| 1 | Conflict blocks remain with their markers. Resolved blocks stay resolved. |
| 2 | `git merge-file` failed; its stderr is passed through. |

`MG64_MERGE_FAVOR=ours|theirs` resolves every block the driver cannot resolve to that side. Forced blocks count as resolved, so the exit code is 0 even when blocks were forced.

# merge-decomp-c

`merge-decomp-c <base> <ours> <theirs> [<marker-size>]` (`tools/merge-decomp-c`, marker size default `7`) resolves each conflict block whose two sides hold only extern variables, prototypes, typedefs (one line, or a braced block ending `} Name;`), `INCLUDE_ASM` lines and function definitions:

1. The functions must be the same on both sides, in the same order.
2. A definition beats the `INCLUDE_ASM` of the same function. Identical definitions are kept once. Two different definitions, or two different `INCLUDE_ASM` lines, leave the block conflicted.
3. Declarations are the union of both sides, minus any already declared identically earlier in the file.
4. A block stays conflicted on a symbol declared differently on the two sides or differently from an earlier declaration in the file, and on any other line.

# merge-splat-yaml

`merge-splat-yaml <base> <ours> <theirs> [<marker-size>]` (`tools/merge-splat-yaml`) resolves each conflict block whose base and two sides hold only `- [0xOFFSET, ...]` subsegment lines at one indent. That is the conflict two shards make when each splits data or rodata out of the same segment.

- Entries are keyed by offset. A side's change to an offset wins over the unchanged other side; removing an offset counts as a change. An offset both sides changed differently leaves the block conflicted.
- An offset that appears twice on one side leaves the block conflicted, as does any other line.
- The resolved entries are sorted by offset.

# sync-shard

`tools/sync-shard` runs first in the candidate source of the four decompile tasks (see the [task catalog](/tools/task-runner.md#task-catalog)). It does nothing unless `MG64_SYNC_BRANCH` is set and `NIGEL_CANDIDATE` is empty. nigel sets `NIGEL_CANDIDATE` on the judge run that follows the agent, so that run skips the sync and a commit the agent made does not read as a sync. The script always exits 0 and never fails the candidate source.

1. Record HEAD and run `git rebase -q "$MG64_SYNC_BRANCH"`. Commits already in the sync branch drop out. If HEAD moved, `./tools/build-and-verify.sh` must pass. Otherwise reset to the recorded HEAD and print `sync-shard: rebase onto <branch> does not build; continuing unsynced` on stderr. Without a move, nothing is built.
2. If the rebase conflicts, abort it and retry once with `MG64_MERGE_FAVOR=ours git rebase -q -X ours <branch>`. In a rebase "ours" is the sync branch, so a commit that was integrated with a hand-resolved conflict becomes empty and drops. The merge drivers apply in both rebases.
3. Keep the retry's result only if all of these hold:
   - Every function whose `INCLUDE_ASM` line the shard removed has no `INCLUDE_ASM` left under `src/`. Declarations may differ.
   - Outside `src/` and `include/`, every non-blank line the shard added is still in its file.
   - If HEAD moved, the build passes.
4. Otherwise abort the rebase, reset to the recorded HEAD, and print `sync-shard: rebase onto <branch> conflicted; continuing unsynced` on stderr. A kept retry prints `sync-shard: rebased onto <branch> preferring its side of conflicts`. A clean rebase prints nothing.

# integrate-shards

Run `tools/integrate-shards <branch>...` from the checkout that receives the commits. The script does not check which branch is checked out; it cherry-picks onto `HEAD`.

Exit 2 when run with no arguments, when the first argument starts with `-`, or when tracked files have uncommitted changes (untracked files are ignored).

For each branch in order, it walks the commits that `git cherry HEAD <branch>` marks `+`, oldest first, and prints `<status> <branch> <8-char sha> <subject>` for each one. It decides each commit in this order:

| Status | When | Effect |
|---|---|---|
| `skipped` | A commit with the same author time and subject is already on HEAD, including one picked by hand. | Next commit. |
| `review` | The commit touches a path outside `src/`, `include/`, `docs/`, `symbol_addrs.txt` and `mariogolf64.yaml`, such as harness code, tasks or hooks. | Branch stops. |
| `skipped` | The commit removes `INCLUDE_ASM` lines under `src/`, and none of the functions they name is still `INCLUDE_ASM` under `src/`. | Next commit. |
| `conflict` | `git cherry-pick` fails. It is aborted. | Branch stops. |
| `build-failed` | `./tools/build-and-verify.sh` fails. HEAD is reset one commit. | Branch stops. |
| `picked` | Cherry-picked, and the build passed. | Next commit. |

`git cherry-pick` runs the merge drivers, so `src/**/*.c` conflicts go through `merge-decomp-c`. A `review` or `conflict` commit is for a person. A commit picked by hand keeps its author time and subject, so later runs skip it.

A stop ends that branch only; the remaining branches still run. The exit code is 1 if any branch stopped and 0 otherwise. A wrapper loop such as `while tools/integrate-shards mg64-a mg64-b mg64-c mg64-d; do sleep 600; done` ends on exit 1 (a branch stopped) or exit 2 (usage error or uncommitted tracked changes), and both need a person.

# install-git-hooks.sh

`make install-hooks` runs `tools/install-git-hooks.sh`, and `make setup` depends on that target. The script takes no arguments and configures the repository it runs in.

1. Exits 1 with `Error: not inside a git repository.` outside a git checkout.
2. Sets the `name` and `driver` of the `merge.decomp-c` and `merge.splat-yaml` drivers in git config, shared by linked worktrees. Git runs each driver from the worktree's top level, so every worktree runs its own checked-out copy.
3. Exits 1 if `tools/git-hooks/pre-commit` is missing or not executable.
4. Links `$(git rev-parse --git-dir)/hooks/pre-commit` to `tools/git-hooks/pre-commit`. An existing different file there is renamed to `pre-commit.bak.<unix time>` first. If the link already points at the script, it prints `pre-commit hook already installed.` and exits 0.

What the hook checks is in [Git hooks and CI](/harness/git-hook-and-ci.md).

# Related

- [Task runner](/tools/task-runner.md): the decompile tasks that run `sync-shard`.
- [New worktree](/tools/new-worktree.md): creates the shard worktrees.
- [Build and verify](/tools/build-and-verify.md): the build check that `sync-shard` and `integrate-shards` run.
- [Git hooks and CI](/harness/git-hook-and-ci.md): the pre-commit hook this script links.
