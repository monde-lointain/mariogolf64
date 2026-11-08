#!/usr/bin/env python3
"""Measure editsearch on matched functions, of this and other GCC projects.

    benchmark.py verify PROJECT
    benchmark.py functions PROJECT
    benchmark.py export PROJECT FLAGS C_FILE FUNCTION OUT_DIR
    benchmark.py respond PROJECT FLAGS OUT_DIR [LIMIT]
    benchmark.py history OUT_DIR

A benchmark's job is scored against what its matching body compiles to
(see editsearch's targets), so any project whose C files rebuild its
objects gives cases, compiled with the project's own flags or with this
project's, as FLAGS ("original" or "mariogolf") says:
- verify writes the project's C files that KMC cc1 rebuilds, .text and
  its relocations alike, under their original flags, to
  bench/PROJECT.files;
- functions writes the top-level functions of those files, defined once,
  to bench/PROJECT.functions;
- export writes FUNCTION's job, with targets, to OUT_DIR;
- respond exports each listed function, LIMIT of them spread evenly
  through the list, and runs each built-in generator's edits alone on it,
  writing what each edit's compile gives to OUT_DIR/responses.tsv.

CC1 in the environment names KMC cc1 built as the fork server editsearch
runs (see ~/projects/tools/kmc-cc1/build-cc1.sh), whose codegen is KMC
GCC's and GCC 2.7.2's alike. The projects and their flags are in
tools/benchmark_projects.json; output goes to build/bench, or BENCH_DIR.
"""

import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import cast

import fncompare

N64 = Path(__file__).resolve().parent.parent.parent
BENCH_DIR = Path(os.environ.get("BENCH_DIR", fncompare.ROOT / "build" / "bench"))
CC1 = os.environ.get("CC1", "")
PROJECTS_PATH = Path(__file__).with_name("benchmark_projects.json")
EDITSEARCH = Path.home() / "projects/tools/editsearch/target/release/editsearch"
GENERATORS = ("--steps", "--arms", "--counters", "--cache", "--regroup")
# This project's code generation flags, as fncompare compiles with them
MARIOGOLF_CODEGEN = [
    *(flag for flag in fncompare.CFLAGS if flag not in ("-nostdinc", "-w")),
    "-O2",
]
# Each processes' editsearch workers, and the processes run at once
SEARCH_JOBS = 2
PROCESSES = 8
# The words and shape differences of a ranked combination, and its label
RANKED_RE = re.compile(r"^  [0-9.]+ \((\d+) differ, (\d+) in shape\)[^|]*\| (.*)$")
LEVEL_RE = re.compile(
    r"^level 1: (\d+) tried, (\d+) same source, (\d+) failed, (\d+) same code"
)


@dataclass(frozen=True)
class Project:
    """How a project builds its C files.

    LANGUAGE holds the flags that say what the C means: includes, defines
    and dialect, with {dir} for the file's directory under src. CODEGEN
    holds the rest. FILTER, if any, is the command the build pipes each
    file through first, with {src} for it. FIX, if any, is the command the
    build runs on each object after, with {obj} for it.
    """

    driver_dir: str
    language: tuple[str, ...]
    codegen: tuple[str, ...]
    objects: str
    filter: tuple[str, ...] = ()
    fix: tuple[str, ...] = ()


def read_projects() -> dict[str, Project]:
    """Return the projects of PROJECTS_PATH, by name.

    "mariogolf64" is this project, whose flags are fncompare's.
    """
    projects = {
        "mariogolf64": Project(
            driver_dir="tools/gcc_kmc",
            language=(
                "-nostdinc",
                "-w",
                "-I{src_dir}",
                *fncompare.INCLUDES,
                *(flag for flag in fncompare.DEFINES if flag != "-DNON_MATCHING"),
            ),
            codegen=tuple(MARIOGOLF_CODEGEN),
            objects="build/{stem}.c.o",
        )
    }
    listed = cast("dict[str, dict[str, str]]", json.loads(PROJECTS_PATH.read_text()))
    for name, fields in listed.items():
        projects[name] = Project(
            driver_dir=fields["driver_dir"],
            language=tuple(fields["language"].split()),
            codegen=tuple(fields["codegen"].split()),
            objects=fields["objects"],
            filter=tuple(fields.get("filter", "").split()),
            fix=tuple(fields.get("fix", "").split()),
        )
    return projects


