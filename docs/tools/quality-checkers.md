---
type: Tool
title: "Code-quality checkers"
description: "Scripts that find pointer arithmetic, low-quality matches, redundant externs, header-ownership problems, struct unions and oversized symbols."
tags: [quality, checkers]
---

They enforce the [code-quality standards](/project/code-quality.md) and feed the nigel cleanup tasks ([task-runner](/tools/task-runner.md)). Because they decide what is an acceptable match, changing them is a [comparison change](/harness/changing-the-harness.md).

| Command | Reports |
|---|---|
| `python3 tools/check_pointer_arithmetic.py [paths] [--strict] [-v] [--include-vendored]` | `(type *)ptr + offset` casts. Paths default to `src`. Exit 0 normally; with `--strict`, exit 1 on a violation or an unparsable file; exit 2 on a bad path. |
| `python3 tools/detect_low_quality_matches.py <path> [--exhaustive] [--json] [--limit N] [-v]` | Functions with low-quality match patterns. By default it prints only the worst function name; `--exhaustive` ranks every violating function by badness. `--json` emits `[[file, function], ...]`, the input for `fix-low-quality`. |
| `python3 tools/fix_redundant_externs.py [paths] [--fix] [--include-vendored]` | Function externs in a source file that repeat a project header's prototype. Paths default to `src`. Without `--fix` it reports and exits 1 when violations exist. With `--fix` it removes each redundant extern, adds a direct `#include`, and exits 1 only if ambiguous cases remain. A declaration is left alone when two equally suitable headers declare the same function; the report lists the candidate headers. |
| `python3 tools/analyse_headers.py <dir> [--limit N] [--clean] [--json] [--include-vendor]` | Functions defined in `.c` files but declared `extern` elsewhere, externs without a declaring include, and declarations in `.c` files that belong in a header. `--json` emits `file:line` references for the task runner. `--clean` removes unused entries from `tools/difficult-headers`. Known issues are suppressed through `tools/difficult-headers`, one `path:line` per line; the file is optional, and when it is absent nothing is suppressed. |
| `python3 tools/find_header_hygiene_candidates.py [--json]` | Header-ownership problems, the candidate list for `clean-headers`. `--json` emits a JSON array. |
| `python3 tools/find_source_header_pairs.py [--json]` | Source/header pairs for struct cleanup, the candidate list for `cleanup-structs`. `--json` emits a JSON array. |
| `python3 tools/find_inconsistent_global_types.py [--root ROOT]` | Global data symbols declared with inconsistent C types (`consolidate-global-types`). `--root` defaults to the repository root. |
| `python3 tools/find_struct_unions.py [paths] [--include-vendor] [--json] [-v]` | Structs with inline or typedef'd union fields (`remove-struct-unions`). Paths default to `src` and `include`. |
| `python3 tools/find_oversized_symbols.py [--symbol-addrs FILE]... [--elf ELF] [--readelf READELF] [--verbose]` | Data symbols whose linked ELF size exceeds the size recorded in `symbol_addrs.txt` (`fix-symbol-footprints`). `--symbol-addrs` is repeatable. Needs a built ELF (default `build/mariogolf64.elf`) and `mips-linux-gnu-readelf`. |

Their unit tests, and how to run them: [harness code map](/harness/code-map.md#tests).
