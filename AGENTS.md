# AGENTS.md

Matching decompilation of Mario Golf 64 (N64, US revision 0): C that, built with KMC GCC 2.7.2, reproduces the original ROM byte for byte.

## Knowledge base

Project knowledge lives in `docs/`, an [OKF](https://github.com/GoogleCloudPlatform/open-knowledge-format) bundle read top-down: open `docs/index.md`, then the `index.md` of the section you need, then only the concepts that apply. Links starting with `/` are relative to `docs/`. Do not read the whole bundle.

| Need | Open |
|---|---|
| What the repo is, layout, setup, build | `docs/project/` |
| How to do a task (each task-shaped one is also a skill) | `docs/procedures/` |
| What a script does and how to call it | `docs/tools/` |
| KMC GCC behavior, `symbol_addrs.txt`, `mariogolf64.yaml`, F3DEX2 | `docs/reference/` |
| Build, scoring, policy hook, CI, how to change them | `docs/harness/` |

Matching a function: use the `decompile-function` skill and stop at its stop rules.

## Non-negotiables

Full statement: `docs/project/ground-rules.md`.

- `./tools/build-and-verify.sh` (ROM SHA1) decides a match; in a decomp environment `./build.sh <file>.c` decides a function. Never declare a match.
- Never edit `mariogolf64.sha1`, `baserom.z64`, `asm/`, `tools/gcc_kmc`, `tools/kmc-cc`, `tools/decomp-permuter` or `tools/asm-differ`.
- Run scripts, not `make` (the policy hook blocks it; `make -s print-cflags SRC=<file>` is allowed).
- If a tool, script, skill, prompt, doc or the hook is wrong or in your way, fix it generally, commit it separately with the `harness:` prefix, then return to your task. Rules for what a fix needs: `docs/harness/changing-the-harness.md`.

## Code standards

Full list and validation checklist: `docs/project/code-quality.md`. Never use inline assembly (including empty barriers and register bindings), raw display-list words, manual pointer arithmetic, or `void *` for types you can determine. Keep every `INCLUDE_ASM(...)` on one line. Verify with `./tools/build-and-verify.sh` before declaring C changes complete.

## Docs maintenance

Add or move a concept: update its directory's `index.md` (one-line description per entry) and, for notable changes, `docs/log.md`. Skills in `.omp/skills/` are symlinks into `docs/procedures/`; edit the concept. Generic compiler lessons go in `docs/reference/kmc-gcc/` only under `editing-rules.md`; per-function sessions keep findings in their workspace `LEARNINGS.md`.
