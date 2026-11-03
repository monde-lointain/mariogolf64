# Matching investigation at 20f5853f

## Baseline and measurement

`make -j4` passes the ROM SHA-1 check. `make progress -j4` reports:

| Measure | Matched | Total |
| --- | ---: | ---: |
| Functions | 1,922 | 2,040 |
| Code bytes | 902,740 | 1,309,608 |
| Reported data bytes | 176,502 | 182,112 |

These are the progress tool's measured sections, not coverage of every ROM
asset. A matching ROM still includes assembly fallbacks and extracted data;
it does not establish a complete C decompilation.

The experiments below did **not** improve matching. No experimental source
change was retained. Counts are `fncompare.py`'s differing instruction words
on both sides, not objdiff's fuzzy percentages. In particular, a shifted
branch target can add differences beyond the inserted instruction itself.

## RecordComCharacterResult

Baseline: **3 words, 1 in shape**. Reproduce the diagnosis with:

```sh
.venv/bin/python tools/blame.py RecordComCharacterResult
```

The extra mask is insn 84, `zero_extendqisi2/1`, present from RTL at
`newValue = value`. Its input lives across blocks; local allocation cannot
tie it to the result. The register-to-register alternatives of
`zero_extendqisi2` and `zero_extendhisi2` in KMC GCC's
`config/mips/mips.md` explicitly mask to 8 and 16 bits respectively.

Additional trials beyond the source comment:

- Changing only `value` to `u16` leaves 3 differing words; `s16` gives 4.
- With `value` widened to `s32` or `u32`, placing `END_BASIC_BLOCK()` before
  the count extraction, count test, flag combination, or final promotion
  leaves 9 words. Before the save-count test it gives 19. An empty
  `IN_OWN_BLOCK({})` at those positions gives the same results.
- Forming `newValue` directly from the byte `value`, then recomputing
  `count | (value & WON_UNSAVED_FLAG) | WON_UNSAVED_FLAG` in the unsaved
  branch gives 5 words, 3 in shape. It is not an improvement over the draft.

Next work must account for both the promotion and the original's branch
copy; suppressing the mask alone is insufficient.

## UpdateCoursePointsResults

Baseline: **5 words, 1 in shape**. `blame.py` identifies the direct
player-count load as insn 620, present from jump and changed by combine.
The original instead forms the address of the game-mode field and loads
player count at an offset from it.

- Replacing only the start-tally outer loop's player-count expression with
  a local pointer dereference gives 46 words, 6 in shape. Ending a basic
  block after assigning the pointer gives 46 words, 4 in shape.
- An explicit initial nonzero-count test followed by a `do` loop leaves
  the baseline 5 words. This remains true with a pointer-based initial
  load, and block boundaries before the initial test or inside its body.

Merely spelling the initial load through a pointer does not preserve the
original addressing sequence through optimization.

## DrawScreenFade

Baseline: **10 words, 8 in shape**. `blame.py` traces the early setting of
bit 0x400 to insn 441, introduced in sched. Its final placement interacts
with reload and sched2 dependencies, so the difference is not just the
order of the color loads in C.

All 64 combinations of `u8`, `u16`, `s32`, and `u32` for the three color
locals were compiled. The existing all-byte version remains best at 10
words; the next best gives 11. Individual block boundaries around the
single-line display-list commands and color assignments also yield no
improvement. Avoid repeating color-width sweeps without a new explanation
of the scheduler/reload interaction.
