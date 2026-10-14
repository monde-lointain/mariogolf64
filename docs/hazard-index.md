---
type: Navigation Reference
title: Hazard flag lookup
description: Routes ranker flags and assembly symptoms to the applicable hazard playbook.
sources:
  - resource: ../tools/pick_target_hazards.py
    title: Ranker hazard kinds and rendering
  - resource: hazards.md
    title: Detailed playbooks and historical evidence
---

# Hazard index (flag -> playbook)

Maps each `tools/pick_target.py` hazard flag, and each asm symptom, to the one `docs/hazards.md`
section that handles it. Use this table to route from ranker output; for instruction, control-flow,
register-allocation, scheduling, or data-layout residuals, use the dedicated [symptom lookup](hazard-symptoms.md#symptom-to-section).
The detail behind each flag lives in that playbook section, not here. Consulted from both candidate
pricing and execution.

## Flag to section

When `pick_target.py` flags a hazard, read the matching `docs/hazards.md` section. For a residual
without a ranker flag, use the symptom lookup above. Playbooks preserve historical evidence; where
old command transcripts conflict, the current build contract at the top of `hazards.md` controls.

| pick_target.py flag / symptom | docs/hazards.md section |
| ----------------------------- | ----------------------- |
| upstream column is a lib | [upstream-mirror-pattern](hazards.md#upstream-mirror-pattern) |
| `single-file-pack:<n>fn[…]` | [upstream-mirror-pattern](hazards.md#upstream-mirror-pattern) |
| `upstream-fncount-mismatch:<m>vs<n>` | [upstream-mirror-pattern](hazards.md#upstream-mirror-pattern) |
| `one-tu` | [upstream-mirror-pattern](hazards.md#upstream-mirror-pattern) / [non16align](hazards.md#non16align) |
| `refs-unplaced:<g>@0x…` | [recover-extern-refs-unplaced](hazards.md#recover-extern-refs-unplaced) |
| `calls-unplaced:<fn>@0x…` | [calls-unplaced-function-callee-dual](hazards.md#calls-unplaced-function-callee-dual) |
| macro-hidden extern | [macro-hidden-recover-extern](hazards.md#macro-hidden-recover-extern) |
| `jal-count-mismatch:<C>vs<asm>` | [near-verbatim-mirror-jal-count-mismatch](hazards.md#near-verbatim-mirror-jal-count-mismatch) |
| `block-reorder-sibling:<file>` | [near-verbatim-mirror-jal-count-mismatch](hazards.md#near-verbatim-mirror-jal-count-mismatch) |
| `file-static` | [file-static-bss-layout-conflict](hazards.md#file-static-bss-layout-conflict) |
| `drop-static-mirror:<n>bss` | [file-static-bss-layout-conflict](hazards.md#file-static-bss-layout-conflict) |
| `defines-data:<g>` / `data-static:<addr>` | [defines-data](hazards.md#defines-data) |
| `data-carve:<names>` | [defines-data](hazards.md#defines-data) |
| `twin-of:<file>` | [defines-data](hazards.md#defines-data) / [rodata-sibling-yaml-pattern](hazards.md#rodata-sibling-yaml-pattern) |
| whole-region `.data`/`.rodata` sweep | [data-rodata-carve](hazards.md#data-rodata-carve) |
| `needs-header:<inc>` | [needs-header](hazards.md#needs-header) |
| `stale-header:os_version.h(<V>)` | [stale-vendored-header](hazards.md#stale-vendored-header) |
| `needs-define:<def>` | [needs-define](hazards.md#needs-define) |
| clean mirror SHA-miss, exact N×8B / fn shorter than its asm | [needs-define](hazards.md#needs-define) |
| clean mirror SHA-miss, single `li`/`addiu` immediate byte (not a reloc'd hi/lo) | [needs-define](hazards.md#needs-define) |
| `header-renames-symbol:<fn>@<hdr>` | [header-renames-symbol](hazards.md#header-renames-symbol-vendored-header-rewrites-the-curated-symbol) |
| `wrong-ghidra-name:<ghidra>-><correct>@<hdr>` | [wrong-ghidra-name-override](hazards.md#wrong-ghidra-name-override-correct-a-mislabeled-symbol-without-sync-names) |
| mirror parse error / undefined ref on a helper macro | [vendored-header-incomplete](hazards.md#vendored-header-incomplete-a-reconstructed-header-is-already-vendored-yet-missing-a-macro) |
| `rodata-straddle:<addr>` | [decomposed-one-tu rodata alignment split](hazards.md#decomposed-one-tu-rodata-alignment-split-a-counter-case-to-the-8-point-decompose-gate) |
| `jal-free` | [upstream-mirror-pattern](hazards.md#upstream-mirror-pattern) |
| `remaining:<n>` | [open-band fast-path](hazards.md#open-band-fast-path) |
| `body-divergence-suspect:<file>@<pct>` / `coddog-source-banked:<file>` | [coddog cross-ref](hazards.md#coddog-cross-ref) |
| parse-error cascade at macro-def lines / `stray '\'` after vendoring an SDK header | [crlf-vendored-header](hazards.md#crlf-vendored-header-a-copied-sdk-header-breaks-kmc-cpps--continuations) |
| `pack:<n>fn[…]` | [multi-function-segment-splitting-pack](hazards.md#multi-function-segment-splitting-pack) |
| `c-combined:<n>file[…]` | [multi-function-segment-splitting-pack](hazards.md#multi-function-segment-splitting-pack) |
| `unattrib-leaf:<addr>` | [multi-function-segment-splitting-pack](hazards.md#multi-function-segment-splitting-pack) |
| `non16align` | [non16align](hazards.md#non16align) |
| `trailing-pad:<n>B@<align>` | [trailing-alignment-pad-after-a-c-mirror](hazards.md#trailing-alignment-pad-after-a-c-mirror) |
| `intrinsic-likely:<tu>.s` (and `(kmc-as)`, `(has-rodata:<sym>)`, `(asm-mirror-jtbl:<head>)`) | [asm-mirror-vendoring](hazards.md#asm-mirror-vendoring) |
| `intrinsic-likely:cp0-asm(identify-TU)` | [asm-mirror-vendoring](hazards.md#asm-mirror-vendoring) |
| `combined-subseg:<n>tu[…]` | [asm-mirror-vendoring](hazards.md#asm-mirror-vendoring) |
| `intrinsic-likely` (bare) / `maybe-upstream:…` | [intrinsic-likely--maybe-upstream-signature-hints](hazards.md#intrinsic-likely--maybe-upstream-signature-hints) |
| `coddog-mirror:<file>@<pct>` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `coddog-twin:<matched>!=<member-src>` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `coddog-fncount-mismatch:<m>vs<n>` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `c-combined-undercount:<m>vs<n>` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `coddog-structural:<file>@<pct>` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `coddog-partial:<m>of<n>fn` | [coddog-cross-ref](hazards.md#coddog-cross-ref) |
| `static-name-collision:<name>@<addr>` | [static-name-collision](hazards.md#static-name-collision-an-upstream-file-static-reuses-a-placed-global-name) |
| official static name shared across two instances / splat `Duplicate symbol detected` | [overlapping-symbols--allow_duplicated](hazards.md#overlapping-symbols--allow_duplicated-two-instances-share-one-official-static-name) |
| SUPPORT_NAUDIO libmus mirror `alInit`→dead `n_al*` / bundled-synth `fncount-mismatch` | [libmus-bundled-n_audio-duplicate](hazards.md#libmus-bundled-n_audio-duplicate-a-support_naudio-libmus-archive-links-its-own-n_audio-synth-copy) |
| `game-region-mirror:0x<vram>` | [game-region-mirror--o2-profile](hazards.md#game-region-mirror--o2-profile) |
| `game-embedded:0x<vram>` | [game-region-mirror--o2-profile](hazards.md#game-region-mirror--o2-profile) |
| clean asm-first seed, full-make SHA-miss, build .o shows fp-kept + no-CSE + arg-spill | [Boot/SDK -O0 profile](hazards.md#-o0-bootsdk-glue-file-profile-a-per-file-opt-level-exception) |
| build SHA-miss, suspect compile flags (opt/-g/-fdelayed-branch) not the C | [profile-probe](hazards.md#profile-probe-pinning-the-flags-of-a-sha-missing-build) |
| public-API rename "resists" (`__*` symbol), vendored header macro may be inverted vs ultralib | [vendored-header-inversion](hazards.md#vendored-header-inversion-a-curated-libultra-header-diverges-from-the-ultralib-pin) |
| libultra leaf, bare std header | [per-library-standard-c-header-isolation](hazards.md#per-library-standard-c-header-isolation) |
| match locks ~0.9 on a lib target | [compile-profiles-libkmc--o-libultra--o3](hazards.md#compile-profiles-libkmc--o-libultra--o3) |
| compiler rodata wrong offset / `rodata-literal:<addr>` | [rodata-sibling-yaml-pattern](hazards.md#rodata-sibling-yaml-pattern) |
| `rodata-jtbl:<addr>` | [rodata-sibling-yaml-pattern](hazards.md#rodata-sibling-yaml-pattern) |
| `…;owner-per-member` on a `rodata-jtbl`/`rodata-literal` | [rodata-sibling-yaml-pattern](hazards.md#rodata-sibling-yaml-pattern) |
| carve done, next `.double` pool 4 bytes off in either direction | [rodata-sibling-yaml-pattern](hazards.md#rodata-sibling-yaml-pattern) |
| N block-move loops too many, frame over by exactly `sizeof(T)` | [named-aggregate-local-extra-block-move](hazards.md#named-aggregate-local-extra-block-move) |
| big stack frame of `sb` triples + `pad` zero stores + `lwl`/`swl` pairs | [grep-gbih-before-modelling-a-large-stack-struct](hazards.md#grep-gbih-before-modelling-a-large-stack-struct) |
| allocator leaf priced on "N globals to type" | [read-the-freeing-twin-before-pricing-an-allocator-leaf](hazards.md#read-the-freeing-twin-before-pricing-an-allocator-leaf) |
| MMIO fn, flat score, empty top_mismatches | [io_writeio_read-isolation-artifact](hazards.md#io_writeio_read-isolation-artifact) |
| warm band, no hazard | [open-band-fast-path](hazards.md#open-band-fast-path) |
| `undefined reference` after mid-sprint sync-names | [make-sync-names-eviction-recovery](hazards.md#make-sync-names-eviction-recovery) |
| `caller-evict:<func_vram>@<file>` | [caller-evict](hazards.md#caller-evict) |
| `carried-wall:<fn>[…][;characterization-only]` | [base-register-vs-displacement](hazards.md#base-register-vs-displacement-full-symbolbase-materialized-vs-lo-in-displacement) |
| loop cannot find label / reloc-name mismatch | [stale-top-level-asm-label-sync](hazards.md#stale-top-level-asm-label-sync) |
| decomp_loop bogus near-match (empty `base_text`, ref names the parent seg) just after a subseg split | [stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split](hazards.md#stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split) |
| clean mirror SHA-miss, one field's high word | [mirror-cast-divergence-sign--vs-zero-extend](hazards.md#mirror-cast-divergence-sign--vs-zero-extend) |
| clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi | [char-signedness](hazards.md#char-signedness-libultra-is--fsigned-char) |
| clean mirror SHA-miss, extra `jal __assert` / bare `assert()` / `bare-assert:<n>` | [assert-strip](hazards.md#assert-strip-bare-upstream-assert-vs-ndebug) |

## Matching residuals

For instruction-count, control-flow, register-allocation, scheduling, display-list, and
data-layout mismatches, use the [matching residual lookup](hazard-symptoms.md#symptom-to-section).
For a topic-first view, use the [playbook family index](hazards.md#playbook-index).
