# Mario Golf 64 Decomp agent workflow

<role>
The routing stub for this project's workflow docs. It stays at this path because the append-only logs
cite it ~35 times and those citations are history, not to be rewritten. Read the row for the phase you
are in; do not read `docs/workflow/` wholesale.
</role>

Decompile Mario Golf 64 (N64) one function at a time toward a byte-exact ROM. The ROM SHA-1 against
`baserom.z64` is the sole arbiter of a match, and `tools/verify-rom.sh` is the gate that reads it.

## Workflow at a glance

Two gate workflows bound each sprint. Between them the agent runs the execution loop inline: one
function at a time, no per-function stop, a single serial session. `SPRINT.md` is the resume surface
when the middle spans context windows.

- **Effort.** Run the gates as cheap triage; run classical decomp and the permuter at higher effort,
  where the extra reasoning pays off.
- **Parallelism.** Read-only context gathering may run in parallel: read `BACKLOG.md`, run
  `pick_target.py`, and read the flagged hazard sections at once. Keep MCP, build, and edit steps
  serial, since the session holds no MCP lock and the build is one shared tree.
- **Delegation.** A subagent fan-out is a specific triggered move, not a default. The trigger table is
  in `docs/workflow/fan-out.md`.

## Where to read

| Phase you are in | Read exactly this |
| --- | --- |
| plan gate, review gate | `docs/workflow/gates.md` |
| execution loop | `docs/workflow/loop.md` |
| a `pick_target.py` hazard flag, or a residual to route | `docs/hazard-index.md`, then the one `docs/hazards.md` section it names |
| a crack slice or fan-out decision | `docs/workflow/fan-out.md` |
| writing or naming C | `docs/coding-style.md` |
| editing any prompt surface | `docs/prompt-style.md` |

## Slash commands

- **Claude Code:** `/sprint-plan [scope]`, `/decomp-loop`, `/sprint-review`.
- **Codex:** `$mg64-sprint-plan [scope]`, `$mg64-decomp-loop`, `$mg64-sprint-review`.

Both sets are described in `docs/workflow/gates.md ## Gate entry points`.

## Scrum operating model

Moved to `docs/workflow/gates.md ## Scrum operating model`: roles, the cycle, sprint sizing, the
Definition of Ready and Done, spikes and carry-overs, retro-gating, and the resume protocol.

## Story points

Moved to `docs/workflow/gates.md ## Story points`: the deterministic seed, the 8-point decompose gate
and its exemptions, per-file all-or-nothing banking, and the realized tier.

## Execution loop

Moved to `docs/workflow/loop.md ## Execution loop`, with the oracle table and the every-sprint
conventions alongside it.

## Conventions

Moved to `docs/workflow/loop.md ## Conventions`.

## Hazard index

Moved to `docs/hazard-index.md`, a sibling of `docs/hazards.md` because both gates and the loop
consult it and its anchors are the input to `tools/hazard_anchors.py`.

## Cross-repo sync

Moved to `docs/workflow/loop.md ## Cross-repo sync`.
