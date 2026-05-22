Never delete symbol_addrs.txt or reloc_addrs.txt

# C Naming Conventions Guide

## General Rules

| Identifier Type                   | Convention   | Example                      |
| --------------------------------- | ------------ | ---------------------------- |
| Namespace-like Prefixes / Modules | `lower_case` | `audio_system`, `input_`     |
| Struct                            | `CamelCase`  | `PacketHeader`               |
| Enum                              | `CamelCase`  | `ConnectionState`            |
| Function                          | `lower_case` | `initialize_system()`        |
| Variable                          | `lower_case` | `frame_count`                |

---

# Constants

## Global Constants

* Use `CamelCase`
* Prefix with `k`

```c
static const int kMaxConnections = 16;
static const float kGravityScale = 9.81f;
```

## Static Constants

* Use `CamelCase`
* Prefix with `k`

```c
static const int kDefaultPort = 8080;
```

## Local Constants

* Use `lower_case`
* No prefix

```c
void retry_operation(void) {
    const int max_retries = 5;
}
```

---

# Struct Naming

* Struct names use `CamelCase`

```c
typedef struct PacketHeader {
    int packet_size;
    int packet_type;
} PacketHeader;
```

* Struct members use `lower_case`

```c
typedef struct AudioDevice {
    int sample_rate;
    int channel_count;
} AudioDevice;
```

---

# Enum Naming

* Enum type names use `CamelCase`
* Enumerator values use `CamelCase`

```c
typedef enum ConnectionState {
    Disconnected,
    Connecting,
    Connected
} ConnectionState;
```

---

# Functions

* Use `lower_case`
* Separate words with underscores

```c
void initialize_system(void);
int open_connection(const char *host_name);
```

---

# Variables

## Local Variables

* Use `lower_case`

```c
int frame_count;
float delta_time;
```

## Global Variables

* Use `lower_case`

```c
int active_connection_count;
```

---

# Static Variables

* Use `lower_case`

```c
static int initialized;
```

---

# Example

```c
#include <stdbool.h>

static const int kDefaultChannels = 2;

typedef enum PlaybackState {
    Stopped,
    Playing,
    Paused
} PlaybackState;

typedef struct AudioManager {
    int channel_count;
    bool initialized;
} AudioManager;

void initialize_system(AudioManager *manager) {
    manager->channel_count = kDefaultChannels;
    manager->initialized = true;
}
```

---

# Decomp workflow

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting), KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (asm→C seed), asm-differ (library mode), decomp-permuter (escalation), Ghidra MCP bridge (live decompile + struct/symbol lookup).

## Slash commands

- **`/decomp <func>`** — Tactical. Match-decompiles one function (placeholder or curated name). Runs `seed C → KMC GCC → objdump → asm-differ` until byte-exact, spot-checks in-tree by byte-level cmp, then **finalizes**: moves the body into `src/<seg>.c`, runs `clang-format`, and does a full `make` whose ROM SHA-1 must equal the baserom. Only then does it declare Match and propose the symbol-name patch. Always ends with a numbered "Suggested workflow improvements" section.
- **`/decomp-lead [scope]`** — Strategic. Reads project state and rewrites `docs/decomp_roadmap.md` (strategy, next 5 targets, subsegments to flip, sync status). Never dispatches `/decomp`. Optional scope arg filters to a subsystem.
- **`/sync-names`** — Pulls curated function + data names from the Ghidra workspace into `ghidra_symbols.txt`. Filters splat placeholders, Ghidra defaults (`FUN_/DAT_/LAB_/…`), `_NON_MATCHING` mirrors, and section-boundary labels. Respects `symbol_addrs.txt` overrides. Equivalent to `make sync-names`. No args.

Only one of `/decomp`, `/decomp-lead`, or `/sync-names` may run at a time — they share the Ghidra MCP slot via `tools/mcp_lock.py` (machine-global directory mutex at `~/.cache/mariogolf64/mcp.lock/`, 30-min stale-TTL; the lock is shared across git worktrees of the same repo).

## Conventions

