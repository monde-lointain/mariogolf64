#!/usr/bin/env python3
"""Create and operate the local, derived Mario Golf 64 Ghidra database."""

from __future__ import annotations

import argparse
import contextlib
import copy
import fcntl
import hashlib
import json
import os
import re
import signal
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

import decomp_common as dc
import yaml
from m2ctx import CPP_FLAGS
from pycparser import c_ast, c_generator, c_parser
from rom_build import profile_flags

ROOT = dc.ROOT_DIR
SCRIPTS = ROOT / "tools" / "ghidra"
PROJECT = "mariogolf64"
LANGUAGE = "MIPS:BE:64:64-32addr"
COMPILER = "o32"
SYMBOL_RE = re.compile(r"^\s*(\w+)\s*=\s*0x([\da-fA-F]+)\s*;\s*(.*)")
QUALIFIER_RE = re.compile(r"\b(rom|size|type|segment):([^\s;]+)")
ASM_RE = re.compile(
    r"nonmatching\s+(\w+),\s*0x([\da-fA-F]+)\s*\n\s*\n?"
    r"glabel\s+\1\s*\n\s*/\*\s*([\da-fA-F]+)\s+([\da-fA-F]+)\s+"
)
C_TOKEN_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{};()]|[^{};()"\']+')


def preprocessing_flags(source: Path) -> list[str]:
    """Keep the ROM profile's include/define/undefine order, not host CPU options."""
    flags = []
    include_next = False
    for flag in profile_flags(source):
        if include_next or flag == "-I" or flag.startswith(("-I", "-D", "-U")):
            flags.append(flag)
        include_next = flag == "-I"
    flags.extend(
        flag
        for flag in CPP_FLAGS
        if flag.startswith("-D")
        and flag not in ("-D_FINALROM", "-D_LANGUAGE_C", "-DF3DEX_GBI")
    )
    return [*flags, "-ffreestanding", "-std=gnu89"]


def declaration_context(
    text: str,
    definitions: set[str] | None = None,
    definition_lines: set[int] | None = None,
) -> str:
    """Remove function bodies, not structure/union/initializer braces, from CPP output."""
    # These compiler/linker directives do not affect C layouts. Keep pack pragmas.
    text = re.sub(
        r"^[ \t]*#(?:ident\b|pragma[ \t]+weak\b)[^\n]*", "", text, flags=re.MULTILINE
    )
    result: list[str] = []
    statement: list[str] = []
    depth = 0
    skipping = False
    knr: list[str] = []
    knr_name = ""
    for token_match in C_TOKEN_RE.finditer(text):
        token = token_match.group()
        if token == "{":
            if depth == 0:
                prefix = "".join(statement).strip()
                if knr:
                    signature = "".join([*knr, *statement])
                    if re.match(rf"^\s*{re.escape(knr_name)}\s*\(", signature):
                        signature = "int " + signature.lstrip()
                    result.append(signature + "\n{}\n")
                    skipping = True
                    if definitions is not None and (
                        definition_lines is None
                        or text.count("\n", 0, token_match.start()) + 1
                        in definition_lines
                    ):
                        definitions.add(knr_name)
                    knr = []
                    statement = []
                else:
                    skipping = prefix.endswith(")") and "=" not in prefix
                    if skipping:
                        if definitions is not None and (
                            definition_lines is None
                            or text.count("\n", 0, token_match.start()) + 1
                            in definition_lines
                        ):
                            match = re.search(r"\b(\w+)\s*\([^;]*\)\s*$", prefix)
                            if match:
                                definitions.add(match.group(1))
                        result.append("".join(statement) + ";\n")
                        statement = []
            depth += 1
        elif token == "}":
            depth -= 1
            if skipping and depth == 0:
                skipping = False
                continue
            if statement and statement[-1].rstrip().endswith(","):
                statement[-1] = statement[-1].rstrip()[:-1]
        if skipping:
            continue
        statement.append(token)
        if token == ";" and depth == 0:
            declaration = "".join(statement)
            old_style = re.match(
                r"(?s)^\s*(.*?\b(\w+)\s*\(\s*\w+\s*(?:,\s*\w+\s*)*\))\s+[^;]+;\s*$",
                declaration,
            )
            if knr:
                knr.append(declaration)
            elif old_style and "=" not in old_style.group(1):
                knr = [declaration]
                knr_name = old_style.group(2)
            else:
                result.append(declaration)
            statement = []
    if depth != 0:
        raise ValueError("Unbalanced preprocessed C braces")
    return remove_initializers("".join(result + statement).replace("__extension__", ""))


def remove_initializers(text: str) -> str:
    """C values are ROM-owned; import declarators and enum values, not definitions."""
    tokens = re.findall(
        r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{};(),=\[\]]|[^{};(),=\[\]"\']+', text
    )
    output = []
    braces = parentheses = brackets = 0
    skipping = False
    for token in tokens:
        top = braces == parentheses == brackets == 0
        if token == "=" and top:
            skipping = True
            continue
        if token in (",", ";") and top:
            skipping = False
        if not skipping:
            output.append(token)
        if token == "{":
            braces += 1
        elif token == "}":
            braces -= 1
        elif token == "(":
            parentheses += 1
        elif token == ")":
            parentheses -= 1
        elif token == "[":
            brackets += 1
        elif token == "]":
            brackets -= 1
    return "".join(output)


def global_type_bindings(context: str) -> tuple[str, list[dict]]:
    """Turn variable declarators into type aliases; Ghidra's declaration map is empty."""
    parsed = c_parser.CParser().parse(context)
    generator = c_generator.CGenerator()
    declarations, bindings = [], []
    known_names = {node.name for node in parsed.ext if isinstance(node, c_ast.Typedef)}
    for node in parsed.ext:
        if (
            not isinstance(node, c_ast.Decl)
            or not node.name
            or isinstance(node.type, c_ast.FuncDecl)
        ):
            continue
        alias = "__mg64_global_" + node.name
        datatype = copy.deepcopy(node.type)
        incomplete = isinstance(datatype, c_ast.ArrayDecl) and datatype.dim is None
        if incomplete:
            datatype = datatype.type
        binding = {
            "name": node.name,
            "incomplete_array": incomplete,
            "static": "static" in node.storage,
        }
        if (
            isinstance(datatype, c_ast.TypeDecl)
            and isinstance(datatype.type, c_ast.IdentifierType)
            and len(datatype.type.names) == 1
            and datatype.type.names[0] in known_names
        ):
            binding["type_name"] = datatype.type.names[0]
        else:
            inner = datatype
            while not isinstance(inner, c_ast.TypeDecl):
                inner = inner.type
            inner.declname = alias
            typedef = c_ast.Typedef(alias, [], ["typedef"], datatype)
            declarations.append(generator.visit(typedef) + ";")
            binding["alias"] = alias
        bindings.append(binding)
    return "\n".join(declarations), bindings


def segment_layout(config: dict) -> list[dict]:
    """Translate Splat intervals, including zero-filled BSS, without flattening banks."""
    segments = config["segments"]
    result = []
    for index, segment in enumerate(segments):
        if not isinstance(segment, dict) or segment["type"] != "code":
            continue
        following = segments[index + 1]
        end = following["start"] if isinstance(following, dict) else following[0]
        name, start, vram = segment["name"], segment["start"], segment["vram"]
        regions = []
        source_paths = []
        data_sources: dict[str, list[list[int]]] = {}
        for sub_index, entry in enumerate(segment["subsegments"]):
            sub = (
                dict(zip(("start", "type", "name"), entry, strict=False))
                if isinstance(entry, list)
                else entry
            )
            kind = sub["type"].lstrip(".")
            if kind == "bss":
                regions.append(
                    {
                        "name": f"{name}_bss",
                        "address": sub["vram"],
                        "length": segment["bss_size"],
                        "offset": -1,
                        "execute": False,
                        "write": True,
                    }
                )
                continue
            sub_start = sub["start"]
            next_sub = (
                segment["subsegments"][sub_index + 1]
                if sub_index + 1 < len(segment["subsegments"])
                else None
            )
            sub_end = (
                (
                    next_sub.get("start", end)
                    if isinstance(next_sub, dict)
                    else next_sub[0]
                )
                if next_sub
                else end
            )
            execute = kind in ("asm", "hasm", "c")
            section = (
                "text"
                if execute
                else ("rodata" if kind in ("rodata", "textbin", "databin") else "data")
            )
            region = {
                "name": f"{name}_{section}",
                "address": vram + sub_start - start,
                "length": sub_end - sub_start,
                "offset": sub_start,
                "execute": execute,
                "write": section == "data",
            }
            if region["length"] <= 0:
                raise ValueError(f"Invalid {name} interval at 0x{sub_start:X}")
            if (
                regions
                and regions[-1]["name"] == region["name"]
                and regions[-1]["offset"] + regions[-1]["length"] == sub_start
            ):
                regions[-1]["length"] += region["length"]
            else:
                region["name"] += f"_{sub_start:X}"
                # Compare section semantics rather than the unique block name.
                if (
                    regions
                    and regions[-1]["execute"] == execute
                    and regions[-1]["write"] == region["write"]
                    and regions[-1]["offset"] >= 0
                    and regions[-1]["address"] + regions[-1]["length"]
                    == region["address"]
                ):
                    regions[-1]["length"] += region["length"]
                else:
                    regions.append(region)
            if sub.get("name") and (ROOT / "src" / f"{sub['name']}.c").is_file():
                source_paths.append(f"src/{sub['name']}.c")
                if not execute:
                    data_sources.setdefault(f"src/{sub['name']}.c", []).append(
                        [vram + sub_start - start, vram + sub_end - start]
                    )
        result.append(
            {
                "name": name,
                "start": start,
                "end": end,
                "vram": vram,
                "regions": regions,
                "sources": sorted(set(source_paths)),
                "data_sources": data_sources,
            }
        )
    return result


def read_symbols(root: Path = ROOT) -> list[dict]:
    """Retain aliases; the maintainer file owns the primary name at a shared address."""
    symbols = []
    for filename in ("symbol_addrs.txt", "ghidra_symbols.txt", "hardware_regs.txt"):
        for line in (root / filename).read_text().splitlines():
            match = SYMBOL_RE.match(line)
            if not match:
                continue
            name, address, comment = match.groups()
            qualifiers = dict(QUALIFIER_RE.findall(comment))
            symbols.append(
                {
                    "name": name,
                    "address": int(address, 16),
                    "source": filename,
                    "rom": int(qualifiers["rom"], 0) if "rom" in qualifiers else None,
                    "segment": qualifiers.get("segment"),
                    "function": qualifiers.get("type") == "func",
                    "size": int(qualifiers["size"], 0) if "size" in qualifiers else 0,
                }
            )
    return symbols


def assembly_functions(
    layout: list[dict], asm_dirs: list[Path]
) -> dict[str, list[dict]]:
    functions: dict[str, dict[int, dict]] = {segment["name"]: {} for segment in layout}
    # Current build extraction wins; the old extraction only fills absent boundaries.
    for directory in asm_dirs:
        for path in sorted(directory.rglob("*.s")):
            if (
                "data" in path.relative_to(directory).parts
                or "matching" in path.relative_to(directory).parts
            ):
                continue
            for match in ASM_RE.finditer(path.read_text(errors="replace")):
                name, size, rom, address = match.groups()
                rom, address, size = int(rom, 16), int(address, 16), int(size, 16)
                for segment in layout:
                    if segment["start"] <= rom < segment["end"]:
                        if address != segment["vram"] + rom - segment["start"]:
                            raise ValueError(
                                f"Assembly/YAML address disagreement: {path}: {name}"
                            )
                        executable = any(
                            r["execute"]
                            and r["address"] <= address < r["address"] + r["length"]
                            for r in segment["regions"]
                        )
                        if executable:
                            functions[segment["name"]].setdefault(
                                address,
                                {
                                    "name": name,
                                    "address": address,
                                    "size": size,
                                    "rom": rom,
                                    "segment": segment["name"],
                                },
                            )
                        break
    return {
        name: sorted(records.values(), key=lambda f: f["address"])
        for name, records in functions.items()
    }


def assembly_data_symbols(layout: list[dict], directories: list[Path]) -> list[dict]:
    records = {}
    initialized = re.compile(
        r"dlabel\s+(\w+)\s*\n\s*/\*\s*([\da-fA-F]+)\s+([\da-fA-F]+)\s+"
    )
    bss = re.compile(
        r"dlabel\s+(\w+)\s*\n\s*/\*\s*([\da-fA-F]+)\s*\*/\s*\.space\s+(0x[\da-fA-F]+)"
    )
    for directory in directories:
        for path in sorted(directory.rglob("*.s")):
            content = path.read_text(errors="replace")
            for match in initialized.finditer(content):
                name, rom, address = match.groups()
                rom, address = int(rom, 16), int(address, 16)
                owners = [
                    s["name"]
                    for s in layout
                    if s["start"] <= rom < s["end"]
                    and s["vram"] + rom - s["start"] == address
                ]
                if len(owners) == 1:
                    records.setdefault(
                        (name, owners[0]),
                        {
                            "name": name,
                            "address": address,
                            "rom": rom,
                            "segment": owners[0],
                            "function": False,
                            "size": 0,
                            "source": "extracted assembly",
                        },
                    )
            for match in bss.finditer(content):
                name, address, size = match.groups()
                address = int(address, 16)
                owners = [
                    s["name"]
                    for s in layout
                    if path.name == f"{s['end']:X}.bss.s"
                    and any(
                        r["offset"] == -1
                        and r["address"] <= address < r["address"] + r["length"]
                        for r in s["regions"]
                    )
                ]
                if len(owners) == 1:
                    records.setdefault(
                        (name, owners[0]),
                        {
                            "name": name,
                            "address": address,
                            "rom": None,
                            "segment": owners[0],
                            "function": False,
                            "size": int(size, 16),
                            "source": "extracted assembly",
                        },
                    )
    return list(records.values())


def bind_symbol_ownership(
    symbols: list[dict], layout: list[dict], units: list[dict]
) -> list[dict]:
    """Absence of a bank qualifier is not proof that a reused address is shared."""
    definitions: dict[str, set[str]] = {}
    for unit in units:
        for name in unit["definitions"]:
            definitions.setdefault(name, set()).add(unit["segment"])
    extracted: dict[tuple[str, int], set[str]] = {}
    for symbol in symbols:
        if symbol["source"] == "extracted assembly":
            extracted.setdefault((symbol["name"], symbol["address"]), set()).add(
                symbol["segment"]
            )
    ambiguous = []
    for symbol in symbols:
        if symbol["segment"] or symbol["source"] == "hardware_regs.txt":
            continue
        candidates = {
            s["name"]
            for s in layout
            if any(
                r["address"] <= symbol["address"] < r["address"] + r["length"]
                for r in s["regions"]
            )
        }
        if symbol["rom"] is not None:
            candidates &= {
                s["name"] for s in layout if s["start"] <= symbol["rom"] < s["end"]
            }
        prefix = re.search(r"(?:overlay_?|ovl)(\d+|gap)_", symbol["name"])
        if prefix:
            candidates = {"overlay_" + prefix.group(1)} & {s["name"] for s in layout}
        defining = definitions.get(symbol["name"], set()) & candidates
        if defining:
            candidates = defining
        elif (
            len(extracted.get((symbol["name"], symbol["address"]), set()) & candidates)
            == 1
        ):
            candidates &= extracted[(symbol["name"], symbol["address"])]
        if len(candidates) == 1:
            symbol["segment"] = next(iter(candidates))
        elif len(candidates) > 1:
            ambiguous.append(
                {
                    "name": symbol["name"],
                    "address": symbol["address"],
                    "candidates": sorted(candidates),
                }
            )
            symbol["segment"] = "ambiguous"
        else:
            symbol["segment"] = "unmapped"
    committed = {
        (s["name"], s["address"]): s["segment"]
        for s in symbols
        if s["source"] != "extracted assembly"
    }
    for symbol in symbols:
        if symbol["source"] != "extracted assembly":
            continue
        owner = committed.get((symbol["name"], symbol["address"]))
        if (
            owner is not None
            and owner != symbol["segment"]
            and symbol["segment"].startswith("overlay_")
        ):
            suffix = symbol["segment"].removeprefix("overlay_")
            symbol["name"] = f"D_ovl{suffix}_{symbol['address']:08X}"
    return ambiguous


def make_manifest(state: Path) -> dict:
    config = yaml.safe_load((ROOT / "mariogolf64.yaml").read_text())
    rom = ROOT / config["options"]["target_path"]
    digest = hashlib.sha1(rom.read_bytes()).hexdigest()
    if digest != config["sha1"]:
        raise ValueError("Original ROM does not match mariogolf64.yaml")
    layout = segment_layout(config)
    functions = assembly_functions(layout, [dc.ASM_DIR, ROOT / "asm"])
    if not all(
        functions[s["name"]] for s in layout if any(r["execute"] for r in s["regions"])
    ):
        raise ValueError(
            "Missing assembly boundaries; run the project's extract target first"
        )
    symbols = read_symbols()
    units = []
    inputs = state / "inputs"
    inputs.mkdir(parents=True, exist_ok=True)
    baseline = inputs / "sdk.c"
    baseline.write_text('#include "common.h"\n#include "nusys.h"\n')
    sources = [
        ("sdk", baseline),
        *[(s["name"], ROOT / p) for s in layout for p in s["sources"]],
    ]
    for index, (segment, source) in enumerate(sources):
        cpp = subprocess.run(
            ["gcc", "-E", *preprocessing_flags(source), str(source)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if cpp.returncode:
            raise RuntimeError(f"Preprocessing {source} failed:\n{cpp.stderr}")
        lines, own_lines = [], set()
        current_source = None
        for line in cpp.stdout.splitlines():
            marker = re.match(r'#\s+\d+\s+"([^"]+)"', line)
            if marker:
                current_source = (ROOT / marker.group(1)).resolve()
                continue
            lines.append(line)
            if current_source == source.resolve():
                own_lines.add(len(lines))
        definitions: set[str] = set()
        path = inputs / f"types_{index}.h"
        context = declaration_context("\n".join(lines), definitions, own_lines)
        path.write_text(context)
        try:
            binding_types, bindings = global_type_bindings(context)
        except c_parser.ParseError as error:
            raise RuntimeError(
                f"C declaration extraction failed for {source}: {error}; inspect {path}"
            ) from error
        path.write_text(context + "\n" + binding_types + "\n")
        units.append(
            {
                "segment": segment,
                "source": str(source.relative_to(ROOT))
                if source.is_relative_to(ROOT)
                else "SDK headers",
                "path": str(path),
                "definitions": sorted(definitions),
                "globals": bindings,
                "data_ranges": next(
                    (
                        s["data_sources"].get(str(source.relative_to(ROOT)), [])
                        for s in layout
                        if s["name"] == segment
                    ),
                    [],
                ),
            }
        )
    symbols.extend(assembly_data_symbols(layout, [dc.ASM_DIR / "data"]))
    ambiguous = bind_symbol_ownership(symbols, layout, units)
    (state / "ambiguous-symbols.json").write_text(
        json.dumps(ambiguous, indent=2) + "\n"
    )
    for segment in layout:
        for function in functions[segment["name"]]:
            candidates = [
                s
                for s in symbols
                if s["function"]
                and s["address"] == function["address"]
                and s["segment"] == segment["name"]
            ]
            if candidates:
                function["name"] = candidates[0]["name"]
            elif segment["name"].startswith("overlay_"):
                suffix = segment["name"].removeprefix("overlay_")
                function["name"] = f"func_ovl{suffix}_{function['address']:08X}"
    resident = [s for s in layout if s["name"] in ("entry", "main")]
    banks = [s for s in layout if s not in resident]
    programs = [
        {"name": "main", "segments": [s["name"] for s in resident]},
        *[
            {"name": s["name"], "segments": [*[r["name"] for r in resident], s["name"]]}
            for s in banks
        ],
    ]
    result = {
        "rom": str(rom),
        "rom_sha1": digest,
        "language": LANGUAGE,
        "compiler": COMPILER,
        "project_dir": str(state),
        "project_name": PROJECT,
        "segments": layout,
        "programs": programs,
        "functions": functions,
        "symbols": symbols,
        "units": units,
    }
    identity = {
        **result,
        "unit_contents": [
            hashlib.sha256(Path(u["path"]).read_bytes()).hexdigest() for u in units
        ],
    }
    result["manifest_id"] = hashlib.sha256(
        json.dumps(identity, sort_keys=True).encode()
    ).hexdigest()
    return result


def classpath(ghidra_home: Path, mcp_root: Path, state: Path) -> str:
    plugin = mcp_root / "target" / "GhidraMCP-7.0.0.jar"
    if not plugin.is_file():
        raise FileNotFoundError(f"Installed MCP JAR not found: {plugin}")
    jars = [plugin]
    for section in ("Framework", "Features", "Processors"):
        jars.extend(sorted((ghidra_home / "Ghidra" / section).glob("*/lib/*.jar")))
    return os.pathsep.join(map(str, [state / "classes", *jars]))


def java_command(args, command: str, *extra: str) -> list[str]:
    cp = classpath(args.ghidra_home, args.mcp_root, args.state_dir)
    classes = args.state_dir / "classes"
    classes.mkdir(parents=True, exist_ok=True)
    source = SCRIPTS / "Mg64Project.java"
    target = classes / "Mg64Project.class"
    if not target.exists() or target.stat().st_mtime < source.stat().st_mtime:
        subprocess.run(
            ["javac", "-cp", cp, "-d", str(classes), str(source)], check=True
        )
    return [
        "java",
        "-Xmx4g",
        f"-Dghidra.home={args.ghidra_home}",
        "-Dapplication.name=GhidraMCP",
        "-cp",
        cp,
        "Mg64Project",
        command,
        str(args.state_dir / "manifest.json"),
        *extra,
    ]


@contextlib.contextmanager
def database_lock(state: Path):
    state.mkdir(parents=True, exist_ok=True)
    with (state / "operation.lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise RuntimeError(
                "Ghidra database is in use; stop its headless service first"
            ) from exc
        yield lock


@contextlib.contextmanager
def control_lock(state: Path, name: str):
    state.mkdir(parents=True, exist_ok=True)
    with (state / name).open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def request(
    args,
    path: str,
    params: dict | None = None,
    *,
    post: bool = False,
    query: dict | None = None,
):
    url = f"http://127.0.0.1:{args.port}/{path}"
    data = json.dumps(params or {}).encode() if post else None
    query = query if post else params
    if query:
        url += "?" + urllib.parse.urlencode(query)
    headers = {"Content-Type": "application/json"}
    token = os.environ.get("GHIDRA_MCP_AUTH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(
        urllib.request.Request(url, data=data, headers=headers), timeout=180
    ) as response:
        text = response.read().decode()
    try:
        result = json.loads(text)
    except json.JSONDecodeError:
        result = text
    if isinstance(result, dict) and (
        "error" in result or result.get("success") is False
    ):
        raise RuntimeError(str(result))
    return result


def script_json(result: dict) -> dict:
    for line in result["console_output"].splitlines():
        if "MG64_JSON:" in line:
            return json.loads(line.split("MG64_JSON:", 1)[1])
    raise RuntimeError(
        "Reviewed script produced no result: " + result["console_output"]
    )


def manifest_for(args) -> dict:
    return json.loads((args.state_dir / "manifest.json").read_text())


def require_ready(args) -> dict:
    ready = json.loads((args.state_dir / "ready.json").read_text())
    manifest = manifest_for(args)
    if (
        not ready.get("manifest_id")
        or ready["manifest_id"] != manifest.get("manifest_id")
        or ready["rom_sha1"] != manifest["rom_sha1"]
    ):
        raise RuntimeError(
            "Database import is incomplete or does not match its manifest"
        )
    return ready


def checked_server(args) -> dict:
    result = request(args, "get_project_info")
    if result.get("project_name") != PROJECT:
        raise RuntimeError(f"Port {args.port} serves another project: {result}")
    identity = script_json(
        request(
            args,
            "run_ghidra_script",
            {
                "script_name": str(SCRIPTS / "Mg64Context.java"),
                "args": "identity",
                "timeout_seconds": 120,
            },
            post=True,
            query={"program": "main"},
        )
    )
    if Path(identity["manifest"]).resolve() != args.state_dir / "manifest.json":
        raise RuntimeError("Ghidra service belongs to another checkout")
    if identity["rom_sha1"] != manifest_for(args)["rom_sha1"]:
        raise RuntimeError("Ghidra service belongs to another ROM")
    if identity["manifest_id"] != manifest_for(args)["manifest_id"]:
        raise RuntimeError(
            "Ghidra service has a different source snapshot; reconnect after refresh"
        )
    return result


def open_program(args, program: str):
    manifest = manifest_for(args)
    if program not in {p["name"] for p in manifest["programs"]}:
        raise ValueError(f"Unknown program: {program}; use an exact manifest name")
    checked_server(args)
    opened = request(args, "list_open_programs")
    if program not in {p["name"] for p in opened["programs"]}:
        request(args, "load_program_from_project", {"path": f"/{program}"}, post=True)


def resolve_function(
    manifest: dict, reference: str, segment: str | None = None, rom: int | None = None
) -> dict:
    records = [
        f
        for name, functions in manifest["functions"].items()
        for f in functions
        if segment in (None, name)
    ]
    try:
        address = int(reference, 16) if reference.startswith("0x") else None
    except ValueError as exc:
        raise ValueError(f"Invalid address: {reference}") from exc
    matches = [
        f
        for f in records
        if (f["name"] == reference or f["address"] == address)
        and rom in (None, f["rom"])
    ]
    if len(matches) != 1:
        raise ValueError(
            f"Expected one exact function, found {len(matches)}; qualify reused VRAM with --segment or --rom"
        )
    function = dict(matches[0])
    function["program"] = (
        "main" if function["segment"] in ("entry", "main") else function["segment"]
    )
    return function


def script_result(args, program: str, mode: str, address: int = 0) -> dict:
    open_program(args, program)
    result = request(
        args,
        "run_ghidra_script",
        {
            "script_name": str(SCRIPTS / "Mg64Context.java"),
            "args": f"{mode} 0x{address:X}",
            "timeout_seconds": 120,
        },
        post=True,
        query={"program": program},
    )
    return script_json(result)


def _start_server(args):
    require_ready(args)
    try:
        checked_server(args)
        return
    except (urllib.error.URLError, ConnectionError):
        pass
    with database_lock(args.state_dir) as lock:
        command = java_command(args, "serve", str(args.port))
        environment = dict(
            os.environ,
            GHIDRA_MCP_ALLOW_SCRIPTS="1",
            GHIDRA_MCP_BIND_ADDRESS="127.0.0.1",
            GHIDRA_MCP_FILE_ROOT=str(ROOT),
        )
        with (args.state_dir / "server.log").open("a") as log:
            child = subprocess.Popen(
                command,
                env=environment,
                stdout=log,
                stderr=log,
                start_new_session=True,
                pass_fds=(lock.fileno(),),
            )
        (args.state_dir / "server.pid").write_text(str(child.pid))
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            if child.poll() is not None:
                raise RuntimeError(
                    f"Ghidra server exited; read {args.state_dir / 'server.log'}"
                )
            try:
                checked_server(args)
                return
            except (urllib.error.URLError, ConnectionError):
                time.sleep(0.25)
        child.terminate()
        child.wait(timeout=15)
        raise RuntimeError(
            f"Ghidra startup timed out; read {args.state_dir / 'server.log'}"
        )


def _stop_server(args):
    pid_path = args.state_dir / "server.pid"
    if not pid_path.exists():
        raise RuntimeError("No server PID recorded for this checkout")
    pid = int(pid_path.read_text())
    command = Path(f"/proc/{pid}/cmdline")
    if command.exists():
        words = command.read_bytes().split(b"\0")
        if (
            b"Mg64Project" not in words
            or str(args.state_dir / "manifest.json").encode() not in words
        ):
            raise RuntimeError("Refusing to signal an unrelated process")
        os.kill(pid, signal.SIGTERM)
        for _ in range(100):
            if (
                not command.exists()
                or Path(f"/proc/{pid}/stat").read_text().split()[2] == "Z"
            ):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("Server did not stop; project was not force-killed")
    pid_path.unlink()


def start_server(args):
    with (
        control_lock(args.state_dir, "requests.lock"),
        control_lock(args.state_dir, "lifecycle.lock"),
    ):
        _start_server(args)


def stop_server(args):
    with (
        control_lock(args.state_dir, "requests.lock"),
        control_lock(args.state_dir, "lifecycle.lock"),
    ):
        _stop_server(args)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    homes = sorted(
        (Path.home() / ".local/opt/ghidra").glob("ghidra_*/support/analyzeHeadless")
    )
    default_home = homes[-1].parent.parent if homes else Path("/opt/ghidra")
    parser.add_argument(
        "--mcp-root",
        type=Path,
        default=Path(
            os.environ.get(
                "MG64_GHIDRA_MCP_ROOT", str(Path.home() / "projects/tools/ghidra-mcp")
            )
        ),
    )
    parser.add_argument("--ghidra-home", type=Path)
    parser.add_argument("--state-dir", type=Path, default=ROOT / "private" / "ghidra")
    parser.add_argument(
        "--port", type=int, default=int(os.environ.get("MG64_GHIDRA_PORT", "8089"))
    )
    commands = parser.add_subparsers(dest="command", required=True)
    for command in (
        "init",
        "refresh",
        "start",
        "stop",
        "status",
        "bridge",
        "sync-names",
    ):
        commands.add_parser(command)
    context = commands.add_parser("context")
    context.add_argument("--func", required=True)
    context.add_argument("--segment")
    context.add_argument("--rom", type=lambda s: int(s, 0))
    context.add_argument("--output", type=Path)
    archive = commands.add_parser("archive")
    archive.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.state_dir = args.state_dir.expanduser().resolve()
    args.mcp_root = args.mcp_root.expanduser().resolve()
    if args.ghidra_home is None:
        pinned = (
            json.loads((args.state_dir / "ready.json").read_text()).get("ghidra_home")
            if (args.state_dir / "ready.json").exists()
            else None
        )
        args.ghidra_home = Path(
            os.environ.get("MG64_GHIDRA_HOME") or pinned or default_home
        )
    args.ghidra_home = args.ghidra_home.expanduser().resolve()
    ready_path = args.state_dir / "ready.json"
    locks = contextlib.ExitStack()
    try:
        if args.command in ("context", "sync-names", "archive", "status"):
            locks.enter_context(control_lock(args.state_dir, "requests.lock"))
        if args.command in ("init", "refresh"):
            locks.enter_context(control_lock(args.state_dir, "lifecycle.lock"))
        if args.command == "init":
            with database_lock(args.state_dir):
                if (args.state_dir / f"{PROJECT}.gpr").exists():
                    require_ready(args)
                    dc.emit(
                        {
                            "ok": True,
                            "existing": True,
                            "project": str(args.state_dir / f"{PROJECT}.gpr"),
                        }
                    )
                    return
                manifest = make_manifest(args.state_dir)
                (args.state_dir / "manifest.json").write_text(
                    json.dumps(manifest, indent=2) + "\n"
                )
                subprocess.run(java_command(args, "init"), check=True)
                dc.emit(
                    {
                        "ok": True,
                        "project": str(args.state_dir / f"{PROJECT}.gpr"),
                        "programs": [p["name"] for p in manifest["programs"]],
                    }
                )
        elif args.command == "refresh":
            with database_lock(args.state_dir):
                if not (args.state_dir / f"{PROJECT}.gpr").is_file():
                    raise RuntimeError("No existing database to refresh")
                old = manifest_for(args)
                manifest = make_manifest(args.state_dir)
                if (
                    manifest["segments"] != old["segments"]
                    or manifest["programs"] != old["programs"]
                ):
                    # C file names can change without a byte-map change.
                    old_maps = [
                        {
                            k: v
                            for k, v in s.items()
                            if k in ("name", "start", "end", "vram", "regions")
                        }
                        for s in old["segments"]
                    ]
                    new_maps = [
                        {
                            k: v
                            for k, v in s.items()
                            if k in ("name", "start", "end", "vram", "regions")
                        }
                        for s in manifest["segments"]
                    ]
                    if old_maps != new_maps or manifest["programs"] != old["programs"]:
                        raise RuntimeError(
                            "Memory map changed; preserve this project and initialize a new --state-dir"
                        )
                (args.state_dir / "manifest.json").write_text(
                    json.dumps(manifest, indent=2) + "\n"
                )
                ready_path.unlink(missing_ok=True)
                subprocess.run(java_command(args, "refresh"), check=True)
                dc.emit(
                    {
                        "ok": True,
                        "refreshed": True,
                        "project": str(args.state_dir / f"{PROJECT}.gpr"),
                    }
                )
        elif args.command in ("start", "bridge"):
            start_server(args)
            if args.command == "bridge":
                python = args.mcp_root / ".venv/bin/python"
                os.execv(
                    str(python),
                    [
                        str(python),
                        str(ROOT / "tools/ghidra_mcp.py"),
                        "--state-dir",
                        str(args.state_dir),
                        "--port",
                        str(args.port),
                    ],
                )
            dc.emit(
                {
                    "ok": True,
                    "url": f"http://127.0.0.1:{args.port}",
                    "project": str(args.state_dir / f"{PROJECT}.gpr"),
                }
            )
        elif args.command == "stop":
            stop_server(args)
            dc.emit({"ok": True, "stopped": True})
        elif args.command == "status":
            dc.emit(
                {
                    "ok": True,
                    "project": checked_server(args),
                    "programs": request(args, "list_open_programs"),
                }
            )
        elif args.command == "context":
            function = resolve_function(
                manifest_for(args), args.func, args.segment, args.rom
            )
            result = script_result(
                args, function["program"], "context", function["address"]
            )
            result["origin"] = {**function, "rom_sha1": manifest_for(args)["rom_sha1"]}
            output = args.output or ROOT / "nonmatchings" / function["name"]
            output.mkdir(parents=True, exist_ok=True)
            for filename, content in (
                ("ghidra.c", result["decompiled"]),
                ("ghidra_ctx.c", result["context"]),
                ("ghidra.json", json.dumps(result, indent=2)),
            ):
                (output / filename).write_text(content + "\n")
            dc.emit({"ok": True, "output": str(output), "origin": result["origin"]})
        elif args.command == "sync-names":
            proposals = []
            manifest = manifest_for(args)
            for program in manifest["programs"]:
                result = script_result(args, program["name"], "symbols")
                expected = {
                    s["name"]: s["address"]
                    for s in manifest["symbols"]
                    if s["segment"] in program["segments"]
                }
                expected.update(
                    {
                        f["name"]: f["address"]
                        for segment in program["segments"]
                        for f in manifest["functions"][segment]
                    }
                )
                for symbol in result["symbols"]:
                    if (
                        not symbol["name"].startswith(
                            ("FUN_", "DAT_", "LAB_", "func_", "D_")
                        )
                        and expected.get(symbol["name"]) != symbol["address"]
                    ):
                        proposals.append({"program": program["name"], **symbol})
            path = args.state_dir / "name-proposals.json"
            path.write_text(json.dumps(proposals, indent=2) + "\n")
            dc.emit(
                {
                    "ok": True,
                    "proposals": str(path),
                    "count": len(proposals),
                    "symbol_files_unchanged": True,
                }
            )
        elif args.command == "archive":
            checked_server(args)
            output = args.output.expanduser().resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            if output.exists():
                raise FileExistsError(output)
            request(args, "save_all_programs")
            dc.emit(
                request(
                    args,
                    "archive_project",
                    {"output_dir": str(output.parent), "output_name": output.stem},
                    post=True,
                )
            )
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        if args.command == "bridge":
            dc.log(f"Ghidra bridge startup failed: {exc}")
        else:
            dc.emit({"ok": False, "error": str(exc)})
        raise SystemExit(1) from exc
    finally:
        locks.close()


if __name__ == "__main__":
    main()
