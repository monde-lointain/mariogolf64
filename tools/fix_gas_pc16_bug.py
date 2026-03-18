#!/usr/bin/env python3
"""
fix_gas_pc16_bug.py - Workaround for GNU Binutils R_MIPS_PC16 relocation bug

Bug description:
    When a branch instruction targets a global symbol (declared via .global or
    glabel) in the same section, mips-linux-gnu-as (GNU Binutils 2.42) emits an
    R_MIPS_PC16 relocation and defers resolution to the linker. The linker's
    R_MIPS_PC16 handler then computes the branch offset incorrectly, producing
    a branch that targets the wrong address.

    When a branch targets a local (non-global) symbol, GAS resolves the offset
    directly at assembly time with no relocation record, and the result is correct.

Root cause:
    Confirmed via objdump -r: global symbol branches emit R_MIPS_PC16 relocations,
    local symbol branches do not. The linker's R_MIPS_PC16 calculation is wrong
    for same-section references.

    Minimal reproducer:
        .global test_func    # With this: bnez encodes as 1440ffff (wrong)
        test_func:           # Without:   bnez encodes as 1440fffd (correct)
            nop
            nop
            bnez $v0, test_func
            nop

Workaround:
    Replace branch targets that reference global symbols with local labels.
    Local labels bypass the R_MIPS_PC16 relocation path entirely.

Files patched and why:
    asm/nusys/nugfxtaskallendwait.s:
        nuGfxTaskAllEndWait is a tight self-referencing loop. The branch target
        is replaced with local label .L800A1600 at the top of the loop.
"""

from pathlib import Path

PATCHES = [
    {
        "file": "asm/nusys/nugfxtaskallendwait.s",
        "description": "Replace self-referencing branch to global nuGfxTaskAllEndWait with local label",
        "add_after": "glabel nuGfxTaskAllEndWait",
        "add_line": ".L800A1600:",
        "replace": {
            "bnez       $v0, nuGfxTaskAllEndWait": "bnez       $v0, .L800A1600",
        },
    },
]

def apply_patches():
    for patch in PATCHES:
        path = Path(patch["file"])
        if not path.exists():
            print(f"WARNING: {path} does not exist, skipping")
            continue

        content = path.read_text()

        # Check if already patched by looking for the add_line
        if "add_line" in patch and patch["add_line"] in content:
            print(f"Already patched: {path}")
            continue

        lines = content.splitlines(keepends=True)
        out = []
        changed = False

        for line in lines:
            stripped = line.strip()

            # Add local label after specified line
            if "add_after" in patch and stripped == patch["add_after"]:
                out.append(line)
                out.append(patch["add_line"] + "\n")
                changed = True
                continue

            # Apply replacements
            new_line = line
            for old, new in patch.get("replace", {}).items():
                if old in line:
                    new_line = line.replace(old, new)
                    changed = True
            out.append(new_line)

        if changed:
            path.write_text("".join(out))
            print(f"Patched: {path} - {patch['description']}")
        else:
            print(f"No match found: {path}")

if __name__ == "__main__":
    apply_patches()
    print("Done")
