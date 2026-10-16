# Documentation update log

## 2026-10-15

- **clangd navigation**: Added a repository-local omp override for `clangd-23` and
  an editor-only compilation database generated from the existing KMC profiles.
  CMake configure regenerates it; standalone setup requires no ROM or target toolchain.
  Removed duplicated fixed/per-library editor flags and documented background indexing,
  regeneration and unsupported GNU nested functions/KMC lvalue casts.
  Verified CMake configure, seven frontend profile regressions, cross-file definition
  lookup and 34 `osCreateMesgQueue` references through omp's live clangd server.

- **Agent-based decomp workflow refinement**: Five parallel read-only deep dives
  compared REA's seed-first investigation, Snowboard Kids' matching queues, native
  Oh My Pi task/context contracts, the Ghidra book (chapters 7/8/19/20), and Eilam's
  recovery/optimization guidance. Selected book claims were checked in the originals.
  - Separated function-specific WIP characterization from broad BACKLOG prose in
    loose-stub discovery, carry checks and campaign records. `BACKLOG-ONLY` remains
    visible but does not silently exclude a leaf. Whole-pack parking, YAML bank
    membership, nested/pool checks and the full-ROM bank authority are unchanged.
  - On the same 183 live main stubs, carried classification drops 180 → 47;
    133 backlog-only advisories remain visible and 103 leaves satisfy the existing
    eligibility filter. These are not newly banked functions or a difficulty forecast.
  - Removed the old per-leaf BACKLOG union and historical justification for safe
    false positives rather than adding another prose-exclusion heuristic. Ignored
    local candidate/measurement presence does not affect discovery.
  - Tightened native handoffs to the immutable measured base, dump-producing command,
    differing region and rejected levers. Workers ask for missing observations
    instead of proposing unsupported patches. Retained existing resource ownership;
    no new roles, runner wrappers or evidence ledgers.
  - Clarified best-base restoration, dump/source pairing and provisional integration
    of confirmed linker-alias-only residuals. Neither a compiler citation nor a low
    isolated score replaces source eligibility, integration checks or ROM verification.
  - Regression fixtures failed before the classification fix and pass afterward.
    The tooling suite passes with two skips. Actual ranker, combined carry/nested,
    file-closure and default/explicit campaign CLIs exercise the new behavior.
    Ruff reports the existing `pick_target.py:13` import-order diagnostic, also
    reproduced against HEAD; no unrelated import reformat was applied.
    Game sources, compiler profiles, YAML and symbol files are unchanged.

- **Five additional main banks**: `func_800451E4`, `func_800719A0`,
  `func_8006CD50`, `func_8007E7B8`, and `animation_play` replace 498 instructions /
  1,992 bytes of assembly. Main C-host stubs decrease 188 → 183.
  - Recovered meter-category transitions, HUD entry setup, round-score totals,
    wind-pool initialization and typed animation part-state resets. Corrected
    the HUD caller declarations; the only overlay edit is its existing prototype.
  - Source levers: threshold scratch materialization, post-offset dimension
    copies, in-place signed fixed-point truncation, and a const-qualified read of
    the fixed model part-list pointer. The latter resolves the old 79-instruction
    animation scheduling carry without const-qualifying mutable part state.
  - Inspected the user-supplied GCC 2.7.2 CSE/expansion/scheduler sources and used
    read-only parallel compiler analysis. All source edits and builds serialized.
    No compiler/profile, YAML or symbol changes.
  - Each bank passed `tools/verify-rom.sh`, including the final integrated state:
    `e2c4e7a905b29529b49a1619a401fe699224829b`.
    A throwaway 32-bit UBSan smoke passed 19,704 checks across all five bodies,
    including boundary transitions, signed arithmetic, sentinel termination and
    preservation of unrelated records/bytes. No emulator run.
  - Refreshed BACKLOG measurements for isolated audio, camera, terrain, particle
    and player-state carries. Nonmatching bodies remain isolated; their live
    `INCLUDE_ASM` stubs are unchanged. Retired stale caller-eviction claims for
    the three newly banked standalone helpers.

- **Compiler-source loop-note investigation**: Four parallel read-only reviewers inspected the
  requested GCC 2.7.2 and binutils 2.6 trees. The inspected `sched.c`, `flow.c`, `reorg.c`, and
  `tc-mips.c` match the pinned build's vendor copies by SHA-256. Compiler experiments and source
  integration remained serialized; each hypothesis was measured before advancing to another.
  - Tested nine technique families with 19 exact-profile source variants: union/scratch lifetimes,
    compare-owned constants, loop induction/bounds, real loop-entry guards, one-use call arguments,
    pre-combine rectangle arithmetic, per-branch decoder scratch, field-width coalescing, and actual
    scratch anti-dependencies.
  - Removed the remaining wrapper in `resolve_shot_quality_table`: the table byte and subsequent
    signed power load now reuse `raw`, while decoded/result value `v` retains its existing lifetime.
    The scheduling dump shows `REG_DEP_ANTI` from the power load to digit decoding on their shared
    pseudo, preserving `lbu; subtract; lh` without an artificial loop note. Main's one-shot wrappers
    decrease 36 → 35.
  - Distinguished mechanisms: loop notes fence scheduling without delimiting basic blocks;
    `flow.c` weights references by loop depth; both local and global allocation rank quantities by
    references and lifetime before their numeric tie-breakers. Call-free local allocation is not
    simply birth order. GCC owns the scheduling/slot decisions; GAS only handles local hazards and
    limited adjacent branch swapping.
  - The closest nonmatching banner experiment restores height/width coloring through meaningful
    rectangle arithmetic but still changes the right-edge instructions. Guarded leaderboard loops
    add a branch or reduce the stack frame from 2,352 to 2,344 bytes. Decoder scratch/width changes
    still alter inlined-helper register allocation. None of these candidates was integrated.
  - `tools/verify-rom.sh` passes with USA retail SHA-1
    `e2c4e7a905b29529b49a1619a401fe699224829b`.
    A throwaway 32-bit UBSan quality-helper smoke run passed 2,462,223 checks covering table/default
    selection, club indices, signed scaling, rounding, clamps, and unchanged input records.
    No emulator run. Compiler/assembler sources, profiles, YAML, symbols, and overlays are unchanged.

