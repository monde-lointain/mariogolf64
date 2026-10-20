---
type: Tool
title: "verify-candidate"
description: "The gate nigel runs before committing a candidate: protected paths, baserom hash, optional decompiled-in-src check, new pointer arithmetic on changed lines, then build-and-verify."
tags: [verify, comparison, nigel]
resource: ../../tools/verify-candidate
---

```bash
./tools/verify-candidate [--decompiled FUNCTION]
```

Run from the repo root. Exit 0 prints `verify-candidate: OK`. A failed check or build, or a `--decompiled` value that names no function, exits 1 and prints one line on stderr: `verify-candidate: FAILED: <reason>`. Unknown options exit 2 (argparse).

`--decompiled` takes a function name, an asm path (`asm/nonmatchings/main/func_x.s`), or nigel's candidate key: a JSON array whose first element is the function (decompile-similar keys look like `["func_x","ref1",...]`). A trailing ` (worktree)` annotation is accepted.

# What it checks

1. **Protected paths** `mariogolf64.sha1`, `tools/kmc-cc`, `tools/decomp-permuter` and `tools/asm-differ` have no changes against HEAD. `git status --ignore-submodules=none` makes submodule content and a moved submodule HEAD count.
2. **Baserom**: `baserom.z64` hashes to the sha1 recorded in `mariogolf64.sha1`.
3. **Decompiled** (only with `--decompiled`): the function is defined in `src/**/*.c` as `FUNCTION(...) {`, and no `INCLUDE_ASM("...", FUNCTION)` line remains. Comments are ignored. Prototypes and call sites do not count.
4. **Pointer arithmetic** (`tools/check_pointer_arithmetic.py`): no `(type*)ptr + offset` cast-arithmetic violation sits on a line that a changed project `.c` file adds or changes. The diff base is HEAD, so staged and unstaged edits count, and a new untracked file counts in full. Violations on other lines pass. A file the checker cannot parse prints a warning and passes.
5. **Build**: `./tools/build-and-verify.sh` exits 0, which is the full ROM SHA1 check.

Checks 1 to 4 run first, so a rejected candidate does not pay for a rebuild.

# Why check 3 exists

A candidate that leaves the function as `INCLUDE_ASM` still produces a matching ROM, because the original assembly is linked back in. build-and-verify alone accepts that state, so nigel would commit a "success" that changed no C. Check 3 rejects it. `tools/test_verify_candidate.py` covers this case, along with the protected-path and baserom checks.

# Why check 4 exists

Pointer arithmetic through a cast hides the struct or array layout that decompiled code should use. Existing violations are debt, so the check reports only lines a candidate adds or changes. The message names the file, line, and expression. The gate re-runs itself under `.venv/bin/python3` when the running Python lacks pycparser (from `requirements.txt`), so the `#!/usr/bin/env python3` shebang works in any environment that has the venv. Without a venv, check 4 fails and names the missing import. `tools/test_verify_candidate.py` covers the boundaries: added, untouched, edited, staged, untracked, unparsable, and vendored lines.

# Not checked

* `asm/` is regenerated from `baserom.z64` by build-and-verify, so edits there cannot survive the build.
* `tools/gcc_kmc` is gitignored and comes from `make setup`, which downloads the latest unpinned release. The repo holds no reference digest for it, so this gate cannot detect changes to it. The policy hook blocks agent shell writes into it. A change that alters output for matched functions also fails the build.
* Check 1 compares against HEAD. A protected file that an earlier commit in the same session changed is invisible to it. nigel runs its verify before committing, so a candidate's own edits are still uncommitted at that point.
* Check 4 compares against HEAD. nigel runs its verify before its commit step, but the agent may commit during its run. Commits made during the run are then already in HEAD, so check 4 does not see them. nigel does not export its start commit, so no other base is available.
* Check 4 scans only `.c` files in `src/` and `include/`. A macro in a header that expands to pointer arithmetic is reported at its definition, which check 4 does not scan.

# Use

`verify_command` in `task-runner/config.yaml` is `./tools/verify-candidate`. Decompile tasks pass `--decompiled $CANDIDATE` in their own `verify_command` ([task-runner](/tools/task-runner.md)). Global tasks run the gate without the flag.
