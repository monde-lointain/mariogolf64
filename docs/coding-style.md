# Coding guidelines (on-demand reference)

The C quality bar + naming conventions for code promoted into the tree. `CLAUDE.md` keeps a compact
naming table; read this when writing or naming non-trivial C (classical matches, struct authoring,
new helpers). Verbatim upstream copies under `src/libultra/` and `src/libkmc/` are exempt — they are
never reformatted.

## Prime directive: manage complexity

No one can hold a whole nontrivial system in their head. Every rule below exists to reduce how much
you must think about at one time. When choosing between options, pick the one that leaves less to
keep in mind.

- Minimize *accidental* complexity (introduced by our choices); partition the *essential* complexity
  (inherent to the problem) into brain-sized pieces.
- Write for people first, the machine second. Code is read far more often than written.
- Program *into* your language, not *in* it. Decide what you want, then express it with available
  tools; compensate for missing features with conventions, not by limiting your thinking.
- If it's hard, it's probably wrong. Tricky code, code that resists a clean name, comment, or test,
  is a warning sign — simplify instead of pressing on.
- Be eclectic, not dogmatic. Heuristics, not commandments.
- Make code so simple there are obviously no defects, not so complex there are no obvious defects.

## Naming

- The name should fully and accurately describe what the thing represents. If you can say in words
  what it is, that is usually the best name.
- Name the problem, not the mechanics: `employeeData`, not `inputRec`.
- Be specific. Vague names usable for anything (`x`, `temp`, `data`, `flag`, `handle`, `process`)
  signal a vague design.
- Length follows scope: short for tiny/local scope, longer for wide scope (~10–16 chars is a good
  target for most names).
- Put computed-value qualifiers last: `totalCost`, `customerCount`, `maxLength`.
- Use `count` (a total) and `index` (one element), not the ambiguous `num`.
- Use precise opposites consistently: begin/end, first/last, min/max, next/prev, old/new,
  source/target, add/remove, get/set, open/close.
- Booleans read as true/false and are stated positively: `done`, `found`, `success` — never
  `notDone`. Avoid `flag`; name the condition.
- Loop indices `i`/`j`/`k` only in short, non-nested loops; meaningful names when nested, long, or
  used outside the loop.
- Adopt one convention and apply it everywhere. It should distinguish local vs. member vs. global,
  and constants/types/variables.
- Avoid: misleading names, names differing by 1–2 chars or only by case, numerals (`data1`,
  `data2`), homophones, hard-to-read characters (`l`/`1`, `O`/`0`), and abbreviations that fail the
  "read it aloud over the phone" test.

```c
/* Bad: mechanics, no meaning */          /* Good: states the problem */
x = x - xx;                               balance = balance - lastPayment;
```

## Variables and data

- Give every variable the smallest scope that works. Start restrictive (loop → function → file →
  global, last resort) and widen only when forced.
- Minimize live time (lines from first to last use): keep references close together.
- Declare each variable near its first use and initialize it there; never in a block at the top.
- One variable, one purpose. Don't recycle a `temp` across unrelated jobs, and don't overload a
  value with a hidden second meaning.
- Mark values that shouldn't change as `const`.
- Replace magic numbers/strings with named constants. Only `0` and `1` belong as bare literals.
- Guard every division against a zero denominator. Watch integer overflow and truncation, including
  in intermediate results.
- Never compare floats for exact equality — compare within a tolerance.
- Keep array/string indices in bounds; check first, middle, and last endpoints for off-by-one.
- Prefer enums over loose constants for a fixed value set; reserve the first slot for "invalid".

```c
for (i = 0; i < MAX_EMPLOYEES; i++) { ... }      /* not: i < 100 */

static const double EPSILON = 1e-9;
int approx_equal(double a, double b) { return fabs(a - b) < EPSILON; }

discriminant = sqrt(b*b - 4*a*c);   /* not: temp = sqrt(...); ... temp = swap */
```

## Functions

- Create a function for a sound reason: to name an abstraction, remove duplication, hide a sequence
  or a complex test, or reduce complexity. Deep nesting is a signal to extract one.
