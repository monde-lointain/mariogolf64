#!/usr/bin/env python3
"""Localise a per-function count residual to ONE region, in one command.

`tools/cmpfn.sh` aligns two instruction streams line by line, so a body whose
registers play different roles reports every line as a difference: S305's
`func_8007EF0C` produced an 889-row diff whose 6-instruction deficit was invisible
in it. Splitting BOTH streams at rare anchor mnemonics and comparing segment
LENGTHS put the same residual on one line (`61 vs 50` before a `div`, `92 vs 101`
after it), which named the divergence as one scheduling placement.

    venv/bin/python3 tools/seg_diff.py <func> [<object>] [--anchors jal,div,...]

The default anchors (`jal`, `div`, `divu`, `mult`, `sqrt.s`, `bc1tl`, `trunc.w.s`,
`break`) are chosen because they are rare and their COUNTS usually match even when
everything between them has moved; a segment boundary is only meaningful when both
sides have the same anchor sequence, which the tool checks and reports. For every
segment whose lengths differ it prints the per-mnemonic delta, which is what says
whether the region is structurally short or just re-ordered around the anchor.

This is a LOCALISER, like `cmpfn.sh --mnemonics`: equal segment lengths prove
nothing about operands, and every bank still gates on `tools/verify-rom.sh`.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from decomp_common import ASM_DIR, ROOT_DIR, reexec_into_venv  # noqa: E402

reexec_into_venv(__file__)

DEFAULT_ANCHORS = "jal,div,divu,mult,sqrt.s,bc1tl,trunc.w.s,break"
OBJDUMP = "mips-linux-gnu-objdump"

ASM_INSN_RE = re.compile(r"^\s*/\*[^*]*\*/\s+(\S+)")
OBJ_INSN_RE = re.compile(r"^\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+(\S+)")

# splat and objdump spell the same encoding differently; fold both onto one name so a
# segment's mnemonic delta is real. `-M no-aliases` already keeps the object side from
# printing `li` for `addiu rX,zero,N` AND `ori rX,zero,N`, which is the pair that makes a
# hand-rolled histogram report two false deltas (S305).
ALIASES = {"move": "addu", "li": "addiu", "b": "beq", "negu": "subu", "nop": "sll"}


def fold(mnemonic: str) -> str:
    return ALIASES.get(mnemonic, mnemonic)


def rom_stream(asm_file: Path) -> list[str]:
    out = []
    for line in asm_file.read_text(errors="replace").splitlines():
        m = ASM_INSN_RE.match(line)
        if m:
            out.append(fold(m.group(1)))
    return out


def obj_stream(obj: Path, func: str) -> list[str]:
    # -z so runs of identical zero words (nops) are not collapsed to `...`, which
    # under-counts a byte-exact body (the cmpfn.sh header records the same trap).
    text = subprocess.run(
        [OBJDUMP, "-dz", "-M", "no-aliases", str(obj)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    out: list[str] = []
    inside = False
    for line in text.splitlines():
        if line.rstrip().endswith(f"<{func}>:"):
            inside = True
            continue
        if inside:
            if not line.strip():
                break
            m = OBJ_INSN_RE.match(line)
            if m:
                out.append(fold(m.group(1)))
    if not inside:
        sys.exit(f"seg_diff: {func} not found in {obj}")
    return out


def split(stream: list[str], anchors: set[str]) -> list[tuple[list[str], str]]:
    segments: list[tuple[list[str], str]] = []
    current: list[str] = []
    for mnemonic in stream:
        if mnemonic in anchors:
            segments.append((current, mnemonic))
            current = []
        else:
            current.append(mnemonic)
    segments.append((current, "END"))
    return segments


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("func")
    ap.add_argument("obj", nargs="?")
    ap.add_argument("--anchors", default=DEFAULT_ANCHORS)
    args = ap.parse_args()

    matches = list(ASM_DIR.glob(f"nonmatchings/**/{args.func}.s"))
    if not matches:
        sys.exit(f"seg_diff: no asm/nonmatchings/**/{args.func}.s")
    asm_file = matches[0]

    if args.obj:
        obj = Path(args.obj)
    else:
        stem = asm_file.parent.relative_to(ASM_DIR / "nonmatchings")
        obj = ROOT_DIR / "build" / "src" / f"{stem}.o"
    if not obj.is_file():
        sys.exit(f"seg_diff: {obj} not built")

    anchors = {a for a in args.anchors.split(",") if a}
    rom = rom_stream(asm_file)
    mine = obj_stream(obj, args.func)
    rom_segments = split(rom, anchors)
    mine_segments = split(mine, anchors)

    print(f"rom={len(rom)} mine={len(mine)}  ({obj})")
    if len(rom_segments) != len(mine_segments):
        print(
            f"anchor COUNT differs ({len(rom_segments) - 1} vs "
            f"{len(mine_segments) - 1}): the residual is structural at the anchor "
            "level, so segment lengths below are not aligned"
        )

    for i, (rom_seg, mine_seg) in enumerate(zip(rom_segments, mine_segments)):
        rom_body, rom_anchor = rom_seg
        mine_body, mine_anchor = mine_seg
        tag = "" if rom_anchor == mine_anchor else f"  anchor rom={rom_anchor} mine={mine_anchor}"
        if len(rom_body) == len(mine_body) and not tag:
            continue
        print(
            f"[{i:2}] before {rom_anchor:<10} rom={len(rom_body):4} "
            f"mine={len(mine_body):4}  ({len(mine_body) - len(rom_body):+d}){tag}"
        )
        rom_hist, mine_hist = Counter(rom_body), Counter(mine_body)
        for key in sorted(set(rom_hist) | set(mine_hist)):
            if rom_hist[key] != mine_hist[key]:
                print(f"       {key:<10} rom={rom_hist[key]} mine={mine_hist[key]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
