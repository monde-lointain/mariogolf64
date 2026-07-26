---
description: Review gate. Verify the ROM-SHA-1 DoD on the increment, PO scope sign-off, apply buffered suggestions, write the RETRO.md digest.
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-review, the review gate

Read `docs/agent-workflow.md` before acting. Follow `## Slash commands`, `## Scrum operating model`,
`## Story points`, and the `sprint-review` procedure.

## Critical command rules

- Reject arguments; `/sprint-review` takes none.
- Abort if `SPRINT.md` is absent.
- Verify with `tools/verify-rom.sh`; require exit 0. A hand-rolled `make; sha1sum` is what this gate must not do: a failed `make` leaves the previous ROM in place, so the hash reads green off a stale build.
- For every file claimed md5-candidate, run `grep -c 'INCLUDE_ASM' src/<seg>.c` and require `0`.
- Report progress delta, descriptive count, quality counter-metric, story points, and scope vs goal from real evidence.
- Ask the Product Owner for scope sign-off, accepted buffered suggestions, and push/PR decision.
- Apply only accepted process/tooling edits, then update `VELOCITY.md`, prepend `RETRO.md`, and update `BACKLOG.md`.
- Push or open a PR only when the Product Owner approves outward ops.
