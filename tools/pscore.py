#!/usr/bin/env python3
"""pscore.py -- score single decomp-permuter candidate(s) with the permuter's own Scorer.

Usage:
    venv/bin/python3 tools/pscore.py <scaffold_dir> <candidate.c> [candidate2.c ...]

Prints one `<score>\t<candidate>` line per candidate (lower is closer; 0 is a byte-match).
Handy for A/B-testing hand-written source variants against the ROM without launching a full
permuter run. Faithful for what matters: a byte-match scores 0, and relative ranking between
candidates is exact (S175: `s32 seed` and `register s32 seed` both scored 16613 -> proved the
`register` keyword is score-neutral via the authoritative scorer). Caveat: the absolute non-zero
number can sit a small constant above a live run's recorded output-<score>-*/ value (S175 saw a
steady +3, independent of stack_differences) -- use it for comparison and match-detection, not to
reproduce a run's exact absolute score.

Why the monkeypatch: the permuter's objdump reloc parser (src/objdump.py:259) does
`int(imm, 0)` on a relocation addend, which raises on a bare-hex `R_MIPS_26 .text+<hex>`
internal-jump addend (e.g. `23c`) emitted by newer mips-linux-gnu-objdump when the target.o
holds multiple functions (a decomposed one-tu). We normalize such bare-hex addends to `0x<hex>`
at runtime, in THIS process only, leaving the vendored permuter untouched (its live runs score
single-function candidate objects that never hit this path).
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PERMUTER = os.path.join(HERE, "decomp-permuter")
sys.path.insert(0, PERMUTER)

import src.objdump as _od  # noqa: E402
from src.scorer import Scorer  # noqa: E402
from src.helpers import get_settings, json_prop  # noqa: E402

_orig_process_mips_reloc = _od.process_mips_reloc


def _hex_tolerant(reloc_row, prev, repl, imm):
    if imm not in ("0", "imm", "addr"):
        try:
            int(imm, 0)
        except ValueError:
            try:
                int(imm, 16)
                imm = "0x" + imm
            except ValueError:
                pass
    return _orig_process_mips_reloc(reloc_row, prev, repl, imm)


_od.process_mips_reloc = _hex_tolerant


def main():
    if len(sys.argv) < 3:
        sys.exit(f"usage: {sys.argv[0]} <scaffold_dir> <candidate.c> [candidate2.c ...]")
    d = sys.argv[1]
    target_o = os.path.join(d, "target.o")
    compile_sh = os.path.join(d, "compile.sh")
    settings = get_settings(d)
    objdump_command = json_prop(settings, "objdump_command", str, "") or None
    scorer = Scorer(
        target_o,
        stack_differences=False,
        algorithm="difflib",
        ign_branch_targets=False,
        objdump_command=objdump_command,
        debug_mode=False,
    )
    for cand in sys.argv[2:]:
        with tempfile.NamedTemporaryFile(suffix=".c", delete=False) as tf:
            tf.write(open(cand, "rb").read())
            csrc = tf.name
        oo = csrc[:-2] + ".o"
        r = subprocess.run(["bash", compile_sh, csrc, "-o", oo], capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(oo):
            print(f"COMPILE_FAIL\t{cand}")
        else:
            score, _hashv = scorer.score(oo)
            print(f"{score}\t{cand}")
            os.unlink(oo)
        os.unlink(csrc)


if __name__ == "__main__":
    main()
