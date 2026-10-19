#!/usr/bin/env python3
"""Rank unmatched functions by their overall decompilation leverage.

For each unmatched function, this script asks two counterfactual questions:

* How much ROM progress would matching the function make directly?
* How much easier would it make other unmatched functions by becoming a new
  decompile-similar reference?

The default "overall" ranking estimates value per unit of effort. Value covers
the candidate's own instructions plus affected targets, weighted by target size
and the improvement in similarity. Effort accounts for candidate size,
structural difficulty, the quality of its current reference, and prior failed
task-runner attempts.

Hints mirror `score_functions.py --by-similarity --json --top-n N`: the top N
matched src/ functions from find_similar_functions. A task input is therefore
the candidate name followed by hint names, exactly as nigel records it in
task-runner/decompile-similar/ignored.log.

Usage:
    python3 tools/rank_decompile_similar_impact.py
    python3 tools/rank_decompile_similar_impact.py --limit 50 --show-targets 5
    python3 tools/rank_decompile_similar_impact.py --ranking reach
    python3 tools/rank_decompile_similar_impact.py --json
"""

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from statistics import mean
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import (  # noqa: E402
    ParsedFunction,
    build_function_index,
    calculate_similarity,
    collect_unmatched,
    find_similar_functions,
    get_project_root,
)
from score_functions import decompilation_difficulty_score  # noqa: E402

DEFAULT_UNMATCHED_DIR = 'asm/nonmatchings'
DEFAULT_IGNORED_LOG = 'task-runner/decompile-similar/ignored.log'
# Matches --top-n in task-runner/decompile-similar/task.yaml candidate_source.
DEFAULT_TOP_N = 3


@dataclass(frozen=True)
class Reference:
    """One similarity hint for an unmatched target."""

    name: str
    score: float


@dataclass(frozen=True)
class TargetImpact:
    """How a candidate would change one target's hint list."""

    target: str
    score: float
    new_rank: int
    replaced_score: float
    unblocked: bool
    instruction_count: int

    @property
    def score_gain(self) -> float:
        return self.score - self.replaced_score

    @property
    def effective_score_gain(self) -> float:
        return self.score if self.unblocked else self.score_gain

    @property
    def value(self) -> float:
        """Target instructions weighted by the gain in hint quality."""
        return self.instruction_count * self.effective_score_gain


@dataclass
class CandidateImpact:
    """Aggregated impact of matching one currently unmatched function."""

    function: ParsedFunction
    candidate_blocked: bool
    reference_similarity: float = 0.0
    historical_attempts: int = 0
    targets: List[TargetImpact] = field(default_factory=list)

    @property
    def difficulty(self) -> float:
        return decompilation_difficulty_score(
            self.function.instruction_count,
            self.function.branch_count,
            self.function.jump_count,
            self.function.label_count,
        )

    @property
    def direct_value(self) -> float:
        return float(self.function.instruction_count)

    @property
    def downstream_value(self) -> float:
        return sum(impact.value for impact in self.targets)

    @property
    def total_value(self) -> float:
        return self.direct_value + self.downstream_value

    @property
    def estimated_effort(self) -> float:
        """Heuristic effort: size, difficulty, reference quality, and retries."""
        return (
            25.0
            + self.function.instruction_count
            * (1.0 + self.difficulty)
            * (1.25 - 0.5 * self.reference_similarity)
            * (1.0 + 0.25 * self.historical_attempts)
        )

    @property
    def overall_score(self) -> float:
        return self.total_value / self.estimated_effort

    @property
    def changed_count(self) -> int:
        return len(self.targets)

    @property
    def unblocked_count(self) -> int:
        return sum(item.unblocked for item in self.targets)


def reference_sort_key(reference: Reference) -> Tuple[float, str]:
    """Best score first, then name, matching find_similar_functions."""
    return -reference.score, reference.name


def select_top_references(references: Iterable[Reference], top_n: int) -> List[Reference]:
    return sorted(references, key=reference_sort_key)[:top_n]


def add_hypothetical_reference(
    current: Sequence[Reference], candidate: Reference, top_n: int
) -> List[Reference]:
    return select_top_references([*current, candidate], top_n)


