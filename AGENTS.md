# Mario Golf 64 Decomp workflow

Codex project instructions. Keep this file concise so it stays inside Codex project-doc loading limits. The detailed workflow source is `docs/agent-workflow.md`.

## Critical invariants

- The ROM SHA-1 against `baserom.z64` is the sole match oracle.
- Never commit a non-matching function.
- Keep Ghidra MCP, build, yaml, and source edits serial. Read-only context gathering may run in parallel.
- Process/tooling edits are retro-gated: record suggestions in `SPRINT.md`; apply them at review unless the PO directly requests out-of-band rework.
- Forbidden direct edits: `ghidra_symbols.txt` except via `make sync-names`, `mariogolf64.ld`, `undefined_syms_auto.txt`, `undefined_funcs_auto.txt`, `mariogolf64.d`, `ctx.c`, `ctx.c.m2c`, `undefined_funcs.txt`, `undefined_syms.txt`, `checksum.sha1`, `symbol_addrs_manual.txt`, `re_tracking/*.yml`, `reloc_addrs.txt`.
- Agent-editable low-level project files: `mariogolf64.yaml` subseg flip/split/path-qualifier lines only, and `symbol_addrs.txt` add-only.

## Workflow entrypoints

- Use `$mg64-sprint-plan [scope]` for the plan gate.
- Use `$mg64-decomp-loop` for the inline execution loop.
- Use `$mg64-sprint-review` for the review gate.
- Before gate or execution work, read `docs/agent-workflow.md` and follow the matching section.

## Prompt style

Prompt-surface edits follow `PROMPT_GUIDELINES.md` and the project checklist in `docs/prompt-style.md`. Prefer GPT-5.5 style: outcome-first, concise, explicit validation, and clear stop conditions.
