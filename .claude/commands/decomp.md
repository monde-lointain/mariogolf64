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
tools/mcp_lock.py acquire --command decomp --identifier $ARGUMENTS
```

Non-zero exit means another `/decomp` or `/decomp-lead` already holds it — abort with the message printed by `mcp_lock.py` on stderr.

`tools/mcp_lock.py` writes `nonmatchings/.mcp.lock/holder.json` and survives across Bash tool calls (unlike `flock`, which only lives for the lifetime of a single subshell). The lock self-reclaims after a 30-minute TTL so a crashed slash command doesn't strand the slot. **Step 9 MUST end with `tools/mcp_lock.py release --identifier $ARGUMENTS`** — explicit release is the contract; the TTL is only a safety net.

If the slash command takes an early abort path (subseg is `hasm`, subseg is still `asm`, MCP is down, etc.) the release call MUST still run before returning the abort message.

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
- The placeholder is **whatever splat emitted in asm** — splat uses the curated name verbatim (e.g. `glabel rand`) for any symbol present in `ghidra_symbols.txt` or `symbol_addrs.txt`, and falls back to `func_<VRAM>` only for unnamed functions. So:
  - If `$ARGUMENTS` matches `^func_[0-9A-Fa-f]{8}$`, use it directly.
  - Else look up `$ARGUMENTS` in this order:
    1. `symbol_addrs.txt` (maintainer overrides)
    2. `ghidra_symbols.txt` (Ghidra sync state)
    3. Ghidra MCP via the discovered symbol-lookup tool, with `program="baserom.z64"`
  - If found in `symbol_addrs.txt` or `ghidra_symbols.txt`, the placeholder is the curated name itself (Step 4 will then find `glabel rand`, not `glabel func_<VRAM>`). If resolved only via Ghidra MCP and not yet synced, derive `func_{VRAM_UPPER_HEX}` from the resolved vram — that's what splat used in asm.
- If still unresolved, abort with a single line stating which sources were checked.
- `seed_c.py` and `decomp_loop.py` accept either placeholder form (curated name or `func_<VRAM>`); both grep `glabel <name>` against the asm.
</resolution-rules>

## Step 4 — Resolve the subsegment

Grep `asm/*.s` for `glabel <placeholder>` to find the asm file. The file's stem is the segment rom-offset. Look that subsegment up in `mariogolf64.yaml`. Three cases:

| Subsegment type | Action |
| --- | --- |
| `hasm` | Abort: "func lives in a handwritten-asm (hasm) subsegment; INCLUDE_ASM is permanent." |
| `asm` | Print the splat-flip instructions (set the subsegment to `c` in `mariogolf64.yaml`, run `make extract`) and STOP. This command does not modify yaml. Before STOPping, **also probe the libultra/libkmc upstreams** (`find ~/development/repos/libultra_modern/src -name "*.c" \| xargs grep -l "<curated>"; find ~/development/repos/libkmc/src -name "*.c" \| xargs grep -l "<curated>"` — only when a curated name was resolved at Step 3). If a hit lands, **also `grep -nE '^\s*static\b[^=]*;\s*$' <upstream-path>`** to check for file-scope statics: if no statics, recommend `/decomp-libupstream <curated>` instead and include the exact path qualifier (e.g. `c, libultra/shared/system/afterprenmi`); if statics are present, mention the upstream exists but `/decomp-libupstream` is blocked by BSS-layout-conflict (see CLAUDE.md), and fall through to the plain `c` flip so `/decomp` can take over with the `static` dropped. No probe runs for raw `func_XXXXXXXX` placeholders. |
| `c` | Proceed. |

## Step 5 — Locate the parent C file

`grep -l "INCLUDE_ASM(\"asm/<seg>\", <placeholder>)" src/*.c` → the parent's path. If no parent exists, the subsegment was flipped to `c` but `make extract` wasn't run; abort with that exact diagnostic.

## Step 6 — Seed `nonmatchings/<placeholder>/base.c`

Deterministic two-step (do not parallelize — the seed depends on ghidra.c being on disk):

1. **Fetch Ghidra decompile and write `nonmatchings/<placeholder>/ghidra.c` first.**

   - Resolve `<placeholder>`'s VRAM (the hex tail of the placeholder).
   - Call `mcp__ghidra__decompile_function` with `address=0x<VRAM>` and `program="baserom.z64"`.
   - Strip the leading `{"result":"..."}` JSON envelope and unescape, then write the raw decompile body verbatim to `nonmatchings/<placeholder>/ghidra.c`. `mkdir -p` the dir first.
   - Do NOT hand-clean the body here — `seed_c.py` runs `sanitize_ghidra_body` (Ghidra-type → ultra64.h-type substitution, function-symbol rename to `<placeholder>`, audit-header strip).

2. **Run the seed:**

   ```bash
   venv/bin/python3 tools/seed_c.py --func <placeholder> --parent <parent.c>
   ```

`seed_c.py` does the m2ctx + m2c + rodata slicing + parent-extern collection + auto-extern emission for `%hi/%lo` labels + Ghidra-body sanitization + base.c stitching. Always invoke via `venv/bin/python3` (project policy — keeps asm-differ deps in scope; the script also self-promotes if invoked from system python).

**Type discipline**: never let raw `int` / `long` / `volatile unsigned long long` etc. land in base.c. Use the ultra64.h typedefs (`s8`/`s16`/`s32`/`s64`/`u8`/`u16`/`u32`/`u64`, `f32`/`f64`, `vs8`–`vu64`). `sanitize_ghidra_body` covers the common Ghidra→ultra64 mappings; anything else the agent leaves on the table is its responsibility to convert before declaring a match.

**Trivial-getter shortcut**: For 1–3 instruction functions that are clearly getters/setters (single global load+return, or single global store), the agent should skip m2c iteration and hand-write the body from the asm directly after `seed_c.py` lays down the scaffold. The seed's `extern` and `#include` framing still applies; just overwrite the function body in one shot and jump straight to step 7's `decomp_loop.py` invocation. Saves several iteration cycles.

## Step 7 — Iterate

```
for iteration in 1..25:
    result = venv/bin/python3 tools/decomp_loop.py --func <placeholder>
    if result.score == 0:        break (candidate match)
    if 5 consecutive compile_ok == False:
        report "seed appears fundamentally broken" and stop
    inspect result.top_mismatches, edit nonmatchings/<placeholder>/base.c, continue
```

The agent reads the top mismatch context (instruction pairs ± 3 lines), updates `base.c`, and re-invokes `decomp_loop.py`. **Triage rule for extern address loads**: if remaining mismatches cluster around HI/LO16 relocs against project-local symbols, those are isolation-only — they'll vanish in the spot-check at step 8. Don't burn iterations chasing them.

## Step 8 — In-tree spot-check (only if score 0)

This step proves the match survives the parent-file context. Compare **bytes**, not disassembly mnemonics — `00001021` and `00001025` both render as `move v0,zero` in objdump but encode different instructions.

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

# Byte-level compare:
mips-linux-gnu-objcopy -O binary -j .text build/asm/<seg>.o /tmp/<seg>.ref.bin
mips-linux-gnu-objcopy -O binary -j .text build/src/<seg>.spotcheck.o /tmp/<seg>.now.bin
cmp /tmp/<seg>.ref.bin /tmp/<seg>.now.bin
```

If `cmp` is silent → genuine match → proceed to step 9. If `cmp` reports a difference → the in-tree compile diverges (cross-function regalloc/scheduling, or a KMC-`as`-vs-modern-`as` codegen divergence not yet handled — diagnose against the original KMC binutils 2.6 source under `~/development/repos/mips-binutils-2.6/`).

**Always** clean up: `rm -f src/<seg>.c.spotcheck build/src/<seg>.spotcheck.o /tmp/<seg>.ref.bin /tmp/<seg>.now.bin` before returning, even on error. The src/build paths are gitignored as belt-and-suspenders.

## Step 9 — Finalize the match (only if Step 8 passed)

Spot-check passing is **not** the same as "the function is in the ROM-matching build." Three steps must complete before Step 10 declares Match.

**Guard (run before step 1)**: compute the subseg size from `mariogolf64.yaml` as `next_subseg_offset - this_subseg_offset`. If that size is not a multiple of 16, ABORT finalization and emit:

> "subseg `[0x<this>, c]` size 0x\<size> is not 16-aligned. KMC `as` auto-pads `.text` to 16-byte alignment, which will over-pad this subseg by 0x\<delta> and shift every subsequent object. To proceed: either split this subseg in `mariogolf64.yaml` so the C-only portion ends at a 16-byte boundary, or add an explicit `__asm__(".balign 8\n");` after the function. Both are user actions."

Outcome becomes **Subseg-alignment unsupported**. All four 8-aligned-only subsegs in the current yaml are asm-to-asm, so this guard does not fire on any present `c` subseg — it's future-proofing for if a user tries to match a function in one of those exceptional subsegs.

1. **Inline the body into `src/<seg>.c`.** Replace the `INCLUDE_ASM(...)` line with the verbatim C body from `nonmatchings/<placeholder>/base.c`. Strip the seed-comment scaffold (`// === Function body ===`, `// === Externs ===`, the `#if 0` m2c reference block, the DLIST_HINT) — those live in `base.c` only.

2. **Run `clang-format -i src/<seg>.c` UNLESS the target lives under `src/libultra/` or `src/libkmc/`.** Both library trees carry a local `.clang-format` with `DisableFormat: true` because their files are verbatim upstream copies; reformatting them defeats cross-referencing. If `<seg>.c`'s path starts with `src/libultra/` or `src/libkmc/`, skip this step entirely. The project's `.clang-format` for all other paths is `BasedOnStyle: Google`. No other exceptions.

3. **Build with cache forced clean for the target object: `rm -f build/$(realpath_from_repo src/<seg>.c).o; make`.** The bare `make` is not enough — stale `.o` files from prior layouts (e.g., a renamed subseg path) can hide a broken build. The targeted `rm` forces re-compile of just the file we touched. Output must end with `build/mariogolf64.z64: OK`. Confirm with `sha1sum build/mariogolf64.z64` — expected `e2c4e7a905b29529b49a1619a401fe699224829b` (baserom SHA-1). If the ROM doesn't match, the function did **not** match. Loop back to Step 7 or diagnose the gap. KMC `as` auto-pads `.text` to 16 — so trailing padding is handled automatically; subseg-alignment cases are caught by the guard above; what remains is genuine codegen mismatch.

4. **`git status --short`.** Any files listed beyond `src/<seg>.c` and `nonmatchings/` paths is unexpected drift — surface them in the final report under "Working-tree drift" before declaring Match. This catches the failure mode where prior turns left files in a half-finished state that this run accidentally completes or breaks.

## Step 10 — Final report

Before emitting the report, release the MCP lock:

```bash
tools/mcp_lock.py release --identifier $ARGUMENTS
```

The release call MUST run on every exit path (Match / Stuck-near / Stuck-far / Spot-check failed / Finalize failed / Subseg-alignment unsupported / any early abort from Steps 1–8). Step 9 completion is required for a Match outcome — if any of Step 9's substeps or its guard failed, the outcome is one of the non-Match states even if the spot-check passed.


Emit one final response with these sections in this exact order, authored per `PROMPT_GUIDELINES.md` (concise, no preamble, no validation phrasing):

<final-report-template>
### Outcome

One of:
- **Match** — isolated + spot-check + Step 9 finalization (inline + clang-format + full `make`) all clean, and `sha1sum build/mariogolf64.z64` equals `e2c4e7a905b29529b49a1619a401fe699224829b`.
- **Stuck-near** — score > 0, last `percent` ≥ 0.97.
- **Stuck-far** — score > 0, last `percent` < 0.97.
- **Spot-check failed** — isolated matched, in-tree didn't (cross-function regalloc/scheduling divergence).
- **Finalize failed** — spot-check matched but Step 9's full `make` didn't produce a checksum-OK ROM (genuine codegen drift not caught by isolated/spot-check).
- **Subseg-alignment unsupported** — Step 9 guard fired: this subseg is not 16-byte aligned and KMC `as`'s auto-pad would over-pad. User must split the yaml or insert explicit `.balign`.

### Applied changes (only if Match)

Report what Step 9 wrote:
1. `src/<seg>.c` — inlined `<placeholder>`'s body, dropped its `INCLUDE_ASM` line, ran clang-format.
2. `build/mariogolf64.z64` SHA-1 matches baserom.

### Patch to propose (only if Match)

If Ghidra had a curated name, propose adding to `symbol_addrs.txt`:
```
<curated_name> = 0x<vram>; // type:func
```
The agent does NOT apply this — user reviews and applies (per the cross-repo sync flow in `CLAUDE.md`).

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
