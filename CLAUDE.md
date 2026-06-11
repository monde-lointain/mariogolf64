# Mario Golf 64 Decomp workflow

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting), KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (asm→C seed), asm-differ (library mode), decomp-permuter (escalation), Ghidra MCP bridge (live decompile + struct/symbol lookup).

## Slash commands

- **`/decomp <func>`** — Tactical. Match-decompiles one function (placeholder or curated name). Runs `seed C → KMC GCC → objdump → asm-differ` until byte-exact, spot-checks in-tree by byte-level cmp, then **finalizes**: moves the body into `src/<seg>.c`, runs `clang-format`, and does a full `make` whose ROM SHA-1 must equal the baserom. Only then does it declare Match and propose the symbol-name patch. Always ends with a numbered "Suggested workflow improvements" section.
- **`/decomp-lead [scope]`** — Strategic. Reads project state and rewrites `docs/decomp_roadmap.md` (strategy, next 5 targets, subsegments to flip, sync status). Never dispatches `/decomp`. Optional scope arg filters to a subsystem.
- **`/sync-names`** — Pulls curated function + data names from the Ghidra workspace into `ghidra_symbols.txt`. Filters splat placeholders, Ghidra defaults (`FUN_/DAT_/LAB_/…`), `_NON_MATCHING` mirrors, and section-boundary labels. Respects `symbol_addrs.txt` overrides. Equivalent to `make sync-names`. No args.

Only one of `/decomp`, `/decomp-lead`, or `/sync-names` may run at a time — they share the Ghidra MCP slot via `tools/mcp_lock.py` (machine-global directory mutex at `~/.cache/mariogolf64/mcp.lock/`, 30-min stale-TTL; the lock is shared across git worktrees of the same repo).

## Conventions

