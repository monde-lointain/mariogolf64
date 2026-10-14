#!/usr/bin/env python3
"""Authoritative compiler profiles and CMake build interface for ROM tooling."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PROFILES = ROOT / "tools/build_profiles.json"


def build_dir() -> Path:
    path = Path(os.environ.get("MG64_BUILD_DIR", "build/us"))
    return (ROOT / path).resolve()


def work_dir() -> Path:
    return build_dir() / "work"


def build_tools() -> dict:
    path = build_dir() / "build-tools.json"
    if path.exists():
        return json.loads(path.read_text())
    base = build_dir()
    config = {"root": str(ROOT), "build_dir": str(base), "work_dir": str(base / "work"),
              "python": str(ROOT / ".venv/bin/python3"), "nonmatching": False}
    config.update({name: str(base / "toolchain/bin" / f"mips-linux-gnu-{name}")
                   for name in ("as", "ld", "objcopy", "strip", "objdump", "nm")})
    config.update({"gcc": str(base / "toolchain/kmc/gcc"),
                   "kmc_as": str(base / "toolchain/kmc/as"), "cpp": "cpp"})
    return config


def tool_path(name: str) -> str:
    return build_tools()[name]


def python_path() -> Path:
    return ROOT / ".venv/bin/python3"


def source_path(source: str | Path) -> Path:
    return (ROOT / source).resolve()


def source_key(source: str | Path) -> str:
    path = source_path(source)
    return path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else str(path)

def profile_name(source: str | Path) -> str:
    data = json.loads(PROFILES.read_text())
    key = source_key(source)
    parts = Path(key).parts
    name = parts[1] if len(parts) > 1 and parts[0] == "src" and parts[1] in data["profiles"] else "default"
    return data["file_overrides"].get(key, {}).get("profile", name)



def profile_flags(source: str | Path, profile: str | None = None,
                  nonmatching: bool = False, fastmath: bool = False) -> list[str]:
    data = json.loads(PROFILES.read_text())
    key = source_key(source)
    override = data["file_overrides"].get(key, {})
    if profile is None:
        parts = Path(key).parts
        profile = parts[1] if len(parts) > 1 and parts[0] == "src" and parts[1] in data["profiles"] else "default"
        profile = override.get("profile", profile)
    settings = data["profiles"][profile]
    flags = list(data["base_flags"])
    for layer in (settings, override):
        if "optimization" in layer:
            flags = [layer["optimization"] if re.fullmatch(r"-O[0-3s]?", f) else f for f in flags]
        flags = layer.get("prepend_flags", []) + flags + layer.get("append_flags", [])
        flags = [f for f in flags if f not in layer.get("remove_flags", [])]
    if nonmatching:
        flags.append("-DNONMATCHING")
    if fastmath and "-ffast-math" not in flags:
        flags.append("-ffast-math")
    resolved = []
    include_next = False
    for flag in flags:
        if include_next:
            resolved.append(str(ROOT / flag))
            include_next = False
        elif flag == "-I":
            resolved.append(flag)
            include_next = True
        elif flag.startswith("-I"):
            resolved.append("-I" + str(ROOT / flag[2:]))
        else:
            resolved.append(flag)
    return resolved


def compile_command(source: str | Path, output: str | Path,
                    profile: str | None = None, nonmatching: bool = False,
                    fastmath: bool = False, profile_source: str | Path | None = None) -> list[str]:
    # Stable .file spelling is load-bearing: absolute checkout paths changed KMC
    # GAS padding in a clean export of func_800453E0. Compile from ROOT.
    return [tool_path("gcc"), "-S", *profile_flags(profile_source or source, profile, nonmatching, fastmath),
            "-o", str(Path(output).resolve()), source_key(source)]


def compiler_environment() -> dict[str, str]:
    return {**os.environ, "COMPILER_PATH": str(Path(tool_path("gcc")).parent)}


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    environment = dict(os.environ)
    environment.pop("COMPILER_PATH", None)
    if command[0] == tool_path("gcc"):
        environment = compiler_environment()
    directory = ROOT if command[0] == tool_path("gcc") else work_dir()
    return subprocess.run(command, cwd=directory, env=environment, check=True, **kwargs)


def assembly_dependencies(path: Path, seen: set[Path] | None = None) -> set[Path]:
    """GAS .include inputs are invisible to a C preprocessor's dependency output."""
    seen = set() if seen is None else seen
    if path in seen:
        return seen
    seen.add(path)
    if path.suffix == ".bin":
        return seen
    for name in re.findall(r'^\s*\.(?:include|incbin)\s+"([^"]+)"', path.read_text(errors="replace"), re.MULTILINE):
        for directory in (path.parent, work_dir(), ROOT, ROOT / "include", ROOT / "src/libkmc"):
            candidate = directory / name
            if candidate.is_file():
                assembly_dependencies(candidate.resolve(), seen)
                break
        else:
            raise FileNotFoundError(f"assembly include {name!r} from {path}")
    return seen