- **Main-bank one-shot-loop cleanup**: Three parallel read-only reviewers inspected the 59
  `do { ... } while (0)` loops in 11 `src/main/` hosts against Code Complete chapters 10–17.
  Candidate edits, compiler measurements, source integration, formatting, and ROM builds remained
  serialized.
  - Removed 23 wrappers across eight hosts. Flattened straight-line initialization and clamp code,
    removed redundant empty boundaries, and reduced the absolute-value helper's nested notes from
    three to two. The supershot scorer now returns its result directly instead of repurposing its
    selector, allowing all ten of its one-shot wrappers to disappear.
  - Kept the `acosf` representation scratch in its existing positive-input branch and the `atan2f`
    sign-transfer scratch in its case scope. Preserved union access, arithmetic, real loops,
    decoder dispatch edges, and data layouts; removed or corrected obsolete loop-note comments.
  - Retained 36 measured code-generation constraints. In particular, the leaderboard insertion
    boundary, saved-entry validation notes, and player-slot clear note remain: their removal changes
    scheduling, allocation, or function size. Decoder and circle-clamp reductions were measured
    cumulatively rather than assuming individually safe removals compose.
  - `tools/verify-rom.sh` passes with USA retail SHA-1
    `e2c4e7a905b29529b49a1619a401fe699224829b`.
    A throwaway 32-bit UBSan smoke run passed 16,985,961 checks over nine before/after function pairs:
    score categories and signed-byte boundaries, float representation transfers and math edges,
    glyph output and terminators, interpolation, circle alpha/color preservation, and extended-LZ
    literals, ring back-references, and nonfinal-window carry state. No emulator run.
    YAML, symbol files, compiler profiles, library mirrors, and overlay sources are unchanged.

- **Extended structure recovery and interior-symbol reconciliation**: Eight parallel implementors
  read Code Complete Chapter 11 before inspecting the 58 main C hosts; two additional alias-audit
  scouts read the same chapter. Source integration, compiler experiments, and ROM builds remained
  serialized.
  - Recovered model attachment points and bone-matrix pointers, animation frame ranges and update
    controls, the mixed integer/float projection output, club-quality scaling and club indices,
    spark motion/lifetime fields, deferred player-sound records, and the 34-by-36 terrain color grid.
    Corrected the grid's interior alias and preserved retail's partial 1024-halfword initialization.
  - The overlap audit identified the former `CameraSnapshot` as `HoleEnvironmentState`: the fixed-point
    pin position shares its 0x34-byte storage with wind and putter-related state. Migrated all live C
    consumers to the canonical record while preserving its retail `camera_position_x` linker binding.
    Continue and saved-mode records now describe that same environment, not a camera snapshot.
  - Unified the seven-word round accumulator as `RoundStatistics`; modeled the player's pending-result
    field, used the actual player club array, recovered `TerrainTileCache.lod_tile_index`, and replaced
    the spark timer/enabled and saved-mode scalar aliases with member accesses. Kept unproved meanings
    unknown and kept CPU/RSP address forms distinct. Existing matching-sensitive scalar projections
    remain explicitly rooted in canonical members instead of duplicate padded records.
  - The already-decompiled overlay-15 asset caller now uses `RomAssetLoad`, replacing its misleading
    pair of read slots. No new overlay functions were selected or decompiled; no stubs were removed.
  - `tools/verify-rom.sh` passes with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
    A throwaway 32-bit UBSan smoke run passed 5,834 layout and runtime checks, exercising animation,
    deferred sounds, terrain color/LOD accesses, spark controls, shot quality, and round-result state.
    The two live candidates affected by shared-header changes compile under their parent profiles;
    immutable measurement snapshots remain untouched. No emulator run. YAML, symbol files, compiler
    profiles, and upstream library mirrors are unchanged.

- **Five more main functions banked**: `func_80047CAC`, `get_tile_attribute`,
  `func_8005470C`, `func_80060190`, and `func_800604F4`
  (276 instructions / 1,104 bytes). Main stubs decrease 203 → 198.
  - The interpolation stepper uses a shared scaled-coordinate scratch and an
    empty scheduling boundary before its smoothing constant, separating
    instruction order from reference weighting. The exact middle constant is
    `19.6500015f`. The terrain lookup uses explicit byte offsets and distinct
    attribute/result locals; the historical phantom-addend wall does not recur.
  - The character transform matches directly using `CharacterState`, the
    existing double coefficient, and five SDK matrix calls. Removed its obsolete
    `void*` declaration. Shared zero/positive return destinations recover both
    selector diamonds. The roster predicate uses the real second player's
    character field and masks the rank tie bit.
  - Four isolated comparisons score zero; `func_800604F4` scores 10 only for
    `player_records + 0xD5` versus its interior `D_801B72A5` relocation.
    Fresh integrated ranges agree with retail instructions and resolved targets.
    `tools/verify-rom.sh` passes on the final five-function state with SHA-1
    `e2c4e7a905b29529b49a1619a401fe699224829b`.
  - A throwaway 32-bit UBSan harness extracted all five integrated bodies and
    passed 404,568 checks: float interpolation, terrain clamps/nibble parity,
    signed score categories, roster bounds/tie ranks, and 1,200 transform
    scenarios using the repository's actual matrix routines. External roster
    results were injected at dependency boundaries; no emulator run.
    YAML, symbols, compiler profiles, library mirrors, and overlays are unchanged.
  - Retained unbanked carries separately: the byte mapper improves from score
    225 to 105 with a store scope; the collector remains at its branch-target
    residual (score 5), and the abs helper retains its input coalescing residual
    (score 10). Their live stubs remain.

- **Five carried main functions banked with loop-note scheduling boundaries**:
  `resolve_shot_quality_table`, `func_80047DBC`, `func_80079EBC`,
  `get_surface_type`, and `func_8003DE80` (457 instructions / 1,828 bytes).
  Main stubs decrease 208 → 203.
  - Compiler inspection at `mips-gcc-2.7.2/sched.c:2053-2080` confirms that loop
    notes prohibit cross-boundary scheduling inside one basic block. One-shot
    C loops recover the abs/threshold order, early edge constants, and shared
    double load/count/store order without asm or compiler-profile changes.
    The quality helper additionally needs a result scratch live through
    clamping and return to preserve the byte-load/subtract/power-load registers.
  - Extended the quality host's rodata sibling from `0xA7930` through `0xA7B60`
    for its 23 individual digit tables and regenerated jump table. The particle
    spawn retains the existing extern double pool, signed random remainders,
    three-slot limit, and canonical particle layout.
  - Four isolated comparisons score zero; the quality helper's score 5 is only
    a local-label spelling difference, with the same `1040003d` branch encoding.
    Fresh integrated ranges preserve instruction words and relocation identities.
    `tools/verify-rom.sh` passes with ROM SHA-1
    `e2c4e7a905b29529b49a1619a401fe699224829b`.
  - A throwaway 32-bit UBSan harness of all five integrated bodies passed
    47,077 checks covering quality categories/clamps, grid edges and selectors,
    threshold boundaries, particle allocation limits, random order/signedness,
    and untouched state. No emulator run. Symbol files, compiler profiles,
    library mirrors, and overlay sources are unchanged. Updated the resolved
    carry notes, backlog, hazard guidance, and source-lever index.

