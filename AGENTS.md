# AGENTS.md

## Repository Overview

This is a matching decompilation project for Mario Golf 64 (N64, US revision 0). The goal is to create C code that, when compiled, produces the exact same ROM as the original game.

## Project Structure

- `src` decompiled or partially decompiled C code (`src/main` game code, `src/overlay_*` overlays, `src/libultra`, `src/libnusys`, `src/libmus`, `src/libnaudio`, `src/libkmc` libraries). Hand-written library assembly lives next to its C (`src/libultra/**/*.s`, `src/libkmc/*.s`, `src/entry/entry.s`).
- `include` shared headers (`common.h`, `include_asm.h`) and vendored SDK headers (`include/libultra`, `include/libnusys`, ...). Project headers usually live next to their sources (`src/main/*.h`).
- `asm/nonmatchings` unmatched assembly functions extracted from the ROM, one file per function, included from C with `INCLUDE_ASM`.
- `asm/<rom offset>.s` assembly-only segments that do not have a C file yet; `asm/<dir>/<file>.s` full disassembly of each C file for reference.
- `asm/data` extracted data assembly.
- `assets` binary asset blobs extracted from the ROM.
- `symbol_addrs.txt` symbol names and addresses for every segment.
- `mariogolf64.yaml` Splat configuration for ROM extraction and segment layout.
- `linker_scripts` hand-written linker definitions for hardware registers and fixed OS addresses.

## Tools

