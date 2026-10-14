"""Exercise profile precedence through the real KMC compiler and assembler."""
from __future__ import annotations

from pathlib import Path
import struct
import subprocess
import sys

import pytest
from conftest import load_tool

rb = load_tool("rom_build")
dl = load_tool("decomp_loop")


def compiled_return(tmp_path, body, profile, parent=None):
    if not Path(rb.tool_path("gcc")).is_file() or not Path(rb.tool_path("kmc_as")).is_file():
        pytest.skip("build the vendored tools before running compiler behavior tests")
    source = tmp_path / "probe.c"
    output = tmp_path / "probe.o"
    source.write_text("int selected_profile(void) {\n" + body + "\n}\n")
    rb.compile_source(source, output, profile=profile, profile_source=parent)
    text = subprocess.check_output([
        rb.tool_path("objcopy"), "-O", "binary", "-j", ".text", str(output), "/dev/stdout"
    ])
    words = struct.unpack(f">{len(text) // 4}I", text)
    return [word & 0xFFFF for word in words if word & 0xFFFF0000 == 0x24020000]


@pytest.mark.parametrize("segment, expected", [
    ("libnusys/nupireadrom", 101),
    ("libnusys/nuboot", 42),
])
def test_isolated_compile_honors_parent_file_profile(tmp_path, segment, expected):
    profile = dl.resolve_profile("auto", "selected_profile", segment)
    result = compiled_return(tmp_path, "#ifdef USE_EPI\nreturn 101;\n#else\nreturn 42;\n#endif",
                             profile.name, profile.source)
    assert result == [expected]


def test_libmus_undefines_finalrom_after_base_defines(tmp_path):
    result = compiled_return(tmp_path,
        "#if defined(_FINALROM)\nreturn 1;\n#elif defined(SUPPORT_NAUDIO)\nreturn 2;\n#else\nreturn 3;\n#endif",
        "libmus")
    assert result == [2]


@pytest.mark.parametrize("section", ['.section .text, "ax"', '.section .text,"ax"'])
def test_permuter_recognizes_text_section_with_flags(section):
    assembly = '.section .rodata,"a"\nglabel data_label\n.word 1234\n' + section + '\nglabel chosen\nnop\n'
    program = (
        "import importlib,json,sys;sys.path.insert(0,sys.argv[1]);"
        "parser=importlib.import_module('import');"
        "print(json.dumps(parser.prune_asm(sys.argv[2])[0]))"
    )
    result = subprocess.check_output([
        sys.executable, "-c", program, str(rb.ROOT / "tools/decomp-permuter"), assembly
    ], text=True)
    assert result.strip() == '"chosen"'