- **One function per `/decomp` run.** You pick the target.
- **Scratch dir**: `nonmatchings/<func>/` (gitignored, shared with permuter).
- **Ghidra MCP** is assumed running via `~/development/reversing/ghidra/start-headless.sh` (port 8089). All MCP calls pass `program="baserom.z64"` (including `sync_decomp_names.py`).
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
- **Never run clang-format on library code under `src/libultra/` or `src/libkmc/`.** These files are verbatim copies from upstream (`~/development/repos/libultra_modern/`, `~/development/repos/libkmc/`); reformatting them defeats cross-referencing. Both dirs carry a local `.clang-format` with `DisableFormat: true` as a belt-and-suspenders. When `/decomp` finalizes a match into one of these dirs, skip the clang-format step.
- **Upstream-mirror pattern for SDK functions**: when a function address maps to a known SDK upstream — libultra (`~/development/repos/libultra_modern/src/`) or libkmc (`~/development/repos/libkmc/src/`) — strongly prefer **copying the upstream source verbatim** over hand-decompiling. **The project source path mirrors the upstream path exactly**: take the upstream's path relative to its `src/` root and prepend `src/lib<name>/`. So upstream `libultra_modern/src/shared/system/afterprenmi.c` → project `src/libultra/shared/system/afterprenmi.c`; upstream `libultra_modern/src/monegi/vi/vigetcurrcontext.c` → project `src/libultra/monegi/vi/vigetcurrcontext.c`; upstream `libkmc/src/matherr.c` → project `src/libkmc/matherr.c`. The yaml subseg path qualifier matches: `[0x<seg>, c, libultra/shared/system/afterprenmi]`. Do NOT collapse upstream variant dirs (`shared/`, `monegi/`, `nintendo/`) into a flat `libultra/<subdir>/` — the variant carries information about which SDK branch the function came from. Companion headers go under `include/libultra/` or `include/libkmc/` mirroring their upstream `include/` layout the same way; CFLAGS already adds `-I include/libultra -I include/libultra/internal -I include/libkmc` so canonical `<PR/x.h>` / `"viint.h"` / `<math.h>` includes resolve unchanged. The Makefile's recursive `find` over `src/` automatically picks up nested paths. `/decomp-libupstream <func>` automates the whole flow.
- **File-scope `static` blocks `/decomp-libupstream`**: if the upstream `.c` declares a file-scope `static <type> <name>;` (BSS global), KMC GCC emits a section-relative `.bss` reloc instead of a symbol reloc. The linker can't place rand.o-style local BSS at the splat-side `next` slot, so the 0x10-aligned chunk inserts ahead and shifts every downstream BSS symbol — every code reloc into that range breaks (saw 10,853 ROM bytes diff for `rand`). `/decomp-libupstream`'s Step 3.5 pre-flight catches this and aborts with **BSS-layout-conflict**. Fall back to `/decomp <curated>` and **drop the `static`** in the C body — the linker then resolves to the splat-side global at the correct vram. (Function-local `static` is fine; only file-scope is the problem.)
- **Multi-function-segment splitting (`/decomp` and `/decomp-libupstream`)**: splat's auto-extraction often packs several functions — sometimes from different upstream files — into a single `[0x<rom>, asm]` subseg. The 0x8E620 case bundled `rand`+`srand` (upstream `libkmc/rand.c`) with `_xsincos`+`sin`+`cos`+`tan` (upstream `libkmc/sin.c`). When more than one upstream file lives in the same subseg, **split the subseg** before flipping rather than flipping the whole block — insert a new `[0x<offset>, asm]` line at the boundary between the upstream files. Two cases: (a) `/decomp-libupstream` on the first file — flip the first chunk to `[0x<seg>, c, lib<name>/<basename>]`, leave the second chunk as `asm` for a follow-up run; (b) plain `/decomp` (e.g., when statics block libupstream) — flip the first chunk to `[0x<seg>, c]` so the splat scaffold for `src/<seg>.c` only carries `INCLUDE_ASM` stubs for the functions you're actively targeting. Address splitting at function-aligned boundaries (read the first instruction's vram from the asm). Same principle applies to overlay-BSS multi-file packing.
- **libkmc compile profile is `-O`, libultra is `-O2`**: libkmc upstream was built with `gcc -O` (per `~/development/repos/libkmc/src/genn64.bat`), not `-O2`. The instruction scheduler differs between the two: at `-O` rand.c stores `next` between the two `addiu` ops; at `-O2` it gets pushed to the end. Same C, same compiler, different `-O` level → byte mismatch. The Makefile carves this via `LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))` and a `$(BUILD_DIR)/$(SRC_DIR)/libkmc/%.o` pattern rule whose specificity beats the generic `src/%` rule. libultra continues to use `-O2`. **If a libkmc match locks at ~0.9 percent across every C rewrite, the diff is almost certainly scheduler-level — verify by compiling `nonmatchings/<func>/base.c` with `-O` directly before iterating further on the C.** `decomp_loop.py` auto-applies the `-O` profile when a matching `src/libkmc/<basename>.c` exists for the target's segment.
- **Workflow retrospection**: every `/decomp` run ends with a numbered "Suggested workflow improvements" section in the agent's final chat response. No file is written. User replies with which numbered items to apply; agent edits CLAUDE.md / seed_c.py / decomp_loop.py / decomp.md within the same session.

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)

- **Names (Ghidra → decomp)**: `make sync-names` (or `/sync-names`) pulls curated function + data names into `ghidra_symbols.txt`. The agent edits `ghidra_symbols.txt` ONLY through this tool — direct hand-edits remain forbidden, as does editing from any other slash command. `symbol_addrs.txt` stays hand-curated; the sync skips any address already in it (the two files must be disjoint).
- **Names (decomp → Ghidra)**: agent proposes `symbol_addrs.txt` edits in its final response. User applies, then runs `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` followed by `make extract` and `make`.
- **Structs**: agent pulls from Ghidra MCP, cross-checks against `re_tracking/structs.yml`, appends to `include/structs.h` (created on first use; flat layout, reshape later). Discrepancies between MCP-live and the snapshot are surfaced in the final response and block the write.
- **Memory map**: read-only on both sides for the agent. Drift is reported in `/decomp-lead`'s roadmap.
- **Forbidden agent edits**: `ghidra_symbols.txt` (hand-edit forbidden; modify only via `make sync-names` or `/sync-names`), `mariogolf64.yaml`, `mariogolf64.ld`, `re_tracking/*.yml`, `symbol_addrs.txt` (unless explicitly approved), `reloc_addrs.txt`.

