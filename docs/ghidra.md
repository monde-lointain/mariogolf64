---
type: Guide
title: Persistent headless Ghidra analysis
description: Creates, operates, and preserves a bank-aware Ghidra database without making analysis a matching authority.
sources:
  - resource: ../tools/ghidra.py
    title: Project lifecycle and context CLI
  - resource: ../tools/ghidra/Mg64Project.java
    title: YAML-backed program importer
  - resource: ../tools/ghidra/Mg64Context.java
    title: Read-only live context exporter
  - resource: ../tools/ghidra_mcp.py
    title: Project-scoped agent bridge
---

# Persistent headless Ghidra analysis

The database is a durable analysis notebook, not a build input. Keep discovered names, structures,
comments, and prototypes between agent sessions; use live decompilation and cross-references to
answer questions that extracted assembly alone does not settle. Assembly remains the instruction
and boundary authority; only `tools/verify-rom.sh` can approve banking.

## Prerequisites and creation

Use the normal [build setup](build.md) and supply the supported big-endian `baserom.z64`.
The integration needs host GCC for preprocessing, a Ghidra-compatible JDK with `java` and `javac`,
Ghidra, and the installed Ghidra MCP 7.0.0 checkout with `target/GhidraMCP-7.0.0.jar` and its
`.venv/bin/python` containing the MCP SDK. Ghidra 12.1.3 and MCP 7.0.0 were exercised locally.
These optional analysis dependencies are not vendored into the ROM toolchain.

```sh
cmake --preset us
cmake --build build/us --target ghidra-init
cmake --build build/us --target ghidra-start
uv run --locked python tools/ghidra.py status
```

`ghidra-init` depends on extraction. Initialization verifies the input ROM against YAML's expected
SHA-1, builds a manifest from YAML, committed symbols, extracted assembly, and preprocessed source,
then imports and analyzes every program. This input check is not a ROM bank verdict. Repeated
`init` on a completed database is non-destructive; an existing incomplete project is never overwritten.

The persistent project is `private/ghidra/mariogolf64.gpr` with its sibling `.rep` directory.
Keep both. `private/` is ignored; the database and archives contain ROM bytes and must stay private.
State paths must not contain dot-prefixed directory components: Ghidra rejects those project paths.

Common options precede the subcommand:

- `--ghidra-home <installation>` or `MG64_GHIDRA_HOME`: explicit installation override. Otherwise
  reuse the completed database's installation, then discover under `~/.local/opt/ghidra/`.
- `--mcp-root <checkout>` or `MG64_GHIDRA_MCP_ROOT`: defaults to `~/tools/ghidra-mcp`.
- `--state-dir <directory>`: defaults to this checkout's `private/ghidra`.
- `--port <port>` or `MG64_GHIDRA_PORT`: defaults to loopback port 8089.

## Banks and source fidelity

The importer creates one `main` program and one program per YAML code overlay (currently 17
programs). Each overlay program contains resident entry/main mappings as well as its own overlay
bank; those resident addresses therefore recur across programs. Select an exact manifest program
name such as `main` or `overlay_8`, never a fuzzy selector or `baserom.z64`. `mg64_programs`
returns the catalog. An address alone is not a bank identity.

- ROM intervals and VRAM come from YAML; BSS and hardware/vector memory are uninitialized, not
  fabricated ROM bytes. Hardware pages are volatile. RSP microcode blobs are not CPU-executable.
- Function boundaries and ROM offsets come from extracted assembly. Automatically discovered
  functions may also appear in Ghidra, but do not become authoritative boundaries.
- `symbol_addrs.txt` takes primary-name precedence over `ghidra_symbols.txt`. Overlay names are
  assigned only when segment/ROM qualifiers, source ownership, or unique extraction evidence prove
  their bank. Ambiguous legacy names are listed in `private/ghidra/ambiguous-symbols.json`;
  bank-specific placeholders remain instead of spreading a guessed name across overlays.
- Source preprocessing reuses each TU's include/define/undefine profile. Function definitions take
  priority over weaker extern declarations. SDK/game types and addressable globals are imported
  conservatively; existing defined data and user-defined prototypes are preserved. Unsized arrays
  need a known object extent. File-static data needs a matching source data interval.
- The language is `MIPS:BE:64:64-32addr` with `o32`: 64-bit R4300 instructions and 32-bit addresses,
  pointers, and `long`. Stock Ghidra's floating-register ABI is not a verified KMC `-mfp32` model;
  inspect assembly before trusting inferred FP storage, arguments, or returns.

Source-local partial structures can conflict legitimately. Query the type used by the function,
not merely the first structure with a familiar name. Context export sanitizes conflicting type names
in a temporary type manager without renaming the persistent database.

## Agent access and context

Configure the local MCP client to launch the project bridge from this checkout:

```json
{
  "mcpServers": {
    "ghidra": {
      "command": "uv",
      "args": ["run", "--locked", "python", "tools/ghidra.py", "bridge"]
    }
  }
}
```

Use an absolute `--project <checkout>` and script path if the client does not set the repository
working directory. This machine's ignored `.mcp.json` uses absolute paths. Reload the client after
configuration changes. `bridge` starts the service when absent, then speaks MCP stdio; logs and
startup failures go to stderr. Closing the bridge does not discard or stop the persistent service.

The curated bridge exposes decompilation, disassembly, references, memory, strings, types,
renames, comments/bookmarks, prototype/variable/storage edits, and explicit saves. Every
program-specific call requires an exact program name. `mg64_context` accepts that program plus an
exact imported function name or hexadecimal entry address and returns live C, transitive types,
callees, typed/unknown globals, assembly, and ROM provenance. Import/delete/rebase/debugger,
arbitrary script execution, dynamic tool discovery, and server administration are not agent tools.

