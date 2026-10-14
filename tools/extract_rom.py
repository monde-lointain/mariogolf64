#!/usr/bin/env python3
"""Extract the private US ROM entirely into the selected CMake binary tree."""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import filecmp
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import yaml
from rom_build import ROOT, build_dir, work_dir

INCLUDES = re.compile(r'INCLUDE_(ASM|RODATA)\s*\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)')
JUMP = re.compile(
    r"(?P<prefix>/\* [0-9A-F]+ (?P<pc>[0-9A-F]+) (?P<word>[0-9A-F]+) \*/\s+j\s+)"
    r"(?P<label>\.L\w+)"
)


def resolve_external_jumps(text: str) -> str:
    """Keep absolute J operands when the disassembler emits an undefined local label."""
    labels = {
        colon or macro
        for colon, macro in re.findall(
            r"^\s*(?:(\.L\w+):|(?:glabel|jlabel|alabel)\s+(\.L\w+))",
            text,
            re.MULTILINE,
        )
    }

    def replace(match: re.Match) -> str:
        if match["label"] in labels:
            return match.group()
        word = int(match["word"], 16)
        if word >> 26 != 2:
            raise ValueError(
                f"J instruction has an inconsistent opcode: {match.group()}"
            )
        target = ((int(match["pc"], 16) + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)
        return f"{match['prefix']}0x{target:08X}"

    return JUMP.sub(replace, text)


def split(config: dict, path: Path, *, recovery: bool = False) -> None:
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    command = [sys.executable, "-m", "splat", "split", str(path)]
    if recovery:
        command += ["--modes", "code"]
    else:
        command.append("--make-full-disasm-for-code")
    subprocess.run(command, cwd=build_dir(), check=True)


def recover_stubs(config: dict, output: Path) -> None:
    missing = {}
    for source in (ROOT / "src").rglob("*.c"):
        for kind, folder, name in INCLUDES.findall(source.read_text()):
            destination = output / folder / f"{name}.s"
            if not destination.exists():
                if kind != "ASM":
                    raise FileNotFoundError(f"Splat did not emit {destination} required by {source}")
                missing.setdefault(source.relative_to(ROOT / "src").with_suffix("").as_posix(), []).append((name, destination))
    if not missing:
        return
    # Splat's C scanner can leave gaps at functions inside a split translation
    # unit. The asm scanner disassembles the full interval. Recover only missing
    # function blocks in a separate output tree; do not flip the curated YAML.
    recovery = deepcopy(config)
    directory = output / "recovery"
    directory.mkdir(parents=True, exist_ok=True)
    recovery["options"].update(base_path=str(directory), asm_path="asm", asset_path="assets",
                               src_path=str(output / "src"),
                               undefined_funcs_auto_path=str(directory / "undefined_funcs_auto.txt"),
                               undefined_syms_auto_path=str(directory / "undefined_syms_auto.txt"))
    offsets = {}
    for segment in recovery["segments"]:
        if not isinstance(segment, dict):
            continue
        for sub in segment.get("subsegments", []):
            if isinstance(sub, list) and len(sub) >= 3 and sub[1] == "c" and sub[2] in missing:
                offsets[sub[2]] = sub[0]
                sub[1:] = ["asm"]
    absent = missing.keys() - offsets.keys()
    if absent:
        raise ValueError(f"Missing stubs are not in configured C segments: {sorted(absent)}")
    split(recovery, directory / "recovery.yaml", recovery=True)
    for source, required in missing.items():
        assembly = (directory / "asm" / f"{offsets[source]:X}.s").read_text()
        for name, destination in required:
            match = re.search(rf'^nonmatching {re.escape(name)},[^\n]*\n.*?^endlabel {re.escape(name)}\s*$',
                              assembly, re.MULTILINE | re.DOTALL)
            if match is None:
                raise ValueError(f"Full-range disassembly does not contain {name} required by {source}")
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(match.group() + "\n")
            print(f"Recovered {name} from the ROM", flush=True)
    shutil.rmtree(directory)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--stamp", type=Path, required=True)
    args = parser.parse_args()
    config = yaml.safe_load((ROOT / "mariogolf64.yaml").read_text())
    actual = hashlib.sha1(args.rom.read_bytes()).hexdigest()
    if actual != config["sha1"]:
        raise ValueError(f"US retail ROM SHA-1 mismatch: got {actual}, expected {config['sha1']}")
    work = work_dir()
    work.mkdir(parents=True, exist_ok=True)
    # Start clean so recovered stubs cannot retain obsolete symbols or ranges.
    # Publish only changed files: a C edit must not rebuild unrelated assembly.
    with tempfile.TemporaryDirectory(prefix="extract-", dir=build_dir()) as temporary:
        staging = Path(temporary)
        shutil.copytree(ROOT / "src", staging / "src")
        options = config["options"]
        options.update(base_path=str(staging), target_path=str(args.rom.resolve()),
                       elf_path=str(build_dir() / "mariogolf64.elf"), build_path=str(build_dir() / "obj"),
                       src_path="src", asm_path="asm", asset_path="assets",
                       ld_script_path=str(staging / "mariogolf64.ld"),
                       undefined_funcs_auto_path=str(staging / "undefined_funcs_auto.txt"),
                       undefined_syms_auto_path=str(staging / "undefined_syms_auto.txt"),
                       extensions_path=str(ROOT / options["extensions_path"]),
                       symbol_addrs_path=[str(ROOT / p) for p in options["symbol_addrs_path"]],
                       reloc_addrs_path=[str(ROOT / p) for p in options["reloc_addrs_path"]],
                       create_c_files=False, create_asm_dependencies=False)
        split(config, staging / "extraction.yaml")
        recover_stubs(config, staging)
        # Splat emits absolute .incbin paths. Relocate its generated references
        # together with the binary assets when publishing the staging tree.
        for assembly in (staging / "asm").rglob("*.s"):
            text = assembly.read_text()
            relocated = resolve_external_jumps(text).replace(str(staging), str(work))
            if relocated != text:
                assembly.write_text(relocated)
        # Leave a usable diagnostic configuration, not references to staging.
        options["base_path"] = str(work)
        for key in ("ld_script_path", "undefined_funcs_auto_path", "undefined_syms_auto_path"):
            options[key] = str(work / Path(options[key]).name)
        (staging / "extraction.yaml").write_text(yaml.safe_dump(config, sort_keys=False))
        incoming = {path.relative_to(staging): path for path in staging.rglob("*") if path.is_file()}
        for relative, source in incoming.items():
            destination = work / relative
            if not destination.is_file() or not filecmp.cmp(source, destination, shallow=False):
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, destination)
        for obsolete in work.rglob("*"):
            if obsolete.is_file() and obsolete.relative_to(work) not in incoming:
                obsolete.unlink()
    args.stamp.write_text(actual + "\n")


if __name__ == "__main__":
    main()
