---
name: mg64-sprint-review
description: Review gate for the Mario Golf 64 decomp workflow. Use when the user invokes $mg64-sprint-review, asks to close or review an MG64 sprint, verify ROM SHA-1 DoD, apply retro-gated suggestions, update RETRO/BACKLOG/VELOCITY, or decide push/PR after sprint completion.
---

# MG64 Sprint Review

Read `docs/agent-workflow.md` before acting. Follow `## Slash commands`, `## Scrum operating model`, `## Story points`, and the review-gate procedure.

## Workflow

1. Reject arguments; this gate takes none.
2. Read `SPRINT.md`; abort if no sprint is open.
3. Verify DoD with `make` and `sha1sum build/mariogolf64.z64`; require `build/mariogolf64.z64: OK` and SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
4. For each file claimed md5-candidate, run `grep -c 'INCLUDE_ASM' src/<seg>.c` and require `0`.
5. Report progress delta, descriptive count, quality counter-metric, story points, and scope vs goal.
6. Ask the Product Owner for scope sign-off, accepted buffered suggestions, and push/PR decision. Use `request_user_input` when available; otherwise ask directly.
7. Apply only accepted process/tooling edits, then update `VELOCITY.md`, prepend `RETRO.md`, and update `BACKLOG.md` carry-overs/active phase.
8. Perform outward push/PR only if approved.

Claim nothing that the verification output does not show.