- **Five carried main functions banked**: Replaced `INCLUDE_ASM` for
  `func_8006DEB4`, `func_8005DE88`, `func_80048CF8`, `func_80067A60`, and
  `func_80051D8C` (218 instructions / 872 bytes). Main stubs decrease 218 → 213.
  - Preserved three pending-result snapshots in the statistics update; the save
    flag helper's formerly shifted globals now resolve at their named addresses.
    Explicit `atan2f(dx, dz)` arguments recover the angle helper's FPR homes and
    delay slot. Branch-local sound calls plus separate table-base/row indexing
    recover the sound selector's argument copies and address order.
  - The audio initializer distinguishes the pointer-bank end from the following
    wave-table start at ROM `0x18BE00`. The ROM headers identify `PtrTablesV2` at
    `0x171790` and `WaveTables` at `0x18BE00`; `load_audio_wave_bank` passes the latter
    to `MusPtrBankInitialize`. Added the two semantic boundary symbols and explicit
    PI-ROM linker bindings, following `rom_asset_directory`'s existing convention.
    Used the libmus header instead of six incompatible local API declarations.
  - Four isolated comparisons score zero. The audio candidate scores 10 solely
    for the new bank-end relocation name
    (`nonmatchings/func_80051D8C/measurements/ddc8b43269ea4c2f16cb7f79a285a64c7d5a398c82c8889a39a30515f2b5b5b4.json`);
    the final source also names the wave-table start. Fresh integrated function
    ranges preserve retail instructions and resolve both boundaries to `0x18BE00`.
    `tools/verify-rom.sh` passed after every bank, including the final state:
    SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  - A throwaway 32-bit UBSan harness extracted the five integrated bodies and
    passed 3,482 checks covering statistics branches, flag save/restore, angle
    guards, sound-table selection, and audio initialization state/order.
    No emulator run was performed. YAML, compiler profiles, and overlay sources
    are unchanged.

- **Main-bank structure recovery and cross-file reconciliation**: Parallel implementors read
  Code Complete Chapter 11 before preparing isolated proposals; live integration, source edits,
  compiler experiments, and ROM builds remained serialized. Audited the 58 main C hosts and
  established shared animation, club, controller, fog, ROM-asset, session, terrain, and vector
  layouts, with private HUD, particle, scenery-transform, result, and save-region records kept
  in their translation units.
  - Replaced manual member offsets with named fields and typed arrays; reconciled interior labels
    with their enclosing storage. Corrected cross-consumer interpretations: character-indexed
    statistics, sprite indices rather than active flags, live club shots rather than distance
    presets, and compressed-load envelopes rather than duplicate read slots. Unproven fields
    retain offset-based unknown names.
  - GCC 2.7.2 source and original/current disassembly identified aggregate-address sharing,
    scalar alias classification, and induction ordering as matching constraints. The few required
    member-rooted scalar views/address projections retain named layouts instead of separate
    synthetic storage.
  - `tools/verify-rom.sh` passes with SHA-1
    `e2c4e7a905b29529b49a1619a401fe699224829b`. Fresh 32-bit smoke runs pass 113 layout/member
    checks across 19 recovered record types and 46 runtime checks exercising actual controller
    transitions, signed scoring, winner resolution, and ROM-slot lifecycle functions.
    No emulator run was performed. YAML, symbol files, compiler profiles, overlay sources, and
    upstream library mirrors are unchanged.

- **Five main camera-family banks**: Replaced the stubs for `func_80095A10`,
  `func_8009676C`, `func_800977E0`, `func_80097A08`, and `func_80097E30` in
  `src/main/func_80095A10.c` (469 instructions / 1,876 bytes). Ordinary clamped
  interpolation and terrain-height limiting match directly; the three shot-view
  builders reuse the banked siblings' source shape and required scratch frame.
  All five isolated comparisons score zero, and freshly built host ranges agree
  in instruction words and resolved relocation targets. `tools/verify-rom.sh`
  passes with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  A throwaway 32-bit UBSan harness of the integrated bodies passes 10,837 checks,
  including interpolation boundaries, terrain clamps, and 1,200 camera scenarios.
  Main stubs decrease 229 → 224; this host retains eight. Updated stale camera
  carry guidance.
  YAML, symbols, compiler profiles, and overlay sources are unchanged.
  No emulator run was performed.

- **Five main-segment decompilation banks**: Banked `func_80054900`,
  `func_80054938`, `func_80054B7C`, `func_80054BC0`, and `func_80029B08`
  (348 instructions, 1,392 bytes). The two animation pairs use real GCC nested
  definitions, including the unused length helper's static-chain spill.
  Captured-pointer allocation before the row-array declaration fixes the debug
  dump's frame; base-before-offset formation and a shared return fix the distance
  selector. The diagnostic delay loop needs an equality-counted bound.
  - Extended the animation host's single rodata carve to `0xABAE0..0xABC40`;
    unbanked siblings retain generated diagnostic strings, while source owns
    remaining constants. Natural eight-byte section padding replaces the former
    four-byte gap subsegment. Added the ROM-required `-ffast-math` host override.
  - Isolated comparisons and freshly built function ranges agree. The full-ROM
    verifier passes with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
    A throwaway 32-bit UBSan harness passed 624 checks covering numerical and
    selection paths, bounded appends, all 18 generated table rows, and the
    diagnostic loop's first output. No emulator run was performed.
  - `animation_play` remains isolated at 79/79 instructions, score 225, with a
    part-table load-order/register residual. Nine animation-host stubs and six
    vector-host stubs remain. Overlay source and symbol files are unchanged.

- **Main guard-wrapper decompilation**: Banked `func_800564F0` and `func_80055738`
  in `src/main/func_80054900.c` (128 bytes total). Non-void fall-through
  declarations preserve the empty null-guard delay slots without extra return
  instructions, disproving the historical paired source-wall claim. Updated the
  reconstruction note, evidence index, source lever, and hazard guidance.
  Isolated comparisons match 12/12 and 20/20 instructions; freshly built host
  ranges preserve instruction words and call relocations. A throwaway sanitized
  host harness passed 20 null/non-null and full-state checks. `tools/verify-rom.sh`
  passes with ROM SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  YAML, compiler profiles, and overlay sources are unchanged.

