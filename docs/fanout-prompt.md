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
measure one. Work in isolation.

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

**`nonmatchings/{{FN}}/STATUS` plus a best-so-far `base.c` is the whole contract**, not your return:
you can die mid-experiment and the orchestrator recovers from that pair rather than the seed. Append a
one-line status per material iteration (`38/38 count, residual=uniform reg rotation, form=u32
b=src[i]`), keep `base.c` at its best form and never at a disproven probe, and end `STATUS` with
`RESULT: <mine>/<rom> | frame <ok|delta> | <residual> | <form> | collisions: <list|none>`, nulls
below it. Wake a background permuter rather than leaving it running.

**Deliverable, either of these two.** A byte-exact C body plus the struct and extern additions it
needs. Or a terminal verdict with a `file:line` citation into the compiler or assembler source,
written to `docs/wip/{{FN}}.near-match.md`. Both are real results: a cited no-lever verdict retires
the wall so no later sprint re-grinds it.

**Report your extern collisions.** Your isolated build cannot see the host file, so a declaration it
already carries for one of your `extern`s at a different type is invisible to you and surfaces at
integration. `grep -n '<sym>' <host .c>` for every `extern` you need and list the ones that exist,
with their spelling (S295 `D_800E2158`: scalar `void*` in the host, indexed in the new body, correct
on both sides).

**Standing policy.** An empty `__asm__ __volatile__("")` scheduling barrier may be used to
characterize a wall. It is never a bank candidate. A byte-exact body of raw command words is a result
to challenge, not integrate: decode each word against the SDK header and re-verify. "No macro
produces this" reads as custom packing, the expensive misread (S297: 27 of 27 were reachable).