--- 

# Coding Guidelines

## Prime directive: manage complexity

No one can hold a whole nontrivial system in their head. Every rule below exists
to **reduce how much you must think about at one time**. When choosing between
options, pick the one that leaves less to keep in mind.

- Minimize *accidental* complexity (introduced by our choices); partition the
  *essential* complexity (inherent to the problem) into brain-sized pieces.
- **Write for people first, the machine second.** Code is read far more often
  than written — optimize for the reader, not for typing speed.
- **Program *into* your language, not *in* it.** Decide what you want, then
  express it with available tools; compensate for missing features with
  conventions, not by limiting your thinking.
- **If it's hard, it's probably wrong.** "Tricky code," code that resists a
  clean name, comment, or test, is a warning sign — simplify instead of pressing on.
- **Be eclectic, not dogmatic.** Heuristics, not commandments. Hold conventions
  as tools you can drop when a better one fits.
- Make code so simple there are obviously no defects, not so complex there are no
  obvious defects.

---

## Naming

- The name should fully and accurately describe what the thing **represents**.
  If you can say in words what it is, that is usually the best name.
- Name the **problem**, not the mechanics: `employeeData`, not `inputRec`.
- Be specific. Vague names usable for anything (`x`, `temp`, `data`, `flag`,
  `handle`, `process`) signal a vague design.
- Length follows scope: short for tiny/local scope, longer for wide scope.
  ~10–16 chars is a good target for most names.
- Put computed-value qualifiers last: `totalCost`, `customerCount`, `maxLength`.
- Use `count` (a total) and `index` (one element), not the ambiguous `num`.
- Use precise opposites consistently: begin/end, first/last, min/max, next/prev,
  old/new, source/target, add/remove, get/set, open/close.
- Booleans read as true/false and are stated positively: `done`, `found`,
  `success` — never `notDone`. Avoid `flag`; name the condition.
- Loop indices `i`/`j`/`k` only in short, non-nested loops; meaningful names when
  nested, long, or used outside the loop.
- Adopt one convention and apply it everywhere — **any** convention beats none.
  It should distinguish local vs. member vs. global, and constants/types/variables.
- Avoid: misleading names, names differing by 1–2 chars or only by case,
  numerals (`data1`, `data2`), homophones, hard-to-read characters (`l`/`1`,
  `O`/`0`), and abbreviations that fail the "read it aloud over the phone" test.

```c
/* Bad: mechanics, no meaning */          /* Good: states the problem */
x = x - xx;                               balance = balance - lastPayment;
```

---

## Variables and data

- Give every variable the **smallest scope** that works. Start restrictive
  (loop → function → file → global, last resort) and widen only when forced.
- Minimize **live time** (lines from first to last use): keep references close
  together so the window where the value can be wrongly altered stays small.
- Declare each variable **near its first use** and initialize it there; never in
  a block at the top.
- **One variable, one purpose.** Don't recycle a `temp` across unrelated jobs,
  and don't overload a value with a hidden second meaning.
- Mark values that shouldn't change as `const`.
- Replace **magic numbers/strings** with named constants. Only `0` and `1`
  belong as bare literals.
- Guard every division against a zero denominator. Watch integer overflow and
  truncation, **including in intermediate results**.
- **Never compare floats for exact equality** — compare within a tolerance.
- Keep array/string indices in bounds; check first, middle, and last endpoints
  for off-by-one.
