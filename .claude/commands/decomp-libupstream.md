---
description: Reposition a libultra/libkmc function by copying its upstream source verbatim into src/. Arg = func_XXXXXXXX placeholder or vram hex (0x800B1A80).
args:
  - name: target
    description: Either `func_XXXXXXXX` placeholder or a `0x80XXXXXX` vram hex.
    required: true
---

# /decomp-libupstream — upstream SDK source reposition

TARGET = `$ARGUMENTS`

Tactical command. Skips `/decomp`'s match loop entirely when the target function maps to a known upstream SDK source. Pipeline: identify upstream file → copy verbatim into `src/lib<name>/...` → update yaml subseg path qualifier → propose `symbol_addrs.txt` entries → `make extract && make clean && make` → verify ROM SHA-1.

Use when: the target is in libultra (`__osViGetCurrentContext`, `__osViSwapBuffer`, …) or libkmc (`_matherr`, `__matherr`, `_atoi`, …). For project-specific functions, use `/decomp` instead.

<command-prereqs>
- Ghidra MCP must be running (verify with `ss -tln | grep 8089`); used to confirm the curated name.
- `~/development/repos/libultra_modern/` and `~/development/repos/libkmc/` must be checked out — these are the upstream search trees.
- The user must explicitly allow `mariogolf64.yaml` edits for this run (this command modifies the subseg path qualifier).
- Only one of `/decomp`, `/decomp-lead`, `/sync-names`, `/decomp-libupstream` may run at a time; `tools/mcp_lock.py` is the gate.
</command-prereqs>

## Step 0 — Validate argument

Anchor-check `$ARGUMENTS` against one of:
- `^func_[0-9A-Fa-f]{8}$` (splat placeholder)
- `^0x[0-9A-Fa-f]{8}$` (vram hex)
- `^[A-Za-z_][A-Za-z0-9_]+$` (curated name — look up vram via `symbol_addrs.txt` / `ghidra_symbols.txt`)

On mismatch, abort:

```
/decomp-libupstream: arg `$ARGUMENTS` is not a placeholder, vram hex, or curated name. Aborting.
```

## Step 1 — Acquire the MCP lock

```bash
tools/mcp_lock.py acquire --command decomp-libupstream --identifier $ARGUMENTS
```

Non-zero exit → another holder is alive. Abort. Don't release a lock you don't own.

## Step 2 — Resolve curated name + vram + subseg

Get the canonical curated name via Ghidra MCP (`mcp__ghidra__get_function_by_address` or `decompile_function`). Then grep `asm/*.s` for `glabel <placeholder>` to find the segment file; its stem is the segment rom-offset. Look that subseg up in `mariogolf64.yaml`. Three cases:

- `hasm` — abort: "func lives in `hasm`; INCLUDE_ASM is permanent."
- `c` — proceed.
- `asm` — **before** emitting flip instructions, run Step 3's upstream search **now** so you can hand the user the exact yaml path qualifier in one step. Emit:

  ```
  /decomp-libupstream: subseg [0x<seg>, asm] needs to be `c` first. Suggested flip:
  -      - [0x<seg>, asm]
  +      - [0x<seg>, c, lib<name>/<exact-upstream-rel-path>/<basename>]
  Then run `make extract` and re-run /decomp-libupstream <curated>.
  ```

  Replace `<exact-upstream-rel-path>` with the upstream's path relative to its `src/` root (verbatim — preserve `shared/`, `monegi/`, `nintendo/` variant dirs). If the upstream search returns nothing, fall back to the **No-upstream-source** abort. Either way, STOP — `/decomp-libupstream` does not modify yaml on the flip-needed path.

## Step 3 — Find the upstream file

Map curated name → upstream path. Heuristic search order:

```bash
# libultra: search by function name
find ~/development/repos/libultra_modern/src -name "*.c" -exec grep -l "^<RETURN_TYPE>[* ]*<CURATED>(" {} +
# libkmc: typically <funcname>.c at top level
find ~/development/repos/libkmc/src -name "*.c" -exec grep -l "^[A-Za-z_][A-Za-z0-9_ *]*<CURATED>[\\\(\\s]" {} +
```

If multiple matches, prefer the one whose filename matches the curated name (e.g. `vigetcurrcontext.c` for `__osViGetCurrentContext`, `matherr.c` for `_matherr`). If no match, abort with **No-upstream-source**:

```
/decomp-libupstream: no upstream source found for <curated_name>. This function may be project-specific.
Run /decomp <placeholder> instead. Aborting.
```

## Step 4 — Determine destination + verify required headers

**Destination rule (exact upstream mirror, no remapping)**: take the upstream's path relative to its `src/` root and prepend `src/lib<name>/`. Preserve every directory level — including upstream variant dirs (`shared/`, `monegi/`, `nintendo/` under libultra_modern). Examples:

| Upstream path | Project destination |
| --- | --- |
| `libultra_modern/src/shared/system/afterprenmi.c` | `src/libultra/shared/system/afterprenmi.c` |
| `libultra_modern/src/monegi/vi/vigetcurrcontext.c` | `src/libultra/monegi/vi/vigetcurrcontext.c` |
| `libkmc/src/matherr.c` | `src/libkmc/matherr.c` |

