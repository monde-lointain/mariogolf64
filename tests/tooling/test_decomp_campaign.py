"""Consumer-visible identity, retention, reference freshness and deadline boundaries."""

from __future__ import annotations

import json
import subprocess
import sys
import time
from dataclasses import replace
from pathlib import Path

import pytest
from conftest import load_tool

refs = load_tool("decomp_references")
budget = load_tool("decomp_budget")
campaign = load_tool("decomp_campaign")
evidence = load_tool("decomp_evidence")
ROM = "a" * 40


def checkout(root: Path, *, rom: str = ROM):
    root.mkdir(parents=True)
    (root / "mariogolf64.yaml").write_text(
        f"sha1: {rom}\nsegments:\n"
        "  - {name: main, type: code, start: 4096}\n"
        "  - {name: overlay_3, type: code, start: 8192}\n"
        "  - [12288]\n"
    )
    (root / "src/main").mkdir(parents=True)
    (root / "build/us/work/asm/main").mkdir(parents=True)
    return root


def assembly(root: Path, name: str, rom=0x1000, immediate="0007", stem="pack"):
    path = root / f"build/us/work/asm/main/{stem}.s"
    path.write_text(
        f"glabel {name}\n"
        f"/* {rom:X} 801F4A30 03E00008 */ jr $ra\n"
        f"/* {rom + 4:X} 801F4A34 2402{immediate} */ addiu $v0, $zero, 0x{immediate}\n"
        f"endlabel {name}\n"
    )
    return path


def test_identity_survives_rename_but_separates_reused_overlay_vram(tmp_path):
    root = checkout(tmp_path / "tree")
    assembly(root, "first", 0x1000)
    assembly(root, "second", 0x2000, stem="other")
    functions = refs.function_index(root)
    left = refs.find_function(functions, "first")
    right = refs.find_function(functions, "second")
    assert left.vram == right.vram
    assert left.identity(ROM) != right.identity(ROM)
    renamed = replace(left, name="new_name", segment="libultra/renamed")
    assert left.identity(ROM) == renamed.identity(ROM)
    assert campaign.shard_number(left.identity(ROM), 4) == campaign.shard_number(
        renamed.identity(ROM), 4
    )
    assert left.words == ("03E00008", "24020007")  # Includes the return delay slot.


def test_ambiguous_function_name_is_not_resolved_by_first_file(tmp_path):
    root = checkout(tmp_path / "tree")
    assembly(root, "same", 0x1000)
    assembly(root, "same", 0x2000, stem="other")
    with pytest.raises(ValueError, match="found 2"):
        refs.find_function(refs.function_index(root), "same")


def test_new_worktree_reference_appears_without_merge_or_stale_cache(tmp_path):
    active = checkout(tmp_path / "active")
    sibling = checkout(tmp_path / "sibling")
    assembly(active, "target")
    assembly(sibling, "reference", 0x1100)
    query = refs.function_index(active)[0]
    entries, _ = refs.reference_index([sibling], ROM)
    assert refs.rank_references(query, ROM, entries) == []
    source = sibling / "src/main/pack.c"
    source.write_text("int reference(void) { return 7; }\n")
    entries, coverage = refs.reference_index([sibling], ROM)
    result = refs.rank_references(query, ROM, entries)[0]
    assert result["source"] == "src/main/pack.c"
    assert result["root"] == str(sibling)
    assert result["similarity"] == 1
    assert coverage[0]["available"] is True
    old_hash = result["source_sha256"]
    source.write_text("int reference(void) { return 8; }\n")
    entries, _ = refs.reference_index([sibling], ROM)
    assert refs.rank_references(query, ROM, entries)[0]["source_sha256"] != old_hash


def test_other_worktree_same_target_is_a_reference_not_excluded(tmp_path):
    active = checkout(tmp_path / "active")
    sibling = checkout(tmp_path / "sibling")
    assembly(active, "target")
    assembly(sibling, "renamed_target")
    (sibling / "src/main/pack.c").write_text("int renamed_target(void) { return 7; }")
    entries, _ = refs.reference_index([sibling], ROM)
    query = refs.function_index(active)[0]
    result = refs.rank_references(query, ROM, entries)[0]
    assert result["identity"] == query.identity(ROM)
    assert result["func"] == "renamed_target"


def test_partial_c_and_comment_stubs_are_distinguished(tmp_path):
    root = checkout(tmp_path / "tree")
    (root / "src/main/pack.c").write_text(
        'INCLUDE_ASM("asm/nonmatchings", partial);\n'
        "#ifdef NONMATCHING\nint partial(void) { return 7; }\n#endif\n"
        '// INCLUDE_ASM("asm/nonmatchings", complete);\n'
        "int complete(void) { return 8; }\n"
    )
    assert set(refs.c_sources(root)) == {"complete"}


