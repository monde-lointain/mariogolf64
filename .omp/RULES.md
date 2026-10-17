# Mario Golf 64 invariants

- Complete the YAML `main` segment, including its linked libraries, before selecting or dispatching
  overlay decompilation ([main-completion gate](skills/mg64-decomp/SKILL.md#main-before-overlays));
  an empty shortlist or a carried wall does not unlock overlays.
- `tools/verify-rom.sh` is the sole bank authority. An isolated compile, object diff, permuter
  score, or direct hash is never a bank verdict.
- Never commit a non-matching function. Keep unfinished candidates isolated under `nonmatchings/`.
- Serialize builds, Ghidra MCP, YAML changes, and source edits; the build tree and MCP are shared.
- Preserve compiler-profile fidelity between isolated candidate builds and the ROM build.
- Never hand-edit generated extraction artifacts: `mariogolf64.ld`, `undefined_syms_auto.txt`,
  `undefined_funcs_auto.txt`, `mariogolf64.d`, `ctx.c`, `ctx.c.m2c`, or anything under `build/`.
- Edit `mariogolf64.yaml` only for required subsegment flip/split/path-qualifier changes. Additions
  to `symbol_addrs.txt` are add-only. Change `ghidra_symbols.txt` only through the name-sync
  workflow; do not edit `reloc_addrs.txt`.
- In-flight work names its function, host translation unit, and latest measurement path wherever
  it is tracked (todo, note, or handoff), so it survives context compaction.
