#!/usr/bin/env python3
"""splat-yaml subseg parsing for pick_target: the subseg list + the .rodata carve-range helpers.

Self-contained stdlib leaf (imports only decomp_asm, like the other extracted
layers), so pick_target imports the parse without a cycle. Owns the YAML path and
the subseg-line regex; pick_target re-imports them.
"""
import functools
import os
import re

from decomp_asm import subseg_vram

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YAML = os.path.join(ROOT, "mariogolf64.yaml")

# Subseg line: `- [0x8DF10, c, libultra/monegi/rdp/dp]` or `- [0x8D230, asm]`.
# (bss entries use the `{ type: bss, ... }` brace form and are intentionally skipped.)
SUBSEG_RE = re.compile(
    r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*([a-z]+)\s*(?:,\s*([^\]]+?))?\s*\]"
)


def parse_subsegs():
    """Return all `[0x..,type,..]` subsegs as (rom_off:int, type, path|None), sorted by offset."""
    subs = []
    with open(YAML) as f:
        for line in f:
            m = SUBSEG_RE.match(line)
            if not m:
                continue
            off = int(m.group(1), 16)
            path = m.group(3).strip() if m.group(3) else None
            subs.append((off, m.group(2), path))
    subs.sort(key=lambda s: s[0])
    return subs


@functools.lru_cache(maxsize=1)
def _rodata_rom_ranges():
    """[(start_rom, end_rom)] of every `rodata`/`.rodata` subseg, for classifying a `%lo(D_<vram>)`
    literal. A constant whose ROM offset (vram − the fn's code-segment delta) lands in one of these
    ranges is a compiler-pooled rodata literal the mirror re-emits → a `.rodata` sibling split;
    one that lands elsewhere is a function-local `static` the mirror re-emits into the data segment
    → a recover-extern + file-scope-extern drop. The two enablers differ, so the gate must be routed
    to the right one."""
    subs = parse_subsegs()
    ranges = []
    for i, (off, typ, _path) in enumerate(subs):
        if typ in ("rodata", ".rodata"):
            end = subs[i + 1][0] if i + 1 < len(subs) else off
            ranges.append((off, end))
    return tuple(ranges)


def _literal_in_rodata(vram, off):
    """True when the `%lo(D_<vram>)` constant loaded by the fn at ROM `off` lives in the code
    segment's pooled `.rodata` (vs. the data segment). Text and pooled rodata share one segment
    delta, so vram − (fn_vram − off) is the constant's ROM offset; test it against the rodata
    subseg ranges. Returns False when the fn's vram is unknown (asm listing absent)."""
    fn_vram = subseg_vram(off)
    if fn_vram is None:
        return False
    rom = vram - (fn_vram - off)
    return any(start <= rom < end for start, end in _rodata_rom_ranges())


def _rodata_carve_start_vram(off, lit_vram):
    """VRAM where the rodata subseg containing `lit_vram` BEGINS — the carve-start companion to
    _rodata_carve_end_vram. The FP-literal scan reports only the scalar `ldc1/lwc1 %lo` loads, so its
    min understates a `.rodata` block that opens with a `static const` array base (an `addiu %lo`
    address-of the scan misses) or string literals. The `.rodata` sibling places the WHOLE object's
    rodata, which the splat subseg already bounds, so the carve runs from the subseg start. Returns
    the start vram (lower bound: the subseg start, exact when the mirror's rodata fills the subseg —
    the common case, symmetric to the carve-end upper bound), or None when the fn vram is unknown or
    the literal is not in a code-segment rodata range. Gated by defines_file_static_const_array at the
    call site to stay FP-safe (a `const` static is file-private, so the whole subseg is this object's)."""
    fn_vram = subseg_vram(off)
    if fn_vram is None:
        return None
    delta = fn_vram - off
    rom = lit_vram - delta
    for start, end in _rodata_rom_ranges():
        if start <= rom < end:
            return start + delta
    return None


def _rodata_carve_end_vram(off, lit_vram):
    """VRAM of the rodata-subseg boundary that ends the carve containing `lit_vram`.

    The rodata-literal scan reports the `%lo(D_…)`-referenced literals, but the sibling-split's
    carve extends to the next `.rodata` subseg boundary, which can run past the last referenced
    literal: a multi-`du` dlabel block's trailing word has no `%lo` of its own, so the scan's max
    understates the carve end. Pre-noting the boundary turns the finalize-time `.o`-sized carve into
    a planned extent at the gate. Returns the end vram (upper
    bound: the whole subseg end, exact when the mirror's literals are the subseg tail, the common
    case), or None when the fn vram is unknown or the literal is not in a code-segment rodata range."""
    fn_vram = subseg_vram(off)
    if fn_vram is None:
        return None
    delta = fn_vram - off
    rom = lit_vram - delta
    for start, end in _rodata_rom_ranges():
        if start <= rom < end:
            return end + delta
    return None
