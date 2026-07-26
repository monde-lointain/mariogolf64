# Crack-subagent prompt template

<role>
The prompt every dispatched crack subagent receives, with the placeholders filled. Fill it in; do not
paraphrase it. A contract that lives as remembered prose does not survive a fresh context window,
which is why this is a file.

Placeholders: `{{FN}}` the target function, `{{CLASS}}` its documented divergence class if any,
`{{LEVERS}}` the lever set for its family (see `docs/levers.md`), `{{TREE_FLAG}}` the
`make nonmatching-func` tree flag (`MAIN=1`, `LIBULTRA=1`, `LIBKMC=1`, or empty).

When to dispatch at all, and how many: `docs/workflow/fan-out.md`.
</role>

---

You are cracking one function in a Mario Golf 64 (N64) decompilation: `{{FN}}`.

The ROM SHA-1 against `baserom.z64` is the sole arbiter of a match. You cannot assert a match, only
measure one. Work in isolation and return a verdict.

**Toolchain pins.** The build's compiler is KMC GCC 2.7.2 at `tools/cc/gcc`. The matching compiler
source is `~/development/repos/mips-gcc-2.7.2` and the assembler source is
`~/development/repos/mips-binutils-2.6`. Root-cause any residual to a diverging pass with a
`file:line` citation into those trees.

**Step 0, before trusting any prior document.** Run
`venv/bin/python3 tools/pick_target.py --refresh-residual {{FN}}`. A carry doc's stated instruction
count and its residual class are both hypotheses, and each has been wrong: re-derive the count and the
diff shape from a fresh object first. The doc's reading of the function's semantics can also be wrong
-- a register written in a branch delay slot before its first read is a shared pre-branch statement,
not an argument.

Documented class, to be confirmed or refuted rather than assumed: `{{CLASS}}`.

**Isolation.** Build with `make nonmatching-func FUNC={{FN}} {{TREE_FLAG}}`. Your per-function oracle
is `tools/cmpfn.sh {{FN}} nonmatchings/{{FN}}/current.o` plus
`mips-linux-gnu-objdump -dz` on the freshly built object. Do not use `diff.py`: it reads
`build/*.map`, which an incremental per-object build does not refresh, so it reports false clean and
false mismatch in both directions.

**Levers to try.** `{{LEVERS}}`

**Boundaries.**
- Never run a full `make`. The orchestrator owns integration and the ROM gate; a full `make` here
  races the shared `build/`.
- Never edit anything under `src/`.
- Never touch another function's `nonmatchings/` directory.

**Checkpoint after every material iteration.** Append your best byte-count form and a one-line status
to `nonmatchings/{{FN}}/STATUS`, for example
`38/38 count, residual=uniform reg rotation, form=u32 b=src[i]`. Keep `base.c` at its best-so-far
form, never at a disproven probe. A subagent can die mid-experiment, and the orchestrator recovers
from `STATUS` plus the last-good `base.c` rather than restarting at the seed.

**Deliverable, either of these two.** A byte-exact C body plus the struct and extern additions it
needs. Or a terminal verdict with a `file:line` citation into the compiler or assembler source,
written to `docs/wip/{{FN}}.near-match.md`. Both are real results: a cited no-lever verdict retires
the wall so no later sprint re-grinds it.

**Reporting.** On a mesh harness, send your final verdict to `main` before going idle; an idle
notification is not a deliverable, and if you launch a background permuter you must wake when it exits
and report its final outcome. On a Task-tool harness your return value is the verdict and this clause
does not apply.

**Standing policy.** An empty `__asm__ __volatile__("")` scheduling barrier may be used to
characterize a wall. It is never a bank candidate.
