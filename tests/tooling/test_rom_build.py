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


def test_permuter_callback_preserves_profile_flags_and_compile_errors(tmp_path):
    if not Path(rb.tool_path("gcc")).is_file() or not Path(rb.tool_path("kmc_as")).is_file():
        pytest.skip("build the vendored tools before running compiler behavior tests")
    source = tmp_path / "callback.c"
    output = tmp_path / "callback.o"
    source.write_text(
        "int selected_profile(void) {\n"
        "#if defined(SUPPORT_NAUDIO) && !defined(_FINALROM)\n"
        "return CALLBACK_RESULT;\n"
        "#else\nreturn 1;\n#endif\n}\n"
    )
    command = [
        str(rb.ROOT / "tools/kmc_permuter_compile.sh"),
        "--profile-source", "src/libmus/aud_thread.c",
        "-DCALLBACK_RESULT=73", str(source), "-o", str(output),
    ]
    subprocess.run(command, cwd=rb.ROOT, check=True, capture_output=True, text=True)
    text = subprocess.check_output([
        rb.tool_path("objcopy"), "-O", "binary", "-j", ".text", str(output), "/dev/stdout"
    ])
    words = struct.unpack(f">{len(text) // 4}I", text)
    assert [word & 0xFFFF for word in words if word & 0xFFFF0000 == 0x24020000] == [73]

    source.write_text("int selected_profile(void) { return ; invalid C }\n")
    result = subprocess.run(command, cwd=rb.ROOT, capture_output=True, text=True)
    assert result.returncode != 0
    assert source.name in result.stderr
    assert not output.exists()


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


@pytest.mark.parametrize("pc", [0x80452250, 0xA0452250])
@pytest.mark.parametrize("label", [".Llocal:", "jlabel .Llocal"])
def test_extracted_external_and_local_jumps_preserve_machine_code(tmp_path, pc, label):
    if not Path(rb.tool_path("as")).is_file() or not Path(rb.tool_path("ld")).is_file():
        pytest.skip("build the vendored tools before running assembler behavior tests")
    extract = load_tool("extract_rom")
    external_word = 0x08100303
    local_word = 0x08000000 | (((pc + 16) & 0x0FFFFFFF) >> 2)
    assembly = (
        ".macro jlabel name\n\\name:\n.endm\n"
        ".set noreorder\n.text\n.globl start\nstart:\n"
        f"/* 11C250 {pc:08X} {external_word:08X} */ j .L80400C0C\nnop\n"
        f"/* 11C258 {pc + 8:08X} {local_word:08X} */ j .Llocal\nnop\n"
        f"{label}\nnop\n"
    )
    source = tmp_path / "jumps.s"
    obj = tmp_path / "jumps.o"
    elf = tmp_path / "jumps.elf"
    source.write_text(extract.resolve_external_jumps(assembly))
    subprocess.run(
        [rb.tool_path("as"), "-EB", "-mips2", "-o", str(obj), str(source)], check=True
    )
    relocated_pc = pc + 0x1000
    subprocess.run(
        [
            rb.tool_path("ld"),
            f"-Ttext=0x{relocated_pc:X}",
            "-e",
            "start",
            "-o",
            str(elf),
            str(obj),
        ],
        check=True,
    )
    machine_code = subprocess.check_output(
        [
            rb.tool_path("objcopy"),
            "-O",
            "binary",
            "-j",
            ".text",
            str(elf),
            "/dev/stdout",
        ]
    )
    relocated_local_word = 0x08000000 | (((relocated_pc + 16) & 0x0FFFFFFF) >> 2)
    assert machine_code[:16] == struct.pack(
        ">4I", external_word, 0, relocated_local_word, 0
    )