- Prefer enums over loose constants for a fixed value set; reserve the first slot
  for "invalid" and handle the unexpected case.

```c
/* Magic number → named constant */
for (i = 0; i < MAX_EMPLOYEES; i++) { ... }      /* not: i < 100 */

/* Never test floats with == ; compare within a tolerance */
static const double EPSILON = 1e-9;
int approx_equal(double a, double b) { return fabs(a - b) < EPSILON; }

/* One purpose per variable: don't reuse `temp` for two unrelated jobs */
discriminant = sqrt(b*b - 4*a*c);   /* not: temp = sqrt(...); ... temp = swap */
```

---

## Functions

- Create a function for a sound reason: to **name an abstraction**, remove
  duplication, hide a sequence or a complex test, or reduce complexity. Deep
  nesting is a signal to extract one.
- Aim for **functional cohesion**: a function does one and only one thing.
- The name describes **everything** it does, including side effects. If the name
  needs "and," remove the side effect rather than lengthening the name. Avoid
  vague verbs (`handle`, `process`, `dealWith`).
- Let length follow the logic, not an arbitrary cap — but be suspicious past a
  screenful.
- Parameters: order **input → modify → output**; keep that order consistent
  across similar functions; put status/error params last.
- Limit parameters to ~7. Needing more consistently means coupling is too tight —
  group the data.
- Use every parameter. Don't reuse an input parameter as a working variable —
  copy to a local; mark inputs `const`.
- Ensure every path returns a valid value (initialize the return value up top).
- Never return a pointer/reference to a local.

```c
/* Don't mutate input params; copy to a working local */
int scale(const int input) {
    int working = input * current_multiplier(input);
    working      = working + current_adder(working);
    return working;            /* `input` still holds the original */
}
```

---

## Modules and data abstraction

A "class" here means any **module**: data plus the functions that own it (an
Abstract Data Type). The principles apply whether or not your language has classes.

- Model each module as an **ADT**: expose operations, hide representation. Callers
  should never touch the internal data.
- **Information hiding** — for every module ask "what should I hide?" Hide two
  things: complexity, and decisions likely to change.
- Hide a decision behind a function and a type, not scattered literals:

```c
typedef int IdType;                 /* hide the representation */
IdType id = new_id();               /* hide the creation policy */
/* The body of new_id() is the ONE place that changes when IDs must become
   non-sequential, range-reserved, thread-safe, etc. Compare the leaky version:
   id = ++g_max_id;  — duplicated everywhere, impossible to evolve. */
```

- Give each module **one** consistent abstraction. If you can't name what it
  abstracts, or it does two unrelated things, split it.
- Minimize accessibility; expose data through accessor functions, never raw.
- **Prefer containment ("has-a") over inheritance ("is-a").** Use inheritance
  only for genuine specialization, and only when every subtype honors the base's
  full contract (Liskov substitution). Keep hierarchies shallow (2–3 levels).
- Replace a repeated `switch` on a type field with one dispatch point (e.g. a
  function-pointer table).
- **Avoid global data.** Make variables local first; if data must be shared,
  reach it only through access functions — they give one control point,
  validation on every access, and an easy path to a real ADT later.

---

## Control flow

- **Make the nominal path obvious.** Put the normal case right after the `if`,
  stack error/exception cases below. Don't write an empty `if` with work in the
  `else` — negate the test.
- Cover every case: give `if/else` chains a final `else`, and `switch` a
  `default`, that catches the unexpected value and reports it to the developer.
- Use **guard clauses** (early returns) to check error cases up front and keep the
  nominal code un-indented:

```c
if (!valid_name(name)) return ERR_NAME;
if (!file_open(name))  return ERR_OPEN;
if (!key_valid(key))   return ERR_KEY;
/* real work lives here, at the top indent level */
```