def test_reference_coverage_reports_incompatible_rom_and_missing_extraction(tmp_path):
    wrong = checkout(tmp_path / "wrong", rom="b" * 40)
    missing = tmp_path / "missing"
    missing.mkdir()
    (missing / "mariogolf64.yaml").write_text(f"sha1: {ROM}\n")
    entries, coverage = refs.reference_index([wrong, missing], ROM)
    assert entries == []
    assert [row["available"] for row in coverage] == [False, False]
    assert "different ROM" in coverage[0]["reason"]
    assert "extract reference assembly" in coverage[1]["reason"]
    query = refs.Function(
        "empty", "main/empty", 0x1000, 0, (), (), frozenset(), missing, 1
    )
    assert refs.similarity(query, query) is None


@pytest.mark.parametrize("index", [1, 2, 3, 4])
def test_shards_partition_target_identities(index):
    identities = [f"{ROM}:overlay_3:{offset:X}" for offset in range(1000)]
    selected = {
        identity
        for identity in identities
        if campaign.shard_number(identity, 4) == index
    }
    rest = {
        identity
        for identity in identities
        if campaign.shard_number(identity, 4) != index
    }
    assert selected.isdisjoint(rest)
    assert selected | rest == set(identities)
    assert all(1 <= campaign.shard_number(identity, 4) <= 4 for identity in identities)


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


def test_prepass_preserves_existing_candidate_before_any_build(tmp_path, monkeypatch):
    monkeypatch.setattr(campaign.dc, "NONMATCHINGS_DIR", tmp_path)
    directory = tmp_path / "target"
    directory.mkdir()
    source = directory / "base.c"
    source.write_text("int target(void) { return 42; }\n")
    outcome = campaign.prepass({"func": "target"}, 10)
    assert outcome["status"] == "existing-candidate-preserved"
    assert source.read_text() == "int target(void) { return 42; }\n"


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


def test_ambiguous_reference_name_cannot_borrow_one_c_definition(tmp_path):
    root = checkout(tmp_path / "tree")
    assembly(root, "same", 0x1000)
    assembly(root, "same", 0x2000, stem="other")
    (root / "src/main/pack.c").write_text("int same(void) { return 7; }\n")
    entries, coverage = refs.reference_index([root], ROM)
    assert entries == []
    assert coverage[0]["ambiguous_labels"] == ["same"]


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


def test_explicit_pool_coupled_target_is_classified_and_prepass_skipped(
    tmp_path, monkeypatch, capsys
):
    import decomp_loop as loop
    import pick_target_score as scoring
    import rom_build

    root = checkout(tmp_path / "tree")
    asm = root / "build/us/work/asm"
    path = asm / "main/pool.s"
    path.write_text(
        "glabel pool\n/* 1000 80001000 3C048001 */ lui $a0, %hi(D_80010000)\n"
        + "".join(
            f"/* {0x1004 + i * 8:X} {0x80001004 + i * 8:X} 8C880000 */ lw $t0, 0x{i * 4:X}($a0)\n"
            f"/* {0x1008 + i * 8:X} {0x80001008 + i * 8:X} AFA80000 */ sw $t0, 0x{i * 4:X}($sp)\n"
            for i in range(3)
        )
        + "endlabel pool\n"
    )
    (asm / "data").mkdir()
    (asm / "data/shared.rodata.s").write_text("dlabel D_80010000\n.word 7\n")
    (root / "src/main/pool.c").write_text(
        'INCLUDE_ASM("asm/nonmatchings/main", pool);\n'
    )
    monkeypatch.setattr(rom_build, "ROOT", root)
    monkeypatch.setattr(campaign.dc, "ROOT_DIR", root)
    monkeypatch.setattr(campaign.dc, "NONMATCHINGS_DIR", root / "nonmatchings")
    monkeypatch.setattr(loop, "ROOT_DIR", root)
    monkeypatch.setattr(scoring, "_ASM_ROOT", str(asm))
    monkeypatch.setattr(scoring, "ASM_DATA_DIR", asm / "data")
    monkeypatch.setattr(
        sys, "argv", ["decomp_campaign.py", "--func", "pool", "-n", "1", "--prepass"]
    )
    campaign.main()
    candidate = json.loads(capsys.readouterr().out)["queue"][0]
    assert candidate["pool_blocked"] == "pool-blocked"
    assert candidate["prepass"]["status"] == "coupled-or-intrinsic-skipped"
    assert not (root / "nonmatchings/pool").exists()
