---
type: Tool
title: "verify-candidate"
description: "The gate nigel runs before committing a candidate: protected paths, baserom hash, optional decompiled-in-src check, then build-and-verify."
tags: [verify, comparison, nigel]
resource: ../../tools/verify-candidate
---

```bash
./tools/verify-candidate [--decompiled FUNCTION]
```

Run from the repo root. Exit 0 prints `verify-candidate: OK`. Any other failure exits 1 and prints one line on stderr: `verify-candidate: FAILED: <reason>`.

`--decompiled` takes a function name, an asm path (`asm/nonmatchings/main/func_x.s`), or nigel's candidate key: a JSON array whose first element is the function (decompile-similar keys look like `["func_x","ref1",...]`). A trailing ` (worktree)` annotation is accepted.

# What it checks

1. **Protected paths** `mariogolf64.sha1`, `tools/kmc-cc`, `tools/decomp-permuter` and `tools/asm-differ` have no changes against HEAD. `git status --ignore-submodules=none` makes submodule content and a moved submodule HEAD count.
2. **Baserom**: `baserom.z64` hashes to the sha1 recorded in `mariogolf64.sha1`.
3. **Decompiled** (only with `--decompiled`): the function is defined in `src/**/*.c` as `FUNCTION(...) {`, and no `INCLUDE_ASM("...", FUNCTION)` line remains. Comments are ignored. Prototypes and call sites do not count.
4. **Build**: `./tools/build-and-verify.sh` exits 0, which is the full ROM SHA1 check.

Checks 1 to 3 run first, so a rejected candidate does not pay for a rebuild.

# Why check 3 exists

A candidate that leaves the function as `INCLUDE_ASM` still produces a matching ROM, because the original assembly is linked back in. build-and-verify alone accepts that state, so nigel would commit a "success" that changed no C. Check 3 rejects it. `tools/test_verify_candidate.py` covers this case, along with the protected-path and baserom checks.

# Not checked

* `asm/` is regenerated from `baserom.z64` by build-and-verify, so edits there cannot survive the build.
* `tools/gcc_kmc` is gitignored and comes from `make setup`, which downloads the latest unpinned release. The repo holds no reference digest for it, so this gate cannot detect changes to it. The policy hook blocks agent shell writes into it. A change that alters output for matched functions also fails the build.
* Check 1 compares against HEAD. A protected file that an earlier commit in the same session changed is invisible to it. nigel runs its verify before committing, so a candidate's own edits are still uncommitted at that point.

# Use

`verify_command` in `task-runner/config.yaml` is `./tools/verify-candidate`. Decompile tasks pass `--decompiled $CANDIDATE` in their own `verify_command` ([task-runner](task-runner.md)). Global tasks run the gate without the flag.
