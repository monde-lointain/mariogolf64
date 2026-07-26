# Hazard index (flag -> playbook)

<role>
Maps each `tools/pick_target.py` hazard flag, and each asm symptom, to the one `docs/hazards.md`
section that handles it. A lookup table: read the row you need, then that section. Consulted from
both the plan gate (pricing a candidate) and the execution loop (routing a residual).
</role>

## Flag to section

When `pick_target.py` flags a hazard (or a match shows its symptom), read the matching
`docs/hazards.md` section. The detail behind each flag lives in that section, not here.

| pick_target.py flag / symptom | docs/hazards.md section |
| ----------------------------- | ----------------------- |
| upstream column is a lib | #upstream-mirror-pattern |
| `single-file-pack:<n>fn[…]` | #upstream-mirror-pattern |
| `upstream-fncount-mismatch:<m>vs<n>` | #upstream-mirror-pattern |
| `one-tu` | #upstream-mirror-pattern / #non16align |
| `refs-unplaced:<g>@0x…` | #recover-extern-refs-unplaced |
| `calls-unplaced:<fn>@0x…` | #calls-unplaced-function-callee-dual |
| macro-hidden extern | #macro-hidden-recover-extern |
| `jal-count-mismatch:<C>vs<asm>` | #near-verbatim-mirror-jal-count-mismatch |
| `block-reorder-sibling:<file>` | #near-verbatim-mirror-jal-count-mismatch |
| `file-static` | #file-static-bss-layout-conflict |
| `drop-static-mirror:<n>bss` | #file-static-bss-layout-conflict |
| `defines-data:<g>` / `data-static:<addr>` | #defines-data |
| `data-carve:<names>` | #defines-data |
| `twin-of:<file>` | #defines-data / #rodata-sibling-yaml-pattern |
| whole-region `.data`/`.rodata` sweep | #data-rodata-carve |
| `needs-header:<inc>` | #needs-header |
| `stale-header:os_version.h(<V>)` | #stale-vendored-header |
| `needs-define:<def>` | #needs-define |
| clean mirror SHA-miss, exact N×8B / fn shorter than its asm | #needs-define |
| clean mirror SHA-miss, single `li`/`addiu` immediate byte (not a reloc'd hi/lo) | #needs-define |
| `header-renames-symbol:<fn>@<hdr>` | #header-renames-symbol |
| `wrong-ghidra-name:<ghidra>-><correct>@<hdr>` | #wrong-ghidra-name-override |
| mirror parse error / undefined ref on a helper macro | #vendored-header-incomplete |
| parse-error cascade at macro-def lines / `stray '\'` after vendoring an SDK header | #crlf-vendored-header |
| `pack:<n>fn[…]` | #multi-function-segment-splitting-pack |
| `c-combined:<n>file[…]` | #multi-function-segment-splitting-pack |
| `unattrib-leaf:<addr>` | #multi-function-segment-splitting-pack |
| `non16align` | #non16align |
| `trailing-pad:<n>B@<align>` | #trailing-alignment-pad-after-a-c-mirror |
| `intrinsic-likely:<tu>.s` (and `(kmc-as)`, `(has-rodata:<sym>)`, `(asm-mirror-jtbl:<head>)`) | #asm-mirror-vendoring |
| `intrinsic-likely:cp0-asm(identify-TU)` | #asm-mirror-vendoring |
| `combined-subseg:<n>tu[…]` | #asm-mirror-vendoring |
| `intrinsic-likely` (bare) / `maybe-upstream:…` | #intrinsic-likely--maybe-upstream-signature-hints |
| `coddog-mirror:<file>@<pct>` | #coddog-cross-ref |
| `coddog-twin:<matched>!=<member-src>` | #coddog-cross-ref |
| `coddog-fncount-mismatch:<m>vs<n>` | #coddog-cross-ref |
| `c-combined-undercount:<m>vs<n>` | #coddog-cross-ref |
| `coddog-structural:<file>@<pct>` | #coddog-cross-ref |
| `coddog-partial:<m>of<n>fn` | #coddog-cross-ref |
| `static-name-collision:<name>@<addr>` | #static-name-collision |
| official static name shared across two instances / splat `Duplicate symbol detected` | #overlapping-symbols--allow_duplicated |
| SUPPORT_NAUDIO libmus mirror `alInit`→dead `n_al*` / bundled-synth `fncount-mismatch` | #libmus-bundled-n_audio-duplicate |
| `game-region-mirror:0x<vram>` | #game-region-mirror--o2-profile |
| `game-embedded:0x<vram>` | #game-region-mirror--o2-profile |
| clean asm-first seed, full-make SHA-miss, build .o shows fp-kept + no-CSE + arg-spill | #-o0-bootsdk-glue-file-profile |
| build SHA-miss, suspect compile flags (opt/-g/-fdelayed-branch) not the C | #profile-probe |
| public-API rename "resists" (`__*` symbol), vendored header macro may be inverted vs ultralib | #vendored-header-inversion |
| libultra leaf, bare std header | #per-library-standard-c-header-isolation |
| match locks ~0.9 on a lib target | #compile-profiles-libkmc--o-libultra--o3 |
| compiler rodata wrong offset / `rodata-literal:<addr>` | #rodata-sibling-yaml-pattern |
| `rodata-jtbl:<addr>` | #rodata-sibling-yaml-pattern |
| `…;owner-per-member` on a `rodata-jtbl`/`rodata-literal` | #rodata-sibling-yaml-pattern |
| MMIO fn, flat score, empty top_mismatches | #io_writeio_read-isolation-artifact |
| warm band, no hazard | #open-band-fast-path |
| `undefined reference` after mid-sprint sync-names | #make-sync-names-eviction-recovery |
| `caller-evict:<func_vram>@<file>` | #caller-evict |
| `carried-wall:<fn>[…][;characterization-only]` | #base-register-vs-displacement |
| loop cannot find label / reloc-name mismatch | #stale-top-level-asm-label-sync |
| decomp_loop bogus near-match (empty `base_text`, ref names the parent seg) just after a subseg split | #stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split |
| clean mirror SHA-miss, one field's high word | #mirror-cast-divergence-sign--vs-zero-extend |
| clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi | #char-signedness |
| clean mirror SHA-miss, extra `jal __assert` / bare `assert()` / `bare-assert:<n>` | #assert-strip |
| clean mirror SHA-miss, same insn count reordered / jal-mismatch + no `coddog-mirror` | #near-verbatim-mirror-jal-count-mismatch |
| clean mirror SHA-miss, build instr-count < target (shorter) / collateral post-fn addr shifts | #cross-jump-tail-merge |
| classical top-tested loop's `if(x<lo){x++;continue;}` guard: build cross-jumps the two `x++;j loop` tails vs ROM branch-likely (`bnezl`+annulled `x++`) — re-express as nested `if(x>=lo){…;x++}else{x++}` (+ lazy global-base load for the reg cycle) | #cross-jump-tail-merge |
| `body-divergence-suspect:<file>@<pct>` | #cross-jump-tail-merge |
| build over-inlines a small callee the ROM `jal`s (or reverse), same/cross TU | #same-tu-inline-mismatch-definition-order--cross-tu-split |
| ROM `subu`+`bgez`/`bltz` for a `<` compare (not `slt`/`sltu`) | #same-tu-inline-mismatch-definition-order--cross-tu-split |
| clean mirror SHA-miss, one fn's frame immediates shift by a fixed delta (`_FINALROM`/build-config struct size) | #upstream-mirror-pattern |
| array-of-struct init loop shorter than target + a field stored twice (doubled store-offset) | #struct-init-loop-dup-store--dual-induction-var |
| permuter on a KMC-toolchain (libnusys/libultra/libkmc) mirror fn | #permuter-setup-for-kmc-toolchain-mirrors |
| libnusys carry whose carried fns == the upstream's `#ifdef NU_DEBUG` fns | #nu_debug-stock-not-custom-carried-perf-fn-triage |
| libnusys inline `divu`, build byte-perfect except 2 missing `nop`s after `mflo` | #libnusys-inline-div-mflo-hazard-nop |
| mirror global w/ dead-reload-after-store on `x++` or recompute-not-CSE of `a-b` | #volatile-global-tell-dead-reload--recompute-not-cse |
| Gfx* manipulation | #display-lists |
| global-`glistp++` DL fn with fill color computed from global vars: color symbol-load hoisted into the glistp load-shadow (full reg cascade, structurally complete). Faithful fix = retype the color triple as a `Color {s32 r,g,b;}` struct (mem-in-struct defers the loads) + inline the pack after the fill-color w0 store, not the permuter (S180); `& ~7` on a phys addr is game-specific | #display-lists |
| permuter on a `src/main/`/overlay -O2/F3DEX2 fn (`setup-permuter.sh --main`) | #permuter-setup-for-kmc-toolchain-mirrors |
| ROM has bare `sqrt.d`/`sqrt.s`, build links `jal sqrt`/`jal sqrtf` or a guarded `sqrt.{d,s}`+`c.eq.{d,s}`/`bc1t` | #double-sqrt-fast-math |
| ROM loop top-tested plain `beq`/`bne`, build inverts to guard-`j`+`beql` or reloads loop-invariant constants | #top-tested-loop-goto-local-hoist |
| ROM up-counts a loop (`addiu +1`/`sltiu`), build reverses to `li N-1`/`addiu -1`/`bgez` | #top-tested-loop-goto-local-hoist |
| near-match, ROM hoists a compiler-generated div/mod magic (`0x66666667`/`0x1B4E81B5`) or a loop char-literal into the preamble but the goto-loop rematerializes it each iter (use structured `while(1){…break}`) | #top-tested-loop-goto-local-hoist |
| ROM selectively hoists (holds invariant array bases in regs but re-materializes a `%`/`/` magic at the loop tail); structured loop hoists both, plain goto de-hoists both (goto = partial fix) | #top-tested-loop-goto-local-hoist |
| clean per-fn match, full-make SHA-miss, hundreds of scattered 1-byte `%lo` diffs all `base-4` (decomposed one-tu rodata split) | #decomposed-one-tu-rodata-alignment-split |
| ROM reads `$ra` (reg 31) as a printf/log arg; `__builtin_return_address(0)` emits a stack-slot `lw` | #capturing-ra-return-address-as-a-call-argument |
| sentinel (`!=-1`) array walk matches except a 1-instr preheader swap (`move base` vs `li` const in the entry-`beq` delay slot, or a `-1` hoisted to an outer loop) | #indexed-vs-pointer-loop-strength-reduction |
| copy/scan loop re-indexes `arr[off]` each iter (insn count short vs ROM's pointer+offset dual-IV), or a running ptr-add groups base-before-index (`base+i*s+c` vs the ROM's `&base[i*s+c]`) | #indexed-vs-pointer-loop-strength-reduction |
| ROM re-materializes `%hi(SYM)+idx` per access (no walking pointer) or keeps a loop bound inline at the exit test, and every structured spelling comes out short | #goto-loop--loopc-never-runs-defeating-strength-reduction-and-bound-hoisting |
| ROM re-reads a count/bound global at more than one nesting level; build caches it in one pseudo and comes out a few instrs short | #multi-level-bound-re-read-array-element-form-not-a-cached-pointer |
| leaf spills the incoming `$v0` and never reloads it, and a near-match doc calls it a "spurious dead frame, not a nested fn" | #nested-function-static-chain-spill (check the caller for `addiu $v0,$sp,K`; writing it nested banks the parent too) |
| body byte-exact except 2-3 independent loads emitted in the wrong order, register-to-value mapping already correct | one local reused for two successive values — split it (memory `one-variable-reuse-reorders-loads`) |
| build emits `addiu rX,<elemreg>,C; addu rX,<globreg>,rX` where the ROM emits `addiu rX,<globreg>,C` (same count, swapped operands + downstream reg permutation) | #fold-associate-which-operand-of-a-3-term-sum-carries-the-constant |
| string loop: ROM has a redundant `andi rX,rY,0xFF` after an `lbu`, or a `beql` whose annulled slot holds a one-instruction handler | #string-classify-loop-the-redundant-char-andi-and-the-branch-likely-handler |
| hand-rolled raw-DL-word block (per-glyph/per-sprite `u32` stores through a manual cursor) called a terminal regalloc/reorg wall | #display-lists (S258: find the gbi.h macro first) |
| classical fn structurally correct (rows align) but locks high on a pervasive hard-reg permutation (`i:s4↔s5`) + spill-slot order + scheduling | #pervasive-regalloc-classical-main |
| `void` classical fn mis-allocates at loop-entry/delay-slot, resists every body lever | #return-type-is-load-bearing |
| struct-array fn byte-matches with per-field base symbols but not the combined struct (link-identical) | #struct-access-folding-changes-scheduling |
| classical `switch(x)` dispatch via a compiler jump table (`jtbl_<vram>`, `sltiu`+`jr $v0`), esp. w/ sparse inner cases or `a==K1\|\|K2` | #switch-jtbl-dispatch |
| clean per-fn match, full-make SHA-miss, lone `slti`<->`sltiu` at a switch/range bound-check (global signedness) | #switch-jtbl-dispatch |
| clean fn byte-exact except a fixed-global re-load after a nonscalar `arr[idx]=0` store (build CSE-forwards, 1 load short); read the global as `G[0]` array-elem for MEM_IN_STRUCT_P | #mem-in-struct-scheduling-lever |
| ROM cond-branch is plain `beqz`+`li v0,CONST`+`move v0,<scratch>` but build emits branch-likely `beqzl` skipping the lone `li v0,CONST` (return-var coalesced to v0) | #register-reuse-nudge-classical-regalloc |
| classical fn's global load/store schedules differently (build pipelines indep load-stores the ROM keeps strict-`$f0`-pairs, or hoists a `& K` flag load past a pointer store the ROM keeps late+`nop`) | #mem-in-struct-scheduling-lever |
| clean fn byte-exact except a fixed-global struct/array field RMW (`+=`/`-=`): build folds `ARR[k].field` into a `la` base reg where the ROM re-materializes `%hi/%lo` (+ a cascading reg permutation) | #offset-0-symbol-re-materialization |
| clean fn byte-exact except the ROM reloads a just-stored global field with no intervening store (`sw v1,f; lw v1,f; sw v1,g` for `g=f`); build forwards the stored reg (1 load short) | #volatile-view-cse-reload |
| structural-complete regalloc miss where a re-materializable constant loop-invariant (`&arr[K]`) is callee-saved (crosses the guarding call) but the ROM wants it caller-saved, displacing a call-arg copy-pref (mask→$a0 vs $t1); move its define point after the call + inline the sentinel | #loop-weight-and-live-length-regalloc-steering |
| delay-slot / instruction-count analysis off by ±1 (a "1 word short" / "needs a synthetic no-op" verdict read from GCC `-S`, not the assembled `.o`) | #assembler-differences--byte-cmp-spot-check |
| structural-complete regalloc miss where a constant loop-invariant (`&arr[K]`) is caller-saved (a competitor) but the variable-index sibling's `&arr[i]` is callee-saved; flips a call-arg copy-pref (mask→`$a0`) | #loop-weight-and-live-length-regalloc-steering |
| classical fn full-make SHA-miss and a same-file sibling reads wrong data addr (`%lo` off a fixed delta, whole `0x8010xxxx` .bss region shifted) | #short-text-shifts-flowing-bss |
| classical fn full-make SHA-miss where a too-long fn overflows its decomposed subseg and the auto-`.bss` `D_<vram>` symbols all float to `name+N` (looks like symbol/reloc corruption, not a length bug) | #short-text-shifts-flowing-bss |
| classical struct-array-of-`.bss` fn: build keeps a base pointer (`offset(v1)`) where ROM re-derives each field via `%hi/%lo(D_<field>)`, or the reverse on an `&arr[i]` self-store (`sw v1,0xC(v1)` vs re-derived) | #struct-array-of-bss-direct-index-vs-base-pointer-var |
| classical constant-dispatch (small selector to const results via a shared return var; ROM per-case `beql cond,RETURN`) locks pervasive BB-layout, resists if-else/switch/ternary/goto-end, lone `if(x==K)v=CONST` branchless-if-converts | #goto-dispatch-branch-toward-vs-branchless |
| classical call result the ROM holds in `$a0` (`move a0,v0` / `move v0,a0` bookends) but build coalesces into `$v0` (shorter); distinct-var/extra-use levers fail | #call-result-a0-vs-v0-single-allocno |
| pervasive classical BB-layout/regalloc/scheduling miss resists every idiom and the permuter plateaus | #compiler-source-fan-out-escalation-above-the-permuter |
| source dive proves a regalloc/codegen artifact unreachable from faithful C; need the missing idiom or a compiler-config/patchlevel confirmation (mine sibling KMC-2.7.2 decomps + a cross-compile probe) | #cross-project-matched-corpus-mining |
| classical fn byte-exact except a 3-word branch-direction triple (bnez/beqz+delay) on a `cond?t\|K:t` store/print through a reused loaded-var arg | #cse-make_regs_eqv-branch-fold |
| classical fn byte-exact except a 3-instr reg swap in `if(fabsf(x)<K)` (target `abs.s f2,f0`+const in `f0`; build `abs.s f0,f0` in-place+const in `f2`); permuter plateaus | #abs-coalescing-reg-swap |
| structural-complete regalloc miss = which value wins an earlier caller-saved reg; before "irreducible" | #loop-weight-and-live-length-regalloc-steering |
| structural-complete regalloc miss where a call-crossing param/local grabs `$s0` and rotates the loop vars off `s0/s1/s2` (local-alloc pre-empts before global priority); fix = mutate the param in place (`p=f(p)`) to make it a global qty; diagnose with the `-dg`/`-dl` allocno dumps | #loop-weight-and-live-length-regalloc-steering |
| tempted to structure/clean a matched goto-loop fn's loops; zero-goto rewrite attempt | #loop-weight-and-live-length-regalloc-steering |
| leaf fn opens `addiu sp,-8`+`sw $v0,0(sp)` never reloaded (dead spill of incoming `$v0`) and its caller sets `$v0=&sp[K]` before each `jal` = GCC nested function (static chain in `$v0`); bank as a nested fn in the parent's TU, or carry the orphaned child | #nested-function-static-chain-spill |
| leaf reads its arg/data via incoming `$v0` (`move a0,v0`/`lw x,K(v0)`) with a caller `addiu $v0,$sp,K` before the `jal` = chain-used GCC nested function (not a `$v0`-arg-convention wall); crack once its parent's TU is decompiled by writing it nested | #nested-function-static-chain-spill |
| permuter base.c with `__asm__ __volatile__(...)` aborts pycparser (`before: __volatile__`); b64literal-wrap that line by hand | #permuter-setup-for-kmc-toolchain-mirrors |
| permuter "best" on a goto-loop fn beats the hand-derived structural floor by a suspicious margin | #permuter-goto-backedge-liveness-unsound |
| classical fn structure/scheduling/hoisting fully matched, only residual = target reserves a dead stack frame (`addiu sp,-N`/`+N`, zero `sp)` access) + the reg permutation it drives; no source trigger (address-taken forces real sp loads) | #dead-frame-reload-artifact-regalloc-wall |
| classical fn byte-identical body, only residual = the two `addiu sp` frame immediates + `ra` slot offset, no reg permutation / no signed-divide (pure dead frame) — crackable via `s32 unused[(delta)/4]` (delta = ROM_frame − 0x18) | #dead-frame-reload-artifact-regalloc-wall (S251 pure-variant subsection) |
| >=2 alloc-artifact walls (dead-frame / non-coalesced reg-copy / target-spills-but-build-doesn't = "mine more optimal than target") cluster in one classical TU whose simple fns bank clean | #profile-probe (run one TU-wide probe before N per-fn dives) |
| signed divide-by-const dividend/magic in the wrong two regs (`sra r,r,0x1f` reg = dividend, `lui 0x<magic>` reg = magic); flippable-in-isolation (return/reg-2-set → local-alloc suggestion pass), but a void/callless/returnless loop-fed leaf is deterministically magic-in-low-reg; cross-project matched-corpus mining is the escalation | #signed-divide-const-v0v1-quotient-destination |
| tempted to use the plain `register` keyword (no `asm`) as a regalloc match lever — it is a zero-`.text`-effect no-op at -O2 (REG_USERVAR_P absent from local-alloc/global priority; DECL_REGISTER ignored when obey_regdecls==0) | #signed-divide-const-v0v1-quotient-destination |
| straight-line (1 basic block, `.flow` dump) classical fn locks on a pure s-register permutation + 1 independent-store schedule move; source levers don't move it | #local-alloc-qty-permutation |
| `nonmatching-func`/`decomp_loop` isolated object diverges from the in-tree build of the same 1-BB fn | #local-alloc-qty-permutation |
| large straight-line dump fn (one `T* p` param, `sub=&p->big_substruct` at fixed offset, many `sub->field` accesses): ROM materializes `p+C` as base (`addiu sN,a0,C`), build keeps the param base + folds `+C` into every displacement; pervasive base-reg + uniform-offset diff, `match_count==total_rows`+empty `top_mismatches` at low percent | #cse-derived-pointer-base-canonicalization |
| clean fn byte-exact except N `r` rows on one data-access chain: ROM materializes a full base addr into a reg + `0(reg)` deref, build keeps `%hi`+index + folds `%lo`/const into the load/store displacement; struct-array or `T* row=` intermediates backfire (pervasive regalloc shift), permuter does not flip it | #base-register-vs-displacement |
| near-match at mid percent (not high, not near-zero) where every residual row is a `sym+K`-vs-sibling reloc-addend on contiguous globals (struct/array base+addend form vs ROM separate per-field symbols); link-both-and-cmp with real addresses proves byte-exact -> isolation artifact, not a base-vs-disp wall | #isolated-compile-caveat |
| new C references a `D_<addr>` global whose `build/*.map` addr != its name (shifted `.NON_MATCHING` carve, e.g. name+0x10); referencing it corrupts the whole region incl. banked siblings | #base-register-vs-displacement (data-carve subsection) / #defines-data |
| `p ? field : sentinel` accessor (call returns ptr, return a field-or-default): build emits short branch-likely `beqzl` vs ROM `bnez/nop/j/li`; ternary + early-return both collapse; whole-file ±1 `cmp` cascade from the 2-insn deficit | #value-select-if-else-vs-branch-likely |
| null-guard `if(p){…}` byte-exact except the guard `beqz` delay slot (ROM `nop`, build steals the block's first insn); fires when body-first is pointer-independent (`li`/`sll`), matches free when body-first derefs the guarded ptr | #delay-slot-fill-of-a-null-guard-beqz |
| default-sentinel return var (`result=0`/`-1`) forces an extra saved `sN` + frame grows `0x18`->`0x20` + regalloc cascade because it is init before a call (crosses it -> callee-saved); init it after the call | #default-return-var-must-init-after-call |