def write_depfile(depfile: Path, output: Path, source: Path,
                  flags: list[str], assembly: Path | None = None) -> None:
    proc = run([tool_path("gcc"), "-M", *flags, str(source)], capture_output=True, text=True)
    # GCC 2.7.2 predates -MT/-MF; replace only its generated target field.
    dependencies = proc.stdout.split(":", 1)[1].strip()
    extra = assembly_dependencies(assembly) - {assembly} if assembly else set()
    escape = lambda p: str(p).replace("\\", "\\\\").replace(" ", "\\ ").replace("#", "\\#").replace("$", "$$")
    depfile.write_text(f"{escape(output)}: {dependencies} " + " ".join(escape(p) for p in sorted(extra)) + "\n")


def compile_source(source: str | Path, output: str | Path, profile: str | None = None,
                   nonmatching: bool = False, fastmath: bool = False,
                   assembly_only: bool = False, extra_flags=(), depfile: str | Path | None = None,
                   profile_source: str | Path | None = None, *,
                   capture_output: bool = False) -> None:
    source, output = source_path(source), Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    work_dir().mkdir(parents=True, exist_ok=True)
    assembly = output if assembly_only else output.with_suffix(output.suffix + ".s")
    temporary = output.with_suffix(output.suffix + ".tmp")
    command = compile_command(source, assembly, profile, nonmatching, fastmath, profile_source)
    command[-3:-3] = list(extra_flags)
    diagnostics = {"capture_output": True, "text": True} if capture_output else {}
    try:
        run(command, **diagnostics)
        if not assembly_only:
            run([tool_path("kmc_as"), "-EB", "-mips2", "-G", "0", "-I", str(ROOT / "include"),
                 "-I", str(ROOT), "-I", str(work_dir()), "-o", str(temporary), str(assembly)],
                **diagnostics)
            run([tool_path("strip"), str(temporary), "-N", "dummy-symbol-name"], **diagnostics)
        if depfile:
            write_depfile(Path(depfile), output, source,
                          profile_flags(profile_source or source, profile, nonmatching, fastmath) + list(extra_flags), assembly)
        if not assembly_only:
            temporary.replace(output)
    except BaseException:
        output.unlink(missing_ok=True)
        raise
    finally:
        if not assembly_only:
            assembly.unlink(missing_ok=True)
        temporary.unlink(missing_ok=True)


ALIGN = [f"--set-section-alignment={s}=4" for s in (".text", ".data", ".rodata", ".bss")]


def assemble_source(source: Path, output: Path, depfile: Path) -> None:
    source, output = source.resolve(), output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    preprocessed, temporary = output.with_suffix(".i"), output.with_suffix(".tmp")
    try:
        if source.is_relative_to(ROOT / "src/libkmc"):
            run([tool_path("kmc_as"), "-EB", "-mips2", "-I", str(ROOT / "src/libkmc"), "-o", str(temporary), str(source)])
            dependencies = assembly_dependencies(source)
            temporary.replace(output)
        elif source.is_relative_to(ROOT / "src/libultra"):
            flags = ["-x", "assembler-with-cpp", "-w", "-nostdinc", "-c", "-G", "0", "-mips3", "-mgp32", "-mfp32",
                     "-DMIPSEB", "-D_LANGUAGE_ASSEMBLY", "-D_MIPS_SIM=1", "-D_ULTRA64", "-D_MIPS_SZLONG=32", "-D__USE_ISOC99",
                     "-DBUILD_VERSION=VERSION_J", "-D_FINALROM"]
            for directory in ("include", "include/libultra", "include/libultra/PR", "include/libultra/compiler/gcc"):
                flags += ["-I", str(ROOT / directory)]
            run([tool_path("gcc"), *flags, "-o", str(temporary), str(source)])
            run([tool_path("objcopy"), *ALIGN, str(temporary), str(output)])
            write_depfile(depfile, output, source, [f for f in flags if f != "-c"])
            return
        else:
            flags = ["-fno-dollars-in-identifiers", "-P", "-I", str(ROOT / "include"),
                     "-DMIPSEB", "-D_LANGUAGE_ASSEMBLY", "-D_ULTRA64"]
            run([tool_path("cpp"), *flags, str(source), str(preprocessed)])
            run([tool_path("as"), "-march=vr4300", "-32", "-I", str(ROOT / "include"), "--no-pad-sections",
                 "-o", str(temporary), str(preprocessed)])
            run([tool_path("objcopy"), *ALIGN, str(temporary), str(output)])
            dependencies = assembly_dependencies(preprocessed) - {preprocessed}
            dependencies.add(source)
            # Modern CPP records preprocessing includes, too.
            proc = run([tool_path("cpp"), "-M", "-MT", str(output), *flags, str(source)], capture_output=True, text=True)
            depfile.write_text(proc.stdout.rstrip() + " " + " ".join(str(p).replace(" ", "\\ ") for p in sorted(dependencies)) + "\n")
            return
        depfile.write_text(str(output) + ": " + " ".join(str(p).replace(" ", "\\ ") for p in sorted(dependencies)) + "\n")
    except BaseException:
        output.unlink(missing_ok=True)
        raise
    finally:
        preprocessed.unlink(missing_ok=True)
        temporary.unlink(missing_ok=True)


