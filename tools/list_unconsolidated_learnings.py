#!/usr/bin/env python3
"""List per-function notes under nonmatchings/ that have not been consolidated into docs/.

Decomp environments write LEARNINGS.md and NEAR_MISS.md next to their scratch files.
A note counts as consolidated once its exact content hash is recorded in the ledger
at $(git rev-parse --git-common-dir)/mg64/consolidated-learnings.txt, one
"<sha256> <repo-relative path>" line per note. The ledger sits in the common git dir,
so every worktree shares it and nigel's reset does not touch it. Editing a note changes
its hash, so the note is listed again. Empty or whitespace-only notes are never listed.

  list_unconsolidated_learnings.py           repo-relative paths, one per line
  list_unconsolidated_learnings.py --json    JSON array (nigel candidate_source)
  list_unconsolidated_learnings.py --mark P  record note P as consolidated
"""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
NOTES_DIR = Path('nonmatchings')
NOTE_NAMES = ('LEARNINGS.md', 'NEAR_MISS.md')
LEDGER_RELATIVE = Path('mg64') / 'consolidated-learnings.txt'


def ledger_path(root: Path) -> Path:
    common = subprocess.run(
        ['git', '-C', str(root), 'rev-parse', '--git-common-dir'],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    common_dir = Path(common)
    if not common_dir.is_absolute():
        common_dir = root / common_dir
    return common_dir / LEDGER_RELATIVE


def load_consolidated(ledger: Path) -> set[str]:
    if not ledger.exists():
        return set()
    return {line.split(maxsplit=1)[0] for line in ledger.read_text(encoding='utf-8').splitlines() if line.strip()}


def find_notes(root: Path) -> list[Path]:
    notes_dir = root / NOTES_DIR
    return sorted(p for name in NOTE_NAMES for p in notes_dir.glob(f'*/{name}') if p.is_file())


def unconsolidated(root: Path, ledger: Path) -> list[str]:
    done = load_consolidated(ledger)
    rows = []
    for path in find_notes(root):
        data = path.read_bytes()
        if not data.strip():
            continue
        if hashlib.sha256(data).hexdigest() in done:
            continue
        rows.append(path.relative_to(root).as_posix())
    return rows


def mark_consolidated(root: Path, ledger: Path, note: str) -> str:
    path = Path(note)
    if not path.is_absolute():
        path = root / path
    path = path.resolve()
    root = root.resolve()
    if path.name not in NOTE_NAMES or path.parent.parent != (root / NOTES_DIR).resolve():
        raise ValueError(f'not a nonmatchings note ({" or ".join(NOTE_NAMES)}): {note}')
    if not path.is_file():
        raise FileNotFoundError(f'no such note: {note}')

    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    rel = path.relative_to(root).as_posix()
    if digest not in load_consolidated(ledger):
        ledger.parent.mkdir(parents=True, exist_ok=True)
        with ledger.open('a', encoding='utf-8') as fh:
            fh.write(f'{digest} {rel}\n')
    return rel


def main(argv=None, root: Path = ROOT_DIR, ledger: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--json', action='store_true', help='print a JSON array of paths')
    parser.add_argument('--mark', metavar='PATH', help='record a note as consolidated')
    args = parser.parse_args(argv)
    if ledger is None:
        ledger = ledger_path(root)

    if args.mark:
        try:
            rel = mark_consolidated(root, ledger, args.mark)
        except (ValueError, FileNotFoundError) as exc:
            print(f'error: {exc}', file=sys.stderr)
            return 2
        print(f'consolidated {rel}')
        return 0

    paths = unconsolidated(root, ledger)
    if args.json:
        print(json.dumps(paths))
    else:
        for path in paths:
            print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
