---
type: Tool
title: "permuter wrapper"
description: "tools/permuter imports a candidate into decomp-permuter with the owning file's compiler profile and runs it until interrupted or matched."
tags: [permuter, decompile]
resource: ../../tools/permuter
---

```bash
timeout 300s ./tools/permuter --source-file nonmatchings/<function>/base_N.c <function>
```

Procedure, preconditions and the timeout rule: [decomp-permuter](/procedures/decomp-permuter.md).

| Argument | Meaning |
|---|---|
| `<function>` | Function name, `.s` path, or decomp.me scratch ID. Function names win over scratch IDs. |
| `--source-file`, `-s <path>` | Candidate to import. Default `src/temp.c`, which the wrapper recreates and the [policy hook](/harness/policy-hook.md) forbids agents to touch, so agents always pass this flag. |
| `--preserve-f3dex` | Keep `gDP*`, `gSP*`, `G_CC_*`, `G_[CA]CMUX_*`, `G_IM_SIZ_*` and frustration macros from being expanded during import. Use for display-list functions. |

How it works:

1. Finds the function's assembly under `asm/nonmatchings` (or `function.s` next to the source file).
2. Takes the profile from `.source_file` beside the candidate (an environment's), otherwise from the owning C file, otherwise `src/main/__candidate__.c`; reads the flags with `make print-cflags`.
3. Writes a temporary settings file whose `compiler_command` is `tools/kmc-cc <flags> -DPERMUTER` followed by `permuter_settings.toml`, then calls `tools/decomp-permuter/import.py` and runs the permuter in the directory it reports.

Results land in `nonmatchings/<function>[-N]/output-<score>[-N]/` (`source.c`, `diff.txt`). The `tools/decomp-permuter` submodule is vendored and protected.
