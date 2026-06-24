#!/usr/bin/env python3
"""Structured-basis-testing gap analyzer (Code Complete 22.3).

Enumerate every decision point in a target function via AST, compute the
basis-test count (cyclomatic complexity = 1 + decisions), then cross-reference
coverage.py's branch arcs to report which true/false sides are UNTESTED -
the precise "locations to test" before refactoring the function.

usage: basis_gaps.py <src.py> <cov.json> <func_name>
"""

import ast
import json
import sys


def decisions(fn):
    """Yield (lineno, kind, src_hint) for each basis-test decision in a function."""
    out = []
    for node in ast.walk(fn):
        if isinstance(node, ast.If):
            out.append((node.lineno, "if", _src(node.test)))
        elif isinstance(node, (ast.While,)):
            out.append((node.lineno, "while", _src(node.test)))
        elif isinstance(node, ast.For):
            out.append((node.lineno, "for", _src(node.target)))
        elif isinstance(node, ast.BoolOp):
            op = "and" if isinstance(node.op, ast.And) else "or"
            # each operand past the first is a counted short-circuit decision
            for v in node.values[1:]:
                out.append((node.lineno, op, _src(v)))
        elif isinstance(node, ast.IfExp):
            out.append((node.lineno, "ternary", _src(node.test)))
        elif isinstance(node, (ast.comprehension,)):
            for c in node.ifs:
                out.append((c.lineno, "comp-if", _src(c)))
    return out


def _src(node):
    try:
        return ast.unparse(node)
    except Exception:
        return "?"


def main():
    src_path, cov_path, func = sys.argv[1], sys.argv[2], sys.argv[3]
    tree = ast.parse(open(src_path).read())
    target = next(
        (
            n
            for n in ast.walk(tree)
            if isinstance(n, ast.FunctionDef) and n.name == func
        ),
        None,
    )
    if target is None:
        sys.exit(f"function {func} not found")
    lo, hi = (
        target.lineno,
        max(getattr(n, "lineno", target.lineno) for n in ast.walk(target)),
    )

    decs = decisions(target)
    cc = 1 + len(decs)

    cov = json.load(open(cov_path))
    f = next(k for k in cov["files"] if k.endswith(src_path.split("/")[-1]))
    fd = cov["files"][f]
    missing_arcs = [a for a in fd["missing_branches"] if lo <= a[0] <= hi]
    missing_lines = [l for l in fd["missing_lines"] if lo <= l <= hi]

    srclines = open(src_path).read().splitlines()
    print(f"== {func}  lines {lo}-{hi} ==")
    print(
        f"basis-test cases needed (cyclomatic complexity) = {cc}  "
        f"({len(decs)} decisions)"
    )
    print(f"uncovered statement lines: {len(missing_lines)} -> {missing_lines}")
    print(f"uncovered branch arcs: {len(missing_arcs)}")
    print("\n-- untested branch sides (the precise targets) --")
    for src_line, dst in sorted(missing_arcs):
        guard = srclines[src_line - 1].strip()
        if dst < 0:
            side = "implicit-exit (condition never let flow fall through here)"
        elif dst == src_line + 1 or dst > src_line:
            side = f"-> line {dst}"
        else:
            side = f"-> line {dst} (back-edge)"
        # which polarity: does dst land inside the body (true) or skip it (false)?
        polarity = (
            "TRUE"
            if dst == src_line + 1
            else ("FALSE/skip" if dst > src_line + 1 or dst < 0 else "?")
        )
        print(f"  L{src_line}: {guard}")
        print(f"        arc {side}  [{polarity} side untested]")


if __name__ == "__main__":
    main()