## 2026-10-14

- **Progressive-discovery OKF refinement**: Audited the bundle against OKF v0.2 and current
  implementation. Consolidated workflow navigation in the [workflow index](workflow/index.md)
  and native skill, removing the redundant prose navigation guide; the tooling overview now
  routes campaign commands to their [single owner](campaigns.md).
  - Replaced boilerplate reconstruction summaries with discriminating residual, coupling,
    resolution, or interpretation evidence. All 88 note descriptions now match their containing
    indexes; summaries distinguish superseded diagnoses, linked-object evidence, and ROM banks.
  - Corrected isolated-build examples to inherit parent-TU profiles, documented extraction
    prerequisites for reference discovery, and clarified explicit clean and container-mount
    side effects. The [maintenance reference](documentation.md) distinguishes OKF conformance
    from stricter local discovery/link rules.
  - **Proof**: Validated 107 bundle Markdown files (102 concepts, four indexes, one log),
    directory coverage, 632 local links, and 32 local provenance paths; all live routes resolve.
    Exercised workflow, evidence, and hazard reading paths. The hazard guard passed all 115 frozen
    citations. Verified all 88 reconstruction bodies, the three root sprint records, and the
    hazard reference remain unchanged; one historical BACKLOG shorthand self-link is retained.
    Actual read-only reference lookup returned advisory results; campaign preparation returned
    an empty main queue with 1,013 available C references. CLI help also confirmed the documented
    campaign, reference, profile-source, and Ghidra interfaces. Commit review restored the build
    task list after finding stray patch text; a follow-up scan found no remaining patch syntax.
  - **Scope**: Documentation only; no ROM build, Ghidra service, or container was started.

- **Main-bank readability pass**: Read Code Complete Chapters 7, 10–16, 31, and 32,
  then reviewed all 68 functions in the 17 requested source files with six parallel
  proposal-writing workers and serialized integration. Added semantic names for
  49 routines and 52 data/diagnostic symbols, migrated committed C references,
  and let extraction regenerate assembly references. The physical ROM directory
  has an absolute symbol declaration plus its linker definition in `extern_syms.txt`.
  YAML, layouts, compiler profiles, numeric operations, DMA behavior, and GBI
  command order are unchanged.
  - Replaced loader byte strides with typed slots, made slot reset and roster/color
    traversals counted loops, removed magnitude/CRC and audio-callback gotos, and
    simplified the simple decoder's boundary checks and halfword back-reference.
    Made endless token loops explicit and removed a redundant decoder one-shot loop.
  - Narrowed score, scheduler, fault-message, and color-update locals into existing
    blocks; added no scope-only braces. Named podium-achievement fields and retained
    NuSystem/libmus interfaces, correcting the pre-NMI counter's callback type and
    the SRAM wrapper's caller return declarations. Unified the 17 files with
    clang-format 23.1.3 and updated interface comments.
  - Retained measured exceptions: decoder dispatches, remaining allocation-shaping
    one-shot loops and shared halfword scratch; the score-comparison exit; the
    selector argument forwarded to the global-state course lookup; and the roster
    generator's unused legacy return register. Splitting decoder scratch changed
    18 instruction bytes; a direct score return changed branch polarity/delay-slot
    placement. These are documented constraints, not permission to preserve
    unmeasured low-level shapes.
  - **Proof**: Profile-faithful isolated comparisons guided each structural change.
    After formatting and integration, `tools/verify-rom.sh` built the retail ROM
    and reported SHA-1 match `e2c4e7a905b29529b49a1619a401fe699224829b`.

- **Wipe-box readability**: Read every guideline in Code Complete Chapters 7,
  10–16, 31, and 32 and reviewed all three routines in `src/main/func_800500E0.c`.
  Named animation states, transition timing, fixed-point units, rectangle bounds,
  color channels, queue data, arguments, and the advance/draw, enqueue, and
  queued-draw routines. Added authoritative symbols; extraction migrated assembly
  callers and queue definitions. Retained the short counted `for`, typed array
  indexing, four-frame wrap even while open, signed half-width truncation, and
  display-list command order. No standalone scope blocks were added: the retail
  C89 compiler requires declarations before statements, with assignments kept by
  use. Moving the cursor load below the early returns changed scheduling and
  registers; its entry-time initialization remains documented. Formatted with
  `clang-format-23`. A 32-bit smoke of the real routines passed 1,560 checks:
  112 state/frame/geometry combinations and six draw cycles each at queue sizes
  0, 1, and 10, including exact packet words, animation transitions, queue
  consumption, structure layout, and untouched buffer boundaries. Isolated
  instruction words and relocation targets agree with the baseline. Final
  `tools/verify-rom.sh` passed with retail SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`; YAML and compiler profiles are unchanged.

- **Heap-slot readability**: Reviewed every guideline in Code Complete Chapters 7,
  10–16, 31, and 32 against all three routines in `src/main/func_8004DD70.c`.
  Named slot indices, region bounds, the constant region byte count, and the list
  sentinel; renamed `heap_get_total` to `heap_get_total_free` with an add-only
  authoritative symbol. Centralized the three declarations and region preconditions
  in `heap.h`, replacing the scenery reset's local declaration. Named the unknown
  largest-free cache value and reused it in both heap initializers. Retained typed
  member access and the matching mixed indexed/pointer stores; no loops or jumps
  are needed. Formatted the target with `clang-format-23`. A 32-bit host smoke of
  the real three routines passed all four slots, two reset/register cycles, three
  region sizes (16/64/120 bytes), circular links, free-byte totals, unchanged
  largest-free cache, payload preservation, and slot isolation. Isolated and
  integrated instruction streams and relocations agree; `tools/verify-rom.sh`
  passed with retail SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  YAML and compiler profiles are unchanged.

- **Pin-rendering readability**: Applied the C-relevant guidelines from Code Complete
  Chapters 7, 10–16, 31, and 32 to all six routines in `src/main/func_8003E400.c`.
  Named resource loading/freeing, marker-matrix construction, one-render suppression,
  and the visibility predicate; migrated compiled C references and added authoritative
  symbols. Replaced raw metadata bytes with the existing `RomLoadSlot[2]` layout,
  byte-offset matrix arithmetic with `[view_index][render_buffer_index]`, and the
  unsigned range trick with explicit comparisons against states 6 and 7.
  Named assets, state, arguments, locals, and constants; retained load/free order,
  guard short-circuiting, and the retail `3.14f` approximation. GCC 2.7.2 requires
  declarations before statements, and the camera-X address must remain live across
  `atan2` for matching reloads. KMC assembly input paths and identifier spelling also
  affected leading padding in a caller; `active_terrain_index` preserves its layout.
  Corrected extraction's symbol-input order for Splat 0.50's last-wins resolution,
  so maintainer names override legacy Ghidra names without linker shims or edits to
  generated symbols. A 32-bit host smoke of the real six bodies, with dependency
  probes, passed two load/free cycles, independent setters, 864 visibility/lookup
  cases, and six matrix destinations with untouched neighbors. The ROM-build tooling
  suite passed all 10 tests; `tools/verify-rom.sh` passed with retail SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`. YAML and compiler profiles are unchanged.

