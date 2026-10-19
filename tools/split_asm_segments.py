#!/usr/bin/env python3
"""Split assembly-only subsegments of mariogolf64.yaml into C files of INCLUDE_ASM stubs.

mariogolf64.yaml lists code that is still raw assembly as `- [0xOFF, asm]` lines
under their segment. Changing one to `- [0xOFF, c, <segment>/<name>]` makes splat
generate `src/<segment>/<name>.c` with one INCLUDE_ASM per function in the
subsegment. The bytes do not change and nothing is decompiled.

For each subsegment the tool:
  1. rewrites only that yaml line, refusing if src/<segment>/<name>.c exists;
  2. runs ./tools/build-and-verify.sh, which re-extracts and checks the ROM SHA1;
  3. keeps both changes if the build passed and the C file exists; otherwise it
     restores the yaml bytes and removes the C file this attempt created.

Naming: ROM offset O in a segment starting at `start` with `vram` maps to
VRAM = vram + (O - start), named func_<prefix><VRAM as 8 hex digits>, where
prefix is the segment's symbol_name_format with $VRAM replaced (empty when the
segment has none). Examples: func_8003E6C0 in main, func_ovl8_801F55A0 in overlay_8.

Examples (from the repo root):
  tools/split_asm_segments.py --list
  tools/split_asm_segments.py --offset 0x19AC0
  tools/split_asm_segments.py --offset 0x19AC0 --commit
  tools/split_asm_segments.py --all --commit
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

from list_decomp_candidates import find_include_asm_in_text  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
YAML_NAME = 'mariogolf64.yaml'
BUILD_AND_VERIFY = ['./tools/build-and-verify.sh']
LOG_TAIL_LINES = 20

SEGMENT_NAME_RE = re.compile(r'^  - name:\s*(\S+)\s*$')
SEGMENT_FIELD_RE = re.compile(r'^    (start|vram|symbol_name_format):\s*(\S.*?)\s*$')
ASM_SUBSEGMENT_RE = re.compile(r'^(\s*)- \[(0x[0-9A-Fa-f]+),\s*asm\]\s*$')


class SplitError(Exception):
    """A split that cannot be attempted: unknown offset, existing C file, bad yaml."""


@dataclass(frozen=True)
class AsmSubsegment:
    line_index: int  # 0-based index of the yaml line
    indent: str
    offset: int
    offset_text: str  # as written in the yaml, e.g. 0x19AC0
    segment: str
    segment_start: int
    segment_vram: int
    symbol_name_format: Optional[str]

    @property
    def name(self) -> str:
        vram = self.segment_vram + (self.offset - self.segment_start)
        hex_vram = f'{vram:08X}'
        if self.symbol_name_format:
            return 'func_' + self.symbol_name_format.replace('$VRAM', hex_vram)
        return 'func_' + hex_vram

    @property
    def c_entry(self) -> str:
        """The `<segment>/<name>` written into the yaml `c` entry; src file is src/<c_entry>.c."""
        return f'{self.segment}/{self.name}'

    def src_path(self, root: Path) -> Path:
        return root / 'src' / f'{self.c_entry}.c'


@dataclass(frozen=True)
class SplitResult:
    offset: int
    c_entry: str
    kept: bool
    functions: int
    log: Optional[Path]
    reason: str  # empty when kept


def parse_offset(text: str) -> int:
    try:
        return int(text, 16)
    except ValueError as err:
        raise argparse.ArgumentTypeError(f'not a hex ROM offset: {text}') from err


def parse_asm_subsegments(text: str) -> List[AsmSubsegment]:
    """Return every `- [0xOFF, asm]` line in yaml order, with its owning segment."""
    segments = {}  # name -> {field: value}
    current = None
    pending = []  # (line_index, match, segment name)
    for index, line in enumerate(text.splitlines()):
        header = SEGMENT_NAME_RE.match(line)
        if header:
            current = header.group(1)
            if current in segments:
                raise SplitError(f'segment {current} is declared twice in {YAML_NAME}')
            segments[current] = {}
            continue
        if current is None:
            continue
        field = SEGMENT_FIELD_RE.match(line)
        if field:
            segments[current][field.group(1)] = field.group(2).strip('\'"')
            continue
        asm = ASM_SUBSEGMENT_RE.match(line)
        if asm:
            pending.append((index, asm, current))

    result = []
    for index, asm, segment in pending:
        fields = segments[segment]
        if 'start' not in fields or 'vram' not in fields:
            raise SplitError(f'segment {segment} has asm subsegments but no start/vram')
        result.append(AsmSubsegment(
            line_index=index,
            indent=asm.group(1),
            offset=int(asm.group(2), 16),
            offset_text=asm.group(2),
            segment=segment,
            segment_start=int(fields['start'], 0),
            segment_vram=int(fields['vram'], 0),
            symbol_name_format=fields.get('symbol_name_format') or None,
        ))
    return result


def rewrite_line(text: str, sub: AsmSubsegment, c_entry: str) -> str:
    """Return `text` with only the subsegment's own line changed to a `c` entry."""
    lines = text.splitlines(keepends=True)
    old = lines[sub.line_index]
    ending = old[len(old.rstrip('\r\n')):]
    lines[sub.line_index] = f'{sub.indent}- [{sub.offset_text}, c, {c_entry}]{ending}'
    return ''.join(lines)


def run_build_and_verify(root: Path, log: Path) -> int:
    with log.open('wb') as out:
        return subprocess.run(BUILD_AND_VERIFY, cwd=root, stdout=out, stderr=subprocess.STDOUT).returncode


