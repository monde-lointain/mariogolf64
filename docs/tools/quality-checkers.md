---
type: Tool
title: "Code-quality checkers"
description: "Scripts that find pointer arithmetic, low-quality matches, redundant externs, header-ownership problems, struct unions and oversized symbols."
tags: [quality, checkers]
---

They enforce the [code-quality standards](/project/code-quality.md) and feed the nigel cleanup tasks. Because they decide what is an acceptable match, changing them is a [comparison change](/harness/changing-the-harness.md).

| Command | Reports |
|---|---|
| `python3 tools/check_pointer_arithmetic.py [paths] [--strict] [-v] [--include-vendored]` | `(type *)ptr + offset` casts. Exit 1 under `--strict` on a violation or unparsable file; default path `src`. |
| `python3 tools/detect_low_quality_matches.py <path> [--exhaustive] [--json] [--limit N]` | Functions with low-quality match patterns, ranked by badness. Default prints only the worst. `--json` emits `[file, function]` pairs (candidate source of `fix-low-quality`). |
| `python3 tools/fix_redundant_externs.py [paths] [--fix] [--include-vendored]` | Source-local function externs that repeat a project header's prototype; `--fix` removes them and adds the include. Ambiguous cases are listed, not changed. |
| `python3 tools/analyse_headers.py <dir> [--limit N] [--clean] [--json]` | Functions defined in `.c` files but declared `extern` elsewhere, externs without a declaring include, declarations in `.c` that belong in a header. Suppressions go in `tools/difficult-headers` (`path:line`), which does not exist yet. |
| `python3 tools/find_header_hygiene_candidates.py [--json]` | Header-ownership problems (candidate source of `clean-headers`). |
| `python3 tools/find_source_header_pairs.py [--json]` | Source/header pairs for struct cleanup (candidate source of `cleanup-structs`). |
| `python3 tools/find_inconsistent_global_types.py` | Global data symbols declared with inconsistent C types (`consolidate-global-types`). |
| `python3 tools/find_struct_unions.py [paths] [--json] [-v]` | Structs with inline or typedef'd union fields (`remove-struct-unions`). |
| `python3 tools/find_oversized_symbols.py [--verbose]` | Data symbols whose linked ELF size exceeds the `size:` in `symbol_addrs.txt` (`fix-symbol-footprints`). Needs a built ELF and `mips-linux-gnu-readelf`. |

Unit tests for the tools: `.venv/bin/python3 -m unittest discover -s tools -p 'test_*.py'` (the venv interpreter is required; system `python3` fails on import).