- **Scenery billboard reset readability**: Reviewed Code Complete Chapters 7, 10–16,
  31, and 32 and refactored the sole routine in `src/main/func_800263B0.c` as
  `reset_scenery_billboards`. Replaced byte-offset casts and the `do-while` with
  typed descriptor member access and a counted `for`; named the sprite count, heap
  slot, diagnostic, array, and readiness flag. Migrated all three compiled calls
  and added authoritative symbols while preserving the renderer's interior symbols.
  Kept the loop index local to the short routine without a standalone scope block.
  A 32-bit host smoke using the real reset and `heap_init` confirmed all 46 pointers
  clear, metadata and other heaps remain unchanged, and repeated resets work from
  readiness states 1, 0, and -1. Final `tools/verify-rom.sh` passed with retail SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`; YAML and compiler profiles are unchanged.

- **Overlay-manager readability**: Applied the C-relevant guidelines from Code Complete
  Chapters 5–19 to all nine routines in `src/main/func_80025D30.c`. Named the descriptor
  fields, state, diagnostics, constants, arguments, and routines; migrated compiled callers
  and added authoritative symbol names. Replaced the segment pointer walk with indexed
  access and narrowed scratch scopes without changing layout, cache/callback ordering,
  unsigned frame-age arithmetic, or retail error behavior.
  Separate initialization-loop counters changed KMC registers; separate decoded-index
  locals in the load/unload paths changed instructions. Retained and documented those
  matching forms. Array-base symbol extents preserve separately referenced interior
  symbols. All nine isolated instruction streams matched after relocation normalization;
  integrated `tools/verify-rom.sh` passed with retail SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`. YAML, compiler profiles, and overlay
  implementations are unchanged.

- **Main decompilation**: Banked `get_terrain_vertex_pointer` (`0x80041524`, 40 instructions)
  in `src/main/get_tile_attribute.c`. Shared coordinate/address scratch chains resolved
  the carried register permutation and column-select branch-likely residual without
  profile changes, inline assembly, or a permuter. Isolated score zero, integrated
  relocation-aware function spot-check, and `tools/verify-rom.sh` full-ROM SHA-1 match.
  Updated its reconstruction note and index; YAML and overlays are unchanged.

- **Carry/agent workflow refinement**: Four parallel read-only dives inspected REA's
  `reverse-engineer-anything` skill/evidence workflow, Snowboard Kids' candidate/near-match tools
  and task-runner, Oh My Pi's native task/skill implementation, and relevant chapters of both
  local RE/Ghidra books. Kept the thin `/decomp` command, native tasks, optional Ghidra,
  content-addressed measurements, and parent-only shared-resource ownership.
- **Removal and consolidation**: Removed `pick_target.py --refresh-residual` and its duplicate
  shallow host scan/manual compiler-profile path; fresh carry measurements now use
  `decomp_loop.py --func`. Migrated the live hazard citation without changing frozen headings.
  Carry recovery checks the cited source/snapshot before measuring. Removed duplicate fan-out
  acceptance prose and made research findings independent of candidate/patch templates.
- **Evidence handoffs and diagnostics**: Worker finals must preserve the requested findings after
  steering. Dispatch names the measured base/profile and relevant dumps. Documented existing
  profile-aware scheduling diagnostics, root dump-file placement, and function-scoped
  relocation-aware object comparison after target assembly disappears on integration.
  Discovery modes run independently because an all-carried pool can still exit 1.
  Replaced the stale Make/delete-object recipe in `cmpfn.sh` with the authoritative build CLI.
- **Smoke proof**: Canonical `decomp_loop.py` reproduced `func_8005D0D8` at score zero with
  67/67 rows, the main host profile, fast-math, and a retained source/measurement pair.
  Both carry-inclusive discovery modes ran; the removed option exits 2 with no shim.
  The documented `rom_build.py --profile-source ... --extra-flag=-dSRd` command produced all
  three scheduling dumps; temporary assembly/dumps were removed. Tooling suite: 150 passed,
  two skipped. Shell syntax and all 115 frozen hazard anchors passed. Function-scoped objdump
  compared 67 instructions per object; only the expected same-callee relocation field differed.
  Checked 103 concept frontmatters, four indexes, 628 local links/fragments and 27 local source
  paths. The unchanged historical `BACKLOG.md` fragment remains the sole navigation finding.
  Ruff retains the existing import-order finding; language diagnostics retain an unrelated
  optional-value comparison error. This refinement changed no game source, YAML, compiler
  profile, or ROM banking authority and did not run an additional ROM build.

- **Seed recovery and workflow cleanup**: Four parallel read-only deep dives compared this
  session with REA provider/evidence contracts, Snowboard Kids bootstrap/workspace practices,
  native Oh My Pi task delivery, and the Ghidra/Eilam books' interface and type-recovery methods.
  Kept the existing tools, native tasks, measurement/source pairs, and sole ROM banking gate.
- **Tooling**: Assembly slicing now preserves splat `jlabel` switch destinations. Failed body
  recovery retains diagnostics and exits nonzero without creating an empty `base.c` or replacing
  an existing candidate. Removed the blank-function fallback and stale generated display-list hints.
  Replaced exact-formatting characterizations with control-flow/boundary regression coverage;
  added failed-recovery candidate-preservation coverage.
- **Guidance**: One source-recovery gate now covers intact control flow, whole-callee/caller
  signature evidence, access widths/strides, and F3DEX2 packet-word/cursor checks before score
  interpretation. The skill, tool catalog, hazard section, and worker handoffs link to it.
  Removed copied task-delivery mechanics, the arbitrary analysis-angle cap, historical wall-class
  summaries/case-law routing, and duplicate carry/naming guidance from active workflow references.
  Frozen hazard headings, main-first scope, shared-resource ownership, and compiler profiles remain.
