---
description: Match-decompilation loop for one function. Arg = func_XXXXXXXX placeholder or a Ghidra-curated name.
args:
  - name: func_name
    description: Either `func_XXXXXXXX` placeholder or a Ghidra-curated symbol name. Matches `^[A-Za-z_][A-Za-z0-9_]+$`.
    required: true
---

# /decomp — One-function match loop

FUNC = `$ARGUMENTS`

Tactical command. Drives a tight `seed C → KMC GCC → objdump → asm-differ` loop on a single function until byte-exact. Produces a parent-file patch on success; stops and reports on stuck states. The strategic counterpart is `/decomp-lead` (it suggests targets but never dispatches).

<command-prereqs>
- Ghidra MCP must be running: `~/development/reversing/ghidra/start-headless.sh` (verify with `ss -tln | grep 8089`).
- Only one `/decomp` (or `/decomp-lead`) may hold the Ghidra slot at a time. The flock at `nonmatchings/.mcp.lock` is the gate.
- `tools/asm-differ` submodule must be initialized.
- The target function's subsegment must already be flipped from `asm` → `c` in `mariogolf64.yaml` (a one-time user action; `/decomp` prints the flip instructions if it is not).
</command-prereqs>

## Step 0 — Validate argument

Anchor-check `$ARGUMENTS` against `^[A-Za-z_][A-Za-z0-9_]+$`. On mismatch, abort with this exact one-liner and do nothing else:

```
/decomp: arg `$ARGUMENTS` does not match `^[A-Za-z_][A-Za-z0-9_]+$`. Aborting.
```

Do not "best-effort" sanitize. Reject and stop.

## Step 1 — Acquire the MCP lock

```bash
mkdir -p nonmatchings
exec 9>nonmatchings/.mcp.lock
flock -n 9 || { echo "another /decomp or /decomp-lead is already running. aborting."; exit 1; }
```

The lock is held for the lifetime of the slash command's shell — it releases automatically on exit. The lock file itself is not a sentinel; only the kernel-side flock state matters.

## Step 2 — MCP discovery

Call `mcp__ghidra__list_instances`. If the response is empty or errors out, abort with:

```
/decomp: Ghidra MCP is not running. Start it with `~/development/reversing/ghidra/start-headless.sh`. Aborting.
```

Then call `mcp__ghidra__list_tool_groups` and `mcp__ghidra__connect_instance` (or whatever the discovery dance for this bridge requires — the bridge auto-connects on first dynamic call in some versions; do the explicit calls defensively).

<context-binding>
All subsequent MCP calls in this command **must** pass `program="baserom.z64"`. `mariogolf64.elf` is reserved for `sync_decomp_names.py`'s ELF-side renames; everything else targets the rom.
</context-binding>

## Step 3 — Resolve the function name

<resolution-rules>
- If `$ARGUMENTS` matches `^func_[0-9A-Fa-f]{8}$`, use it directly as the placeholder.
- Else, look it up in this order:
  1. `symbol_addrs.txt` (maintainer overrides)
  2. `ghidra_symbols.txt` (Ghidra sync state)
  3. Ghidra MCP via the discovered symbol-lookup tool, with `program="baserom.z64"`
- Derive the placeholder as `func_{VRAM_UPPER_HEX}` from the resolved vram.
- If still unresolved, abort with a single line stating which sources were checked.
</resolution-rules>

## Step 4 — Resolve the subsegment

Grep `asm/*.s` for `glabel <placeholder>` to find the asm file. The file's stem is the segment rom-offset. Look that subsegment up in `mariogolf64.yaml`. Three cases:

| Subsegment type | Action |
| --- | --- |
| `hasm` | Abort: "func lives in a handwritten-asm (hasm) subsegment; INCLUDE_ASM is permanent." |
| `asm` | Print the splat-flip instructions (set the subsegment to `c` in `mariogolf64.yaml`, run `make extract`) and STOP. This command does not modify yaml. |
| `c` | Proceed. |

## Step 5 — Locate the parent C file

`grep -l "INCLUDE_ASM(\"asm/<seg>\", <placeholder>)" src/*.c` → the parent's path. If no parent exists, the subsegment was flipped to `c` but `make extract` wasn't run; abort with that exact diagnostic.

