---
type: Tool
title: "permuter wrapper"
description: "tools/permuter imports a candidate into decomp-permuter with the owning file's compiler profile and runs it until interrupted or matched."
tags: [permuter, decompile]
resource: ../../tools/permuter
---

`tools/permuter` imports one candidate C file into decomp-permuter and runs it. It is an offline tool for a person; the agent decompile loop does not run it, and the stop rules are in [decompile-function](/procedures/decompile-function.md). Preconditions and the timeout rule are in [decomp-permuter](/procedures/decomp-permuter.md).

```bash
timeout 300s ./tools/permuter --source-file nonmatchings/<function>/base_N.c <function>
```

| Argument | Meaning |
|---|---|
| `<function>` | Function name, `.s` path, or decomp.me scratch ID. Function names win over scratch IDs. |
| `--source-file`, `-s <path>` | Candidate to import. Default `src/temp.c`. |
| `--preserve-f3dex` | Keep display-list and combine macros unexpanded during import (see below). |

# Source file

With the default `src/temp.c`, the wrapper deletes `src/temp.c` and `src/ctx.c` before importing. It recreates them only for a scratch ID. For a function name with the default, the wrapper exits with `Error: Source file not found: src/temp.c`. Pass `--source-file` explicitly; the [policy hook](/harness/policy-hook.md) keeps agents away from `src/temp.c`.

A scratch ID is all digits, or 5–10 alphanumeric characters. It must contain no `/`, must not end in `.s`, must have no `asm/nonmatchings/**/<name>.s`, and must have no `function.s` beside the source file. For a scratch, the wrapper fetches the scratch from decomp.me (User-Agent from the first line of `.decomp_dev_secret`), writes its context to `src/ctx.c` when present, writes its source to `src/temp.c`, and imports that. The scratch's `diff_label` names the function.

# How it works

1. Finds the function's assembly under `asm/nonmatchings`, or `function.s` beside the source file.
2. Takes the compiler profile from `.source_file` beside the source file, else from the owning C file (`asm/nonmatchings/<dir>/<function>.s` maps to `src/<dir>.c`), else `src/main/__candidate__.c`. Flags come from `make -s --no-print-directory print-cflags SRC=<profile>`.
3. Writes a temporary settings file (removed on exit). Its first line is `compiler_command = "tools/kmc-cc <flags> -DPERMUTER"`, followed by the repository's `permuter_settings.toml`.
4. Runs `tools/decomp-permuter/import.py --settings <file> [--preserve-macros <regex>] <source> <asm>`. It creates `nonmatchings/<function>[-N]` and reports `Done. Imported into <dir>`.
5. Runs `tools/decomp-permuter/permuter.py -j 16 --best-only --stop-on-zero --stack-diffs --quiet --algorithm levenshtein <dir>`.

The wrapper exits with the permuter's exit code. Ctrl-C only sets an interrupt flag, and decomp-permuter turns the interrupt into exit code 0. Exit code 0 therefore does not mean a match.

# Results

Results land in `<dir>/output-<score>-<n>/` with `source.c` and `diff.txt`. A match is an `output-0-*` directory.

Match notification: when the run is not interrupted, the permuter exits 0, an `output-0-*` directory exists, the run took more than 60 seconds, and `.push_secrets` exists (`PUSH_TOKEN` and `PUSH_USER`, git-ignored), the wrapper sends a Pushover message `Matched <function>` (the scratch's `diff_label` for a scratch ID).

# --preserve-f3dex

Passes `--preserve-macros` to `import.py` with this regex, so the importer keeps these macros as written:

| Pattern | Reason |
|---|---|
| `g[DS]P.*` (`gDP*`, `gSP*`) | Display-list macros keep their original invocation |
| `G_CC_.*` | `gDPSetCombineMode`'s arguments are comma-separated macros |
| `G_[CA]CMUX_.*` | Mux constants used by the nested `gDPSetCombineLERP` |
| `G_IM_SIZ_.*` | Texture-loading macros token-paste this argument, so `1##_TILE_BYTES` must survive preprocessing |
| `FR_(NEG\|POS)_FRUSTRATIO_.*` | `gSPClipRatio` token-pastes its argument into these leaf constants |

The `tools/decomp-permuter` submodule is vendored and protected.