# What a stage's command starts with in the driver's -v output
STAGES = ("cc1", "as")
BODY_MARK = "__benchmark_body__;"
# The directories under lib that this project's C files include from
HEADER_DIRS = tuple(
    flag.removeprefix("-I") for flag in fncompare.INCLUDES if flag.startswith("-Ilib/")
)
# A draft that doesn't match yet: a definition under #ifdef NON_MATCHING
DRAFT_RE = re.compile(
    r"^#ifdef NON_MATCHING\n[A-Za-z_][\w* \t]*?\b(\w+)\s*\(", re.MULTILINE
)
# The tokens of C source that nest braces: comments, strings, characters
# and directives are matched whole, so that the braces in them are passed
# over
BRACE_RE = re.compile(
    r"""/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|^[ \t]*#[^\n]*|[{}]""",
    re.MULTILINE | re.DOTALL,
)


def get_flags(project: Project, flags: str, c_file: str) -> list[str]:
    """Return the driver's flags for C_FILE, under FLAGS."""
    path = Path(c_file)
    directory = path.parent
    under_src = directory.relative_to("src") if path.parts[0] == "src" else directory
    language = [
        flag.format(dir=under_src, src_dir=directory) for flag in project.language
    ]
    codegen = project.codegen if flags == "original" else MARIOGOLF_CODEGEN
    return [*codegen, *language, "-x", "c"]


def run_driver(
    root: Path, project: Project, arguments: list[str], text: str | None = None
) -> subprocess.CompletedProcess[str]:
    """Run the project's driver, with the KMC fork server's cc1 next to it."""
    driver_dir = root / project.driver_dir
    with tempfile.TemporaryDirectory() as compiler:
        for stage in driver_dir.iterdir():
            (Path(compiler) / stage.name).symlink_to(stage)
        (Path(compiler) / "cc1").unlink()
        (Path(compiler) / "cc1").symlink_to(CC1)
        return subprocess.run(
            [str(driver_dir / "gcc"), *arguments],
            cwd=root,
            input=text,
            capture_output=True,
            text=True,
            encoding="latin-1",
            env={"COMPILER_PATH": compiler, "PATH": "/usr/bin:/bin"},
            check=False,
        )


def read_source(root: Path, project: Project, c_file: str) -> str:
    """Return C_FILE's text as the build compiles it, after its filter."""
    if not project.filter:
        return (root / c_file).read_bytes().decode("latin-1")
    command = [part.replace("{src}", c_file) for part in project.filter]
    return subprocess.run(
        command, cwd=root, capture_output=True, check=True
    ).stdout.decode("latin-1")


