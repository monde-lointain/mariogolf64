#!/usr/bin/env python3

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from report_nonmatching_matches import full_unintegrated_scratch_results, report_rows_by_function


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


class FullUnintegratedScratchResultsTests(unittest.TestCase):
    def test_returns_full_scratches_not_yet_integrated(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            write_text(repo_root / "src" / "main" / "integrated.c", "void integratedFunction(void) {}\n")

            scratches_path = repo_root / "scratches.json"
            scratches_path.write_text(
                json.dumps(
                    [
                        {
                            "name": "unintegratedFunction",
                            "score": 0,
                            "max_score": 100,
                            "slug": "full1",
                        },
                        {
                            "name": "integratedFunction",
                            "score": 0,
                            "max_score": 100,
                            "slug": "full2",
                        },
                        {
                            "name": "partialFunction",
                            "score": 1,
                            "max_score": 100,
                            "slug": "part1",
                        },
                    ]
                )
            )

            results = full_unintegrated_scratch_results(repo_root, [scratches_path])

            self.assertEqual([result.function for result in results], ["unintegratedFunction"])
            self.assertEqual(results[0].slug, "full1")


class LocalMatchRowsTests(unittest.TestCase):
    def test_local_rows_skip_stale_and_integrated_functions(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            repo_root = Path(temp_dir)
            stub_dir = repo_root / "asm" / "nonmatchings" / "main" / "file"
            write_text(stub_dir / "doneFunction.s", "glabel doneFunction\n")
            write_text(stub_dir / "partialFunction.s", "glabel partialFunction\n")

            content = b"void doneFunction(void) {}\n"
            digest = hashlib.sha256(content).hexdigest()
            done = repo_root / "nonmatchings" / "doneFunction"
            (done / ".matches").mkdir(parents=True)
            (done / ".matches" / f"base_1-{digest}.c").write_bytes(content)
            write_text(
                done / "match_log.txt",
                f".matches/base_1-{digest}.c 100.000% exact sha256={digest} source=base_1.c\n",
            )

            write_text(repo_root / "nonmatchings" / "partialFunction" / "base_1.c", "partial\n")
            write_text(
                repo_root / "nonmatchings" / "partialFunction" / "match_log.txt",
                "base_1.c 99.944%\n",
            )

            # No asm stub and no src definition: a leftover workspace that must be ignored.
            write_text(repo_root / "nonmatchings" / "staleFunction" / "base_1.c", "stale\n")
            write_text(
                repo_root / "nonmatchings" / "staleFunction" / "match_log.txt",
                "base_1.c 50.000%\n",
            )

            rows = report_rows_by_function(repo_root, [repo_root], [])
            self.assertEqual(set(rows), {"doneFunction", "partialFunction"})
            self.assertEqual(rows["doneFunction"].local.percent, 100.0)
            self.assertEqual(rows["partialFunction"].local.percent, 99.944)

            # Once defined in src/, the certified full match is no longer actionable.
            write_text(repo_root / "src" / "main" / "done.c", "void doneFunction(void) {}\n")
            rows = report_rows_by_function(repo_root, [repo_root], [])
            self.assertEqual(set(rows), {"partialFunction"})


if __name__ == "__main__":
    unittest.main()