def task_input(target: str, references: Sequence[Reference]) -> Tuple[str, ...]:
    """Task-runner input: the target followed by bare hint names."""
    return target, *(reference.name for reference in references)


def load_ignored_inputs(path: Path) -> Set[Tuple[str, ...]]:
    """Read ignored.log entries, one JSON array of strings per line."""
    if not path.exists():
        return set()

    ignored: Set[Tuple[str, ...]] = set()
    malformed = 0
    with path.open(encoding='utf-8') as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                malformed += 1
                continue
            if isinstance(value, list) and all(isinstance(item, str) for item in value):
                ignored.add(tuple(value))
            else:
                malformed += 1

    if malformed:
        print(f'Warning: ignored {malformed} malformed line(s) in {path}', file=sys.stderr)
    return ignored


def build_current_references(
    unmatched: Sequence[ParsedFunction],
    matching: Dict[str, ParsedFunction],
    top_n: int,
    show_progress: bool = True,
) -> Dict[str, List[Reference]]:
    """Compute the exact current top-N hints for every unmatched target."""
    recommendations: Dict[str, List[Reference]] = {}
    for index, query in enumerate(unmatched, 1):
        recommendations[query.name] = [
            Reference(name=name, score=score)
            for name, score in find_similar_functions(query, matching, top_n=top_n)
        ]
        if show_progress and (index % 25 == 0 or index == len(unmatched)):
            print(f'  Current hints: {index}/{len(unmatched)}', file=sys.stderr)
    return recommendations


def rank_candidate_impacts(
    unmatched: Sequence[ParsedFunction],
    current_references: Dict[str, List[Reference]],
    matched_names: Set[str],
    ignored_inputs: Set[Tuple[str, ...]],
    top_n: int,
    ranking: str = 'overall',
    show_progress: bool = True,
) -> Tuple[List[CandidateImpact], List[str]]:
    """Return candidate impacts plus the targets currently blocked by ignore history."""
    if ranking not in ('overall', 'reach'):
        raise ValueError(f'unknown ranking: {ranking}')

    blocked_targets = [
        target.name
        for target in unmatched
        if task_input(target.name, current_references[target.name]) in ignored_inputs
    ]
    attempts: Dict[str, int] = {}
    for ignored in ignored_inputs:
        attempts[ignored[0]] = attempts.get(ignored[0], 0) + 1

    candidates = [function for function in unmatched if function.name not in matched_names]
    impacts: List[CandidateImpact] = []
    for index, candidate in enumerate(candidates, 1):
        candidate_input = task_input(candidate.name, current_references[candidate.name])
        impact = CandidateImpact(
            function=candidate,
            candidate_blocked=candidate_input in ignored_inputs,
            reference_similarity=(
                current_references[candidate.name][0].score
                if current_references[candidate.name]
                else 0.0
            ),
            historical_attempts=attempts.get(candidate.name, 0),
        )

        for target in unmatched:
            if target.name == candidate.name:
                continue

            score = calculate_similarity(target, candidate)
            hypothetical = Reference(name=candidate.name, score=score)
            current = current_references[target.name]
            updated = add_hypothetical_reference(current, hypothetical, top_n)
            if [reference.name for reference in updated] == [reference.name for reference in current]:
                continue

            new_rank = next(
                rank
                for rank, reference in enumerate(updated, 1)
                if reference.name == candidate.name
            )
            replaced_score = current[-1].score if len(current) == top_n else 0.0
            target_input = task_input(target.name, current)
            new_input = task_input(target.name, updated)
            impact.targets.append(
                TargetImpact(
                    target=target.name,
                    score=score,
                    new_rank=new_rank,
                    replaced_score=replaced_score,
                    unblocked=(
                        target_input in ignored_inputs and new_input not in ignored_inputs
                    ),
                    instruction_count=target.instruction_count,
                )
            )

        if show_progress and (index % 10 == 0 or index == len(candidates)):
            print(f'  Candidate impacts: {index}/{len(candidates)}', file=sys.stderr)

        if ranking == 'reach':
            impact.targets.sort(key=lambda item: (-item.effective_score_gain, item.target))
        else:
            impact.targets.sort(key=lambda item: (-item.value, item.target))
        impacts.append(impact)

    if ranking == 'reach':
        impacts.sort(key=lambda item: (-len(item.targets), -item.total_value, item.function.name))
    else:
        impacts.sort(key=lambda item: (-item.overall_score, item.function.name))
    return impacts, blocked_targets