## Step 6 — Seed `nonmatchings/<placeholder>/base.c`

In parallel:
- Call the Ghidra MCP decompile tool (discovered name) for `<placeholder>` with `program="baserom.z64"`. Save the body to `nonmatchings/<placeholder>/ghidra.c`.
- Run `python3 tools/seed_c.py --func <placeholder> --parent <parent.c>`.

`seed_c.py` does the m2ctx + m2c + rodata slicing + parent-extern collection + base.c stitching. The Ghidra body lands at `nonmatchings/<placeholder>/ghidra.c` before stitching; `seed_c.py` picks it up automatically.

## Step 7 — Iterate

```
for iteration in 1..25:
    result = python3 tools/decomp_loop.py --func <placeholder>
    if result.score == 0:        break (candidate match)
    if 5 consecutive compile_ok == False:
        report "seed appears fundamentally broken" and stop
    inspect result.top_mismatches, edit nonmatchings/<placeholder>/base.c, continue
```

The agent reads the top mismatch context (instruction pairs ± 3 lines), updates `base.c`, and re-invokes `decomp_loop.py`. **Triage rule for extern address loads**: if remaining mismatches cluster around HI/LO16 relocs against project-local symbols, those are isolation-only — they'll vanish in the spot-check at step 8. Don't burn iterations chasing them.

## Step 8 — In-tree spot-check (only if score 0)

This step proves the match survives the parent-file context:

```bash
cp src/<seg>.c src/<seg>.c.spotcheck
# In the copy, replace
#   INCLUDE_ASM("asm/<seg>", <placeholder>);
# with
#   #ifndef NONMATCHING
#       INCLUDE_ASM("asm/<seg>", <placeholder>);
#   #else
#       <C body from nonmatchings/<placeholder>/base.c>
#   #endif
make spotcheck-build FUNC=<placeholder> SEG=<seg>
# diff build/src/<seg>.spotcheck.o symbol vs build/asm/<seg>.o symbol via decomp_loop helpers
```

If the spot-check diff is empty → genuine match → proceed to step 9. If non-empty → the in-tree compile diverges; record the new diff as the authoritative one and resume the iteration loop at step 7.

**Always** clean up: `rm -f src/<seg>.c.spotcheck build/src/<seg>.spotcheck.o` before returning, even on error. Both paths are gitignored as belt-and-suspenders.

## Step 9 — Final report

Emit one final response with these sections in this exact order, authored per `PROMPT_GUIDELINES.md` (concise, no preamble, no validation phrasing):

<final-report-template>
### Outcome

One of:
- **Match** — both isolated and in-tree clean.
- **Stuck-near** — score > 0, last `percent` ≥ 0.97.
- **Stuck-far** — score > 0, last `percent` < 0.97.
- **Spot-check failed** — isolated matched, in-tree didn't (cross-function regalloc/scheduling divergence).

### Patches (only if Match)

1. Replace in `src/<seg>.c`:
   ```c
   INCLUDE_ASM("asm/<seg>", <placeholder>);
   ```
   With the final C body verbatim.

2. Add to `symbol_addrs.txt` (if a curated name was used):
   ```
   <curated_name> = 0x<vram>; // type:func
   ```

The agent does NOT apply these. User reviews and applies.

### Next-step suggestion (only if Stuck-near at ≥ 0.97)

```
./setup-permuter.sh <placeholder>
./run-permuter.sh <placeholder>
```

The user decides whether to run permuter. No auto-invocation.

### Blocker analysis (only if Stuck-far at < 0.97)

A 1–3 sentence categorization across {regalloc, struct field offsets, compiler quirk, missing extern, ordering}, plus the last three iterations' delta (one line each: "iter N: score N → N, change: <one-line>").

### Suggested workflow improvements

A numbered list of concrete, actionable changes that would have made this run faster, e.g.

```
1. Extend seed_c.py to strip m2c's `temp_*` casts before stitching.
2. Add the F3DEX2 vertex-loader pattern to CLAUDE.md.
3. /decomp should pre-fetch callers via MCP before iteration 0.
```

User replies with the numbers to apply (e.g. "apply 1, 3"). Agent then edits the named files in this session. No retro file is written; suggestions live in the conversation.
</final-report-template>