- **One function per `/decomp` run.** You pick the target.
- **Scratch dir**: `nonmatchings/<func>/` (gitignored, shared with permuter).
- **Ghidra MCP** is assumed running via `~/development/reversing/ghidra/start-headless.sh` (port 8089). All MCP calls pass `program="baserom.z64"`; `mariogolf64.elf` is reserved for `sync_decomp_names.py`.
- **File-level matching**: a source file matches only when every function in it matches. Splat generates `src/<seg>.c` with `INCLUDE_ASM` stubs; promoting a match means replacing one stub with the C body. Don't expect md5 wins until the last stub in a file is gone.
- **`symbol_addrs.txt`/`reloc_addrs.txt`** are sacrosanct — never delete (existing rule, reinforced here). `ghidra_symbols.txt` is owned by `sync_decomp_names.py`; do not hand-edit. Maintainer overrides go in `symbol_addrs.txt`; the two files must be disjoint (splat errors on duplicates).
- **Subsegment flips** (`asm` → `c`) and the first `make extract` for a new file are **user actions**, not agent actions.
- **Decomp is authoritative for names** (per `~/development/reversing/ghidra/mariogolf64/docs/re/coordination/decomp_coordination.md`).
- **`hasm` subsegments** are handwritten asm (entry stub, `__muldi3` libkmc math module, RSP microcode entries in `extern_syms.txt`). `/decomp` refuses these — they stay as raw asm forever.
- **Permuter** (`./run-permuter.sh`) only runs when asm-differ's `percent` ≥ 0.97. Below that, iterate on C or reconsider whether the subsegment should be `hasm`.
- **Display lists**: F3DEX2 microcode. Include `<PR/gbi.h>` when the target manipulates `Gfx*`. For static dlists in rodata, run `~/development/repos/n64-tools/src/gfxdis-rom/gfxdis` (build the tool with `make -C ~/development/repos/n64-tools` once). `gfxdis` only handles static dlists — dynamic builders are hand-decompiled.
- **C naming conventions**: follow the convention table at the top of this file (functions/variables/struct-members `lower_case`; types `CamelCase`; global/static const `kCamelCase`). When stitching m2c output into base.c, rename m2c's `temp_*` / `local_*` / `arg_*` synthetics before promoting.
- **Prompt authoring**: any time `/decomp` or `/decomp-lead` (or any other slash command in this project) is created or modified, the prompt text follows `PROMPT_GUIDELINES.md` at the project root.
- **Isolated-compile caveat**: `/decomp` compiles `nonmatchings/<func>/base.c` standalone. KMC GCC's regalloc/scheduling can differ from the in-tree compile — that's why every match runs an in-tree spot-check before being declared. If iteration mismatches cluster around extern address loads (HI/LO16 relocs against project-local symbols), those are isolation-only — the spot-check is the truth.
- **`src/%` is assembled by KMC `as` (`tools/cc/as`)**, not modern `mips-linux-gnu-as`. The original assembler emits `addu` for `move dst,zero` (encoding `0x...1021`, matching the original ROM, vs modern as's `0x...1025` from `or`) and auto-pads `.text` to its 16-byte section alignment. `asm/%` continues to use modern as because raw asm uses modern-only directives (`.set gp=64`, `.internal`). `include/macro.inc` had its `.internal _MACRO_INC_GUARD` line removed so KMC `as` parses it cleanly; the `.set _MACRO_INC_GUARD, 1` on the next line still guards the include. `permuter_settings.toml` mirrors the Makefile's src/% pipeline.
- **Spot-check via byte-level `cmp` only**: `mips-linux-gnu-objdump` renders both `0x...1021` and `0x...1025` as `move v0,zero`, so mnemonic-level diff silently false-positives. Always `cmp` raw `.text` bytes.
- **Match finalization is three steps**: inline body into `src/<seg>.c` → `clang-format -i src/<seg>.c` → full `make` until ROM SHA-1 matches baserom. Spot-check passing ≠ ROM matching; the final `make` is what proves the match. See memory `match-finalization-checklist`.
- **Workflow retrospection**: every `/decomp` run ends with a numbered "Suggested workflow improvements" section in the agent's final chat response. No file is written. User replies with which numbered items to apply; agent edits CLAUDE.md / seed_c.py / decomp_loop.py / decomp.md within the same session.

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)

- **Names (Ghidra → decomp)**: `make sync-names` (or `/sync-names`) pulls curated function + data names into `ghidra_symbols.txt`. The agent edits `ghidra_symbols.txt` ONLY through this tool — direct hand-edits remain forbidden, as does editing from any other slash command. `symbol_addrs.txt` stays hand-curated; the sync skips any address already in it (the two files must be disjoint).
- **Names (decomp → Ghidra)**: agent proposes `symbol_addrs.txt` edits in its final response. User applies, then runs `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` followed by `make extract` and `make`.
- **Structs**: agent pulls from Ghidra MCP, cross-checks against `re_tracking/structs.yml`, appends to `include/structs.h` (created on first use; flat layout, reshape later). Discrepancies between MCP-live and the snapshot are surfaced in the final response and block the write.
- **Memory map**: read-only on both sides for the agent. Drift is reported in `/decomp-lead`'s roadmap.
- **Forbidden agent edits**: `ghidra_symbols.txt` (hand-edit forbidden; modify only via `make sync-names` or `/sync-names`), `mariogolf64.yaml`, `mariogolf64.ld`, `re_tracking/*.yml`, `symbol_addrs.txt` (unless explicitly approved), `reloc_addrs.txt`.