- **Proof**: Reproduced the real renderer's missing-label failure before the fix; its intact
  extracted slice now generates an m2c body. A temporary real seed CLI switch executed correctly
  for inputs 0, 1, 2, and -1. Real failed-recovery CLI runs retained diagnostics, preserved an
  existing candidate, and created no empty candidate. Smoke workspaces were removed automatically;
  the carried renderer was untouched. Tooling suite: 150 passed, two skipped. Changed Python files
  pass Ruff; all 115 frozen hazard anchors resolve. Checked 103 concept frontmatters, four
  recursive indexes, native skill metadata, and 604 local links/fragments with no findings.
  No game source/YAML edits, ROM build, live Ghidra mutation, or new bank is claimed for this refinement.

- **Workflow refinement**: Five parallel read-only deep dives compared the session's evidence with
  REA readiness/tool-design contracts, Snowboard Kids selection/workspaces, native Oh My Pi task
  delivery, Ghidra Book chapters 6/19/20, Eilam's decompilation discussion, and the local
  refactoring/legacy-code notes. Kept native delegation and existing measurement artifacts;
  added no orchestration layer, provider stack, or second evidence ledger.
- **Discovery**: Existing loose-stub and file-closure modes now enumerate YAML code-bank C hosts,
  including linked libraries and nested source paths, while excluding comments and unconfigured
  sources. Campaign selection reuses that scanner. The real main shortlist exposes eight
  previously hidden eligible library routines; all nine remaining bundled-synth stubs are visible.
- **Execution**: Permuter compiler/assembler/objdump callbacks reuse the synchronized project
  interpreter instead of resolving dependencies per candidate. The run wrapper no longer forces
  monotonic `--best-only` search or advertises a thread count that caller arguments can override.
  Profile-source flags, compiler errors, bounded execution, and parent resource ownership remain.
- **Guidance and removal**: Consolidated search eligibility in the loop reference: faithful
  integration-eligible source precedes score-based suitability. Made tiny-leaf seeding optional,
  carry reopening contingent on new leverage, and reference-root coverage explicit in worker returns.
  Removed duplicated native-task schema prose, obsolete manual permuter settings/toolchain recipes,
  and retired inlined-body import workarounds. Preserved compiler evidence and hazard headings;
  repaired the existing broken family-of navigation link without changing its heading.
- **Proof**: Exercised real loose-stub, file-closure, campaign, and bounded permuter commands.
  Callback objects were byte-identical before/after; a real search reached zero without repeated
  resolver chatter, and regression coverage exercised profile flags and compiler-failure diagnostics.
  Tooling suite: 150 passed, two skipped. Shell syntax and all 115 frozen hazard anchors passed.
  Checked 102 concept frontmatters, native skill metadata, recursive indexes, and 621 local
  links/fragments; the unchanged historical `BACKLOG.md` self-link remains the one navigation finding.
  Language-server diagnostics retain unrelated optional-value errors in untouched ranker functions.
  The full ROM verifier passed with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  Removed smoke-only inputs/objects/workspace; preserved the earlier four banked functions and carries.

- **Main-first milestone**: Made completion of the YAML `main` bank, including linked libraries,
  a gate before overlay selection, queue execution, dispatch, or integration. The native skill
  owns the completion criteria; root instructions, campaign guidance, fan-out, and discovery
  commands now carry that scope. Empty fresh queues and carried/coupled blockers do not unlock
  overlays; intentional assembly and raw data remain outside C-decompilation backlog.
- **Proof**: Exercised main-range ranking, carry-inclusive and library-scoped ranking, all-class
  main-path stub discovery, file closure, and read-only campaign preparation. The campaign emitted
  one main-bank target and no overlay targets. Checked 102 concept frontmatters, recursive indexes,
  source paths, native skill metadata, and 616 local links/exact fragments; the only reported link
  issue was the unchanged `BACKLOG.md` self-link to `#multi-register-allocno-permutation`.
  All 115 frozen hazard anchors passed. No ROM source or tooling behavior changed; no bank claimed.

- **Structure**: Refactored discovery against OKF v0.2: removed concept frontmatter from the nested
  workflow index, grouped reconstruction notes by symbol/address, and made this update log a flat
  date-grouped list. The repository entry guide now routes to canonical setup and workflow concepts.
- **Discovery**: Split [reference discovery and campaign queues](campaigns.md) from the command
  catalog. Clarified which surfaces own procedure, technical decisions, operational commands,
  matching diagnosis, and historical evidence; reused concept descriptions in discovery indexes.
- **Alignment**: Checked build, dependency, container, campaign, deadline, ranker, and Ghidra guidance
  against implementation. Added provenance, missing hazard routes, and explicit WIP filename
  semantics; removed redundant setup/upgrade guidance and non-Markdown role wrappers.
- **Preservation**: Kept historical reconstruction bodies and paths, native skill/command contracts,
  and all frozen hazard headings. Corrected a broken family-index fragment without renaming its target.
- **Proof**: Parsed all 102 bundle concepts, checked recursive index coverage, reserved-file structure,
  local links/exact fragments and provenance paths. The hazard checker passed all 115 frozen anchors.
  Exercised CMake preset listing, verifier help, real C-reference discovery and read-only queue
  preparation, plus a bounded child command. No ROM build, live Ghidra, container, or hosted CI run
  was needed or claimed for this documentation-only refactor.

- **Milestone**: Reference-driven native campaigns and bounded experiments.

- **Research**: Parallel deep dives examined Snowboard Kids/Nigel's reference ranking, sharding,
  deadlines and m2c prepass; REA's artifact/evidence contracts; The Ghidra Book and Eilam's
  decompilation material; and legacy-code/testing/CMake notes. Adapted the useful mechanisms
  without adding a provider stack, external agent runner, destructive resets or concurrent builds.
- **Discovery**: Added fresh advisory C-reference search across explicitly supplied same-ROM
  checkouts. Results carry source/assembly locations and hashes, disclose unavailable coverage,
  reject ambiguous labels, and keep missing similarity distinct from a measured zero.
- **Campaigns**: Added bounded native-session queues, optional similarity ordering and stable
  ROM/bank/offset sharding. An opt-in serial m2c prepass preserves existing candidates, skips
  coupled/intrinsic/library targets, and reports provisional results without integrating source.
- **Execution**: Added inherited absolute deadlines and process-group cleanup. The permuter
  wrapper now defaults to a configurable 600-second limit and reserves 120 seconds when a
  candidate deadline is inherited. Expiry retains artifacts and never establishes a source wall.
- **Evidence**: Isolated measurements retain immutable source/score pairs with target, effective
  profile and compiler identities. Changed candidates invalidate attribution. Compiler failures
  now retain actual diagnostics; existing compiler callers keep their default output behavior.
  Header/context inputs remain explicitly unarchived and require fresh measurement on resume.
