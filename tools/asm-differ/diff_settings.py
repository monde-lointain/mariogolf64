from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from rom_build import python_path, tool_path


def apply(config, args):
    config["baseimg"] = "target.bin"
    config["myimg"] = "source.bin"
    config["mapfile"] = "build.map"
    config["source_directories"] = ["."]
    config["make_command"] = [
        str(python_path()),
        str(ROOT / "tools" / "rom_build.py"),
        "build",
    ]
    config["objdump_executable"] = tool_path("objdump")
