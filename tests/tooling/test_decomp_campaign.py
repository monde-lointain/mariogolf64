"""Deadline, process-lifetime and measurement boundary coverage."""
from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

import pytest
from conftest import load_tool

budget = load_tool("decomp_budget")
evidence = load_tool("decomp_evidence")


def test_budget_intersection_and_expired_reserve():
    assert (
        budget.execution_seconds(600, 120, {budget.DEADLINE_ENV: "1500"}, now=1000)
        == 380
    )
    with pytest.raises(TimeoutError):
        budget.execution_seconds(600, 120, {budget.DEADLINE_ENV: "1120"}, now=1000)
    with pytest.raises(ValueError):
        budget.execution_seconds(600, environment={budget.DEADLINE_ENV: "nan"})



def test_expiry_stops_descendant_after_group_leader_exits(tmp_path):
    marker = tmp_path / "leaked-child"
    child = f"import time; from pathlib import Path; time.sleep(0.4); Path({str(marker)!r}).write_text('leaked')"
    parent = f"import subprocess,sys; subprocess.Popen([sys.executable,'-c',{child!r}])"
    with pytest.raises(subprocess.TimeoutExpired):
        budget.run_bounded(
            [sys.executable, "-c", parent], 0.1, env={}, capture_output=True
        )
    time.sleep(0.5)
    assert not marker.exists()



def test_successful_command_does_not_leave_background_descendants(tmp_path):
    marker = tmp_path / "escaped-success"
    child = f"import time; from pathlib import Path; time.sleep(0.4); Path({str(marker)!r}).write_text('leaked')"
    parent = f"import subprocess,sys; subprocess.Popen([sys.executable,'-c',{child!r}])"
    result = budget.run_bounded([sys.executable, "-c", parent], 2, env={})
    assert result.returncode == 0
    time.sleep(0.5)
    assert not marker.exists()



def test_nested_budget_cancellation_kills_separate_worker_group(tmp_path):
    marker = tmp_path / "escaped-nested"
    ready = tmp_path / "nested-worker-started"
    child = (
        "import signal,time; from pathlib import Path; "
        "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
        f"Path({str(ready)!r}).write_text('ready'); "
        f"time.sleep(2); Path({str(marker)!r}).write_text('leaked')"
    )
    inner = [
        sys.executable,
        str(Path(budget.__file__)),
        "--seconds",
        "10",
        "--",
        sys.executable,
        "-c",
        child,
    ]
    with pytest.raises(subprocess.TimeoutExpired):
        budget.run_bounded(inner, 0.5, env={}, capture_output=True)
    assert ready.read_text() == "ready"
    time.sleep(1)
    assert not marker.exists()



def test_measurements_keep_previous_bases_and_reject_mid_compile_edits(
    tmp_path, monkeypatch
):
    monkeypatch.setattr(evidence, "ROOT", tmp_path)
    source = tmp_path / "base.c"
    first = b"int target(void) { return 7; }\n"
    second = b"int target(void) { return 8; }\n"
    source.write_bytes(first)
    context = {"candidate_sha256": evidence.sha256(first), "profile": "main"}
    first_path = evidence.retain_measurement(source, first, context, {"score": 0})
    source.write_bytes(second)
    with pytest.raises(ValueError, match="changed during measurement"):
        evidence.retain_measurement(source, first, context, {"score": 0})
    second_path = evidence.retain_measurement(
        source, second, {"profile": "main"}, {"score": 5}
    )
    assert first_path != second_path
    assert (tmp_path / first_path).with_suffix(".c").read_bytes() == first
    assert (tmp_path / second_path).with_suffix(".c").read_bytes() == second
    assert json.loads((tmp_path / first_path).read_text())["authority"] == "unbanked"



def test_real_compile_failure_retains_diagnostic_and_removes_stale_object(
    tmp_path, monkeypatch
):
    import decomp_loop as loop
    from rom_build import tool_path

    if not Path(tool_path("gcc")).is_file():
        pytest.skip("build the vendored compiler before running diagnostic tests")
    monkeypatch.setattr(loop, "NONMATCHINGS_DIR", tmp_path)
    directory = tmp_path / "invalid"
    directory.mkdir()
    (directory / "base.c").write_text(
        "int invalid(void) { return definitely_missing; }\n"
    )
    stale = directory / "current.o"
    stale.write_bytes(b"old object must not survive")
    ok, diagnostic, output = loop.compile_candidate("invalid", loop.CompileProfile())
    assert ok is False
    assert "definitely_missing" in diagnostic
    assert not output.exists()

