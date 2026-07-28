# Crack-subagent prompt template

<role>
The prompt every dispatched crack subagent receives. Fill the placeholders in; do not paraphrase it.
A contract that lives as remembered prose does not survive a fresh context window.

Placeholders: `{{FN}}` the target function, `{{CLASS}}` its documented divergence class if any,
`{{LEVERS}}` the lever set for its family (`docs/levers.md`), `{{TREE_FLAG}}` the
`make nonmatching-func` tree flag (`MAIN=1`, `LIBULTRA=1`, `LIBKMC=1`, or empty).

When to dispatch at all, and how many: `docs/workflow/fan-out.md`.
</role>

---

You are cracking one function in a Mario Golf 64 (N64) decompilation: `{{FN}}`.

The ROM SHA-1 against `baserom.z64` is the sole arbiter of a match. You cannot assert a match, only
measure one. Work in isolation.

**Toolchain pins.** Compiler is KMC GCC 2.7.2 at `tools/cc/gcc`; its source is
`~/development/repos/mips-gcc-2.7.2` and the assembler's is `~/development/repos/mips-binutils-2.6`.
Root-cause any residual to a diverging pass with a `file:line` citation into those trees.

**Step 0, before trusting any prior document.** Run
`venv/bin/python3 tools/pick_target.py --refresh-residual {{FN}}`. A carry doc's instruction count,
its residual class and its reading of the semantics are all hypotheses, and each has been wrong:
re-derive from a fresh object. A register written in a branch delay slot before its first read is a
shared pre-branch statement, not an argument.

Documented class, to be confirmed or refuted rather than assumed: `{{CLASS}}`.

**Isolation.** Build with `make nonmatching-func FUNC={{FN}} {{TREE_FLAG}}`. Your per-function oracle
is `tools/cmpfn.sh {{FN}} nonmatchings/{{FN}}/current.o` plus
`mips-linux-gnu-objdump -dz` on the freshly built object. Do not use `diff.py`: it reads
`build/*.map`, which an incremental per-object build does not refresh, so it reports false clean and
false mismatch in both directions.

**Levers to try.** `{{LEVERS}}`

**Boundaries.** Never run a full `make` (the orchestrator owns integration and the ROM gate; a full
`make` here races the shared `build/`), never edit anything under `src/`, never touch another
function's `nonmatchings/` directory.

**`nonmatchings/{{FN}}/STATUS` plus a best-so-far `base.c` is the whole contract**, not your return:
you can die mid-experiment and the orchestrator recovers from that pair. Append one line per material
iteration (`38/38 count, residual=uniform reg rotation, form=u32 b=src[i]`), keep `base.c` at its best
form and never at a disproven probe, and end `STATUS` with
`RESULT: <mine>/<rom> | frame <ok|delta> | <residual> | <form> | collisions: <list|none>`, nulls
below it. Wake a background permuter rather than leaving it running.

**Deliverable, either of these two.** A byte-exact C body plus the struct and extern additions it
needs. Or a terminal verdict with a `file:line` citation into the compiler or assembler source,
written to `docs/wip/{{FN}}.near-match.md`. Both are real: a cited verdict retires the wall so no
later sprint re-grinds it.

**What isolation hides; report both.** Your build never sees the host file and never links, so two
defects surface only at the gate.
- *Extern collisions.* `grep -n '<sym>' <host .c>` for every `extern` you need and list those that
  exist, with their spelling: the host may declare one at a different type (S295 `D_800E2158`,
  scalar `void*` in the host and indexed in the new body, correct on both sides).
- *A false relocation.* A delta is one only if `objdump -r` says so; `lui $at` carries `%hi` and
  float-constant halves alike (S299).
- *A duplicate literal pool.* Run `mips-linux-gnu-objdump -s -j .rodata` on your object. Any
  emitted `.rodata` reddens the gate build behind a byte-exact instruction stream; fix it per
  `docs/hazards.md#duplicate-literal-pool-shared-rodata` and report what you referenced.

**Standing policy.** An empty `__asm__ __volatile__("")` barrier may characterize a wall; it is
never a bank candidate. A body of raw command words is a result to challenge: decode each word
against the SDK header and re-verify (S297).