Do NOT collapse variant dirs into a flat `libultra/<subdir>/`. The variant carries SDK-branch information and is part of the layout contract (see CLAUDE.md "Upstream-mirror pattern").

Header dependencies of the upstream file must already exist under `include/libultra/` or `include/libkmc/`, OR be addable from upstream verbatim. Inspect via:

```bash
grep -E '#include "[^"]+"' <upstream-path>
grep -E '#include <[^>]+>' <upstream-path>
```

For each include that doesn't yet resolve via the project's CFLAGS (`-I include -I include/libultra -I include/libultra/internal -I include/libkmc`), copy the upstream header into the right `include/` subtree verbatim. **Never modify upstream headers** — if an include doesn't resolve, prefer adjusting CFLAGS (add `-I include/libultra/<subdir>`) over editing the source.

## Step 5 — Copy + yaml + delete placeholder

1. `cp <upstream-path> src/lib<name>/<subdir>/<basename>.c` (mkdir -p first).
2. Edit `mariogolf64.yaml`: replace `[0x<seg>, c]` with `[0x<seg>, c, lib<name>/<subdir>/<basename>]`. **DO NOT** add the `.c` extension — splat handles that.
3. Delete the old placeholder `src/<seg>.c` if it exists.
4. Skip `clang-format` entirely — both library dirs carry `.clang-format` with `DisableFormat: true`. The body must remain byte-exact upstream.

## Step 6 — Propose symbol_addrs.txt entries (only for symbols not already synced)

Identify every symbol the new C body references via `%hi/%lo` / call / data-access. For each, **first check `ghidra_symbols.txt`**:

```bash
grep -E "^<symbol>\s+=" ghidra_symbols.txt symbol_addrs.txt
```

Symbols already in `ghidra_symbols.txt` (splat-fed via `/sync-names`) or `symbol_addrs.txt` need no proposal — splat will rename the asm-side references automatically. Skip them. If every referenced symbol is already covered, this section is empty in the final report ("No proposals — all referenced symbols already synced.") and Step 7 can run immediately.

For symbols **not** in either file, query Ghidra MCP for the curated name + vram + type and propose `symbol_addrs.txt` entries:

```
<curated_func_name>  = 0x<vram>; // type:func
<curated_global>     = 0x<vram>; // type:u32 size:0x4   (or appropriate splat type)
```

Use the same Ghidra-type → splat-type mapping that `sync_decomp_names.py:map_ghidra_dtype` uses. If many entries would be needed, suggest running `/sync-names` first (separate session, separate lock) so the symbols land in `ghidra_symbols.txt` instead — that's preferred over manual `symbol_addrs.txt` curation.

**The agent proposes; the user applies.** `symbol_addrs.txt` edits require explicit approval per project convention.

## Step 7 — Build verification (only after user applies symbol_addrs.txt)

After the user pastes the proposed entries and confirms, run:

```bash
make extract                                       # regenerate asm/data with new names
rm -f build/src/lib<name>/<subdir>/<basename>.o    # force re-compile of the new file
make clean && make                                 # full rebuild to defeat cached objects
sha1sum build/mariogolf64.z64
```

ROM SHA-1 must equal `e2c4e7a905b29529b49a1619a401fe699224829b`. If not, the upstream source diverges from this ROM's compiled form (rare for SDK code but possible across SDK versions). Outcome becomes **Codegen-divergence**; user must hand-decompile via `/decomp`.

## Step 8 — Release lock + final report

```bash
tools/mcp_lock.py release --identifier $ARGUMENTS
```

Must run on every exit path.

Final report sections in this order:

<final-report-template>
### Outcome

One of:
- **Repositioned** — upstream copy in place, yaml updated, headers OK, symbol_addrs.txt entries proposed, build matches baserom SHA-1.
- **Pending user action** — files in place, build will FAIL until user applies the proposed `symbol_addrs.txt` entries and runs `make extract && make clean && make`.
- **No-upstream-source** — function isn't in libultra/libkmc; use `/decomp` instead.
- **Codegen-divergence** — upstream copy compiled but ROM SHA-1 mismatches; upstream SDK version differs from this ROM's. Hand-decompile via `/decomp`.

### Applied changes

1. `src/lib<name>/...` — verbatim from `<upstream-path>`.
2. `mariogolf64.yaml` — `[0x<seg>, c]` → `[0x<seg>, c, lib<name>/...]`.
3. `include/lib<name>/...` (if headers added) — verbatim from upstream.
4. Deleted: `src/<seg>.c` placeholder.

### symbol_addrs.txt entries to apply

If any referenced symbols are missing from `ghidra_symbols.txt`/`symbol_addrs.txt`:

```
<curated_func>  = 0x<vram>; // type:func
<global_1>      = 0x<vram>; // type:u32 size:0x4
...
```

Otherwise: `No proposals — all referenced symbols already synced.`

### Next user actions

1. Append any proposed entries above to `symbol_addrs.txt` (skip this if none).
2. `make extract && make clean && make` — ROM SHA-1 should remain baserom's.

### Workflow notes

If upstream uses bare `#include "viint.h"` (no path prefix) and the project's CFLAGS doesn't have `-I include/libultra/internal`, add it. Same pattern for any future nested upstream dir. Do NOT modify the upstream source's include lines.
</final-report-template>