- `./tools/build-and-verify.sh` re-extracts the ROM, rebuilds it, and verifies the SHA1 checksum.
- `python3 tools/asm-differ/diff.py --no-pager <function name>` compares compiled assembly against the target for a specific function. Note that you *must* rebuild the project manually before digging or you will potentially operate on stale data.
- `./tools/decomp-env --bootstrap-only <function name>` spins up a decompilation environment for a given function under `nonmatchings/`. It can also take an optional `--id` field. The script will only create the directory if one with a matching id does not already exist.
- `python3 tools/score_functions.py <directory>` finds the easiest function to decompile in a given directory (and its subdirectories).
- `python3 tools/project_status.py` shows matched/unmatched progress and the functions that are still non-matching.
- `python3 tools/find_similar_functions.py <function name>` returns a list of similar matched functions. This can provide a useful reference when trying to match code.
- `python3 tools/data-differ/data_diff.py <symbol>` or `./tools/diff-data <symbol>` compares binary data between the target ROM and compiled output for a specific data symbol.
- `python3 tools/data-differ/data_diff.py --find-first-mismatch` scans data symbols in ROM order and shows the first mismatch. Use this after checksum failures when a data variable may be responsible.
- `python3 tools/list_decomp_candidates.py [--summary] <c files...>` lists the `INCLUDE_ASM` functions in the given C files, smallest first, skipping functions with a `nonmatchings/<func>/NEAR_MISS.md` handoff note.
- `python3 tools/rank_decompile_similar_impact.py [--limit N] [--json]` ranks unmatched functions by direct progress plus how much matching them would improve the reference hints for other `decompile-similar` targets.
- `python3 tools/m2c_one_shot.py --dry-run [--limit N] [function...]` runs m2c on unmatched functions and scores each result with the decomp environment's `build.sh`; without `--dry-run` it inlines exact matches into the owning C file.
- `python3 tools/find_unmatched_nonmatchings.py` lists `nonmatchings/` workspaces whose best logged attempt is below 100% and whose function is not yet in `src/`.
- `python3 tools/report_nonmatching_matches.py` reports certified 100% local matches and 100% decomp.me scratches (`tools/scratches.json`) not yet integrated into `src/`, plus partial matches.
- `python3 tools/fix_redundant_externs.py [paths...] [--fix]` finds source-local `extern`s that repeat a project header's prototype; `--fix` removes them and includes the header.
- `python3 tools/check_pointer_arithmetic.py [paths...] [--strict]` reports `(type *)ptr + offset` casts (see Avoid Pointer Arithmetic).
- `.venv/bin/python3 -m unittest discover -s tools -p 'test_*.py'` runs the tool unit tests (the tests import the tools' venv dependencies).
- `bun test ./.omp/hooks/tests/mg64-policy.test.ts` runs the policy hook tests.

## Changing the harness

The harness is everything that builds, scores, selects, or polices the work: `tools/` (except the protected paths below), `tools/decomp-env-template/`, the `Makefile`, `task-runner/`, `.omp/` (config, skills, policy hook), `AGENTS.md`, and `README.md`. [tools/HARNESS.md](tools/HARNESS.md) maps where each job lives and which tests cover it.

The harness is ours to improve. Any session, including a single-function decomp environment or a nigel candidate, may fix a harness bug or remove a friction point, and should do so instead of working around it. Examples: a crash in a tool, a policy-hook false positive, a misleading error message, or a missing flag you needed. Keep the fix general and small, and stay on your original task afterwards.

**Protected, never changed by an agent:** `mariogolf64.sha1`, `baserom.z64`, `asm/`, and the vendored tools (`tools/gcc_kmc`, `tools/kmc-cc`, `tools/decomp-permuter`, `tools/asm-differ`). These are the ground truth a match is measured against, and the policy hook enforces it. If a fix would need one of them, stop and report it to the user.

Changes come in two classes:

- **Workflow changes** (CLI ergonomics, output, selection and ranking, environment setup, docs, skills, task prompts): make them, keep the tests above green, and update the docs or skill that describe the behaviour.
- **Comparison changes**: anything that can change what counts as a match or what may be committed. This covers the build, link, and checksum rules in the `Makefile`, `tools/build-and-verify.sh`, `tools/git-hooks/pre-commit`, the scorer (`tools/decomp-env-template/build.sh`, `dist.py`, `normalize_asm.py`, `objdump.py`), match certification (`tools/nonmatching_match_log.py`, `tools/m2c_one_shot.py`), `tools/data-differ/`, the code-quality checkers, and `.omp/hooks/pre/mg64-policy.ts`. These additionally require:
  1. a test that fails before the change and shows the old behaviour was a real false positive or false negative;
  2. for scorer or certification changes, the score of at least one existing environment (or a fixture) before and after, quoted in the commit message;
  3. for policy-hook changes, the guarded action stays blocked. A hook fix may narrow a pattern that blocks legitimate commands (for example, reading a file). It may never remove a rule or a protected path.

A `COMPILER PROFILES` override in the `Makefile` is a source change, not a comparison change: the checksum verifies it.

Never weaken a check to make a specific function pass. If a check is wrong, fix it for every input and add the test above.

**Commits.** Commit a harness change by itself, as soon as it is done, staging only its own paths (`git add <paths>`, never `-A`). Use the subject prefix `harness:`. This keeps comparison changes bisectable. It also matters under nigel: `reset_command` (`git clean -f; git reset --hard HEAD`) wipes uncommitted work after every failed candidate, and `success_command` would otherwise mix the fix into the candidate's commit. The pre-commit hook rebuilds the whole tree, so commit while `src/` still builds. In a decomp environment, that means before you integrate the function into `src/`.

## Compiler

All C is compiled with KMC GCC 2.7.2 (assembled by KMC binutils 2.6 through `tools/kmc-cc`). Each directory has its own compiler profile (optimization level, defines, include order), with a few file-specific overrides, in the `COMPILER PROFILES` section of the `Makefile`. Generic KMC GCC behavior is summarized in `DECOMPILATION_LEARNINGS.md`.

## Decompilation Learnings

`DECOMPILATION_LEARNINGS.md` is a curated reference of generic KMC GCC and build behavior, not a work log.

- Add an entry only for a generic, reusable property of the compiler or build that the file does not already cover. Put it in the matching section and keep it short.
- Never record function or symbol names, ROM/VRAM addresses, match percentages, attempt histories, or one-off layout discoveries.
- Per-function `tools/decomp-env` sessions treat the file as read-only and keep their findings in the workspace `LEARNINGS.md`.

## Tasks

### Decompile assembly to C code

Use the `decompile-function` skill. Its stop rules are mandatory.

### Matching Data

After changing data definitions or data-related struct layout, build first:

```bash
./tools/build-and-verify.sh
```

If the checksum fails and a data mismatch is suspected, locate the first mismatching symbol:

```bash
python3 tools/data-differ/data_diff.py --find-first-mismatch
```

For a known symbol, compare it directly:

```bash
python3 tools/data-differ/data_diff.py <symbol>
./tools/diff-data <symbol>
```

The data differ reads sizes from `symbol_addrs.txt` (`size:0x...`), the ELF, or `asm/data`, and finds the ROM address through the linker map.

## Validation Checklist

Before declaring any changes to C code complete (including decompiling functions), verify:

- [ ] No pointer arithmetic with manual offset calculations
- [ ] All struct field accesses use `->` or `.` operators
- [ ] No `void*` parameters that should be typed structs
- [ ] Struct sizes match the assembly access patterns
- [ ] Every `INCLUDE_ASM(...)` is on a single line
- [ ] `./tools/build-and-verify.sh` succeeds

## Code Quality Standards

### Use C and Proper Graphics Macros

- Do not use inline assembly, including empty assembly barriers, assembly constraints, or explicit register bindings, to force a match.
- Construct display lists with the appropriate F3DEX2 macros from `PR/gbi.h` and named constants. Do not substitute raw command words or manually encoded opcodes for those macros.
- Look for aggregate macros such as `gDPLoadTLUT_pal16` and `gDPLoadTextureTile_4b` before expanding command sequences individually.
- A matching checksum does not make an implementation complete if it violates these requirements. Resolve differences through C types, control flow, and the appropriate graphics macros.

### Avoid Pointer Arithmetic

When you see pointer arithmetic patterns like `*(type*)((u8*)ptr + offset)`:

1. **Identify the access pattern:**
   - What offset is being accessed? (e.g., `0xC` means field at offset 12)
   - Is it accessing an array element? (e.g., `arg1 * 36` means 36-byte elements)
   - What field within the element? (e.g., `+ 0xA` means field at offset 10)

2. **Create appropriate structs:**
   - Define the element struct with correct size and field offsets
   - Define the container struct with pointer at correct offset
   - Use meaningful names or `unk[Offset]` naming convention

3. **Verify struct sizes:**
   - Calculate total size to ensure it matches the multiplier in pointer arithmetic
   - Example: `arg1 * 36` means struct must be exactly 36 (0x24) bytes

### Struct Modification and Extension

When modifying struct definitions:

- Search the entire codebase for other references to the same struct
- Check if other functions access fields at nearby offsets
- Verify ALL affected functions still match after struct changes
- Example: If you add a field at offset 0x14, search for all functions accessing that struct and verify they still compile to the correct offsets

### Avoid Redundant Declarations

After adding your decompiled function, check for any redundant extern declarations:

1. **Search for existing declarations**: For each extern function you used, search the codebase to see if it's already declared in a header file:
   - Use `rg "void functionName" include/` to search headers
   - Use `rg "void functionName" src/` to search source files and source-local headers

2. **Remove redundant externs**: If a function is already declared in an included header file, remove your extern declaration to avoid duplication

3. **Verify the build still works** after removing redundant externs

Example: If you added `extern void setCallback(void *);` but a header that is already included declares it, remove your extern declaration.

## Code Structure

C files should be organized in the following way:

- Macro definitions
- Struct definitions
- Global variables
- Function declarations
- Function implementations

You should proactively reorganize code to preserve this structure.
