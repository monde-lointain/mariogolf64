#!/usr/bin/env python3
"""Write what editsearch needs to search one function's edits, to a directory.

    export_search.py FUNCTION OUT_DIR

editsearch (~/projects/tools/editsearch) compiles combinations of edits to a
draft, and scores each against the original as fncompare does. It runs the
compiler's passes itself, so this writes, to OUT_DIR:
- job.txt: the function's name, the repo's root and where the C file's
  definition and body start and end, as "key value" lines;
- source.c: the C file, as its bytes;
- before.i and after.i: the isolated, preprocessed file around the body,
  pruned to the declarations the function uses;
- cpp.args, cc1.args and as.args: each stage's command, an argument a line,
  with {input} and {output} for its files;
- targets.txt: the names of the function and of the functions nested in
  it that have an original routine, the function first;
- target.NAME.txt: each one's original routine, a line for each
  instruction holding its masked word, its destination and its
  reference, or "-" for none;
- symbols.txt: the name and address of each symbol the matching build
  names once;
- base.txt: fncompare's score of the draft, which editsearch checks its
  own against.

The isolated file keeps the function before the draft only where the
draft's words need it, as fncompare keeps it for the assembler's hazard
padding: without it, each compile takes the one function's time.
"""

import sys
import tempfile
from pathlib import Path

import try_variants

import fncompare

ARGUMENT_COUNT = 3
# What each stage's command starts with in the driver's -v output
STAGES = ("cc1", "as")
# The driver's flags for its own output, which editsearch's stages don't want
DRIVER_ONLY_FLAGS = frozenset(("-version", "-v"))


def write_lines(path: Path, lines: list[str]) -> None:
    """Write LINES to PATH, a line each."""
    _ = path.write_text("".join(f"{line}\n" for line in lines), encoding="latin-1")


def format_optional(value: int | None) -> str:
    """Return VALUE in hexadecimal, or "-" for None."""
    return "-" if value is None else f"{value:x}"


def cut_previous(isolated: str, function: str) -> str:
    """Return ISOLATED with the definition before FUNCTION's cut to a prototype.

    Inline functions and builtins stay, as isolate_functions keeps them.
    """
    definitions = fncompare.find_definitions(isolated)
    names = [name for name, _, _, _ in definitions]
    if function not in names:
        return isolated
    position = names.index(function)
    if position == 0:
        return isolated
    name, start, end, is_inline = definitions[position - 1]
    if is_inline or name in fncompare.BUILTIN_NAMES:
        return isolated
    header_start = isolated.rfind("\n", 0, start) + 1
    header = isolated[header_start:start].rstrip()
    cut = fncompare.NOT_LINE_MARKER_RE.sub(
        "", isolated[header_start + len(header) : end]
    )
    return isolated[:header_start] + header + ";" + cut + isolated[end:]


def compile_unit(unit: str, function: str, optflags: list[str]) -> fncompare.Functions:
    """Compile the preprocessed UNIT and return its routines."""
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "unit.i"
        _ = path.write_text(unit, encoding="latin-1")
        compiled, error = fncompare.compile_functions(path, optflags)
    if compiled is None or function not in compiled:
        sys.exit(f"the isolated unit doesn't compile {function}: {error}")
    return compiled


def write_routine(path: Path, routine: fncompare.Routine) -> None:
    """Write ROUTINE's instructions to PATH, as target.NAME.txt holds them."""
    write_lines(
        path,
        [
            f"{word:08x} {format_optional(destination)} {format_optional(reference)}"
            for word, destination, reference in zip(
                routine.words, routine.destinations, routine.references, strict=True
            )
        ],
    )


