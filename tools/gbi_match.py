#!/usr/bin/env python3
"""Name the gbi.h macro call that emits a given display-list word pair.

Reconstructing a DL emitter means turning raw `w0/w1` pairs back into macro calls,
and the two that never decode by inspection are the render mode (an OR of two
`G_RM_*` halves) and the colour combiner (an eight-way `G_CC_*` pair, when a stock
pair exists at all). Both were brute-forced by hand-built harnesses twice in S305;
this is that harness as a tool.

    venv/bin/python3 tools/gbi_match.py 0xE200001C/0x00504340
    venv/bin/python3 tools/gbi_match.py 0xFCFFFFFF/0xFFFE793C 0xFC50B2A1/0x3365FEFF

It compiles a generated host program against the project's own `include/libultra/PR`
with `-DF3DEX_GBI_2`, so the answer is the game's gbi.h, not a remembered one. For a
`G_SETOTHERMODE_L` render-mode word it enumerates every ordered `G_RM_*` pair; for a
`G_SETCOMBINE` word it enumerates every ordered pair of the 8-argument `G_CC_*`
macros through `gDPSetCombineMode`. A combiner with NO stock pair is a real answer
too — it means the body has to spell `gDPSetCombineLERP`, which is what S305's
`0xFC50B2A1/0x3365FEFF` and `0xFCFF97FF/0xFF2DFEFF` both turned out to be, so the
tool prints the decoded LERP argument list in that case (via `tools/combine_word.py`).

Other command bytes are reported with their decoded fields only; use
`tools/rdp_word.py` for those, and settle a composite macro (a texture block, a
scissored texrect) by emitting it in a scratch harness instead — a single word does
not identify one.
"""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from decomp_common import ROOT_DIR, reexec_into_venv  # noqa: E402

reexec_into_venv(__file__)

GBI_H = ROOT_DIR / "include" / "libultra" / "PR" / "gbi.h"
CFLAGS = [
    "-m32",
    "-w",
    "-DF3DEX_GBI_2",
    "-D_LANGUAGE_C",
    "-I",
    str(ROOT_DIR / "include"),
    "-I",
    str(ROOT_DIR / "include" / "libultra"),
    "-I",
    str(ROOT_DIR / "include" / "libultra" / "PR"),
]

G_SETOTHERMODE_L = 0xE2
G_SETCOMBINE = 0xFC


def macro_names(prefix: str) -> list[str]:
    text = GBI_H.read_text(errors="replace")
    return sorted(set(re.findall(rf"#define\s+({prefix}[A-Z0-9_]+)", text)))


def cc_names_with_eight_args(names: list[str], tmp: Path) -> list[str]:
    """Keep the G_CC_ macros that expand to eight combiner arguments.

    The 2-cycle-only names (`G_CC_..2`) expand to fewer and make the generated
    program fail to compile, so they are filtered by expanding every name through
    `cpp` first. Indices are emitted rather than the names themselves, because the
    names inside the probe text would be expanded too.
    """
    probe = tmp / "probe.c"
    probe.write_text(
        "#include <ultra64.h>\n"
        + "\n".join(f"IDX{i} {name}" for i, name in enumerate(names))
        + "\n"
    )
    out = subprocess.run(
        ["cpp", *CFLAGS, str(probe)], capture_output=True, text=True
    ).stdout
    keep = []
    for line in out.splitlines():
        m = re.match(r"IDX(\d+)\s+(.*)", line.strip())
        if m and len(m.group(2).split(",")) == 8:
            keep.append(names[int(m.group(1))])
    return keep


def run_probe(source: str, tmp: Path) -> str:
    src = tmp / "match.c"
    exe = tmp / "match"
    src.write_text(source)
    build = subprocess.run(
        ["gcc", *CFLAGS, "-o", str(exe), str(src)], capture_output=True, text=True
    )
    if build.returncode != 0:
        sys.exit(f"gbi_match: harness build failed\n{build.stderr[:2000]}")
    return subprocess.run([str(exe)], capture_output=True, text=True).stdout


def match_rendermode(w1: int, tmp: Path) -> list[str]:
    names = macro_names("G_RM_")
    body = "\n".join(
        f'if ((unsigned)(({a}) | ({b})) == target) puts("{a} , {b}");'
        for a in names
        for b in names
    )
    src = f"#include <stdio.h>\n#include <ultra64.h>\nint main(void){{unsigned target=0x{w1:08X}u;\n{body}\nreturn 0;}}\n"
    return run_probe(src, tmp).split("\n")


def match_combine(w0: int, w1: int, tmp: Path) -> list[str]:
    names = cc_names_with_eight_args(macro_names("G_CC_"), tmp)
    body = "\n".join(
        f'p = buf; gDPSetCombineMode(p++, {a}, {b}); '
        f'if (buf[0].words.w0 == t0 && buf[0].words.w1 == t1) puts("{a} , {b}");'
        for a in names
        for b in names
    )
    src = (
        "#include <stdio.h>\n#include <ultra64.h>\nstatic Gfx buf[4];\n"
        f"int main(void){{Gfx *p; unsigned t0=0x{w0:08X}u, t1=0x{w1:08X}u;\n{body}\nreturn 0;}}\n"
    )
    return run_probe(src, tmp).split("\n")


def lerp_form(w0: int, w1: int) -> str:
    out = subprocess.run(
        [
            sys.executable,
            str(ROOT_DIR / "tools" / "combine_word.py"),
            f"0x{w0:08X}/0x{w1:08X}",
        ],
        capture_output=True,
        text=True,
    ).stdout.strip()
    return out or "(combine_word.py produced nothing)"


def main() -> int:
    pairs = sys.argv[1:]
    if not pairs:
        sys.exit("usage: gbi_match.py <w0>/<w1> [<w0>/<w1> ...]")

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for pair in pairs:
            try:
                a, b = pair.split("/")
                w0, w1 = int(a, 16), int(b, 16)
            except ValueError:
                sys.exit(f"gbi_match: expected <w0>/<w1>, got {pair!r}")
            cmd = w0 >> 24
            print(f"0x{w0:08X}/0x{w1:08X}  cmd 0x{cmd:02X}")
            if cmd == G_SETOTHERMODE_L and (w0 & 0xFFFF) == 0x001C:
                hits = [h for h in match_rendermode(w1, tmp) if h]
                if hits:
                    for hit in hits:
                        print(f"  gDPSetRenderMode(pkt, {hit})")
                else:
                    print("  no G_RM_ pair produces this word")
            elif cmd == G_SETCOMBINE:
                hits = [h for h in match_combine(w0, w1, tmp) if h]
                if hits:
                    for hit in hits:
                        print(f"  gDPSetCombineMode(pkt, {hit})")
                else:
                    print("  no stock G_CC_ pair -- the body must spell the LERP form:")
                    print(f"  {lerp_form(w0, w1)}")
            else:
                print("  not a render mode or combine word; use tools/rdp_word.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