def impact_to_json(impact: CandidateImpact, show_targets: int) -> dict:
    return {
        'function': impact.function.name,
        'file': impact.function.file_path,
        'instructions': impact.function.instruction_count,
        'difficulty': round(impact.difficulty, 4),
        'candidate_blocked': impact.candidate_blocked,
        'reference_similarity': round(impact.reference_similarity, 4),
        'historical_attempts': impact.historical_attempts,
        'estimated_effort': round(impact.estimated_effort, 2),
        'direct_value': round(impact.direct_value, 2),
        'downstream_value': round(impact.downstream_value, 2),
        'total_value': round(impact.total_value, 2),
        'overall_score': round(impact.overall_score, 6),
        'affected_targets': impact.changed_count,
        'unblocked_targets': impact.unblocked_count,
        'targets': [
            {
                'target': item.target,
                'new_rank': item.new_rank,
                'similarity': round(item.score, 4),
                'replaced_similarity': round(item.replaced_score, 4),
                'gain': round(item.score_gain, 4),
                'unblocked': item.unblocked,
                'value': round(item.value, 2),
            }
            for item in impact.targets[:show_targets]
        ],
    }


def print_human_results(
    impacts: Sequence[CandidateImpact],
    unmatched_count: int,
    matching_count: int,
    blocked_targets: Sequence[str],
    ignored_count: int,
    excluded_existing_count: int,
    top_n: int,
    show_targets: int,
    ranking: str,
) -> None:
    print(
        f'Analyzed {unmatched_count} unmatched functions against '
        f'{matching_count} matched references (top {top_n}).'
    )
    print(f'Ranking: {ranking}')
    print(f'Ignored task inputs loaded: {ignored_count}')
    print(f'Currently blocked targets: {len(blocked_targets)}')
    if excluded_existing_count:
        print(
            f'Excluded {excluded_existing_count} unmatched functions already '
            'matched in src/.'
        )
    print()
    if ranking == 'overall':
        print('Overall score = total value / estimated effort.')
        print(
            'Total value = direct instructions + downstream target value. '
            'Effort uses size, difficulty, reference quality, and prior tries.'
        )
    else:
        print('Reach ranking is legacy: sorted by affected target count.')
    print()
    if not impacts:
        print('No candidates matched the requested filters.')
        return

    for rank, impact in enumerate(impacts, 1):
        print(
            f'{rank:>4}. {impact.function.name:<32} '
            f'score={impact.overall_score:9.4f} '
            f'value={impact.total_value:9.1f} '
            f'effort={impact.estimated_effort:8.1f} '
            f'targets={impact.changed_count:4} '
            f'unblocked={impact.unblocked_count:4} '
            f'instr={impact.function.instruction_count:4} '
            f'blocked={"yes" if impact.candidate_blocked else "no"} '
            f'tries={impact.historical_attempts}'
        )
        for item in impact.targets[:show_targets]:
            unblock = ' unblocked' if item.unblocked else ''
            print(
                f'      -> {item.target} rank={item.new_rank} '
                f'sim={item.score:.3f} replaced={item.replaced_score:.3f} '
                f'gain={item.score_gain:+.3f} value={item.value:.1f}{unblock}'
            )


