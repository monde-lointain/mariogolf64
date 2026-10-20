---
type: Tool
title: "Code-quality checkers"
description: "Scripts that report code-quality problems in src/ and include/: pointer arithmetic, low-quality matches, redundant externs, header and struct problems, global types and oversized symbols."
tags: [quality, checkers]
---

Each checker prints findings for the tree and exits according to its own rules. Only `fix_redundant_externs.py --fix` edits files. Run every script from the repository root. The rules these scripts enforce are in [code quality](/project/code-quality.md).

# check_pointer_arithmetic.py

`.venv/bin/python3 tools/check_pointer_arithmetic.py [paths…] [--include-vendored] [--strict] [-v]`

Reports a cast to a pointer type applied directly to `+` or `-`, such as `(u8 *)ptr + offset`. The script uses pycparser, so run it with the project venv (see [Python environment](/project/setup-and-build.md#python-environment)). It preprocesses each file with the include and define flags from `make print-cflags` (see [compiler profiles](/project/compiler-profiles.md)), using the host `gcc -E`.

| Flag | Effect |
|---|---|
| `paths` | C files or directories, relative to the repo root. Default `src`. |
| `--include-vendored` | Also check vendored SDK sources (`src/lib*`, `include/`). Without it, only project-owned files are checked. |
| `--strict` | Exit 1 when there are violations or a file cannot be parsed. |
| `-v`, `--verbose` | Print each file to stderr as it is checked. |

- Output: `==== Pointer Arithmetic Check ====`, then `No violations found.` or `Found N violation(s) in M file(s):` with one `path:LINE:COL: [date] pointer arithmetic with cast: (TYPE)ptr + offset` line per site. Sites are sorted oldest first by git blame date; the date is omitted when blame fails.
- Unparsable files are listed on stderr as not checked. Nothing is modified.
- Exit: 0 by default; 1 with `--strict` on violations or unparsable files; 2 for a missing path or an explicit file that is not a project-owned `.c` file.
- Used by: [verify-candidate](/tools/verify-candidate.md). Edits change what it accepts, so treat them as [comparison changes](/harness/changing-the-harness.md).

# detect_low_quality_matches.py

`python3 tools/detect_low_quality_matches.py PATH [--exhaustive] [--verbose] [--json] [--limit N]`

Scores each function by how many low-quality patterns it contains. Badness is violations divided by non-blank lines. The patterns are `((T*)expr)[index]`, `(T*)base ± offset`, `*(T*)((u8*)…)`, `->unkXX`, `goto`, and identifiers named `temp_*`, `var_*` or `phi_*`.

| Flag | Effect |
|---|---|
| `PATH` | Required. Directory or file to scan. The libultra, libkmc, libnusys, libmus and libnaudio directories and git-ignored files are skipped. |
| `--exhaustive` | List every function with a violation, worst first. |
| `--verbose`, `-v` | Print `NAME (score: S, V violations in L lines, FILE)` for each function. |
| `--json` | Print `[[file, name], …]`. This is the candidate list for `fix-low-quality`. |
| `--limit N` | Cap the number of results. Applies only with `--json` or `--exhaustive`. |

- Output: by default, the name of the worst function.
- Exit: 0 when it runs, including when it finds violations; 1 when `PATH` does not exist.
- Used by: [fix-low-quality](/tools/task-runner.md#task-catalog). It reuses the C scanner from [find_similar_functions](/tools/selection-and-status.md#find_similar_functionspy).

# fix_redundant_externs.py

`python3 tools/fix_redundant_externs.py [paths…] [--fix] [--include-vendored]`

Finds two kinds of problem. A function `extern` in a source file that repeats a project header prototype is redundant. A forward declaration in the same file, placed after the first definition, must move above it.

| Flag | Effect |
|---|---|
| `paths` | Files or directories. Default `src`. |
| `--fix` | Remove each redundant extern, add a direct `#include` for its unambiguous header, and move same-file forward declarations before the first function definition. |
| `--include-vendored` | Also index vendored headers (`include/` except `common.h`, and `src/lib*`). |

- Ambiguity: when more than one header declares the function, the header already included wins, then a header with the same stem. A tie is ambiguous. Ambiguous cases are reported, not fixed.
- Output, one line per violation: `path:LINE: redundant extern for NAME; declared by HEADER`, `…; ambiguous headers: H1, H2`, or `…; move before the first function definition`. With `--fix`, a summary `fixed N redundant extern(s); M ambiguous`.
- Exit: without `--fix`, 1 when there are violations. With `--fix`, 1 only when ambiguous cases remain. 2 for a bad path.
- Referenced by [code quality](/project/code-quality.md). No task consumes it.

# find_header_hygiene_candidates.py

`python3 tools/find_header_hygiene_candidates.py [--json]`

Takes no paths and scans the whole project. Each finding names a header, a source, or both, and carries one or more tags:

| Tag | Meaning |
|---|---|
| `INCOMPLETE_TYPE:NAME` | A header names a type that it does not define completely. |
| `EXTERN_FUNCTION_IN_HEADER:NAME` | A header declares a function `extern`. |
| `SOURCE_EXTERN_FUNCTION:NAME` | A source file declares a function `extern`. |
| `NO_OWNING_HEADER` | A source file has no header that owns it. The header column shows this sentinel. |

- Output: `HEADER SOURCE [TAG …]` per finding. `--json` prints an array of `[header, source]` pairs, with no tags.
- Exit: 0 always.
- Used by: [clean-headers](/tools/task-runner.md#task-catalog).

# find_source_header_pairs.py

`python3 tools/find_source_header_pairs.py [--json]`

Lists each `.c` file under `src/` and `include/` that includes a header of the same stem.

- Output: `src/x.c src/x.h` per pair. `--json` prints an array of strings in that form.
- Exit: 0 always.
- Used by: [cleanup-structs](/tools/task-runner.md#task-catalog).

# find_inconsistent_global_types.py

`python3 tools/find_inconsistent_global_types.py [--root ROOT]`

Reads `symbol_addrs.txt` and lists each non-function symbol whose C declarations do not share one canonical type. The canonical form drops storage-class keywords, replaces array bounds with `[]`, and removes whitespace.

| Flag | Effect |
|---|---|
| `--root ROOT` | Project root to read. Default: the repository root. |

- Output: symbol names, sorted, one per line. A note goes to stderr when `symbol_addrs.txt` is missing.
- Exit: 0 always.
- Used by: [consolidate-global-types](/tools/task-runner.md#task-catalog), which runs it with no arguments.

# find_struct_unions.py

`python3 tools/find_struct_unions.py [PATHS…] [--include-vendor] [--json] [--verbose|-v]`

Finds a `union`, or a `typedef union {…} NAME`, declared while a `struct` body is open. The default paths are `src` and `include`.

| Flag | Effect |
|---|---|
| `PATHS` | Files or directories to scan. Default `src` and `include`. |
| `--include-vendor` | Also scan vendored code: everything under `include/` except `common.h`, and `src/{libkmc,libmus,libnaudio,libnusys,libultra}`. |
| `--json` | Print an array of repo-relative paths. |
| `--verbose`, `-v` | Print `path: N (lines L1, L2, …)` for each file. |

- `ctx.c` is always excluded.
- Output: one path per line by default.
- Exit: 2 for a missing path; otherwise 0.
- Used by: [remove-struct-unions](/tools/task-runner.md#task-catalog).

# find_oversized_symbols.py

`python3 tools/find_oversized_symbols.py [--symbol-addrs FILE]… [--elf ELF] [--readelf READELF] [--verbose]`

Reports symbols whose linked size is larger than the size recorded in `symbol_addrs.txt`. The recorded size comes from a `size:` field on a non-function entry; if a name appears in more than one file, the first file wins. The linked size is the largest OBJECT symbol size in the ELF at the recorded address, ignoring a `.NON_MATCHING` suffix. A finding is linked > recorded. The entry format is in [symbol_addrs format](/reference/symbol-addrs-format.md).

| Flag | Effect |
|---|---|
| `--symbol-addrs FILE` | Symbol file to read. Repeatable. Default `symbol_addrs.txt`. |
| `--elf ELF` | Built ELF. Default `build/mariogolf64.elf`, which exists after `./tools/build-and-verify.sh`. |
| `--readelf READELF` | Binary run as `READELF -Ws ELF`. Default `mips-linux-gnu-readelf`. |
| `--verbose` | Print `NAME: symbol_addrs=0x… linked=0x… at=symbol_addrs.txt:LINE`. |

- Output: one name per line by default.
- Exit: 0 whether or not there are findings; 2 for bad arguments, a missing ELF or symbol file, or a readelf failure.
- Used by: [fix-symbol-footprints](/tools/task-runner.md#task-catalog).
