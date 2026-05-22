---
description: Pull curated function + data names from the Ghidra workspace into ghidra_symbols.txt. No args.
---

# /sync-names — pull Ghidra-curated names into the decomp

Tactical command. One-shot sync: Ghidra MCP → `ghidra_symbols.txt`. Filters splat placeholders, Ghidra defaults (`FUN_/DAT_/LAB_/…`), `_NON_MATCHING` mirrors, section-boundary labels. Respects `symbol_addrs.txt` (skips any address already present there). Shares the Ghidra MCP slot with `/decomp` via `tools/mcp_lock.py`.

<command-prereqs>
- Ghidra MCP must be running: `~/development/reversing/ghidra/start-headless.sh` (verify with `ss -tln | grep 8089`).
- Only one of `/sync-names`, `/decomp`, or `/decomp-lead` may hold the Ghidra slot at a time. `tools/mcp_lock.py` is the gate.
- `~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py` must be reachable. Set `MARIOGOLF64_GHIDRA_REPO` if the workspace lives outside `$HOME/development/reversing/ghidra/mariogolf64`.
</command-prereqs>

## Step 0 — Reject any arguments

`/sync-names` takes no positional args. If `$ARGUMENTS` is non-empty, abort with:

```
/sync-names: this command takes no arguments. Aborting.
```

## Step 1 — Move to project root + acquire lock

```bash
cd /home/clydew372/development/reversing/projects/mariogolf64
tools/mcp_lock.py acquire --command sync-names --identifier export
```

Non-zero exit on acquire → another holder is alive. Abort with the message printed by `mcp_lock.py` on stderr. Do NOT release a lock you didn't acquire.

## Step 2 — MCP discovery

Call `mcp__ghidra__list_instances`. If the response is empty or errors out, **release the lock first**, then abort with:

```
/sync-names: Ghidra MCP is not running. Start it with `~/development/reversing/ghidra/start-headless.sh`. Aborting.
```

## Step 3 — Clean-tree guard

```bash
git status --porcelain ghidra_symbols.txt symbol_addrs.txt
```

Any output → the working tree has uncommitted changes to one of these files. **Release the lock first**, then abort:

```
/sync-names: ghidra_symbols.txt or symbol_addrs.txt has uncommitted changes. Commit or stash before syncing. Aborting.
```

This prevents `--write-in-place` from clobbering in-progress work.

## Step 4 — Pre-flight + invoke the export

**Pre-flight check** (before invoking the script): the chosen interpreter must be able to `import mcp`. If not, surface a clean install instruction; don't let `sync_decomp_names.py` bury the error in its own SystemExit message.

```bash
GHIDRA_REPO="${MARIOGOLF64_GHIDRA_REPO:-$HOME/development/reversing/ghidra/mariogolf64}"
SYNC_PY="$GHIDRA_REPO/venv/bin/python3"
[ -x "$SYNC_PY" ] || SYNC_PY="./venv/bin/python3"
[ -x "$SYNC_PY" ] || SYNC_PY="python3"

if ! "$SYNC_PY" -c "import mcp" 2>/dev/null; then
    # Lock has been released by an earlier path; abort with a fix-it line.
    echo "/sync-names: mcp Python package not installed in $SYNC_PY." >&2
    echo "  Install with: $SYNC_PY -m pip install mcp" >&2
    exit 1
fi

"$SYNC_PY" "$GHIDRA_REPO/scripts/sync_decomp_names.py" \
  --export-to-decomp --decomp-root . --write-in-place
```

Capture stdout — the last line is the structured summary needed for Step 6's report. The Ghidra-side script does:

1. Read prior `ghidra_symbols.txt` into memory.
2. Load `symbol_addrs.txt` → override addresses (dropped from output).
3. Inherit only labels/jtbls from prior; funcs/globals are fully owned by the new pipeline.
4. Pull funcs + globals via MCP. Globals: dedup `_NON_MATCHING` mirrors → map Ghidra types → splat `(type, size)`. Performance-fallback: heuristic sizing if MCP `get_type_size` unavailable.
5. Cross-address name-uniqueness check (soft-skip collisions with diagnostic).
6. Merge inherited + funcs + globals, sort by address ascending.
7. Atomic write: `.tmp` + `os.replace`.

## Step 5 — Release lock (every exit path)

```bash
tools/mcp_lock.py release --identifier export
```

This MUST run before the final report regardless of Step 4's exit code, EXCEPT when Step 1's acquire failed (don't release a lock you don't own).

## Step 6 — Final report

Emit one final response with these sections in this order:

<final-report-template>
### Outcome

One of:
- **Synced** — Step 4 exited 0 and the structured summary line printed.
- **Skipped — MCP down** — Step 2 aborted; no file changes.
- **Skipped — lock contention** — Step 1 aborted; no file changes.
- **Skipped — dirty tree** — Step 3 aborted; no file changes.
- **Failed — export error** — Step 4 exited non-zero. Lock released; report stderr.

### Summary (only if Synced)

Verbatim from Step 4's last line, e.g.:

```
funcs: +N renamed=R; globals: +G retyped=T; removed=X; skipped: overrides=O collisions=C unknown_type=U
```

Plus `git diff --stat ghidra_symbols.txt` for line-level corroboration.

### Next-step suggestion (only if Synced) — **MANDATORY USER ACTION**

```
make extract && make
```

This is not optional. Any code that already references the curated names (e.g., a `src/libultra/...` file using `__osViCurr`) will fail to link until `make extract` regenerates the asm/data tables with the new name. The slash command does not run `make extract` itself by design (project convention) — but the final report MUST make this loud:

> "Next: `make extract && make`. The build will fail to link until this runs."

If `make extract` errors on `ghidra_symbols.txt` (splat rejects a line), roll back via `git checkout -- ghidra_symbols.txt` and report the offending line. splat is the only real validator — there is no dry-run mode.

### Workflow notes

If `name_collisions > 0`: two distinct Ghidra-curated symbols share a name. Both were dropped from the export. Fix by renaming one in Ghidra (then re-run `/sync-names`) or by adding a maintainer override entry to `symbol_addrs.txt`.

If `unknown_type > 0`: one or more globals had a type that didn't map cleanly to splat (struct with leading underscore, opaque struct without MCP `get_type_size`, etc.). They were skipped, not synced. Inspect via `MARIOGOLF64_GHIDRA_REPO/scripts/sync_decomp_names.py --export-to-decomp --decomp-root . --out /tmp/probe.txt` and diff against `ghidra_symbols.txt`.

If "perf-fallback engaged" appears: MCP `get_type_size` calls were dropped to meet the 60s budget. Struct-by-value globals were skipped. Re-run with a less-loaded Ghidra instance, or commit the current names and address remaining gaps in a follow-up.
</final-report-template>