- Treat each loop as a **black box**: one entry, control conditions stated from
  outside, assured termination (mentally run the first, a middle, and the last
  iteration). Keep the body short enough to see at once; extract long bodies.
- **Always brace** conditional and loop bodies, even one-liners — layout must not
  be able to lie about what the body is (see Layout).
- Use `break`/`continue`/early `return` to *simplify*, sparingly; each weakens the
  black-box property. Avoid `goto`.
- For recursion, ensure a base case stops it; prefer iteration for simple cases.
- Replace complicated `if`/`switch` logic (or inheritance chains) with a
  **lookup table** — logic scales badly, data stays flat:

```c
static const int days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
days = days_per_month[month - 1];     /* not a 12-branch if/else */
```

- Simplify boolean expressions: name complex tests with an intermediate boolean
  or a well-named predicate function; state them positively (DeMorgan); fully
  parenthesize — don't rely on precedence; write comparisons in number-line order
  (`MIN <= x && x <= MAX`).
- Flatten nesting (cap ~3 levels) and keep **cyclomatic complexity low** (~≤10:
  count 1 + each `if`/`while`/`for`/`case`/`&&`/`||`). High count → extract a
  function.

---

## Defensive programming

- **Validate all data from any external source** (user, file, network, another
  module) — range, length, format. Reject or sanitize at the boundary.
- Build a **barricade**: validate data as it crosses into trusted code. Outside
  is dirty (use error handling); inside is clean (use assertions).
- **Assertions** are for conditions that must *never* happen (a bug). **Error
  handling** is for conditions you *expect* but hope don't occur. Keep
  side-effecting code out of assertions.
- Decide **robustness vs. correctness** deliberately: keep running with a
  best-effort value (consumer software) vs. never produce a wrong result
  (safety-critical). Apply it consistently.
- Always check returned error codes / call results, even when failure "can't"
  happen. No empty catch blocks.
- Use exceptions sparingly — same bar as assertions — and throw at the
  abstraction level of the interface, with full context.
- **Fail loud in development, soft in production.** Add lavish debug-only checks;
  make bugs glaring while building so the shipped program can degrade gracefully.

---

## Comments and layout

- **Self-documenting code first.** Good names, structure, named constants, and
  simple control flow carry most of the documentation. Fix bad code; don't paper
  over it with comments.
- Comment **intent and summary ("why")**, never restate the code ("what"). A
  wrong or redundant comment is worse than none.
- A good comment is at the level you'd *name a function* doing the same thing —
  if you can name it cleanly, consider extracting that function.
- Keep comments next to their code and updated; document surprises, workarounds,
  and the reason behind any deliberate oddity (quantify perf tricks so no one
  "fixes" them).

```c
if (account_type == ACCOUNT_NEW)   /* if establishing a new account  — intent */
/* not:  if (account_flag == 0)       // if account flag is zero      — mechanics */
```

- **Layout reveals logical structure** — that is its job; looking pretty is
  secondary. **Consistency matters more than which style** you pick.
- Indent subordinate code (2–4 spaces). Separate "paragraphs" of related
  statements with blank lines. Use whitespace within expressions.
- One statement per line; one declaration per line; no multiple side effects per
  line.
- Brace single-statement bodies so layout and logic can't diverge:

```c
/* No braces: indentation says 3 lines run each pass; the compiler runs only the
   first in the loop, the other two once. The visual structure lies. */
for (i = 0; i < n; i++) {
    int tmp   = left[i];
    left[i]   = right[i];
    right[i]  = tmp;
}
```

---

## Design heuristics

- **Loose coupling**: few, small, visible, flexible connections. Pass the minimum
  a function needs through its parameter list, not via shared/global state.

```c
/* Tight: forces caller to build a whole Employee.
   Loose: depends only on what it actually uses. */
double vacation_days(Date hire_date, JobClass job_class);
```