def object_target(path: str) -> str:
    p = Path(path)
    if p.is_absolute():
        p = p.relative_to(build_dir() / "obj")
    text = p.as_posix()
    for prefix in ("build/us/obj/", "obj/"):
        if text.startswith(prefix):
            text = text[len(prefix):]
    return "mg64_obj_" + re.sub(r"[^A-Za-z0-9]", "_", str(Path(text).with_suffix(".o")))


def import_permuter(source: str, assembly: str, input_source: str | None, arguments: list[str]) -> None:
    flags = profile_flags(source, nonmatching=True)
    command = shlex.join(["tools/kmc_permuter_compile.sh", "--profile-source", source_key(source), *flags])
    portable = []
    skip_include = False
    for flag in flags:
        if skip_include:
            skip_include = False
        elif flag == "-I":
            skip_include = True
        elif not flag.startswith("-I"):
            portable.append(flag)
    template = (ROOT / "tools/permuter.toml.in").read_text()
    template = template.replace("@COMPILER_COMMAND@", json.dumps(command))
    template = template.replace("@COMPILER_KEY@", json.dumps(command))
    template = template.replace("@DECOMPME_FLAGS@", json.dumps(shlex.join(portable)))
    with tempfile.TemporaryDirectory(prefix="mg64-permuter-") as temporary:
        settings = Path(temporary) / "settings.toml"
        settings.write_text(template)
        subprocess.run([str(python_path()), str(ROOT / "tools/decomp-permuter/import.py"),
                        "--settings", str(settings), str(source_path(input_source or source)),
                        str(source_path(assembly)), *arguments], cwd=ROOT, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    for name in ("compile", "command"):
        p = sub.add_parser(name)
        p.add_argument("--source", required=True)
        p.add_argument("--output", required=True)
        p.add_argument("--profile")
        p.add_argument("--profile-source", help="Parent translation unit supplying per-file flags")
        p.add_argument("--nonmatching", action="store_true")
        p.add_argument("--fastmath", action="store_true")
        p.add_argument("--assembly-only", action="store_true")
        p.add_argument("--extra-flag", action="append", default=[])
        p.add_argument("--depfile")
    p = sub.add_parser("assemble")
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--depfile", type=Path, required=True)
    p = sub.add_parser("build")
    p.add_argument("artifact", nargs="?")
    p.add_argument("--object")
    sub.add_parser("paths")
    p = sub.add_parser("import-permuter")
    p.add_argument("--source", required=True)
    p.add_argument("--asm", required=True)
    p.add_argument("--input")
    p.add_argument("import_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.action == "paths":
        print(json.dumps(build_tools()))
    elif args.action == "import-permuter":
        arguments = args.import_args[1:] if args.import_args[:1] == ["--"] else args.import_args
        import_permuter(args.source, args.asm, args.input, arguments)
    elif args.action == "build":
        target = args.object or args.artifact
        cmd = ["cmake", "--build", str(build_dir())]
        if target and not target.endswith((".z64", ".elf")):
            cmd += ["--target", object_target(target)]
        subprocess.run(cmd, check=True, cwd=ROOT)
    elif args.action == "assemble":
        assemble_source(args.source, args.output, args.depfile)
    elif args.action == "command":
        print(json.dumps(compile_command(args.source, args.output, args.profile, args.nonmatching, args.fastmath, args.profile_source)))
    else:
        compile_source(args.source, args.output, args.profile, args.nonmatching, args.fastmath,
                       args.assembly_only, args.extra_flag, args.depfile, args.profile_source)


if __name__ == "__main__":
    main()
