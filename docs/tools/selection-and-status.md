---
type: Tool
title: "Selection, ranking and status tools"
description: "Scripts that rank unmatched functions, find similar matched code, list candidates, batch-run m2c, report nonmatching matches, and clean up per-function environments."
tags: [selection, status, workflow]
---

Scripts print their options with `--help`. Run them from the repository root, with the project venv when third-party imports fail (see [Python environment](/project/setup-and-build.md#python-environment)). Progress is in [progress](/tools/progress.md); the difficulty tiers these scripts filter on are in [difficulty-ledger](/tools/difficulty-ledger.md).

# score_functions.py

`python3 tools/score_functions.py [PATH] [--json] [--min-score MIN] [--max-score MAX] [--tier {1,2,3}] [--by-size | --by-coddog-similarity] [--top-n N]`

Ranks unmatched functions. The default score is a logistic difficulty in (0, 1) from instruction, branch, jump and label counts; higher is harder.

| Flag | Effect |
|---|---|
| `PATH` | File or directory of `.s` stubs. Default `asm/nonmatchings`. Relative paths resolve from the repo root. |
| `--json` | Print names only, as a JSON array. |
| `--min-score`, `--max-score` | Keep functions whose score is in range. Applied before the mode is chosen. |
| `--tier {1,2,3}` | Keep functions of one [difficulty tier](/tools/difficulty-ledger.md#tiers). Default `1`, the functions with no failure history. |
| `--by-size` | Order by instruction count, then name. |
| `--by-coddog-similarity` | For each function, list decompiled functions that coddog finds similar. Mutually exclusive with `--by-size`. |
| `--top-n N` | Matches kept per function. Default 5. Used only with `--by-coddog-similarity`. |

- Default text: `{score:.4f}  {name}  {instructions} instr  {file}`, sorted by score (easiest first), then name.
- `--json` without a mode: `[name, …]`, with no scores.
- `--by-size`: sorted by instruction count, then name, in text and JSON.
- `--by-coddog-similarity`: text is the function name, then one `%6.2f%%  ref  (src/file.c)` line per match. JSON is `[name, ref1, …]`. Rows are ordered by best match percentage, then name; `--by-size` does not change this order. Percentages are not in JSON.
- Coddog mode runs `coddog match <name> -t 0.5` from the repo root with a 300 s timeout, and prints progress to stderr as `[i/N] name`. Only functions that pass `--tier` are scored. A reference is kept only if it has a definition in `src/`; the function's own name is dropped.
- Exit: 0, including when nothing matches. 1 for a missing path (`Error: path not found: …`), a ledger error, or a coddog error (missing binary, timeout, or nonzero exit).
- Consumed by: the decompile tasks in [task-runner](/tools/task-runner.md#task-catalog).

# find_similar_functions.py

`python3 tools/find_similar_functions.py FUNCTION [-n TOP] [-t THRESHOLD] [--coddog-similarity] [--json] [-v]`

Finds decompiled functions similar to one unmatched function, to use as references when matching it.

| Flag | Effect |
|---|---|
| `FUNCTION` | Unmatched function, with a stub under `asm/nonmatchings/`. |
| `-n`, `--top TOP` | Results to show. Default 10. |
| `-t`, `--threshold T` | Minimum similarity from 0 to 1. Default 0.0. Default mode only. |
| `--coddog-similarity` | Score with coddog instead of the built-in score. Needs `coddog` on `PATH` (`cargo install coddog`). |
| `--json` | Print `[FUNCTION, match1, …]`. |
| `-v`, `--verbose` | Add the four category scores under each match. |

- Built-in score: a weighted sum of instruction (0.35), control (0.25), data (0.20) and structural (0.20) similarity. Candidates are every function body in `src/` that has disassembly under `asm/`. Results sort by score, then name, and are filtered by `-t`.
- Coddog mode runs `coddog match <name> -t 0.5` with a 300 s timeout. `-t` is ignored. Matches are kept in coddog order, drop the function itself and names not in `src/`, and are truncated to `--top`. Scores print as percentages.
- Default output: `<name>  (<file>)`, then a line of instruction, branch, jump, label and stack counts, then one `  0.8123 <name>  (<src file>)` line per match, or `  no similar decompiled functions found`.
- Exit: 1 when `FUNCTION` is not an unmatched function (`Error: '<f>' is not an unmatched function (searched asm/nonmatchings)`) or when coddog is missing (`coddog is not on PATH (install it with cargo install coddog)`). 0 otherwise, including empty results.

# list_decomp_candidates.py

`python3 tools/list_decomp_candidates.py FILE [FILE …] [--include-failed] [--summary]`

Lists `INCLUDE_ASM` functions in the given C files, each with its assembly size and [difficulty tier](/tools/difficulty-ledger.md#tiers). Commented-out and preprocessor-disabled lines are ignored.

| Flag | Effect |
|---|---|
| `FILE …` | C files to scan. |
| `--include-failed` | Also list functions with failure history (tier 2 and up). Default: skip them. |
| `--summary` | Print only the trailer line. |

- Output: a header `func`, `source:line`, `asm_size`, `tier`, `asm`, then rows sorted by assembly size, then source and line. A function with no stub sorts last and shows `missing` as its size.
- Trailer, always printed: `# total_include_asm=T eligible=E failed_skipped=F`. `E` is the number of listed rows. `F` is the number of functions skipped for failure history.
- Exit: 0. 1 for an unreadable file (`error: cannot read <path>`) or a ledger error.

# list_matched_functions.py

`python3 tools/list_matched_functions.py [--by-reference-use] [--top-n N]`

Lists matched functions still named `func_<ADDR>` in `src/`. These are the candidates for [cleanup-matched](/tools/task-runner.md#task-catalog).

| Flag | Effect |
|---|---|
| `--by-reference-use` | Order names by how many unmatched functions list them among their references, most used first. Ties keep the input order. |
| `--top-n N` | References per unmatched function, used with `--by-reference-use`. Default 3. |

- Output: always a JSON array of names, with no `--json` flag. Without `--by-reference-use`, names are grouped by file (more matches first, then path) and sorted within each file.
- Exit: 0.

# m2c_one_shot.py

`python3 tools/m2c_one_shot.py [FUNCTION|PATH…] [--dry-run] [--limit N] [--json] [--m2c-timeout S] [--build-timeout S]`

Runs m2c on unmatched functions and scores each result through the decomp-env template's `build.sh`. A candidate is inlined into its owning C file only when it is an exact match.

| Flag | Effect |
|---|---|
| `FUNCTION`, `PATH` | Function names, or `.s` paths under `asm/nonmatchings/`. Default: every unmatched function. |
| `--dry-run` | Score and report only. A match is reported as `dry-run: would inline into owner:line`, and no file changes. |
| `--limit N` | Attempt at most N candidates, counted before results are known. Default 0, no cap. |
| `--json` | Print `{"summary": {status: count}, "results": [{name, status, detail, where}]}`. |
| `--m2c-timeout S` | Timeout for each m2c run, in seconds. Default 30. |
| `--build-timeout S` | Timeout for each `build.sh` run and for the owner's inline compile, in seconds. Default 60. |

- Owner: the single `src/**/*.c` file that contains the function's `INCLUDE_ASM` line. Zero owners gives `skip:no-c-owner`; more than one gives `skip:ambiguous-owner`.
- Inputs: `asm/nonmatchings/**/*.s`; the owner's compile context, built with [m2ctx](#m2ctxpy) and the owner's `make print-cflags` output; the `tools/decomp-env-template` files; `tools/kmc-cc`; `include/`; m2c from `.venv/bin/m2c` or `PATH`; and a MIPS `as`.
- Match: `build.sh` exits 0, prints `Verified exact match: yes`, reports `Score:` of exactly `100.0`, and shows zero differences. Without `--dry-run`, the owner file is rewritten on a match. Everything else happens in a temp directory.
- Output: one line per candidate, `[i/N] <status> <name> <where>[  <detail>]`, where `where` is `owner:line` or the asm path. The last line is `Summary: k=v, …`, or `Summary: no candidates selected`.

| Status | Meaning |
|---|---|
| `match` | Exact match. Inlined, unless `--dry-run`. |
| `miss:m2c` | m2c failed or timed out. |
| `miss:compile` | The base C file did not compile in `build.sh`. |
| `miss:build` | `build.sh` timed out or printed no `Score:`. |
| `miss:score` | Scored, but not an exact match. |
| `skip:no-c-owner` | No owner C file. |
| `skip:ambiguous-owner` | More than one owner C file. |
| `skip:context` | The owner's compile context failed. |
| `skip:target-asm` | The target assembly did not assemble. |
| `skip:include-line` | The owner's `INCLUDE_ASM` line is not alone on its line, or is duplicated. |
| `skip:inline-compile` | The owner did not compile with the inlined function. |

- Exit: 0 for a normal run, including zero matches. 1 for an unknown function, a `.s` path outside the repo, a `.s` path with no unmatched function, or a missing m2c or MIPS `as`. Uncaught exceptions also exit 1.

# m2ctx.py

`python3 tools/m2ctx.py C_FILE`

Writes `ctx.c` at the repo root, a preprocessed context for m2c or decomp.me. The file is overwritten without warning.

- Content: the output of `gcc -E -P -dM` (macro dump), then `gcc -E -P` of `C_FILE`, with the lines of a stock empty-file macro dump removed. Nested function definitions are then removed, since m2c's parser rejects them. Preprocessor lines are kept.
- Flags: `make -s --no-print-directory print-cflags SRC=<file>` supplies only `-I` and `-D`/`-U` flags. Then `-D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -ffreestanding -DM2CTX` is added. Preprocessing uses the host `gcc`, not the KMC toolchain. A `-D` followed by a separate name token loses that name.
- Exit: 0. 1 when preprocessing fails or the output is empty. A failing `make print-cflags` ends the run with a traceback.
- Callers: [decomp-env](/tools/decomp-env.md) (failure there only prints a warning and continues without `--context`) and [m2c_one_shot.py](#m2c_one_shotpy).

# report_nonmatching_matches.py

`python3 tools/report_nonmatching_matches.py [--repo-root DIR] [--scan-root DIR]… [--partial-only | --full-only] [--scratches PATH | --no-scratches] [--min-percent P]`

Reports matches that are not yet in `src/`, and partial matches, from `match_log.txt` files and decomp.me scratches.

| Flag | Effect |
|---|---|
| `--repo-root DIR` | Repository whose `src/` and `symbol_addrs.txt` decide integration. Default: the project root. |
| `--scan-root DIR` | Worktree whose `nonmatchings/` is scanned. Repeatable. The primary repo is always scanned. Roots without `nonmatchings/` are skipped, with a warning when given explicitly. |
| `--full-only` | Print only the 100% sections. |
| `--partial-only` | Print only the partial section. |
| `--scratches PATH` | decomp.me scratch file. Default `tools/scratches.json` under each scanned root. A missing default file means no scratches. |
| `--no-scratches` | Skip decomp.me entirely. Conflicts with `--scratches`. |
| `--min-percent P` | Lower bound for partial matches. Default 0.0. Applies to local match-log rows only. |

- Local rules: a 100% entry counts only if [nonmatching_match_log](#nonmatching_match_logpy) certifies its archive. A partial entry needs only its attempt file under that root's `nonmatchings/`. An entry is skipped when its function is not in the unmatched set or already has a C definition in either root's `src/`. Per function, the best entry has the higher percent, then the lexicographically smaller attempt path.
- decomp.me rows: 100% when the score is 0; otherwise `(max_score − score) / max_score × 100`, which needs a numeric `max_score`. Names are canonicalised through `symbol_addrs.txt` aliases. Integration is checked only in the primary `src/`.
- Output sections:
  1. `100% matched in match_log but not yet integrated into src/`, with rows `name  xx.xxx%  <attempt-file>`, a `local: <workspace>` line, and a `decomp.me` line for scratches that match. Prints `  none` when empty.
  2. `100% matched on decomp.me but not yet integrated into src/`, only when a scratches file is found.
  3. `Partially matched functions`, sorted by percent, highest first.
- A `Totals: full=N decomp.me_full=N partial=N` line prints only when neither `--full-only` nor `--partial-only` is given.
- Exit: 0. 2 for conflicting flags, or when no scan root has `nonmatchings/`.

# fetch_scratches.py

`python3 tools/fetch_scratches.py [--update | --cursor CURSOR]`

Fetches decomp.me scratches for the `gcc2.7.2kmc` compiler into `tools/scratches.json`, which is git-ignored. The request uses the API endpoint `https://decomp.me/api/scratch`, with a 30 s timeout. The User-Agent comes from the first line of `.decomp_dev_secret` when that file exists. The script sleeps 10 s between pages.

| Flag | Effect |
|---|---|
| `--update`, `-u` | Load the existing file. Keep fetching until a page has no new or changed scratch, then rewrite only if something changed. A scratch counts as changed when its score or `last_updated` differs. |
| `--cursor CURSOR`, `-c CURSOR` | Start from a cursor, for example one printed on an error. Default mode overwrites the file with the pages fetched, so scratches from before the cursor are lost. |

- `--update` and `--cursor` are mutually exclusive: stderr `Error: --update and --cursor are mutually exclusive`, exit 1.
- Progress goes to stderr: `Page N: fetched K scratches (total: T)`, then one `New`, `Changed` or `Unchanged` line per scratch, then `No more pages.` and a save line.
- Network errors and bad responses (`OSError`, `ValueError`) print `Error: <e>`. A cursor run also prints `To resume, run:` and the command to continue from that cursor. Exit 1; the file is not written.

# decomp-env-cleanup

`tools/decomp-env-cleanup [--delete] [--verbose] [--force]`

Removes `nonmatchings/<dir>/` environments whose function is already matched. A dry run is the default.

| Flag | Effect |
|---|---|
| `--delete` | Remove the stale directories. Without it the script only reports. |
| `--verbose` | Print `STALE:` and `KEEP:` lines with the reason for each directory. |
| `--force` | Remove stale directories even when they hold unconsolidated notes. |
| `-h`, `--help` | Print usage and exit 0. |

- The function name is the directory name without its trailing `-<suffix>` (for example `func_80001234-ab12` gives `func_80001234`). A directory whose name has no function part is stale.
- Kept, not stale:
  - the function still has a stub under `asm/nonmatchings/`;
  - a `func_<addr>` name was renamed in `symbol_addrs.txt` to a name that still has a stub (`renamed to X, still unmatched`);
  - the directory holds unconsolidated `LEARNINGS.md` or `NEAR_MISS.md` notes, unless `--force` is given (see [list-unconsolidated-learnings](/tools/list-unconsolidated-learnings.md)).
- Stale: the function is already matched (`<name> already matched`, or `renamed to X, already matched`).
- Removal is `rm -rf` of the whole directory, so consolidated notes inside it are deleted too.
- Summary: `Would remove N dirs (X MB). Would keep M dirs.`, followed by `Run with --delete to actually remove them.` when N is above 0. With `--delete`: `Removed N dirs (X MB). Kept M dirs.` Sizes are summed with `du -sb` and shown as whole MiB.
- Unknown options exit 1 with `Unknown option: X`. Requires `python3` and `git`.

# nonmatching_match_log.py

Helper module, with no command line. It parses `match_log.txt` lines and decides which 100% entries are certified.

- Line format: `<name>.c <pct>% <metadata>`. Parsed into an entry with the file name, percent, `exact` flag and sha256.
- Certified attempt path: below 100%, the attempt is `<workspace>/<filename>`, with no existence or hash check. At 100%, the metadata needs an `exact` token and `sha256=<64 hex>`. The file must be `<workspace>/.matches/<name>-<sha256>.c`, and its hash must equal the recorded sha256. Otherwise the entry is not certified.
- Also provides `integrated_function_names` (names defined in `src/`), `unmatched_function_names` (labels plus `func_<ADDR>` stubs under `asm/nonmatchings/`), and symbol alias canonicalisation from `symbol_addrs.txt`.
- Imported by: [difficulty-ledger](/tools/difficulty-ledger.md), [pipeline-metrics](/tools/pipeline-metrics.md), `report_nonmatching_matches.py`, and the unit tests in [code map](/harness/code-map.md#tests).
