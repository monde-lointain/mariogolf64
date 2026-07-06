# Mario Golf 64 Decomp workflow

Claude Code compatibility wrapper. The shared workflow source is `docs/agent-workflow.md`.

<critical_invariants>

## Critical invariants

- The ROM SHA-1 against `baserom.z64` is the sole match oracle.
- Never commit a non-matching function.
- Keep Ghidra MCP, build, yaml, and source edits serial. Read-only context gathering may run in parallel.
- Process/tooling edits are retro-gated: record suggestions in `SPRINT.md`; apply them at review unless the PO directly requests out-of-band rework.
- Forbidden direct edits: `ghidra_symbols.txt` except via `make sync-names`, `mariogolf64.ld`, `undefined_syms_auto.txt`, `undefined_funcs_auto.txt`, `mariogolf64.d`, `ctx.c`, `ctx.c.m2c`, `undefined_funcs.txt`, `undefined_syms.txt`, `checksum.sha1`, `symbol_addrs_manual.txt`, `re_tracking/*.yml`, `reloc_addrs.txt`.
- Agent-editable low-level project files: `mariogolf64.yaml` subseg flip/split/path-qualifier lines only, and `symbol_addrs.txt` add-only.
- Before gate or execution work, read `docs/agent-workflow.md` and follow the matching section.

</critical_invariants>

<workflow_overview>

## Workflow at a glance

Read `docs/agent-workflow.md ## Workflow at a glance`.

</workflow_overview>

<slash_commands>

## Slash commands

Use `/sprint-plan [scope]` and `/sprint-review`. Read `docs/agent-workflow.md ## Slash commands` and the matching command wrapper in `.claude/commands/` before acting.

</slash_commands>

<execution_loop>

## Execution loop

Read `docs/agent-workflow.md ## Execution loop` before executing sprint backlog work.

</execution_loop>

<scrum_model>

## Scrum operating model

Read `docs/agent-workflow.md ## Scrum operating model`.

</scrum_model>

<story_points>

## Story points

Read `docs/agent-workflow.md ## Story points`.

</story_points>

<conventions>

## Conventions

Read `docs/agent-workflow.md ## Conventions`.

</conventions>

<cross_repo_sync>

## Cross-repo sync

Read `docs/agent-workflow.md ## Cross-repo sync`.

</cross_repo_sync>
