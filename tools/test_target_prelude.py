#!/usr/bin/env python3
"""Tests for how a decomp environment assembles target.s (tools/decomp-env-template/prelude.inc).

include/macro.inc declares `jlabel` as a global label. A branch to a global label in the same
section is not resolved by the assembler: it leaves an R_MIPS_PC16 relocation and an unresolved
offset field. The linked ROM has the branch resolved, so the target object must too, or the scorer
cannot read the target. Needs the mips-linux-gnu binutils; the tests are skipped without them.
"""

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TEMPLATE = REPO / 'tools' / 'decomp-env-template'
sys.path.insert(0, str(TEMPLATE))

from objdump import objdump  # noqa: E402

PRELUDE = TEMPLATE / 'prelude.inc'
FUNCTION = 'demo_fn'
# A jump-table target label used as a branch target, as in a switch's default arm.
BODY = f'''glabel {FUNCTION}
    beqz $a0, .L_tail
    nop
    addu $v0, $a0, $a1
    jr $ra
    nop
jlabel .L_tail
    jr $ra
    nop
endlabel {FUNCTION}
'''


@unittest.skipUnless(shutil.which('mips-linux-gnu-as') and shutil.which('mips-linux-gnu-objdump'),
                     'needs mips-linux-gnu binutils')
class TargetPreludeTests(unittest.TestCase):
    def assemble(self, body: str) -> Path:
        tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, tmp)
        (tmp / 'target.s').write_text(PRELUDE.read_text() + body)
        subprocess.run(['mips-linux-gnu-as', '-EB', '-march=vr4300', '-mtune=vr4300',
                        f'-I{REPO / "include"}', '-o', str(tmp / 'target.o'), str(tmp / 'target.s')],
                       check=True)
        return tmp / 'target.o'

    def test_branch_to_jlabel_is_resolved_in_target_object(self):
        lines = objdump(str(self.assemble(BODY)), FUNCTION)
        # The tail label sits at offset 0x14 in the function.
        self.assertIn('beqz    a0,14', lines)

    def test_branch_to_jlabel_has_no_pc16_relocation(self):
        obj = self.assemble(BODY)
        dump = subprocess.run(['mips-linux-gnu-objdump', '-dr', str(obj)],
                              check=True, capture_output=True, text=True).stdout
        self.assertNotIn('R_MIPS_PC16', dump)


if __name__ == '__main__':
    unittest.main()
