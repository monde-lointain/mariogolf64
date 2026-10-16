"""Exercise editor profile semantics through clangd's real Clang frontend."""

from __future__ import annotations

import shutil
import subprocess

import pytest
from conftest import load_tool

clangd_config = load_tool("clangd_config")


@pytest.mark.parametrize(
    ("source_name", "condition"),
    [
        ("main/probe.c", "defined(F3DEX_GBI_2) && defined(_FINALROM)"),
        ("main/vec3f_normalize.c", "defined(__FAST_MATH__)"),
        ("libultra/os/probe.c", "defined(F3DEX_GBI_2) && defined(BUILD_VERSION)"),
        ("libnusys/probe.c", "defined(USE_EPI)"),
        ("libnusys/nuboot.c", "!defined(USE_EPI) && !defined(__OPTIMIZE__)"),
        ("libnaudio/probe.c", "N_MICRO == 1"),
        ("libmus/probe.c", "defined(SUPPORT_NAUDIO) && !defined(_FINALROM)"),
    ],
)
def test_clangd_uses_target_and_effective_source_profile(
    tmp_path, monkeypatch, source_name, condition
):
    clangd = shutil.which("clangd-23")
    if clangd is None:
        pytest.skip("clangd-23 is optional editor tooling")
    root = tmp_path / "checkout with spaces"
    source = root / "src" / source_name
    source.parent.mkdir(parents=True)
    source.write_text(
        "#if !defined(__mips_o32) || !defined(__MIPSEB__) || __mips_fpr != 32\n"
        "#error Incorrect N64 architecture\n"
        "#endif\n"
        "typedef char pointer_must_be_32_bits[sizeof(void*) == 4 ? 1 : -1];\n"
        "typedef char long_must_be_32_bits[sizeof(long) == 4 ? 1 : -1];\n"
        f"#if !({condition})\n#error Incorrect compiler profile\n#endif\n"
        "int editor_probe(void) { return 0; }\n"
    )
    # The libultra compiler headers must precede base includes. Test lookup,
    # not the incidental spelling or position of JSON arguments.
    if source_name.startswith("libultra/"):
        for directory, body in [
            ("include", "#error Wrong include precedence\n"),
            (
                "include/libultra/compiler/gcc",
                "typedef char signed_char_required[(char)-1 < 0 ? 1 : -1];\n",
            ),
        ]:
            header = root / directory / "profile_order.h"
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text(body)
        source.write_text("#include <profile_order.h>\n" + source.read_text())
    monkeypatch.setattr(clangd_config, "ROOT", root)
    monkeypatch.setattr(clangd_config.rom_build, "ROOT", root)
    database = clangd_config.generate()
    result = subprocess.run(
        [
            clangd,
            f"--check={source}",
            f"--compile-commands-dir={database.parent}",
            "--log=error",
        ],
        cwd=tmp_path,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