def read_stage_commands(unit: str, optflags: list[str]) -> dict[str, list[str]]:
    """Return the driver's cc1 and as commands for UNIT, with placeholders.

    The driver's -v output names each stage's input and output files, which
    become {input} and {output}.
    """
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "unit.i"
        _ = path.write_text(unit, encoding="latin-1")
        result = fncompare.run_compiler(
            path, optflags, ["-v", "-c", "-o", str(Path(tmp) / "unit.o")]
        )
    commands: dict[str, list[str]] = {}
    for line in result.stderr.splitlines():
        words = line.split()
        if not words or Path(words[0]).name not in STAGES:
            continue
        stage = Path(words[0]).name
        command = [words[0]]
        expects_output = False
        for word in words[1:]:
            if expects_output:
                command.append("{output}")
                expects_output = False
            elif word == "-o":
                command.append(word)
                expects_output = True
            elif word in DRIVER_ONLY_FLAGS:
                continue
            elif word.endswith((".i", ".s")) and Path(word).name.startswith(
                ("unit", "cc")
            ):
                command.append("{input}")
            else:
                command.append(word)
        commands[stage] = command
    if set(commands) != set(STAGES):
        sys.exit(f"the driver ran {sorted(commands)}:\n{result.stderr}")
    return commands


def main() -> None:
    """Write the job directory."""
    if len(sys.argv) != ARGUMENT_COUNT:
        sys.exit(__doc__)
    function, out_dir = sys.argv[1], Path(sys.argv[2])
    asm_path, c_path = fncompare.locate_function(function)
    source = Path(c_path).read_bytes().decode("latin-1")
    definition = try_variants.find_definition(source, function)
    if definition is None:
        sys.exit(f"{c_path} has no definition of {function}")
    definition_start, body_start, definition_end = definition
    optflags = fncompare.get_optflags(c_path)
    bodies = fncompare.BodyVariants(
        c_path, source, function, body_start, definition_end, optflags
    )
    body = bodies.preprocess_bodies([source[body_start:definition_end]])[0]
    mark = fncompare.BODY_MARK
    whole = bodies.before + mark + body + mark + bodies.after
    compiled = compile_unit(bodies.before + body + bodies.after, function, optflags)
    cut = fncompare.prune_declarations(cut_previous(whole, function), {function})
    before, _, rest = cut.partition(mark)
    after = rest.partition(mark)[2]
    if compile_unit(before + body + after, function, optflags) != compiled:
        before, after = bodies.before, bodies.after
    out_dir.mkdir(parents=True, exist_ok=True)
    _ = (out_dir / "source.c").write_bytes(source.encode("latin-1"))
    _ = (out_dir / "before.i").write_text(before, encoding="latin-1")
    _ = (out_dir / "after.i").write_text(after, encoding="latin-1")
    root = fncompare.ROOT
    c_dir = Path(c_path).resolve().parent.relative_to(root)
    write_lines(
        out_dir / "job.txt",
        [
            f"function {function}",
            f"root {root}",
            f"compiler_path {fncompare.KMC_DIR}",
            f"definition_start {definition_start}",
            f"body_start {body_start}",
            f"definition_end {definition_end}",
        ],
    )
    write_lines(
        out_dir / "cpp.args",
        [
            str(fncompare.KMC_DIR / "gcc"),
            *fncompare.CFLAGS,
            *optflags,
            f"-I{c_dir}",
            *fncompare.INCLUDES,
            *fncompare.DEFINES,
            "-E",
            "{input}",
        ],
    )
    commands = read_stage_commands(before + body + after, optflags)
    write_lines(out_dir / "cc1.args", commands["cc1"])
    write_lines(out_dir / "as.args", commands["as"])
    target = fncompare.read_original(asm_path, c_path, function)
    targets = {function: target.routine, **target.nested}
    write_lines(out_dir / "targets.txt", list(targets))
    for name, routine in targets.items():
        write_routine(out_dir / f"target.{name}.txt", routine)
    write_lines(
        out_dir / "symbols.txt",
        [
            f"{name} {address:x}"
            for name, address in fncompare.read_linked_symbols().items()
        ],
    )
    description = fncompare.describe_original(target, compiled, function)
    write_lines(out_dir / "base.txt", [description])
    print(out_dir, description)


if __name__ == "__main__":
    main()
