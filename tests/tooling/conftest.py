"""Shared fixtures for the decomp-tooling characterization suite.

Tool scripts under tools/ are not a package, so we load them by file path. The
suite runs under the uv-managed .venv (via `uv run --locked pytest`), so the
scripts' venv re-exec guard is a no-op here.
"""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
from rom_build import python_path
GOLDEN = Path(__file__).resolve().parent / "golden"

# Set REGEN_GOLDEN=1 to rewrite golden snapshots instead of asserting against them.
REGEN = os.environ.get("REGEN_GOLDEN") == "1"


def load_tool(name: str):
    """Import a tools/<name>.py module by path (not via sys.path)."""
    # Tool modules import their shared helper as `import decomp_common`; make
    # tools/ importable so that resolves during tests.
    if str(TOOLS) not in sys.path:
        sys.path.insert(0, str(TOOLS))
    spec = importlib.util.spec_from_file_location(f"tool_{name}", TOOLS / f"{name}.py")
    assert spec and spec.loader, f"cannot load tools/{name}.py"
    mod = importlib.util.module_from_spec(spec)
    # Register before exec so decorators that introspect sys.modules (e.g.
    # @dataclass resolving cls.__module__) resolve correctly.
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def run_tool(name: str, *args: str) -> subprocess.CompletedProcess:
    """Run tools/<name>.py under the uv-managed project Python interpreter."""
    return subprocess.run(
        [str(python_path()), str(TOOLS / f"{name}.py"), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


@pytest.fixture(scope="session")
def root() -> Path:
    return ROOT


@pytest.fixture(scope="session")
def golden_dir() -> Path:
    GOLDEN.mkdir(parents=True, exist_ok=True)
    return GOLDEN


@pytest.fixture
def regen() -> bool:
    return REGEN
