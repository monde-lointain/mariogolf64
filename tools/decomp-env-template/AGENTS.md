# AGENTS.md

## Your Job

You are decompiling Nintendo 64 assembly from Mario Golf 64. Produce C for `$functionName` that, compiled, matches the target assembly 100%.

The compiler is KMC GCC 2.7.2 (C89). `./build.sh` applies the exact compiler profile of the C file that owns `$functionName` (recorded in `.source_file`; profiles are in `../../docs/project/compiler-profiles.md`).

Project knowledge is the OKF bundle in `../../docs/` (start at `../../docs/index.md`; links beginning with `/` are relative to `docs/`). The procedure for this job is `../../docs/procedures/decompile-function.md`; its stop rules are mandatory.

## Laying the Foundation

Before matching attempts, gather context for `$functionName` and make `base.c` compile. MAKE THE MINIMAL SET OF CHANGES NECESSARY TO COMPILE `base.c`, so the baseline score is accurate.

Delegate the following block to an omp `task` subagent:

<subagent-instructions>
1. Open `../../docs/reference/kmc-gcc/index.md` and, using `base.c` and `target.s`, select the concepts it lists that apply to this function. Record a compact digest in `LEARNINGS.md` naming each selected concept, why it applies, and the concrete technique to try. Include the digest in your report. Do not copy unrelated sections or merely say the files were read.
2. Explore how $functionName is used: `../../src`, `../../include`, and the unmatched code in `../../asm/nonmatchings`. Write a summary of what it is and how it is used to `LEARNINGS.md`.
3. Make `base.c` build. It may depend only on `"common.h"`; provide any other missing types inline rather than via `#include`. Do not stop until it builds. Report status and findings.
4. Verify that the signatures `base.c` relies on have the right argument count, argument types and return type; these are frequently hallucinated or mis-decompiled.
5. Consider the function's purpose. Does the C reflect it? Note logical errors.
6. Report progress and findings, including the digest from step 1.
</subagent-instructions>

## Build Loop

After `base.c` builds, repeat:

1. Run `./build.sh <attempt>.c` to build and diff against the target. 100% with `Verified exact match: yes` is a match.
2. Inspect `<attempt>_diff`, `target.s` and the object dumps for control-flow and instruction differences.
3. Plan an improvement, considering what the original developers intended given the function's purpose.
4. Write a new self-contained file `base_<n>.c` (`n` = attempt number) with changes you expect to help. Never include another mutable candidate: later edits would invalidate its recorded score. Start small and change one thing at a time.
5. Record useful observations in `LEARNINGS.md`.
6. Stop at a 100% match (`Verified exact match: yes`), or when `build.sh` prints a line starting `STOP:`. That line is the hard stop after the stall limit of non-improving attempts. Do not make another attempt. Record the stop in the difficulty ledger with the command the line prints and the outcome from `../../docs/procedures/decompile-function.md` ("Stop rules"), then report.

## Tools

- `./build.sh <file>.c [opt_flag]` compiles an attempt with the owning file's profile and scores it.
- `./diff.sh <file>.o` shows a normalized diff for a built attempt.
- `./objdump.py <file>.o` dumps normalized MIPS assembly.
- `./map_asm_to_c.py <file>.o <line>` maps an assembly line back to its C.

These helpers are symlinks into `../../tools/decomp-env-template/`. If one of them or any project tool is broken or in your way, fix it at the source and commit it separately (`../../docs/harness/changing-the-harness.md`).

A symbol-only difference (for example `%lo(D_800B67A5)` against `%lo(array+1)`) is a naming difference in the extracted assembly, not an instruction difference. Note it; do not distort the C to chase it.

## Coding Guidelines

Full standard: `../../docs/project/code-quality.md`. In short:

- Look for existing types, prototypes and globals in `../../src` and `../../include` before adding new ones.
- Prefer struct fields and array indexing to pointer arithmetic. Declarations go at the start of each block (C89).
- No comments in candidate C unless project style needs them.
- Never use `INCLUDE_ASM`, `INCLUDE_RODATA`, inline assembly, empty asm barriers, asm constraints or explicit register bindings as a shortcut.
- `#define` is for constants only, not for struct-access hacks or permutations.
- Build display lists with the F3DEX2 macros from `PR/gbi.h` and named constants, never raw command words.

## Strategy

Generic KMC GCC behavior is in `../../docs/reference/kmc-gcc/`. It is read-only for you: put function-specific findings in `LEARNINGS.md`.

- How to approach a function: phases, plateaus, near-miss triage, large functions, data layout: `../../docs/procedures/matching-method.md`.
- m2c artefacts (do-while loops, gotos, duplicated temporaries, shifts, false returns) and GCC 2.7.2 codegen to accept: `../../docs/reference/decompilation-artefacts.md`.
- Functions that write 8-byte command pairs through a `Gfx *` cursor build F3DEX2 display lists: use the `decompile-microcode` skill; never manipulate the cursor by hand.
- If the symbols a function uses look like one array or struct split into many, update `symbol_addrs.txt` to the true layout, even if that means a wider refactor (`../../docs/reference/symbol-addrs-format.md`).