- **Proof**: Tooling suite: 148 passed, two skipped. Reference discovery found 998 C references.
  Native queue and shard commands ran; a real m2c prepass retained a compiler failure, and a
  second invocation preserved its candidate hash. A real `rand` compile at the libkmc profile
  scored zero across 13 rows; changing its delay-slot mask produced score five and a separate
  archived source/measurement. A real permuter run stopped at three seconds with exit 124;
  an expired deadline refused launch. The sole ROM verifier passed with SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`.
- **Review**: Fixed four boundary gaps found by independent reviewers: explicit-target pool tags,
  ambiguous reference C/bank association, nested-guard cancellation, and background descendants
  after a successful command. Regressions exercise actual pool assembly and process lifetimes;
  cleanup includes observed descendant sessions before bounded hard escalation/reaping.
- **Documentation**: Updated the native skill, command catalog, execution artifact contract and
  fan-out guidance. Validated 101 concept metadata records, 60 local links/fragments across six
  active surfaces, and all 115 frozen hazard anchors. New workflow tools and the loop passed
  Ruff checks; `rom_build.py` retains unrelated existing I001/F401/E731 findings.
- **Scope**: No matching C, header, YAML, protected symbol inputs, or vendor edits. No new
  tools or ROM extraction from the supplied archive were needed. Calibration scaffolds were
  removed; the real unbanked prepass candidate remains isolated under `nonmatchings/`.

- **Milestone**: Streamlined agent decompilation workflow.

- **Research**: Parallel deep dives compared Oh My Pi's native task/skill/artifact lifecycle,
  REA's bounded evidence contracts, The Ghidra Book chapters 8/13/19/20, Eilam's
  Decompilation chapter, and legacy-code/refactoring notes. Kept native delegation and
  the existing bank-aware database; added no provider stack, project agents, or orchestration wrappers.
- **Guidance**: The skill owns procedure; `workflow/loop.md` now owns technical decisions and
  oracle limits (378 to 155 lines). Removed repeated sprint narratives and arbitrary failure
  counts while preserving hazard anchors, carries, compiler-profile fidelity, and serialized
  resource ownership. Corrected integration-before-object-comparison ordering and made
  worker proposals explicitly unmeasured until the parent compiles them.
- **Tooling**: Loose-stub ranking now applies `-n` after filtering and supports JSON records.
  Replaced repeated historical advisories with current classification guidance. Reference
  assembly/pseudocode remain separate artifacts rather than duplicate seed blocks; removed
  the obsolete `asm_inlined` result field. m2c failures retain stdout as well as stderr diagnostics.
- **Compatibility**: Actual loop smoke exposed a missing asm-differ `ref_file`, omitted raw
  opcode columns, and mismatch detection using nonexistent formatter cell keys. Updated the
  comparison contract and removed unnecessary table/formatter serialization. Regression tests
  exercise real immediate/register changes, deleted delay slots, filtered limits, and context errors.
- **Proof**: Tooling: 129 passed, two skipped. Real `rand` seed compiled at its libkmc profile
  after normal extern refinement: score zero, 13 matching rows; changing its delay-slot mask
  produced score 5 and the exact operand diagnostic. A compact seed with separate reference
  artifacts compiled to return 7 without changing those artifacts. `--loose-stubs main --all -n 3`
  shrank from 259 candidates/27,264 bytes to three candidates/561 bytes; JSON returned the same
  shortlist. Checked 106 documentation files' metadata, 119 local links/fragments over 15 active
  surfaces, two index scopes, native skill metadata, and all 115 frozen hazard anchors.
  `tools/verify-rom.sh` passed: `e2c4e7a905b29529b49a1619a401fe699224829b`.
- **LSP**: Native rediscovery loaded the already-installed BasedPyright and Ruff from `.venv`;
  references and diagnostics ran without adding project configuration or dependencies.
- **Scope**: No matching C, headers, YAML, pinned vendors, or historical reconstruction evidence
  changed. Live Ghidra export and permuter execution were unchanged and were not exercised here.

- **Milestone**: Persistent bank-aware Ghidra database.

- **Database**: Created private `private/ghidra/mariogolf64.gpr` with 17 programs: resident
  entry/main plus one resident-backed program per code overlay. YAML supplies byte maps and BSS;
  extracted assembly supplies function boundaries and ROM provenance. Source-profile preprocessing
  imports SDK/game types and signatures; ambiguous legacy overlay names remain unassigned.
- **Tooling**: Added reproducible init/refresh, serialized headless lifecycle, private archives,
  proposal-only name sync, live transitive C context, and a curated exact-program MCP proxy.
  `seed_c.py --ghidra` resolves the selected assembly's ROM/VRAM before feeding context to m2c.
  CMake exposes separate analysis targets; the local MCP configuration launches the project bridge.
- **Guidance**: Added [the canonical Ghidra guide](ghidra.md), replaced obsolete external-workspace
  program/script assumptions, and documented source conflicts, FP ABI limits, and conservative
  symbol ownership. Generic N64/Zelda loaders do not replace YAML's overlay layout; no extension
  was needed for the CPU workflow.
- **Proof**: Full 17-program refresh completed with zero source-import diagnostics. Real MCP stdio
  read distinct ROM bytes at shared VRAM in overlays 3/8; bank-specific annotation edits survived
  save/restart. Exported structure/callback/string contexts passed host C syntax checks; an isolated
  live-context seed produced an m2c body with inferred register arguments. Native `.gar` archival
  and CMake init/start/status/name-proposal targets ran. Tooling: 126 passed, two skipped; affected
  Ruff checks passed. `tools/verify-rom.sh` reported SHA-1 match
  `e2c4e7a905b29529b49a1619a401fe699224829b`.

- **Milestone**: Repository and native-agent cleanup.

- **Removed**: Obsolete local `tools/cc` binaries, legacy `venv`, and empty `mk/` directory
  (about 66 MiB); the unused old-path `mariogolf64.md5` and `tools/.gitignore`.
  Kept generated extraction artifacts, candidate experiments, pinned vendors, and technical history.
- **Workflow**: Folded selection/review checkpoints into the native `mg64-decomp` skill and removed
  `workflow/gates.md`. Replaced `fanout-prompt.md` with the native task dispatch contract in
  `workflow/fan-out.md`; workers analyze, and the parent serializes edits, compiles, MCP, and banking.
  Retained measured fan-out evidence and the historical workflow router.
- **Tooling**: Removed the duplicate CMake `test-tools` runner; use `ctest --preset tooling` or
  `uv run --locked pytest tests/tooling`. Removed four whole-backlog ranker snapshots and four
  seed-layout snapshots with their unused fixtures. Made both coddog trap-rescan cases independent
  of external SDK source state; preserved data/static-hazard assertions for primary and tail hits.
- **Guidance**: Corrected stale Make/profile links, extraction-staging instructions, and harness
  edit/dispatch syntax. Small asm-first candidates now use isolation rather than editing matching
  source before measurement. Historical records retain their original file/tool names.
- **Proof**: Tooling: 118 passed, two skipped; CTest tooling passed. `tools/verify-rom.sh` reported
  SHA-1 match `e2c4e7a905b29529b49a1619a401fe699224829b`. Exercised ranking and file-closure CLIs,
  checked all 115 frozen hazard anchors, and validated 105 documentation files' metadata and
  104 active local links across 12 workflow surfaces. The installed harness loaded the skill and
  dispatched `/decomp`; Python Eval ran with the approved session-only `.venv` interpreter.

- **Milestone**: uv-managed Python tooling.

- **Dependencies**: Consolidated runtime, test, and analysis dependencies in `pyproject.toml`;
  resolved current stable compatible releases into `uv.lock`. Removed requirements files and the
  obsolete vendored Python wheelhouse/source distributions. Initial installation now uses PyPI.
- **Environment**: Required uv 0.13.1+ and one root `.venv`; migrated CMake, shell wrappers, CI,
  container bootstrap, active command examples, and analysis configuration away from build-tree
  environments. Ordinary setup uses `uv sync --locked`; upgrades use `uv lock --upgrade`.
- **Tools**: Updated Splat to 0.50.1, spimdisasm to 1.42.4, rabbitizer to 1.16.2, Ruff to 0.17.0,
  basedpyright to 1.40.2, and coverage to 7.16.2. Imported current asm-differ and m2c upstream
  revisions; permuter was already current and retains its local import adaptations.
- **Compatibility**: Kept target compiler profiles and native versions unchanged. Adapted extraction
  to preserve encoded absolute J targets when current disassembly emits undefined local labels;
  retained colon and macro-defined local labels and their relocations.
- **CMake**: Applied Professional CMake's lifecycle and dependency guidance: uv discovery with
  `find_program`, locked configure-time manifest generation, explicit target ordering, and a
  content-stable lockfile byproduct so unchanged environments do not force extraction.
- **Proof**: The full-ROM verifier passed with SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
  Nine targeted compiler/permuter/jump cases passed. The final full tooling run reported 120 passed,
  two skipped, and the six documented ranker/coddog failures; no goldens were regenerated.
- **Limits**: Hosted CI and the private Docker image have not been executed here. Rebuild the image
  before publishing this workflow's container changes.

- **Milestone**: Monorepo and CMake modernization.

- **Ownership**: Imported asm-differ, decomp-permuter, and m2c as ordinary Git source; removed
  submodule configuration. Added pinned compiler/binutils source archives and the complete
  hash-locked offline Python wheelhouse with corresponding source distributions and license notices.
- **Build**: Replaced the repository Make interface with CMake 3.30+ presets, source-built tools,
  binary-tree extraction, output-tracked objects, include depfiles, and authoritative per-source
  compiler profiles. Preserved the KMC/modern-assembler split and exact linker object order.
- **Workflow**: Consolidated prompt entrypoints into the native Oh My Pi `mg64-decomp` skill and
  `/decomp` command. Retired duplicated Claude/Codex prompts, prompt budgets/linting, legacy Make
  fragments, and duplicate permuter profiles; retained reconstruction and sprint history.
- **Integration**: Migrated ranker, diff, isolated compilation, permuter, optional reference builds,
  CI, and container instructions to the configured binary-tree tools. Permuter imports inherit the
  parent TU's profile and recognize CMake roots and GNU assembly section flags.
- **ROM proof**: A source-only export, without old assembly/assets, tool binaries, submodules, or
  Python environments, built the US retail ROM using CMake 3.30.9. The verifier reported SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`; the working tree also passed with CMake 4.2.3.