def disassemble(path: Path) -> str | None:
    """Return the .text of the object at PATH with its relocations."""
    if not path.exists():
        return None
    result = subprocess.run(
        ["mips-linux-gnu-objdump", "-dr", "-j", ".text", str(path)],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout.partition("Disassembly")[2]


def verify(name: str) -> None:
    """Write the C files that KMC cc1 rebuilds as the project built them."""
    project = read_projects()[name]
    root = N64 / name
    rebuilt: list[str] = []
    with tempfile.TemporaryDirectory() as scratch:
        for path in sorted((root / "src").rglob("*.c")):
            c_file = str(path.relative_to(root))
            stem = Path(c_file).with_suffix("")
            built = disassemble(root / project.objects.format(stem=stem))
            if built is None:
                continue
            obj = Path(scratch) / "unit.o"
            obj.unlink(missing_ok=True)
            flags = get_flags(project, "original", c_file)
            text = read_source(root, project, c_file)
            _ = run_driver(root, project, [*flags, "-c", "-o", str(obj), "-"], text)
            if project.fix and obj.exists():
                command = [part.replace("{obj}", str(obj)) for part in project.fix]
                _ = subprocess.run(command, cwd=root, capture_output=True, check=False)
            if disassemble(obj) == built:
                rebuilt.append(c_file)
    BENCH_DIR.mkdir(parents=True, exist_ok=True)
    _ = (BENCH_DIR / f"{name}.files").write_text("".join(f"{f}\n" for f in rebuilt))
    print(f"{name}: {len(rebuilt)} files rebuild")


def find_braced_end(text: str, open_brace: int) -> int | None:
    """Return the index after the brace that closes the one at OPEN_BRACE."""
    depth = 0
    for token in BRACE_RE.finditer(text, open_brace):
        kind = token.group()
        if kind == "{":
            depth += 1
        elif kind == "}":
            depth -= 1
            if depth == 0:
                return token.end()
    return None


def find_definition(source: str, function: str) -> tuple[int, int, int] | None:
    """Return where FUNCTION's only definition starts, its body starts and it ends.

    Each is at a line's start. The header's brace must end its line, on it
    or the next, and the closing brace end the last.
    """
    name = re.escape(function)
    header = re.compile(
        rf"^[A-Za-z_][\w* \t]*[ *\t]{name}\s*\([^;{{}}]*\)\s*\{{[ \t]*\n", re.MULTILINE
    )
    matches = list(header.finditer(source))
    if len(matches) != 1:
        return None
    match = matches[0]
    brace = source.rindex("{", match.start(), match.end())
    end = find_braced_end(source, brace)
    if end is None or source[end : end + 1] != "\n":
        return None
    return match.start(), match.end(), end + 1


def functions(name: str) -> None:
    """Write the top-level functions of the rebuilt files, each defined once."""
    project = read_projects()[name]
    root = N64 / name
    found: list[str] = []
    for c_file in (BENCH_DIR / f"{name}.files").read_text().split():
        source = read_source(root, project, c_file)
        flags = get_flags(project, "original", c_file)
        result = run_driver(root, project, [*flags, "-E", "-"], source)
        for function, _, _, is_inline in fncompare.find_definitions(result.stdout):
            if not is_inline and find_definition(source, function) is not None:
                found.append(f"{c_file} {function}\n")
    _ = (BENCH_DIR / f"{name}.functions").write_text("".join(found))
    print(f"{name}: {len(found)} functions")


def read_stage_commands(
    root: Path, project: Project, flags: list[str], unit: str
) -> dict[str, list[str]] | str:
    """Return the driver's cc1 and as commands for UNIT, with placeholders.

    If the driver stops before as, as when UNIT doesn't compile, returns
    the first error it printed instead.
    """
    with tempfile.TemporaryDirectory() as scratch:
        path = Path(scratch) / "unit.i"
        _ = path.write_text(unit, encoding="latin-1")
        result = run_driver(
            root,
            project,
            [*flags, "-v", "-c", "-o", str(Path(scratch) / "unit.o"), str(path)],
        )
    commands: dict[str, list[str]] = {}
    for line in result.stderr.splitlines():
        words = line.split()
        if not words or Path(words[0]).name not in STAGES:
            continue
        command = [str(root / project.driver_dir / Path(words[0]).name)]
        expects_output = False
        for word in words[1:]:
            if expects_output:
                command.append("{output}")
                expects_output = False
            elif word == "-o":
                command.append(word)
                expects_output = True
            elif word in ("-version", "-v"):
                continue
            elif word.endswith((".i", ".s")) and Path(word).name.startswith(
                ("unit", "cc")
            ):
                command.append("{input}")
            else:
                command.append(word)
        commands[Path(words[0]).name] = command
    if set(commands) != set(STAGES):
        errors = [
            line for line in result.stderr.splitlines() if re.match(r":\d+: ", line)
        ]
        return (errors or ["the driver stopped"])[0]
    return commands


def write_lines(path: Path, lines: list[str]) -> None:
    """Write LINES to PATH, a line each."""
    _ = path.write_text("".join(f"{line}\n" for line in lines), encoding="latin-1")


def export(name: str, flags: str, c_file: str, function: str, out_dir: Path) -> str:
    """Write FUNCTION's job, with its targets, to OUT_DIR; return any error."""
    root = N64 / name
    error = write_job(name, flags, c_file, function, out_dir, root=root)
    return error or make_targets(out_dir)


def make_targets(job: Path) -> str:
    """Write the targets of JOB, its draft compiled; return any error."""
    made = subprocess.run(
        [str(EDITSEARCH), "targets", "--cc1", CC1, str(job)],
        capture_output=True,
        text=True,
        check=False,
    )
    return "" if made.returncode == 0 else made.stderr.strip().splitlines()[-1]


def write_job(  # noqa: PLR0913 - each says where the function is
    name: str,
    flags: str,
    c_file: str,
    function: str,
    out_dir: Path,
    *,
    root: Path,
    defines: tuple[str, ...] = (),
) -> str:
    """Write FUNCTION's job, as ROOT holds it, to OUT_DIR, without targets.

    DEFINES go to the driver after the project's flags. Returns any error.
    """
    project = read_projects()[name]
    source = read_source(root, project, c_file)
    definition = find_definition(source, function)
    if definition is None:
        return "no single definition"
    start, body_start, end = definition
    driver_flags = [*get_flags(project, flags, c_file), *defines]
    marked = source[:body_start] + BODY_MARK + "\n" + source[body_start:]
    result = run_driver(root, project, [*driver_flags, "-E", "-"], marked)
    before, mark, rest = result.stdout.partition(BODY_MARK)
    if not mark:
        return "the body lost its mark in cpp"
    out_dir.mkdir(parents=True, exist_ok=True)
    _ = (out_dir / "source.c").write_bytes(source.encode("latin-1"))
    _ = (out_dir / "before.i").write_text(before, encoding="latin-1")
    _ = (out_dir / "after.i").write_text("\n", encoding="latin-1")
    write_lines(
        out_dir / "job.txt",
        [
            f"function {function}",
            f"root {root}",
            f"compiler_path {root / project.driver_dir}",
            f"definition_start {start}",
            f"body_start {body_start}",
            f"definition_end {end}",
        ],
    )
    write_lines(
        out_dir / "cpp.args",
        [str(root / project.driver_dir / "gcc"), *driver_flags, "-E", "{input}"],
    )
    commands = read_stage_commands(root, project, driver_flags, before + rest)
    if isinstance(commands, str):
        return commands
    write_lines(out_dir / "cc1.args", commands["cc1"])
    write_lines(out_dir / "as.args", commands["as"])
    nested = fncompare.find_nested_functions(before + rest, function)
    write_lines(out_dir / "targets.txt", [function, *nested])
    return ""


def git(*arguments: str) -> str:
    """Return the output of this repository's git with ARGUMENTS."""
    return subprocess.run(
        ["git", *arguments],
        cwd=fncompare.ROOT,
        capture_output=True,
        check=True,
    ).stdout.decode("latin-1")


def check_out(commit: str, trees: Path) -> Path:
    """Return a tree of what COMMIT's C files include, made under TREES.

    The toolchain, generated headers and split assembly are the working
    tree's.
    """
    tree = trees / commit
    if tree.exists():
        return tree
    paths = [
        path
        for path in ("include", "src", *HEADER_DIRS)
        if git("ls-tree", "--name-only", commit, path).strip()
    ]
    scratch = trees / f"{commit}.part"
    scratch.mkdir(parents=True)
    archive = subprocess.run(
        ["git", "archive", commit, *paths],
        cwd=fncompare.ROOT,
        capture_output=True,
        check=True,
    ).stdout
    _ = subprocess.run(["tar", "-x", "-C", str(scratch)], input=archive, check=True)
    for linked in ("tools", "build", "asm"):
        (scratch / linked).symlink_to(fncompare.ROOT / linked)
    _ = scratch.rename(tree)
    return tree


def find_drafts(source: str) -> set[str]:
    """Return the functions SOURCE has drafts of."""
    return {str(match[1]) for match in DRAFT_RE.finditer(source)}


def history(out_dir: Path) -> None:
    """Write a job for each draft that a "Match" commit made match.

    Each job, under OUT_DIR/jobs, is the draft as the commit's parent held
    it, scored against what the commit's matching body compiles to.
    out_dir/history.txt lists the commit, function and file of each, or
    why there is none.
    """
    trees = out_dir / "trees"
    lines: list[str] = []
    for entry in git("log", "--format=%h %s").splitlines():
        commit, subject = entry.split(" ", 1)
        if not subject.startswith("Match "):
            continue
        changed = git(
            "diff", "--name-only", "--diff-filter=M", f"{commit}^", commit, "--", "src"
        )
        for c_file in changed.split():
            if not c_file.endswith(".c"):
                continue
            old = git("show", f"{commit}^:{c_file}")
            new = git("show", f"{commit}:{c_file}")
            drafts = find_drafts(old) - find_drafts(new)
            for function in sorted(drafts):
                error = export_history(commit, c_file, function, trees, out_dir)
                lines.append(f"{commit} {function} {c_file} {error or 'exported'}")
                print(lines[-1])
    write_lines(out_dir / "history.txt", lines)


def export_history(
    commit: str, c_file: str, function: str, trees: Path, out_dir: Path
) -> str:
    """Write the job of FUNCTION's draft, which COMMIT made match."""
    matched = out_dir / "matched" / f"{commit}.{function}"
    draft = out_dir / "jobs" / f"{commit}.{function}"
    tree = check_out(commit, trees)
    error = write_job("mariogolf64", "original", c_file, function, matched, root=tree)
    if error or (error := make_targets(matched)):
        return f"matched: {error}"
    parent = check_out(git("rev-parse", "--short", f"{commit}^").strip(), trees)
    error = write_job(
        "mariogolf64",
        "original",
        c_file,
        function,
        draft,
        root=parent,
        defines=("-DNON_MATCHING",),
    )
    if error:
        return f"draft: {error}"
    for target in [*matched.glob("target.*.txt"), matched / "symbols.txt"]:
        _ = shutil.copy(target, draft)
    _ = shutil.copy(matched / "targets.txt", draft)
    return ""


def respond_one(
    name: str, flags: str, c_file: str, function: str, out_dir: Path
) -> list[str]:
    """Export FUNCTION and run each generator's edits alone on it.

    Returns a row for each generator: its counts at level 1, then the
    words and shape differences of each code its edits gave.
    """
    job = out_dir / "jobs" / f"{Path(c_file).stem}.{function}"
    error = export(name, flags, c_file, function, job)
    if error:
        return [f"{name}\t{flags}\t{function}\t-\terror\t{error}"]
    rows: list[str] = []
    for generator in GENERATORS:
        search_out = job / generator.removeprefix("--")
        result = subprocess.run(
            [
                str(EDITSEARCH), "--jobs", str(SEARCH_JOBS), "--depth", "1",
                generator, "--all-ranked", "--top", "100000", "--cc1", CC1,
                "--out", str(search_out), str(job),
            ],
            capture_output=True,
            text=True,
            check=False,
        )  # fmt: skip
        results = search_out / "results.txt"
        if result.returncode != 0 or not results.exists():
            error = (result.stderr.strip().splitlines() or ["?"])[-1]
            rows.append(f"{name}\t{flags}\t{function}\t{generator}\terror\t{error}")
            continue
        level = ""
        codes: list[str] = []
        for line in results.read_text().splitlines():
            if (match := LEVEL_RE.match(line)) is not None:
                level = "\t".join(match.groups())
            elif (match := RANKED_RE.match(line)) is not None:
                codes.append(f"{match[1]}/{match[2]}")
        rows.append(
            f"{name}\t{flags}\t{function}\t{generator}\t{level}\t{' '.join(codes)}"
        )
        shutil.rmtree(search_out)
    return rows


def respond(name: str, flags: str, out_dir: Path, limit: int | None) -> None:
    """Run respond_one over the project's functions, LIMIT spread evenly."""
    listed = [
        line.split()
        for line in (BENCH_DIR / f"{name}.functions").read_text().splitlines()
    ]
    if limit is not None and limit < len(listed):
        listed = [listed[i * len(listed) // limit] for i in range(limit)]
    out_dir.mkdir(parents=True, exist_ok=True)
    with (
        (out_dir / "responses.tsv").open("a") as table,
        concurrent.futures.ThreadPoolExecutor(PROCESSES) as pool,
    ):
        futures = [
            pool.submit(respond_one, name, flags, c_file, function, out_dir)
            for c_file, function in listed
        ]
        for future in concurrent.futures.as_completed(futures):
            for row in future.result():
                _ = table.write(row + "\n")
            table.flush()


def main() -> None:
    """Run the subcommand the arguments name."""
    arguments = sys.argv[1:]
    if not CC1:
        sys.exit("CC1 must name KMC cc1 built as a fork server")
    match arguments:
        case ["verify", name]:
            verify(name)
        case ["functions", name]:
            functions(name)
        case ["export", name, flags, c_file, function, out_dir]:
            print(export(name, flags, c_file, function, Path(out_dir)) or "exported")
        case ["history", out_dir]:
            history(Path(out_dir))
        case ["respond", name, flags, out_dir, *limit]:
            respond(name, flags, Path(out_dir), int(limit[0]) if limit else None)
        case _:
            sys.exit(__doc__)


if __name__ == "__main__":
    main()