The backend is a trusted local service, not a sandbox: it enables scripting for the reviewed
exporter. The curated stdio bridge does not expose scripting, but another local HTTP client could.
It binds to loopback; do not publish the port. Existing `GHIDRA_MCP_AUTH_TOKEN` is forwarded.
Cross-process request locks serialize bridge/CLI access, and project/ROM/source identity is checked
before live forwarding. Reconnect bridges after a refresh. Keep builds, MCP requests, and source/
YAML changes serial as required by [the project invariants](../AGENTS.md#invariants).

Export candidate context without invoking the MCP client:

```sh
uv run --locked python tools/ghidra.py context --func get_table_entry
uv run --locked python tools/ghidra.py context --func 0x801F4A40 --segment overlay_8
uv run --locked python tools/ghidra.py context --func 0x801F4A40 --rom 0x14EA80
uv run --locked python tools/seed_c.py --func <source-name> --ghidra
```

`context` writes `ghidra.c`, standalone `ghidra_ctx.c`, and `ghidra.json` under
`nonmatchings/<imported-name>/`, or `--output <directory>`. The JSON records the exact selected
program, segment, function address, ROM offset, and ROM SHA-1 in its `origin`; the live export also
identifies program/manifest/ROM and whether a prototype is known. Reused VRAM addresses require a
bank (`--segment`) or ROM qualifier (`--rom`) to select the intended instance. `seed_c.py --ghidra`
derives ROM/VRAM from the selected source assembly and exports into that source-name candidate
directory before running m2c. It does not require `--parent`; parent contexts remain optional and may
conflict with live types. Uncommitted inferred prototypes are not emitted as false `(void)`
declarations, so m2c can still infer register arguments. Unknown globals are reported rather than
given guessed scalar declarations. Reconcile generated seeds against assembly and source; none of
these files are bankable evidence by themselves.

## Maintenance, names, and backups

```sh
uv run --locked python tools/ghidra.py stop
uv run --locked python tools/ghidra.py refresh
uv run --locked python tools/ghidra.py start
cmake --build build/us --target sync-names
uv run --locked python tools/ghidra.py archive --output private/ghidra/archives/session.gar
```

Refresh reapplies source types/prototypes and proven names, analyzes, and saves all programs while
preserving user-defined signatures. It can finish an interrupted metadata refresh. Import diagnostics
prevent a ready marker and service startup; inspect `import-report.json` and fix the cause before
refreshing again. A changed byte map requires a new `--state-dir`, preserving the existing analysis.
Refresh does not infer new bank boundaries or merge independent analyst edits between programs.
Resident annotations made only in one overlay copy are local to that copy; edit `main` deliberately
and propagate useful resident discoveries explicitly.

`sync-names` produces `private/ghidra/name-proposals.json`; it never writes committed symbols.
Review each proposal against source callers and ROM/bank ownership. Decomp source and
`symbol_addrs.txt` remain naming authority; add-only maintainer entries may include `rom:` or
`segment:` qualifiers. Never hand-edit generated `ghidra_symbols.txt`. This project intentionally
has no automatic Ghidra-to-source rename cutover.

Archive saves open programs and writes a non-overwriting native `.gar`. Restore with Ghidra's
File → Restore Project; retain manifest/ready/import-report files alongside backups for the project
bridge. Stop the service before opening the same database in the GUI or copying `.gpr`/`.rep`.
CMake also provides `ghidra-refresh`, `ghidra-stop`, and `ghidra-status`; analysis is never part of
the ROM target or bank gate.

### Recovering after a checkout move

The manifest and each program's `MG64` options record absolute paths. After moving a checkout,
keep its existing `.gpr` and `.rep` together and run `refresh` from the new location before
starting the bridge. Refresh rewrites the manifest path in every program as well as its source
snapshot ID; it does not require recreating the analysis database.

If the MCP checkout also moved, set `MG64_GHIDRA_MCP_ROOT` to its new directory (or pass
`--mcp-root` before the CLI subcommand). For MCP clients, put that environment override in the
`ghidra` server's `env` object in `.mcp.json`, update the absolute project/script paths, and reload
the client. An `Installed MCP JAR not found` startup error identifies a stale or incomplete MCP
installation path, not a missing ROM database.

```sh
export MG64_GHIDRA_MCP_ROOT=/absolute/path/to/ghidra-mcp
uv run --locked python tools/ghidra.py refresh
uv run --locked python tools/ghidra.py start
uv run --locked python tools/ghidra.py status
```

## Extension decision and limits

No N64 extension is required for this CPU workflow. The [N64LoaderWV loader](https://github.com/zeroKilo/N64LoaderWV)
is useful for a generic ROM bootstrap, but cannot replace this project's explicit YAML overlay
mapping. [Zelda64Loader](https://github.com/Random06457/Zelda64Loader) targets Zelda's formats, not
Mario Golf. [Ghidra-RSP](https://github.com/Random06457/Ghidra-RSP) is relevant to
separate microcode analysis, not to these CPU programs; it was not installed. Revisit it only when
an actual RSP task needs its instruction semantics.

Analysis still reports unmapped SDK references and `overlay_gap` references into other runtime
regions. Do not invent initialized memory to hide these warnings. Source conflicts, dynamic overlay
relationships, FP ABI inference, and unknown data need assembly-backed human/agent analysis.