def resolve_project_path(path: Path, root: Path) -> Path:
    """Resolve relative paths against the repository root, like score_functions.py."""
    return path if path.is_absolute() else root / path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            'Rank unmatched functions by overall decompilation value per estimated '
            'effort, including their impact as future decompile-similar hints.'
        )
    )
    parser.add_argument(
        'unmatched_dir',
        nargs='?',
        default=DEFAULT_UNMATCHED_DIR,
        help=f'unmatched assembly file or directory (default: {DEFAULT_UNMATCHED_DIR})',
    )
    parser.add_argument(
        '--top-n',
        type=int,
        default=DEFAULT_TOP_N,
        metavar='N',
        help=f'number of hints used by decompile-similar (default: {DEFAULT_TOP_N})',
    )
    parser.add_argument(
        '--limit',
        type=int,
        default=25,
        metavar='N',
        help='number of candidates to print; 0 prints all (default: 25)',
    )
    parser.add_argument(
        '--show-targets',
        type=int,
        default=3,
        metavar='N',
        help='affected targets to show per candidate (default: 3)',
    )
    parser.add_argument(
        '--ignored-log',
        type=Path,
        default=Path(DEFAULT_IGNORED_LOG),
        help=f'task-runner ignore history (default: {DEFAULT_IGNORED_LOG})',
    )
    parser.add_argument(
        '--ranking',
        choices=('overall', 'reach'),
        default='overall',
        help=(
            'overall: value per estimated effort (default); '
            'reach: legacy count of affected targets'
        ),
    )
    parser.add_argument(
        '--blocked-candidates-only',
        action='store_true',
        help='only list candidates whose current task input is already ignored',
    )
    parser.add_argument(
        '--include-zero-impact',
        action='store_true',
        help='with --ranking reach, keep candidates that affect no targets',
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='write complete machine-readable results to stdout',
    )
    args = parser.parse_args()
    if args.top_n < 1:
        parser.error('--top-n must be at least 1')
    if args.limit < 0:
        parser.error('--limit must be zero or greater')
    if args.show_targets < 0:
        parser.error('--show-targets must be zero or greater')
    return args


def main() -> int:
    args = parse_args()
    root = get_project_root()
    unmatched_path = resolve_project_path(Path(args.unmatched_dir), root)
    if not unmatched_path.exists():
        print(f'Error: path does not exist: {args.unmatched_dir}', file=sys.stderr)
        return 1

    print(f'Parsing unmatched functions from {args.unmatched_dir}...', file=sys.stderr)
    unmatched = sorted(
        collect_unmatched(root, unmatched_path).values(),
        key=lambda function: function.name,
    )
    if not unmatched:
        print(f'Error: no unmatched functions found in {args.unmatched_dir}', file=sys.stderr)
        return 1

    print('Indexing matched references from src/...', file=sys.stderr)
    matching = build_function_index(root)
    matched_names = set(matching)
    print(f'Indexed {len(matching)} matched references.', file=sys.stderr)

    current_references = build_current_references(unmatched, matching, args.top_n)
    ignored_inputs = load_ignored_inputs(resolve_project_path(args.ignored_log, root))
    impacts, blocked_targets = rank_candidate_impacts(
        unmatched,
        current_references,
        matched_names,
        ignored_inputs,
        args.top_n,
        ranking=args.ranking,
    )

    if args.blocked_candidates_only:
        impacts = [impact for impact in impacts if impact.candidate_blocked]
    if args.ranking == 'reach' and not args.include_zero_impact:
        impacts = [impact for impact in impacts if impact.changed_count]
    if args.limit:
        impacts = impacts[: args.limit]

    excluded_existing_count = sum(function.name in matched_names for function in unmatched)

    if args.json:
        result = {
            'metadata': {
                'unmatched_functions': len(unmatched),
                'matched_references': len(matching),
                'top_n': args.top_n,
                'ignored_inputs': len(ignored_inputs),
                'currently_blocked_targets': blocked_targets,
                'excluded_existing_references': excluded_existing_count,
                'ranking': args.ranking,
            },
            'candidates': [impact_to_json(impact, args.show_targets) for impact in impacts],
        }
        print(json.dumps(result, indent=2))
    else:
        print_human_results(
            impacts,
            unmatched_count=len(unmatched),
            matching_count=len(matching),
            blocked_targets=blocked_targets,
            ignored_count=len(ignored_inputs),
            excluded_existing_count=excluded_existing_count,
            top_n=args.top_n,
            show_targets=args.show_targets,
            ranking=args.ranking,
        )
    return 0


if __name__ == '__main__':
    sys.exit(main())