- Aim for functional cohesion: a function does one and only one thing.
- The name describes everything it does, including side effects. If the name needs "and," remove the
  side effect rather than lengthening the name. Avoid vague verbs (`handle`, `process`, `dealWith`).
- Let length follow the logic, not an arbitrary cap — but be suspicious past a screenful.
- Parameters: order input → modify → output; keep that order consistent across similar functions;
  put status/error params last.
- Limit parameters to ~7. Needing more consistently means coupling is too tight — group the data.
- Use every parameter. Don't reuse an input parameter as a working variable — copy to a local; mark
  inputs `const`.
- Ensure every path returns a valid value (initialize the return value up top).
- Never return a pointer/reference to a local.

```c
int scale(const int input) {
    int working = input * current_multiplier(input);
    working      = working + current_adder(working);
    return working;            /* `input` still holds the original */
}
```

## Modules and data abstraction

A "class" here means any module: data plus the functions that own it (an Abstract Data Type).

- Model each module as an ADT: expose operations, hide representation. Callers should never touch the
  internal data.
- Information hiding — for every module ask "what should I hide?" Hide two things: complexity, and
  decisions likely to change.
- Hide a decision behind a function and a type, not scattered literals:

```c
typedef int IdType;                 /* hide the representation */
IdType id = new_id();               /* hide the creation policy */
```

- Give each module one consistent abstraction. If you can't name what it abstracts, or it does two
  unrelated things, split it.
- Minimize accessibility; expose data through accessor functions, never raw.
- Prefer containment ("has-a") over inheritance ("is-a"). Keep hierarchies shallow (2–3 levels).
- Replace a repeated `switch` on a type field with one dispatch point (e.g. a function-pointer table).
- Avoid global data. Make variables local first; if data must be shared, reach it only through access
  functions.

## Control flow

- Make the nominal path obvious. Put the normal case right after the `if`, stack error/exception
  cases below. Don't write an empty `if` with work in the `else` — negate the test.
- Cover every case: give `if/else` chains a final `else`, and `switch` a `default`, that catches the
  unexpected value and reports it.
- Use guard clauses (early returns) to check error cases up front and keep the nominal code
  un-indented:

```c
if (!valid_name(name)) return ERR_NAME;
if (!file_open(name))  return ERR_OPEN;
if (!key_valid(key))   return ERR_KEY;
/* real work lives here, at the top indent level */
```

- Treat each loop as a black box: one entry, control conditions stated from outside, assured
  termination (mentally run the first, a middle, and the last iteration). Extract long bodies.
- Always brace conditional and loop bodies, even one-liners — layout must not be able to lie.
- Use `break`/`continue`/early `return` to simplify, sparingly. Avoid `goto`.
- For recursion, ensure a base case stops it; prefer iteration for simple cases.
- Replace complicated `if`/`switch` logic with a lookup table — logic scales badly, data stays flat:

```c
static const int days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
days = days_per_month[month - 1];
```

- Simplify boolean expressions: name complex tests with an intermediate boolean or a well-named
  predicate; state them positively (DeMorgan); fully parenthesize; write comparisons in number-line
  order (`MIN <= x && x <= MAX`).
- Flatten nesting (cap ~3 levels) and keep cyclomatic complexity low (~≤10: count 1 + each
  `if`/`while`/`for`/`case`/`&&`/`||`). High count → extract a function.

## Defensive programming

- Validate all data from any external source (user, file, network, another module) — range, length,
  format. Reject or sanitize at the boundary.
- Build a barricade: validate data as it crosses into trusted code. Outside is dirty (error
  handling); inside is clean (assertions).
- Assertions are for conditions that must never happen (a bug). Error handling is for conditions you
  expect but hope don't occur. Keep side-effecting code out of assertions.
- Decide robustness vs. correctness deliberately and apply it consistently.
- Always check returned error codes / call results. No empty catch blocks.
- Use exceptions sparingly; throw at the abstraction level of the interface, with full context.
- Fail loud in development, soft in production.

## Comments and layout

- Self-documenting code first. Good names, structure, named constants, and simple control flow carry
  most of the documentation. Fix bad code; don't paper over it with comments.
