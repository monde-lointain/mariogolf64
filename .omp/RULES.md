# Project rules: Mario Golf 64 (always apply)

Carried on every request. `AGENTS.md` has the full instructions.

1. **Matches are measured, never declared.** Only `./tools/build-and-verify.sh` (ROM SHA1) and `./build.sh` in a decomp environment decide a match. Never edit `mariogolf64.sha1`, `baserom.z64`, `asm/`, or the vendored tools (`tools/gcc_kmc`, `tools/kmc-cc`, `tools/decomp-permuter`, `tools/asm-differ`). Never weaken a check to make one function pass.
2. **Fix the harness instead of working around it.** If a tool, script, skill, task prompt, or the policy hook gets in your way (bug, false block, missing option, misleading output), fix it generally rather than routing around it. Follow `AGENTS.md` § Changing the harness: comparison changes need a test that fails before the fix. [tools/HARNESS.md](../tools/HARNESS.md) maps the code.
3. **Commit harness changes separately and immediately.** Stage only the harness paths and use the prefix `harness:`. Uncommitted work does not survive a failed nigel candidate.
4. **Stay on task.** After a harness fix, return to the task you were given.