def split_subsegment(
    root: Path,
    offset: int,
    verify: Callable[[Path, Path], int] = run_build_and_verify,
) -> SplitResult:
    """Split the asm subsegment at `offset`. Leaves the tree unchanged unless the split is kept."""
    yaml_path = root / YAML_NAME
    before_bytes = yaml_path.read_bytes()
    subs = [s for s in parse_asm_subsegments(before_bytes.decode('utf-8')) if s.offset == offset]
    if not subs:
        raise SplitError(f'no asm subsegment at 0x{offset:X} in {YAML_NAME}')
    if len(subs) > 1:
        raise SplitError(f'0x{offset:X} is listed {len(subs)} times in {YAML_NAME}')
    sub = subs[0]
    src = sub.src_path(root)
    if src.exists():
        raise SplitError(f'{src.relative_to(root)} already exists; refusing to overwrite it')

    def restore() -> None:
        yaml_path.write_bytes(before_bytes)
        if src.exists():
            src.unlink()

    yaml_path.write_bytes(rewrite_line(before_bytes.decode('utf-8'), sub, sub.c_entry).encode('utf-8'))
    fd, log_name = tempfile.mkstemp(prefix=f'split-asm-{offset:X}-', suffix='.log')
    os.close(fd)
    log = Path(log_name)
    try:
        rc = verify(root, log)
    except BaseException:
        restore()
        raise

    if rc == 0 and src.is_file():
        functions = len(find_include_asm_in_text(src.read_text(), src.relative_to(root).as_posix()))
        log.unlink(missing_ok=True)
        return SplitResult(offset, sub.c_entry, True, functions, None, '')

    if rc != 0:
        reason = f'build-and-verify exited {rc}'
    else:
        reason = f'build passed but {src.relative_to(root)} was not generated'
    restore()
    return SplitResult(offset, sub.c_entry, False, 0, log, reason)


def commit_message(result: SplitResult) -> str:
    subject = f'Split asm subsegment 0x{result.offset:X} into C'
    body = (
        f'Converts the assembly-only subsegment at 0x{result.offset:X} into\n'
        f'src/{result.c_entry}.c, a C file of {result.functions} INCLUDE_ASM stub(s).\n'
        'The stubs keep the original assembly, so the bytes are unchanged and no\n'
        'function is decompiled here.\n'
        '\n'
        f'mariogolf64.yaml now lists 0x{result.offset:X} as a c entry. build-and-verify\n'
        're-extracted the ROM and reproduced baserom.z64 SHA1, so the split is\n'
        'byte-identical. Only that yaml line and the new file are committed.\n'
    )
    return f'{subject}\n\n{body}'


def commit_split(root: Path, result: SplitResult) -> None:
    """Commit exactly this split's yaml and C file, leaving anything else staged alone."""
    paths = [YAML_NAME, f'src/{result.c_entry}.c']
    subprocess.run(['git', '-C', str(root), 'add', '--', *paths], check=True)
    subprocess.run(
        ['git', '-C', str(root), 'commit', '--only', '-q', '-F', '-', '--', *paths],
        input=commit_message(result), text=True, check=True,
    )


def split_all(
    root: Path,
    verify: Callable[[Path, Path], int] = run_build_and_verify,
    commit: bool = False,
    report: Callable[[SplitResult], None] = lambda result: None,
) -> List[SplitResult]:
    """Attempt every asm subsegment in file order. Failures do not stop the run."""
    subs = parse_asm_subsegments((root / YAML_NAME).read_text(encoding='utf-8'))
    results = []
    for sub in subs:
        try:
            result = split_subsegment(root, sub.offset, verify)
        except SplitError as err:
            result = SplitResult(sub.offset, sub.c_entry, False, 0, None, str(err))
        if result.kept and commit:
            commit_split(root, result)
        report(result)
        results.append(result)
    return results


def describe(result: SplitResult) -> str:
    if result.kept:
        return f'kept   0x{result.offset:X} -> {result.c_entry} ({result.functions} functions)'
    text = f'FAILED 0x{result.offset:X} -> {result.c_entry}: {result.reason}'
    if result.log is not None:
        text += f'\n  log: {result.log}'
    return text


def print_log_tail(result: SplitResult) -> None:
    if result.kept or result.log is None or not result.log.exists():
        return
    lines = result.log.read_text(errors='replace').splitlines()[-LOG_TAIL_LINES:]
    for line in lines:
        print(f'  | {line}', file=sys.stderr)


def list_subsegments(root: Path) -> None:
    for sub in parse_asm_subsegments((root / YAML_NAME).read_text(encoding='utf-8')):
        note = '  (src exists)' if sub.src_path(root).exists() else ''
        print(f'0x{sub.offset:X}  {sub.segment}  {sub.c_entry}{note}')


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--list', action='store_true', help='list every asm subsegment and its planned C file')
    mode.add_argument('--offset', type=parse_offset, help='ROM offset of one asm subsegment, e.g. 0x19AC0')
    mode.add_argument('--all', action='store_true', help='split every asm subsegment in file order')
    parser.add_argument('--commit', action='store_true', help='commit each kept split on its own')
    args = parser.parse_args(argv)

    if args.list:
        list_subsegments(ROOT)
        return 0

    if args.offset is not None:
        try:
            result = split_subsegment(ROOT, args.offset)
        except SplitError as err:
            print(f'error: {err}', file=sys.stderr)
            return 1
        print(describe(result))
        if not result.kept:
            print_log_tail(result)
            return 1
        if args.commit:
            commit_split(ROOT, result)
        return 0

    failed = 0

    def report(result: SplitResult) -> None:
        nonlocal failed
        print(describe(result), flush=True)
        if not result.kept:
            failed += 1
            print_log_tail(result)

    results = split_all(ROOT, commit=args.commit, report=report)
    kept = sum(1 for r in results if r.kept)
    print(f'{kept} kept, {len(results) - kept} failed of {len(results)} asm subsegments')
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
