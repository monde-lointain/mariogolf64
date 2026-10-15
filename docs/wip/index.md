# Read evidence without changing candidate discovery

- [How to interpret reconstruction notes](../history.md#function-evidence) — Recorded scores and wall/banked labels describe their measured base, not a fresh verification of today's source; filenames also affect candidate classification.

# Named functions (C–F)

- [collect_keyframe_events_at near-match (S260) — 54/54, ONE BRANCH-OFFSET BIT](collect_keyframe_events_at.near-match.md) — Records the S260 goto-loop reconstruction's exact instruction count and remaining branch-offset bit after retiring the strength-reduction diagnosis.
- [draw_terrain_aim_grid — near-match (S306)](draw_terrain_aim_grid.near-match.md) — Tracks the 1,349-instruction terrain-grid display-list reconstruction, with exact instruction count and structure but a 0x10 frame-size residual.
- [emit_sky_dome_dl — S297 carry (299/299 count, register permutation; was 297/299, 272/299 at S296)](emit_sky_dome_dl.near-match.md) — Documents the exact-count sky-dome emitter carry whose remaining register permutation is traced to an unresolved allocator gate.
- [find_keyframe_offset_by_tag near-match (base-register caching + regalloc perm)](find_keyframe_offset_by_tag.near-match.md) — Records the decoded keyframe-by-tag search and its base-register-caching and callee-saved register-allocation residual.

# Functions at 0x8002–0x8004

- [func_80029C00 — FCSR mask intrinsic (TERMINAL, hasm candidate)](func_80029C00.near-match.md) — Explains why the 13-instruction FCSR-mask leaf is not independently bankable from C and remains a characterized carry.
- [func_8002BE78 - GCC nested function of draw_ground_shadow_decals (carry, not a wall)](func_8002BE78.near-match.md) — Records a GCC nested child that reads and updates its parent's stack-local state and must be decompiled inside that parent.
- [func_8002CDA8 reconstruction scoping (S303, re-opened S304)](func_8002CDA8.scoping.md) — Scopes the reconstructed 838-instruction function's exact-count register-allocation residual and its carried-wall detector classification.
- [func_8002DAC0 - GCC nested function of render_frame (carry, not a wall)](func_8002DAC0.near-match.md) — Records a GCC nested render-frame child that reads its parent's static chain and must be banked in the parent translation unit.
- [func_800318A8 (src/main/func_8002A640.c) — raw-DL glistp++ store-giv CARRY class](func_800318A8.near-match.md) — Characterizes the frame-render display-list builder's raw-command glistp spill pattern and why it remains a high-risk carry.
- [func_8003DE80 (0x8003DE80) — near-match carry (S233)](func_8003DE80.near-match.md) — Records a terrain-grid neighborhood score blocked by the same constant-15 scheduler/register-allocation coin as `get_surface_type`.
- [func_8003E004 — near-match (S204 -> S235 -> S318, CARRIED)](func_8003E004.near-match.md) — Records the wind-indicator vertex generator's S318 exact-count carry and remaining preheader, angle-chain, and register-order residuals.
- [func_800415C4 near-match (get_tile_attribute.c, S218)](func_800415C4.near-match.md) — Records the S281 byte-exact bank as set_lod_grid_vertex, superseding the S218 register-allocation diagnosis.
- [func_80041878 near-match (get_tile_attribute.c, S218)](func_80041878.near-match.md) — Records the S281 byte-exact bank as set_lod_tile_attribute, superseding the S218 register-allocation diagnosis.
- [func_80042318 (src/main/get_tile_attribute.c) — reorg delay-slot coin near-match](func_80042318.near-match.md) — Records the menu/HUD input handler's 171/172-instruction carry and its two remaining reorg delay-slot decisions.
- [func_80046604 near-match (S262)](func_80046604.near-match.md) — Records the club/terrain sound dispatcher’s S262 branch-scheduling residual in the kind-1/2 arm.
- [func_80047CAC — pure-FP interpolation stepper (FP scheduler coin, near-match)](func_80047CAC.near-match.md) — Documents the three-global FP interpolation stepper's scheduler coin and one-ULP constant residual.
- [func_80047DBC — near-match carry (terminal sched.c const-load-vs-fabs coin)](func_80047DBC.near-match.md) — Records the three-axis FP snap-to-target lerp whose body matches with scheduling disabled but has a real-profile nop residual.
- [func_800484F8 — start-BGM-for-game-state (near-match, S263)](func_800484F8.near-match.md) — Tracks the game-state BGM initializer's 99/102 near-match, including the default-path branch-likely and shared-value register residuals.
- [func_8004C958 — recursive diamond-square cloud generator (near-match, S263)](func_8004C958.near-match.md) — Records the recursive cloud generator's S263 spill-pressure residual: faithful C emits fewer instructions than the ROM.
- [func_8004CDA0 — cloud-buffer blend driver (near-match, S263)](func_8004CDA0.near-match.md) — Records the cloud-buffer blend driver's S263 register-allocation residual and collapsed register-preserving copies.
- [func_8004DC44 WIP — dead-frame reload-artifact regalloc wall (S173 compiler-source dive)](func_8004DC44.wip.md) — Preserves the S173 dead-frame reload-artifact investigation, improved source seed, and remaining regalloc wall evidence.

# Functions at 0x8005

- [func_8005244C — near-match (S316, CARRIED at 93/94)](func_8005244C.near-match.md) — Records the S316 comparison carry whose sole instruction-count residual is an entry-guard delay slot filled instead of left empty.
- [func_80056060 — CARRIED (39/39, 20 diff rows; callee-saved priority inversion)](func_80056060.near-match.md) — Documents the 39/39 near-match with 20 differing rows attributed to a callee-saved register-priority inversion.
- [func_800564F0 + func_80055738 — PROVEN reorg delay-slot WALL (S213)](func_800564F0-func_80055738.delay-slot-wall.md) — Records the S213 proof that the paired functions' remaining mismatch is a reorg delay-slot wall.
- [func_80059BA0 — near-match (CARRIED, terminal sched2 potential-hazard coin)](func_80059BA0.near-match.md) — Documents the carried scheduler-II potential-hazard residual and the evidence against treating it as a fresh crack.
- [func_8005B0B4 — near-match (CARRIED, S263 value-select-branch-likely)](func_8005B0B4.near-match.md) — Records the S263 carried value-select versus branch-likely residual for func_8005B0B4.
- [func_8005C038 -- record-table insert (S289 carry, 268/264)](func_8005C038.near-match.md) — Re-derives the five-argument record-table insert and records its S289 268/264-instruction carry.
- [func_8005CA48 -- near-match, carried (S288)](func_8005CA48.near-match.md) — Records the S288 active-player reset/profile loader reconstruction and its two-instruction excess.
- [func_8005CF78 — CARRIED (large jtbl-dispatch 3-level loop; deferred, not a proven wall)](func_8005CF78.near-match.md) — Records a carried three-level jump-table loop whose deferred residual is not classified as a proven wall.
- [func_8005D0D8 — banked via selective loop weighting](func_8005D0D8.near-match.md) — Explains how selective loop weighting resolved the function and records its verified bank and allocation evidence.
- [func_8005D334 near-match (S210 compiler-source dive)](func_8005D334.near-match.md) — Records the four-slot reset loop's S210 register-pressure residual after structural matching.
- [func_8005DF54 — non-ABI $s2-arg wall (TERMINAL)](func_8005DF54.near-match.md) — Documents the terminal non-ABI `$s2` argument mismatch that prevents a faithful standalone C match.
- [func_8005DFE8 — SRAM load/verify, CARRIED (S292 re-open; 102/102, 36 diff rows)](func_8005DFE8.near-match.md) — Records the S292-reopened SRAM load/verify carry, with 102 instructions and 36 differing rows.

# Functions at 0x8006

- [func_8006BC80 — CRACKED (0/474 words, match candidate)](func_8006BC80.near-match.md) — Records the hole-banner emitter's 474-instruction isolated candidate with zero unexplained word differences, not a verified ROM bank.
- [func_8006C8CC — banked via shared positive-return label](func_8006C8CC.near-match.md) — Records the verified 19-instruction match and the shared-return CFG that defeats jump.c's conditional-assignment conversion.
- [func_8006D164 near-match (S240 carry)](func_8006D164.near-match.md) — Records the four-row halfword-copy loop's S240 coupled invariant-hoist and register-rotation residual.
- [func_8006D214 — near-match (base-register-vs-displacement wall; sibling of func_8006D38C)](func_8006D214.near-match.md) — Documents the fixed-point MAX search and its base-register-versus-displacement residual, shared with func_8006D38C.
- [func_8006D38C — near-match (base-register-vs-displacement wall)](func_8006D38C.near-match.md) — Records the base-register-versus-displacement wall in the sibling reconstruction to func_8006D214.
- [func_8006DF84 near-match (S240 carry; S224-flagged wall CONFIRMED)](func_8006DF84.near-match.md) — Records the shot predicate's S240 cross-jump tail merge blocking an annulled branch after base-register factoring was solved.
- [func_8006E210 + func_8006DFF0 (nested child) - NEAR MATCH (S310)](func_8006E210.near-match.md) — Documents the paired near-match for func_8006E210 and its nested child func_8006DFF0.

# Functions at 0x8007

- [func_8007399C — near-match (S307, re-measured S308)](func_8007399C.near-match.md) — Records the glyph-extents calculator's S308 setup-block residual and refutes the earlier shared-rodata blocker.
- [func_80073F24 near-match (S243)](func_80073F24.near-match.md) — Records the glyph-string emitter's shared S243 schedule-order coin: the compiler defers the atlas calculation behind display-list stores after post-increment solves the store-giv wall.
- [func_80074230 near-match (S243)](func_80074230.near-match.md) — Documents the glyph emitter's solved store-giv form and shared S243 scheduler tie-break that still orders atlas work after stores.
- [func_80074500 near-match (S243)](func_80074500.near-match.md) — Records the shared S243 scheduler-order residual after the post-increment idiom solves the glyph emitter's store-giv wall.
- [func_80074E5C — carried input-copy/CSE residual](func_80074E5C.near-match.md) — Records refreshed 40- and 39-instruction candidates and the CSE substitution that removes the required incoming-argument copy.
- [func_80075E48 (src/main/func_80071370.c) — unrolled raw-DL-word emitter CARRY class](func_80075E48.near-match.md) — Characterizes the unrolled raw-display-list-word emitter as a carried reconstruction class.
- [func_80076138 — near-match carry (S242)](func_80076138.near-match.md) — Records the sprite-draw loop's S242 string-pointer/index register swap and entry-copy live-range diagnosis.
- [func_8007624C near-match (S246 carry)](func_8007624C.near-match.md) — Records the two-pass drop-shadow glyph emitter's S246 raw-packet allocation and scheduling residual.
- [func_800772C4 near-match (S206 carry)](func_800772C4.near-match.md) — Documents the spline-segment stepper's S206 FP register-allocation wall and its 307/321 matched-row reconstruction.
- [func_8007775C near-match (S206 carry)](func_8007775C.near-match.md) — Records the periodic cubic-spline coefficient solver's FP register-allocation carry, including its tridiagonal and wrap-correction passes.
- [func_80077AD4 near-match (S206 carry)](func_80077AD4.near-match.md) — Documents cubic finite-difference interpolation whose ROM computes each polynomial term separately rather than in Horner form.
- [func_80077C18 near-match](func_80077C18.near-match.md) — Records the particle-group initializer's S187 inner grid-scan allocation residual at exact instruction count.
- [func_8007955C near-match (S244 carry)](func_8007955C.near-match.md) — Records the particle spawner's S244 schedule-order residual, superseding its predicted register-allocation wall.
- [func_80079EBC — shared-literal-pool partial-bank carry (S279)](func_80079EBC.near-match.md) — Documents the partial-bank carry blocked by a shared literal pool.
- [func_8007A40C (+ func_8007A10C) — GCC nested-function carry (S279)](func_8007A40C.near-match.md) — Records the GCC nested-function carry for func_8007A40C and its sibling func_8007A10C.
- [func_8007C5D8 — byte-exact body, blocked by the TU's shared literal pool](func_8007C5D8.near-match.md) — Documents the byte-exact function body whose bank is blocked by the translation unit's shared literal pool.
- [func_8007EF0C — near-match (S305, re-opened S306)](func_8007EF0C.near-match.md) — Records the S306 texture-rectangle divide-placement residual, including filled hazard slots and a missing spill pair.

# Functions at 0x8008–0x8009

- [func_800824E4 near-match — S252, RE-DERIVED S258](func_800824E4.near-match.md) — Records the RGBA packer's S258 one-register residual and corrects S252's argument and delay-slot interpretation.
- [func_800842C0 — chain-USED GCC nested function (glistp via static chain in $v0)](func_800842C0.near-match.md) — Documents the chain-used glyph-string display-list child whose GCC static chain arrives in `$v0` from its parent.
- [func_80087CB0 near-match (S257, deepened S258) — aim-cursor sprite DL emitter](func_80087CB0.near-match.md) — Records the aim-cursor sprite display-list emitter's S257 near-match as deepened in S258.
- [func_80088A90 near-match (S254 carry, re-cracked S258)](func_80088A90.near-match.md) — Records the numeric-glyph emitter's S258 extra saved-register residual after refuting its earlier terminal verdict.
- [func_80089094 — open HUD renderer reconstruction](func_80089094.near-match.md) — Preserves the isolated main-bank renderer, F3DEX2 packet checks, and structural matching residual.
- [func_8008DDDC (0x388/904B) — near-match carry (S249)](func_8008DDDC.near-match.md) — Records the fog/material keyframe blender's S249 constant/address placement and resulting saved-register permutation.
- [func_8008E164 (lerp_s32) is a GCC nested function (S248 finding)](func_8008E164.nested.md) — Identifies `lerp_s32` as a GCC nested function and records the S248 evidence for its static-chain relationship.
- [func_8009232C -- near-match carry (S302)](func_8009232C.near-match.md) — Records the snapshot-replay emitter's S302 allocation decision that permutes the body after packet reconstruction.
- [func_800947A8 near-match (S301)](func_800947A8.near-match.md) — Records the sky-panel snapshot emitter's S301 exact-count carry traced to loop.c movable selection.
- [func_80095150 (0x33C/828B) — near-match carry (S249)](func_80095150.near-match.md) — Records the S249 framebuffer-blit and coordinate-grid reconstruction with register permutation across nested loops.
- [func_800958D8 near-match (S248)](func_800958D8.near-match.md) — Records the scissor emitter's S250 linked-byte match, refuting the earlier relocation-scoring wall without claiming a ROM bank.
- [func_800985B4 (src/main/func_80095A10.c) — jtbl trailing-4-align carve WALL](func_800985B4.near-match.md) — Documents the jump-table carve wall caused by trailing four-byte alignment in the S258 function.
- [func_80098C6C near-match (TERMINAL sched.c coin; barrier-only crack, PO-declined)](func_80098C6C.near-match.md) — Records the 13-instruction float-zero test whose exact match required PO-declined empty asm barriers.
- [func_80098CD8 near-match (TERMINAL uniform reg-rotation; exact-count 38/38)](func_80098CD8.near-match.md) — Documents the exact-count S248 carry whose uniform register rotation remains a terminal residual.
- [func_800990D0 — near-match (CARRY)](func_800990D0.near-match.md) — Records the base-28 password generator's S276 exact-count carry with residuals confined to five loop preheaders.

# Named functions (G–R)

- [get_surface_type (0x80039F7C) — near-match carry (S233)](get_surface_type.near-match.md) — Records the terrain-surface lookup's S233 constant-15 register-assignment residual.
- [get_terrain_vertex_pointer — banked via shared scratch chains](get_terrain_vertex_pointer.near-match.md) — Records the verified match and register-reuse lever for get_terrain_vertex_pointer.
- [init_rdp_and_draw_sky_background — 286/286, exact instruction multiset, pure order residual](init_rdp_and_draw_sky_background.near-match.md) — Records the 286-instruction reconstruction whose instruction multiset matches and whose residual is ordering only.
- [init_sky_pool_and_world_state near-match (S256 carry, rebuilt S258)](init_sky_pool_and_world_state.near-match.md) — Records the sky-grid initializer's S258 exact-count register permutation and two-instruction initialization swap.
- [resolve_shot_quality_table — near-match (S185, structurally complete, 3-instr irreducible residual)](resolve_shot_quality_table.near-match.md) — Documents the structurally complete S185 near-match and its three-instruction residual.

# Recorded resolutions

- [emit_lens_flare_dl (func_8008D3F4) -- BANKED S302](emit_lens_flare_dl.near-match.md) — Records the S302 full-ROM match for the lens-flare display-list emitter and its reconstruction evidence.
- [lookup_animation_by_id (0x80055FFC) — SOLVED / BANKED S213](lookup_animation_by_id.near-match.md) — Records the S213 solved and banked animation lookup together with its retained reconstruction evidence.

- [Banked evidence](banked/index.md) — Earlier reconstruction notes retained after their recorded matching resolution.