- **Strong cohesion**: everything in a module serves one central purpose.
- **Design for change**: identify what's likely to change (business rules, I/O
  formats, platform/vendor specifics, status values) and isolate each behind one
  module so a change hits one place. Use an enum, not a bool, for status that may
  grow new states.
- Favor **high fan-in** (reuse low-level utilities) and **low-to-medium fan-out**
  (a module using >~7 others is doing too much).
- Keep the design **lean** — no speculative "might-need-it" parts. Build the code
  the requirements demand, clearly.
- **One Right Place**: one place to find a given piece of code, one place to make
  a given change.
- Design is **iterative and heuristic** — there's no single right answer. Try
  more than one decomposition. Reuse known patterns, but don't force-fit them.
- Prototype to answer a *specific* design question with throwaway code; keep it
  junk so it isn't reused.

---

## Writing, evolving, and verifying code

**Grow code from intent.** Design a non-trivial function in intent-level
pseudocode first (what, not how). Review the pseudocode — iterating is cheap
before you're invested in code. Then turn each line into a comment and fill real
code beneath it. If one line explodes into too much code, extract a function (its
name falls out of the pseudocode).

**Refactor purposefully.** Every change should leave internal quality *better*.

- Small, **behavior-preserving** steps, one at a time; recompile and retest after
  each; keep tests green.
- **Never refactor and add functionality at once.** Save the original first.
- Refactor opportunistically — when adding a feature or fixing a bug, and in the
  error-prone/high-complexity areas first.
- Treat even one-line changes as risky (>50% first-attempt error rate); review them.
- **Code smells** to act on: duplicated code; an over-long function; deep nesting;
  poor cohesion; too many parameters; related data not grouped; a function more
  interested in another module than its own; a primitive overloaded for a richer
  concept (e.g. `int` for money); comments propping up bad code; global variables;
  heavy setup/takedown around a call (the interface is wrong).

```c
/* Smell: many setters before, many getters after → wrong interface */
process_withdrawal(id, balance, amount, date);     /* fix: pass what it needs */
```

**Debug scientifically.** The goal is the *cause*, not the symptom.

- Assume the bug is yours. Reproduce it reliably; shrink to the simplest case.
- Form a hypothesis from all the data; understand the problem well enough to
  predict it before you change anything.
- **Fix the root cause, never special-case the output.** Make one change at a
  time, for a reason you understand — no superstitious "+1 until it works."
- Add a regression test that exposes the bug, then look for the same bug elsewhere
  (defects cluster).

```c
/* Symptom patch (wrong): the real defect is sum[] never initialized to 0 */
if (client == 45) sum[45] += 3.45;     /* do NOT do this — fix the init instead */
```

**Test as you go.** Testing *measures* quality; it can't create it.

- Write tests early (ideally first); keep them as a regression suite.
- Aim for branch coverage, not just line coverage. Derive the minimum cases from
  the branch count (1 + one per decision).
- Test **boundaries** (just below / at / just above each limit) and **bad data**
  (none, too much, wrong kind/size, uninitialized), not just clean inputs.
- Focus on error-prone areas — defects cluster (~80% in ~20% of routines).

**Combine quality techniques; prevention beats detection.** No single
defect-finding method catches more than ~75% — **reviews/inspections catch what
testing can't** and find defects earlier and cheaper. Review every change.
Improving quality *lowers* total cost; it doesn't trade against it.

**Performance comes last.** Build clean, correct, modular code first.

- Don't optimize as you go. Most run time lives in a small fraction of code
  (80/20).
- **Measure before tuning, and again after** — intuition about hot spots is
  usually wrong, and many "optimizations" do nothing or backfire (and vary by
  compiler/platform). Keep the clean version unless a measured need justifies the
  trade.

```c
/* The straightforward version. A hand "pointer-walk" rewrite measured ZERO gain
   here — the optimizer already does it. Don't trade clarity for an unmeasured guess. */
for (row = 0; row < rows; row++)
    for (col = 0; col < cols; col++)
        sum += matrix[row][col];
```

---

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