- **Behavior proof**: Header invalidation rebuilt dependent objects and retained the matching ROM.
  A deliberate compiler error rejected an older matching ROM before hashing; a nonmatching
  configuration was refused. Unchanged extraction preserved generated-file timestamps and avoided
  object rebuilds. Real `rand` isolated compilation and permuter scoring both measured score zero.
- **Checks**: The ROM CTest passed; 14 targeted compiler-profile, assembly-parser, decomp-loop,
  and hazard-anchor tests passed; the tooling undefined-name check passed. Exercised ranking,
  profile probing, allocno reporting, and native Oh My Pi command/skill discovery.
- **Limits**: Docker and hosted CI configuration were updated but not executed. The private CI image
  must be rebuilt. No full-suite-green claim replaces the historical tooling limitations below.

- **Milestone**: Earlier documentation restructuring.

- **Structure**: Established the OKF v0.2 [bundle index](index.md), workflow and reconstruction
  indexes, typed concept metadata, and explicit separation of current guidance from historical evidence.
- **Discovery**: Separated [candidate/build hazards](hazard-index.md) from
  [matching residuals](hazard-symptoms.md), made playbook routes clickable, and repaired the
  default-return-variable anchor without renaming the playbook headings.
- **Guidance**: Added canonical [local build](build.md), [tooling](tooling.md),
  [history](history.md), and [documentation maintenance](documentation.md) references.
- **Alignment**: Corrected the segment-versus-library filter description, made byte-exact matching
  take priority over generic C style, and consolidated duplicated local setup out of the container guide.
- **Preservation**: Retained historical entry bodies, reconstruction evidence, harness invocation
  fields, and the shared safety-invariant block.
- **Tooling integration**: Excluded OKF's reserved `index.md` and `log.md` files from executable
  prompt selection; instruction checks and their byte budgets remain enforced.
- **Validation**: Parsed 102 bundle concepts and checked directory coverage and local navigation;
  prompt and hazard-anchor contracts passed. Exercised Make help, segment ranking, loose-stub
  discovery, and rejection of a segment passed to `--lib`.
- **Guard review**: Token-count losses arise from moving residual rows, replacing code-span
  citations with links, consolidating duplicated routing/naming/oracle text, retiring generic C++
  advice, and updating the formatter version. The guard is documented as a review signal for these
  structural changes, not proof that file-local token counts must remain unchanged.
- **Suite limitation**: The full tooling suite reports six ranker/coddog snapshot or hazard-expectation
  failures; no goldens were regenerated or ranker implementation changed in this documentation work.