- Comment intent and summary ("why"), never restate the code ("what"). A wrong or redundant comment
  is worse than none.
- A good comment is at the level you'd name a function doing the same thing — if you can name it
  cleanly, consider extracting that function.
- Keep comments next to their code and updated; document surprises, workarounds, and the reason
  behind any deliberate oddity (quantify perf tricks so no one "fixes" them).

```c
if (account_type == ACCOUNT_NEW)   /* if establishing a new account  — intent */
/* not:  if (account_flag == 0)       // if account flag is zero      — mechanics */
```

- Layout reveals logical structure — that is its job. Consistency matters more than which style.
- Indent subordinate code (2–4 spaces). Separate "paragraphs" of related statements with blank lines.
- One statement per line; one declaration per line; no multiple side effects per line.
- Brace single-statement bodies so layout and logic can't diverge.

## Writing, evolving, and verifying code

**Grow code from intent.** Design a non-trivial function in intent-level pseudocode first (what, not
how). Review the pseudocode — iterating is cheap before you're invested in code. Then turn each line
into a comment and fill real code beneath it. If one line explodes into too much code, extract a
function (its name falls out of the pseudocode).

**Refactor purposefully.** Every change should leave internal quality better. Small,
behavior-preserving steps, one at a time; recompile and retest after each. Never refactor and add
functionality at once. Treat even one-line changes as risky.

Code smells to act on: duplicated code; an over-long function; deep nesting; poor cohesion; too many
parameters; related data not grouped; a function more interested in another module than its own; a
primitive overloaded for a richer concept; comments propping up bad code; global variables; heavy
setup/takedown around a call (the interface is wrong).

**Debug scientifically.** The goal is the cause, not the symptom. Assume the bug is yours. Reproduce
it reliably; shrink to the simplest case. Form a hypothesis from all the data. Fix the root cause,
never special-case the output. Add a regression test, then look for the same bug elsewhere (defects
cluster).

**Test as you go.** Aim for branch coverage. Test boundaries (just below / at / just above each
limit) and bad data, not just clean inputs. Focus on error-prone areas — defects cluster (~80% in
~20% of routines).

**Performance comes last.** Build clean, correct, modular code first. Don't optimize as you go.
Measure before tuning, and again after — intuition about hot spots is usually wrong.

---

# C naming conventions

| Identifier Type                   | Convention   | Example                  |
| --------------------------------- | ------------ | ------------------------ |
| Namespace-like Prefixes / Modules | `lower_case` | `audio_system`, `input_` |
| Struct                            | `CamelCase`  | `PacketHeader`           |
| Enum                              | `CamelCase`  | `ConnectionState`        |
| Enum Constant                     | `UPPER_CASE` | `CONNECTION_CONNECTED`   |
| Function                          | `lower_case` | `initialize_system()`    |
| Variable                          | `lower_case` | `frame_count`            |
| Parameter                         | `lower_case` | `host_name`              |
| Global Constant                   | `UPPER_CASE` | `MAX_CONNECTIONS`        |
| Macro Definition                  | `UPPER_CASE` | `BUFFER_SIZE`            |

- **Constants** use `UPPER_CASE` at global/static storage duration, `lower_case` for locals. No `k`
  prefix.
- **Macros** use `UPPER_CASE`.
- **Structs** are `CamelCase`; members are `lower_case`.
- **Enums** are `CamelCase`; enumerators are `UPPER_CASE`.
- **Functions, variables, parameters, globals, statics** are `lower_case`.

When stitching m2c output into `base.c`, rename m2c's `temp_*` / `local_*` / `arg_*` synthetics
before promoting. Use ultra64.h types (`s32`/`u64`/`vu32`/`f32`/…), never raw `int`/`long long`/
`volatile unsigned long`.

```c
#include <stdbool.h>

static const int DEFAULT_CHANNELS = 2;

typedef enum PlaybackState {
    PLAYBACK_STOPPED,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
} PlaybackState;

typedef struct AudioManager {
    int channel_count;
    bool initialized;
} AudioManager;

void initialize_system(AudioManager *manager) {
    manager->channel_count = DEFAULT_CHANNELS;
    manager->initialized = true;
}
```
