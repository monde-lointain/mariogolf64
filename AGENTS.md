# Mario Golf 64 Decomp workflow

Codex project instructions. Kept short so it stays inside Codex project-doc loading limits. The
shared workflow source is `docs/workflow/`.

<!-- mg64:invariants:begin -->
## Invariants

- The ROM SHA-1 against `baserom.z64` is the sole match oracle, and `tools/verify-rom.sh` is the gate
  that reads it. A hand-rolled `make; sha1sum` reads a stale ROM when the build fails.
- Never commit a non-matching function.
- Keep Ghidra MCP, build, yaml, and source edits serial. Read-only context gathering may run in
  parallel.
- Process/tooling edits are retro-gated: record suggestions in `SPRINT.md`; apply them at review
  unless the PO directly requests out-of-band rework.
- Forbidden direct edits: `ghidra_symbols.txt` except via `make sync-names`, `mariogolf64.ld`,
  `undefined_syms_auto.txt`, `undefined_funcs_auto.txt`, `mariogolf64.d`, `ctx.c`, `ctx.c.m2c`,
  `undefined_funcs.txt`, `undefined_syms.txt`, `checksum.sha1`, `symbol_addrs_manual.txt`,
  `re_tracking/*.yml`, `reloc_addrs.txt`.
- Agent-editable low-level project files: `mariogolf64.yaml` subseg flip/split/path-qualifier lines
  only, and `symbol_addrs.txt` add-only.
<!-- mg64:invariants:end -->

## Where to read

Read the row for the phase you are in. Do not read `docs/workflow/` wholesale.

| Phase you are in | Read exactly this |
| --- | --- |
| plan gate, review gate | `docs/workflow/gates.md` |
| execution loop | `docs/workflow/loop.md` |
| a `pick_target.py` hazard flag, or a residual to route | `docs/hazard-index.md`, then the one `docs/hazards.md` section it names |
| a crack slice or fan-out decision | `docs/workflow/fan-out.md` |
| dispatching a crack subagent | `docs/fanout-prompt.md`, filled in |
| choosing a source lever | `docs/levers.md` |
| writing or naming C | `docs/coding-style.md` |
| editing any prompt surface | `docs/prompt-style.md` |

## Skills

`$mg64-sprint-plan [scope]`, `$mg64-decomp-loop`, `$mg64-sprint-review`. Skills do not chain
automatically; invoke the next one explicitly.

## Prompt style

Prompt-surface edits follow the project checklist in `docs/prompt-style.md`: outcome-first, concise,
explicit validation, and clear stop conditions.
